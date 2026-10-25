#include "VideoEncoder.h"

#include <algorithm>
#include <cstring>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswscale/swscale.h>
}

namespace sc {

VideoEncoder::~VideoEncoder()
{
    close();
}

void VideoEncoder::releaseContext()
{
    if (ctx_)
        avcodec_free_context(&ctx_);
    ctx_ = nullptr;
}

void VideoEncoder::close()
{
    releaseContext();
    if (frame_)
        av_frame_free(&frame_);
    if (pkt_)
        av_packet_free(&pkt_);
    if (sws_) {
        sws_freeContext(sws_);
        sws_ = nullptr;
    }
    srcWidth_ = srcHeight_ = 0;
}

bool VideoEncoder::open(int width, int height, int fps, int bitrateKbps, bool hevc, int gop)
{
    close();

    width_ = width;
    height_ = height;
    fps_ = std::max(1, fps);
    bitrateKbps_ = bitrateKbps;
    gop_ = std::max(1, gop);
    hevc_ = hevc;
    codecName_ = hevc ? "h265" : "h264";

    if (!openInternal(bitrateKbps_))
        return false;

    frame_ = av_frame_alloc();
    pkt_ = av_packet_alloc();
    if (!frame_ || !pkt_) {
        close();
        return false;
    }

    frame_->format = ctx_->pix_fmt;
    frame_->width = width_;
    frame_->height = height_;
    if (av_frame_get_buffer(frame_, 32) < 0) {
        close();
        return false;
    }
    return true;
}

bool VideoEncoder::openInternal(int bitrateKbps)
{
    const AVCodec *codec = nullptr;
    if (hevc_) {
        codec = avcodec_find_encoder_by_name("libx265");
        if (!codec)
            codec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
    } else {
        codec = avcodec_find_encoder_by_name("libx264");
        if (!codec)
            codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    }
    if (!codec)
        return false;

    ctx_ = avcodec_alloc_context3(codec);
    if (!ctx_)
        return false;

    ctx_->width = width_;
    ctx_->height = height_;
    ctx_->time_base = AVRational{1, fps_};
    ctx_->framerate = AVRational{fps_, 1};
    ctx_->pix_fmt = AV_PIX_FMT_YUV420P;
    ctx_->bit_rate = static_cast<int64_t>(bitrateKbps) * 1000;
    ctx_->gop_size = gop_;
    ctx_->max_b_frames = 0;                 // 低延迟：不要 B 帧
    ctx_->flags |= AV_CODEC_FLAG_LOW_DELAY;
    ctx_->thread_count = 0;                 // 由编码器自行决定
    ctx_->rc_buffer_size = static_cast<int>(ctx_->bit_rate * 2);

    AVDictionary *opts = nullptr;
    if (!hevc_) {
        av_dict_set(&opts, "preset", "ultrafast", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
        av_dict_set(&opts, "x264-params", "repeat-headers=1:scenecut=0", 0);
    } else {
        av_dict_set(&opts, "preset", "ultrafast", 0);
        av_dict_set(&opts, "tune", "zerolatency", 0);
        const std::string x265Params = "repeat-headers=1:keyint=" + std::to_string(gop_);
        av_dict_set(&opts, "x265-params", x265Params.c_str(), 0);
    }

    const int ret = avcodec_open2(ctx_, codec, &opts);
    av_dict_free(&opts);
    if (ret < 0) {
        releaseContext();
        return false;
    }
    return true;
}

bool VideoEncoder::updateBitrate(int bitrateKbps)
{
    if (!ctx_ || bitrateKbps == bitrateKbps_)
        return false;

    bitrateKbps_ = bitrateKbps;
    // 简单粗暴但有效：重建编码器，强制下一个包是关键帧
    if (!openInternal(bitrateKbps_))
        return false;

    if (frame_)
        av_frame_free(&frame_);
    frame_ = av_frame_alloc();
    if (!frame_) {
        close();
        return false;
    }
    frame_->format = ctx_->pix_fmt;
    frame_->width = width_;
    frame_->height = height_;
    if (av_frame_get_buffer(frame_, 32) < 0) {
        close();
        return false;
    }
    return true;
}

bool VideoEncoder::encode(const Frame &bgra, std::vector<Packet> &out)
{
    if (!ctx_ || !frame_ || !pkt_ || bgra.empty())
        return false;

    if (bgra.width != srcWidth_ || bgra.height != srcHeight_) {
        if (sws_)
            sws_freeContext(sws_);
        sws_ = sws_getContext(bgra.width, bgra.height, AV_PIX_FMT_BGRA,
                              width_, height_, AV_PIX_FMT_YUV420P,
                              SWS_BILINEAR, nullptr, nullptr, nullptr);
        srcWidth_ = bgra.width;
        srcHeight_ = bgra.height;
        if (!sws_)
            return false;
    }

    const uint8_t *srcData[4] = { bgra.data.data(), nullptr, nullptr, nullptr };
    const int srcStride[4] = { bgra.stride, 0, 0, 0 };
    sws_scale(sws_, srcData, srcStride, 0, bgra.height,
              frame_->data, frame_->linesize);

    frame_->pts = pts_++;
    frame_->pict_type = AV_PICTURE_TYPE_NONE;

    if (avcodec_send_frame(ctx_, frame_) < 0)
        return false;

    while (true) {
        const int ret = avcodec_receive_packet(ctx_, pkt_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
            break;
        if (ret < 0)
            return false;

        Packet p;
        p.data.assign(pkt_->data, pkt_->data + pkt_->size);
        p.pts = pkt_->pts;
        p.keyframe = (pkt_->flags & AV_PKT_FLAG_KEY) != 0;
        out.push_back(std::move(p));
        av_packet_unref(pkt_);
    }
    return true;
}

} // namespace sc
