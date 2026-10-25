#pragma once

#include "../core/Types.h"

#include <vector>

struct AVCodecContext;
struct AVFrame;
struct AVPacket;
struct SwsContext;

namespace sc {

// H.264 / H.265 解码器：输入 Annex-B 包，输出 YUV420P 帧
class VideoDecoder {
public:
    VideoDecoder() = default;
    ~VideoDecoder();

    bool open(bool hevc);
    void close();
    bool isOpen() const { return ctx_ != nullptr; }

    // 一包可能解出 0~N 帧（例如首包带 SPS/PPS 时可能先出延迟帧）
    bool decode(const Packet &packet, std::vector<Frame> &out);

    int width() const { return width_; }
    int height() const { return height_; }

private:
    bool convert(AVFrame *src, Frame &dst);

    AVCodecContext *ctx_ = nullptr;
    AVFrame *frame_ = nullptr;
    AVPacket *pkt_ = nullptr;
    SwsContext *sws_ = nullptr;
    int swsW_ = 0;
    int swsH_ = 0;
    int swsFmt_ = -1;
    int width_ = 0;
    int height_ = 0;
    bool hevc_ = false;
};

} // namespace sc
