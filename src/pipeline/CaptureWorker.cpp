#include "CaptureWorker.h"

#include "../capture/DxgiCapturer.h"
#include "../capture/GdiCapturer.h"
#include "../codec/VideoEncoder.h"

#include <QDebug>

#include <algorithm>
#include <chrono>

namespace sc {

namespace {
double msSince(const std::chrono::steady_clock::time_point &t0)
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now() - t0).count() / 1e6;
}

int64_t nowUs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

CaptureWorker::CaptureWorker(Config config, QObject *parent)
    : QObject(parent)
    , config_(std::move(config))
{
    // 定时器必须挂在 worker 自己名下，moveToThread 才会一起迁到工作线程
    timer_ = new QTimer(this);
    timer_->setTimerType(Qt::PreciseTimer);
    timer_->setSingleShot(true);  // 每帧结束后按剩余预算重新排程
    connect(timer_, &QTimer::timeout, this, &CaptureWorker::tick);
}

CaptureWorker::~CaptureWorker()
{
    stop();
}

void CaptureWorker::start()
{
    if (running_)
        return;

    if (config_.captureMode == "dxgi") {
        auto dxgi = std::make_unique<DxgiCapturer>();
        if (dxgi->start()) {
            capturer_ = std::move(dxgi);
        } else {
            emit errorOccurred(QStringLiteral("DXGI 初始化失败，回退到 GDI"));
        }
    }
    if (!capturer_) {
        auto gdi = std::make_unique<GdiCapturer>();
        if (!gdi->start()) {
            emit errorOccurred(QStringLiteral("GDI 采集初始化失败"));
            return;
        }
        capturer_ = std::move(gdi);
    }

    encoder_ = std::make_unique<VideoEncoder>();
    bitrateKbps_ = config_.bitrateKbps;
    if (!encoder_->open(config_.width, config_.height, config_.fps,
                        config_.bitrateKbps, config_.encoder == "h265",
                        config_.keyframeInterval)) {
        emit errorOccurred(QStringLiteral("编码器初始化失败（libx264 是否可用？）"));
        capturer_.reset();
        return;
    }

    frames_ = 0;
    fpsClock_.start();
    statClock_.start();
    running_ = true;
    timer_->start(1000 / config_.fps);

    emit started(QString::fromLatin1(capturer_->name()), encoder_->width(), encoder_->height());
}

void CaptureWorker::stop()
{
    if (!running_ && !capturer_)
        return;

    running_ = false;
    if (timer_)
        timer_->stop();
    if (encoder_) {
        encoder_->close();
        encoder_.reset();
    }
    if (capturer_) {
        capturer_->stop();
        capturer_.reset();
    }
}

void CaptureWorker::updateBitrate(int kbps)
{
    if (!encoder_)
        return;
    if (encoder_->updateBitrate(kbps)) {
        bitrateKbps_ = kbps;
        qInfo() << "bitrate updated ->" << kbps << "kbps";
    }
}

void CaptureWorker::tick()
{
    if (!running_ || !capturer_ || !encoder_)
        return;

    const auto t0 = std::chrono::steady_clock::now();
    const int64_t captureUs = nowUs();  // 采集开始时刻，接收端据此算端到端
    if (!capturer_->capture(frame_, 0)) {
        scheduleNext(t0);               // 桌面没变化，跳过这一拍但保持节奏
        return;
    }
    const auto t1 = std::chrono::steady_clock::now();

    std::vector<Packet> packets;
    if (!encoder_->encode(frame_, packets)) {
        emit errorOccurred(QStringLiteral("编码失败"));
        scheduleNext(t0);
        return;
    }
    const auto t2 = std::chrono::steady_clock::now();

    captureMs_ = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count() / 1e6;
    encodeMs_ = std::chrono::duration_cast<std::chrono::nanoseconds>(t2 - t1).count() / 1e6;

    const quint16 w = static_cast<quint16>(encoder_->width());
    const quint16 h = static_cast<quint16>(encoder_->height());

    for (Packet &packet : packets) {
        // pts 用采集时间，接收端可以据此算端到端延迟
        packet.pts = captureUs;
        emit packetReady(std::move(packet), w, h);
    }

    ++frames_;
    if (statClock_.elapsed() >= 1000) {
        const int fps = static_cast<int>(frames_ * 1000.0 / statClock_.elapsed());
        emit statsUpdated(fps, captureMs_, encodeMs_, bitrateKbps_);
        frames_ = 0;
        statClock_.restart();
    }

    scheduleNext(t0);
}

void CaptureWorker::scheduleNext(const std::chrono::steady_clock::time_point &frameStart)
{
    if (!running_ || timer_ == nullptr)
        return;

    // 固定 interval 会把「处理耗时」叠加到帧周期上：33ms 定时 + 12ms 处理 = 45ms/帧（22fps）。
    // 这里按本帧已用时间补偿，保证开始时刻之间的间隔稳定在 1000/fps。
    const int period = (std::max)(1, 1000 / (std::max)(1, config_.fps));
    const int worked = static_cast<int>(msSince(frameStart));
    timer_->start((std::max)(1, period - worked));
}

} // namespace sc
