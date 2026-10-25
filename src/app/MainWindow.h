#pragma once

#include "core/Config.h"
#include "core/Types.h"

#include <QMainWindow>
#include <QElapsedTimer>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QThread;
class QTimer;

namespace sc {
class CaptureWorker;
class StreamClient;
class StreamServer;
class VideoWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    // 演示模式：自动开始推流并回环拉流（host 可传局域网地址）
    void startDemo(const QString &host = QStringLiteral("127.0.0.1"));

    // 抓取当前预览帧（用于无头验证渲染是否正常）
    void grabPreview(const QString &path);

private slots:
    void startStream();
    void stopStream();
    void toggleConnect();
    void startLocalDemo();
    void startLocalDemo(const QString &host);

    void onPacket(sc::Packet packet, quint16 width, quint16 height);
    void onFrame(sc::Frame frame);
    void onWorkerStarted(const QString &capturer, int width, int height);
    void onWorkerStats(int fps, double captureMs, double encodeMs, int bitrateKbps);
    void onClientsChanged(int count);
    void onLog(const QString &text);
    void adaptBitrate();

private:
    void buildUi();
    void setStreaming(bool on);
    void refreshLatency();

    Config config_;
    StreamServer *server_ = nullptr;
    StreamClient *client_ = nullptr;
    VideoWidget *video_ = nullptr;

    QThread *workerThread_ = nullptr;
    CaptureWorker *worker_ = nullptr;

    QPushButton *streamButton_ = nullptr;
    QPushButton *connectButton_ = nullptr;
    QPushButton *demoButton_ = nullptr;
    QLineEdit *hostEdit_ = nullptr;
    QSpinBox *portSpin_ = nullptr;
    QPlainTextEdit *logView_ = nullptr;

    QLabel *captureLabel_ = nullptr;
    QLabel *encodeLabel_ = nullptr;
    QLabel *sendLabel_ = nullptr;
    QLabel *decodeLabel_ = nullptr;
    QLabel *latencyLabel_ = nullptr;
    QLabel *stateLabel_ = nullptr;

    QTimer *latencyTimer_ = nullptr;
    QTimer *bitrateTimer_ = nullptr;

    int currentBitrate_ = 0;
    int lastSentBytes_ = 0;
    int lastFps_ = 0;
    QElapsedTimer statLogClock_;
};

} // namespace sc
