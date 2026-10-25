#pragma once

#include <QMetaType>

#include <cstdint>
#include <vector>

namespace sc {

enum class PixelFormat : uint8_t {
    BGRA = 0,
    YUV420P = 1,
};

// 一帧原始图像：采集端输出，编码端输入，渲染端输出
struct Frame {
    int width = 0;
    int height = 0;
    int stride = 0;              // 行跨度（字节），BGRA 时有效
    PixelFormat format = PixelFormat::BGRA;
    int64_t pts = 0;             // 微秒
    std::vector<uint8_t> data;

    bool empty() const { return width <= 0 || height <= 0 || data.empty(); }
    void reset() { width = height = stride = 0; pts = 0; data.clear(); }
};

// 编码后的码流
struct Packet {
    std::vector<uint8_t> data;
    int64_t pts = 0;
    bool keyframe = false;
};

// 五段耗时（毫秒），用于延迟面板
struct StageTimings {
    double captureMs = 0.0;
    double encodeMs = 0.0;
    double sendMs = 0.0;
    double decodeMs = 0.0;
    double renderMs = 0.0;

    double totalMs() const {
        return captureMs + encodeMs + sendMs + decodeMs + renderMs;
    }
};

struct StreamStats {
    int width = 0;
    int height = 0;
    int fps = 0;
    int bitrateKbps = 0;
    int framesSent = 0;
    int framesDropped = 0;
    double avgLatencyMs = 0.0;
    double avgEncodeMs = 0.0;
    double avgDecodeMs = 0.0;
};

} // namespace sc

Q_DECLARE_METATYPE(sc::Frame)
Q_DECLARE_METATYPE(sc::Packet)
