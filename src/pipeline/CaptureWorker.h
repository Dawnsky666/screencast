#pragma once

#include "../core/Config.h"
#include "../core/Types.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <memory>

namespace sc {

class ICapturer;
class VideoEncoder;

// 采集 + 编码 worker，跑在独立线程里，避免阻塞 UI
class CaptureWorker : public QObject {
    Q_OBJECT
public:
    explicit CaptureWorker(Config config, QObject *parent = nullptr);
    ~CaptureWorker() override;

public slots:
    void start();
    void stop();
    void updateBitrate(int kbps);

signals:
    void packetReady(sc::Packet packet, quint16 width, quint16 height);
    void started(const QString &capturerName, int width, int height);
    void statsUpdated(int fps, double captureMs, double encodeMs, int bitrateKbps);
    void errorOccurred(const QString &text);

private slots:
    void tick();

private:
    void scheduleNext(const std::chrono::steady_clock::time_point &frameStart);

    Config config_;
    std::unique_ptr<ICapturer> capturer_;
    std::unique_ptr<VideoEncoder> encoder_;

    QTimer *timer_ = nullptr;
    QElapsedTimer fpsClock_;
    QElapsedTimer statClock_;
    Frame frame_;

    int frames_ = 0;
    int bitrateKbps_ = 0;
    double captureMs_ = 0.0;
    double encodeMs_ = 0.0;
    bool running_ = false;
};

} // namespace sc
