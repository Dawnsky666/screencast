#pragma once

#include <cstdint>
#include <vector>

namespace sc {

// 消息类型
enum class MsgType : uint8_t {
    VideoConfig = 1,   // 首包：编码参数（宽高、码率能否解码）
    VideoFrame  = 2,   // 视频帧（payload = 一帧 H.264/H.265 码流）
    Ping        = 3,
    Pong        = 4,
    Bye         = 5,
};

// 帧头标志位
enum FrameFlags : uint8_t {
    FlagKeyFrame = 1u << 0,   // 关键帧
    FlagConfig   = 1u << 1,   // 编解码参数变更
};

/*
 * 线上协议（固定 24 字节头 + 变长 payload，全部小端）：
 *
 *   0  magic       4  "SCST" = 0x54534353
 *   4  version     1
 *   5  type        1   MsgType
 *   6  flags       1   FrameFlags
 *   7  reserved    1
 *   8  width       2
 *  10  height      2
 *  12  pts         8   微秒
 *  20  payloadSize 4
 *  24  payload     N
 *
 * 选择固定长度头 + 显式长度字段，接收端用「读头 → 读体」状态机，
 * 天然解决 TCP 粘包 / 半包，不需要分隔符，也不依赖结构体内存布局。
 */
struct FrameHeader {
    uint8_t  version = 1;
    MsgType  type = MsgType::VideoFrame;
    uint8_t  flags = 0;
    uint16_t width = 0;
    uint16_t height = 0;
    int64_t  pts = 0;
    uint32_t payloadSize = 0;
};

constexpr size_t  kHeaderSize = 24;
constexpr uint32_t kMagic = 0x54534353u;      // "SCST" 小端
constexpr uint8_t  kProtocolVersion = 1;
constexpr uint32_t kMaxPayloadSize = 16u * 1024 * 1024;   // 16MB 上限，防脏数据撑爆内存

std::vector<uint8_t> encodeHeader(const FrameHeader &header);

// 返回 false 表示 magic/version/length 非法，调用方应断开连接
bool decodeHeader(const uint8_t *buf, size_t len, FrameHeader &out);

} // namespace sc
