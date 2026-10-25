#pragma once

#include "../core/Types.h"

#include <cstdint>
#include <string>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace sc {

// H.264 / H.265 编码器：输入 BGRA，输出 Annex-B 码流
class VideoEncoder {
public:
    VideoEncoder() = default;
    ~VideoEncoder();

    bool open(int width, int height, int fps, int bitrateKbps, bool hevc, int gop);
    void close();
    bool isOpen() const { return ctx_ != nullptr; }

    // 返回本次产生的包（一帧输入可能吐出 0~N 个包）
    bool encode(const Frame &bgra, std::vector<Packet> &out);

    // 动态码率：x264 不支持运行期改码率，这里用「重建编码器」实现，
    // 调用方需做好冷却，避免每次上报都重建。
    bool updateBitrate(int bitrateKbps);

    int width() const { return width_; }
    int height() const { return height_; }
    const char *name() const { return codecName_; }

private:
    bool openInternal(int bitrateKbps);
    void releaseContext();

    AVCodecContext *ctx_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVPacket *pkt_ = nullptr;
    SwsContext *sws_ = nullptr;

    int srcWidth_ = 0;
    int srcHeight_ = 0;
    int width_ = 0;
    int height_ = 0;
    int fps_ = 60;
    int bitrateKbps_ = 8000;
    int gop_ = 60;
    bool hevc_ = false;
    int64_t pts_ = 0;
    const char *codecName_ = "h264";
};

} // namespace sc
