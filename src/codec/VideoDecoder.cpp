#include "VideoDecoder.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
}

namespace sc {

VideoDecoder::~VideoDecoder()
{
    close();
}

void VideoDecoder::close()
{
    if (ctx_)
        avcodec_free_context(&ctx_);
    if (frame_)
        av_frame_free(&frame_);
    if (pkt_)
        av_packet_free(&pkt_);
    if (sws_) {
        sws_freeContext(sws_);
        sws_ = nullptr;
    }
    ctx_ = nullptr;
    swsW_ = swsH_ = 0;
    swsFmt_ = -1;
    width_ = height_ = 0;
}

bool VideoDecoder::open(bool hevc)
{
    close();
    hevc_ = hevc;

    const AVCodec *codec = hevc ? avcodec_find_decoder(AV_CODEC_ID_HEVC)
                                : avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!codec)
        return false;

    ctx_ = avcodec_alloc_context3(codec);
    if (!ctx_)
        return false;

    ctx_->thread_count = 0;          // 自动多线程解码
    ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;

    if (avcodec_open2(ctx_, codec, nullptr) < 0) {
        close();
        return false;
    }

    frame_ = av_frame_alloc();
    pkt_ = av_packet_alloc();
    if (!frame_ || !pkt_) {
        close();
        return false;
    }
    return true;
}

bool VideoDecoder::convert(AVFrame *src, Frame &dst)
{
    const int w = src->width;
    const int h = src->height;
    if (w <= 0 || h <= 0)
        return false;

    if (src->format == AV_PIX_FMT_YUV420P) {
        dst.width = w;
        dst.height = h;
        dst.stride = w;
        dst.format = PixelFormat::YUV420P;
        dst.pts = src->pts;

        const size_t ySize = static_cast<size_t>(w) * h;
        const size_t cSize = static_cast<size_t>(w / 2) * (h / 2);
        dst.data.resize(ySize + cSize * 2);

        uint8_t *out = dst.data.data();
        for (int y = 0; y < h; ++y)
            std::memcpy(out + static_cast<size_t>(y) * w, src->data[0] + static_cast<size_t>(y) * src->linesize[0], w);
        out += ySize;
        for (int y = 0; y < h / 2; ++y)
            std::memcpy(out + static_cast<size_t>(y) * (w / 2), src->data[1] + static_cast<size_t>(y) * src->linesize[1], w / 2);
        out += cSize;
        for (int y = 0; y < h / 2; ++y)
            std::memcpy(out + static_cast<size_t>(y) * (w / 2), src->data[2] + static_cast<size_t>(y) * src->linesize[2], w / 2);
        return true;
    }

    // 非 420P（例如硬解输出 NV12）统一转一次
    if (!sws_ || swsW_ != w || swsH_ != h || swsFmt_ != src->format) {
        if (sws_)
            sws_freeContext(sws_);
        sws_ = sws_getContext(w, h, static_cast<AVPixelFormat>(src->format),
                              w, h, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
        swsW_ = w;
        swsH_ = h;
        swsFmt_ = src->format;
        if (!sws_)
            return false;
    }

    dst.width = w;
    dst.height = h;
    dst.stride = w;
    dst.format = PixelFormat::YUV420P;
    dst.pts = src->pts;

    const size_t ySize = static_cast<size_t>(w) * h;
    const size_t cSize = static_cast<size_t>(w / 2) * (h / 2);
    dst.data.resize(ySize + cSize * 2);

    uint8_t *planes[4] = { dst.data.data(), dst.data.data() + ySize, dst.data.data() + ySize + cSize, nullptr };
    int strides[4] = { w, w / 2, w / 2, 0 };
    sws_scale(sws_, src->data, src->linesize, 0, h, planes, strides);
    return true;
}

bool VideoDecoder::decode(const Packet &packet, std::vector<Frame> &out)
{
    if (!ctx_ || !frame_ || !pkt_ || packet.data.empty())
        return false;

    av_packet_unref(pkt_);
    if (av_new_packet(pkt_, static_cast<int>(packet.data.size())) < 0)
        return false;
    std::memcpy(pkt_->data, packet.data.data(), packet.data.size());
    pkt_->pts = packet.pts;

    if (avcodec_send_packet(ctx_, pkt_) < 0)
        return false;

    while (true) {
        const int ret = avcodec_receive_frame(ctx_, frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            return false;

        width_ = frame_->width;
        height_ = frame_->height;

        Frame f;
        if (convert(frame_, f))
            out.push_back(std::move(f));
        av_frame_unref(frame_);
    }
    return true;
}

} // namespace sc
