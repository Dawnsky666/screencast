# 架构说明

## 1. 数据流

```
Desktop
  │  DXGI AcquireNextFrame / GDI BitBlt
  ▼
Frame(BGRA)  ──sws_scale──▶  Frame(YUV420P)  ──libx264──▶  Packet(H.264 Annex-B)
                                                                   │
                                                        Protocol: 24B header + payload
                                                                   │
                                                                   ▼  TCP
                                                          StreamClient 拆包
                                                                   │
                                                          VideoDecoder(avcodec)
                                                                   │
                                                          Frame(YUV420P, planar)
                                                                   │
                                                          VideoWidget: 3×GL_R8 纹理
                                                                   │
                                                          shader: YUV→RGB (BT.709)
```

## 2. 线程模型

| 线程 | 职责 | 关键点 |
|---|---|---|
| GUI | 事件循环、TCP 收发、解码、渲染 | 解码 1080p 约 3~6 ms，不阻塞交互 |
| worker | 采集 + 编码，`QTimer(1000/fps)` 驱动 | 编码在独立线程，UI 不掉帧 |

跨线程只用两种方式：

- `CaptureWorker::packetReady(Packet, w, h)` 队列连接 → GUI 线程发送
- `QMetaObject::invokeMethod(worker, "updateBitrate", ...)` 队列调用 → 改码率

`Packet` / `Frame` 在 `main.cpp` 注册为 Qt 元类型，队列连接可安全拷贝。

## 3. 关键取舍

**为什么采集端用 DXGI 而不是 GDI？**
Desktop Duplication 直接拿 GPU 纹理，只在桌面变化时返回新帧（`AcquireNextFrame` 超时），
静止画面不占带宽；GDI 每帧都要 BitBlt 一次全屏。DXGI 初始化失败（远程桌面、部分虚拟机）
自动回退 GDI，保证能用。

**为什么用 YUV420P 而不是 RGB？**
1080p BGRA 一帧 8.3 MB，YUV420P 只有 3.1 MB，送进编码器前少一次大内存拷贝；
且 x264 原生吃 YUV420P，省掉编码器内部的转换。

**为什么渲染放在 GPU？**
CPU 逐像素 `YUV→RGB` 在 1080p60 下约 2 亿次运算/秒。三张 `GL_R8` 纹理 + 片元着色器
把转换压到 GPU，CPU 只负责 `glTexSubImage2D` 上传。

**为什么不用 B 帧？**
B 帧需要缓存未来帧，编码延迟 +1~2 帧（16~33 ms）。投屏场景延迟优先，
`max_b_frames = 0` + `tune=zerolatency`。

**为什么自适应码率要重建编码器？**
libx264 的码率参数在 `avcodec_open2` 时固定，FFmpeg 的 wrapper 不支持运行期重配。
本项目在发送队列积压超过 512 KB 时降档、低于 64 KB 时升档，并带 2 秒冷却，
重建成本（约 10~20 ms + 一个关键帧）可以接受。

**为什么固定 24 字节头？**
定长头让接收端只需要「凑齐 24 字节 → 读 length → 凑齐 payload」三步，
不需要扫描分隔符、不依赖结构体内存对齐，也顺手挡住了超大包（16 MB 上限）。

## 4. 延迟预算（1080p30，本机回环实测）

| 阶段 | 典型值 |
|---|---|
| 采集（DXGI 映射 + 行拷贝） | ~10 ms |
| 编码（ultrafast/zerolatency） | 13~14 ms |
| TCP 本机 | < 1 ms |
| 解码 | 2~3 ms |
| 渲染（GPU） | 1~2 ms |
| **端到端** | **17~19 ms** |

## 5. 目录职责

- `core/Types.h`：帧、包、耗时结构，不含平台代码
- `core/Config.*`：ini 配置与参数兜底（宽高取偶、码率上下限）
- `capture/*`：平台采集，`ICapturer` 抽象便于替换
- `codec/*`：FFmpeg 封装，只暴露 `open/encode/decode`
- `net/Protocol.*`：纯序列化，无 socket 依赖，方便单测
- `net/StreamServer.*`：多客户端广播 + 高水位丢帧
- `net/StreamClient.*`：拆包状态机 + 解码
- `pipeline/CaptureWorker.*`：采集编码循环
- `render/VideoWidget.*`：OpenGL 渲染
- `app/MainWindow.*`：UI 与模块编排

## 6. 后续可做

- 鼠标指针合成（`DXGI_OUTDUPL_FRAME_INFO::PointerPosition` + 光标位图）
- 控制回传（键鼠事件反向通道）
- WebSocket 网页观看端
- 硬件编码（NVENC / QSV / AMF）作为可选后端
