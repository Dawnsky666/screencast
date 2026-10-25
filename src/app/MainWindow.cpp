#include "MainWindow.h"

#include "net/StreamClient.h"
#include "net/StreamServer.h"
#include "pipeline/CaptureWorker.h"
#include "render/VideoWidget.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QImage>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

namespace sc {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    config_ = Config::load();
    currentBitrate_ = config_.bitrateKbps;

    server_ = new StreamServer(this);
    client_ = new StreamClient(this);

    connect(server_, &StreamServer::logMessage, this, &MainWindow::onLog);
    connect(server_, &StreamServer::clientsChanged, this, &MainWindow::onClientsChanged);
    connect(client_, &StreamClient::logMessage, this, &MainWindow::onLog);
    connect(client_, &StreamClient::frameReady, this, &MainWindow::onFrame);

    buildUi();
    setStreaming(false);

    latencyTimer_ = new QTimer(this);
    latencyTimer_->setInterval(500);
    connect(latencyTimer_, &QTimer::timeout, this, &MainWindow::refreshLatency);
    latencyTimer_->start();

    bitrateTimer_ = new QTimer(this);
    bitrateTimer_->setInterval(2000);
    connect(bitrateTimer_, &QTimer::timeout, this, &MainWindow::adaptBitrate);
    bitrateTimer_->start();

    statLogClock_.start();
}

MainWindow::~MainWindow()
{
    stopStream();
}

void MainWindow::startDemo(const QString &host)
{
    startLocalDemo(host);
}

void MainWindow::grabPreview(const QString &path)
{
    const QImage image = video_->grabFramebuffer();
    const bool ok = !image.isNull() && image.save(path);
    qInfo() << "grabPreview" << image.size() << path << (ok ? "ok" : "failed");
    QCoreApplication::quit();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("ScreenCast - 低延迟投屏"));
    resize(1280, 760);

    auto *toolbar = addToolBar(QStringLiteral("控制"));
    toolbar->setMovable(false);

    streamButton_ = new QPushButton(QStringLiteral("开始推流"));
    connectButton_ = new QPushButton(QStringLiteral("连接"));
    demoButton_ = new QPushButton(QStringLiteral("本机演示"));
    toolbar->addWidget(streamButton_);
    toolbar->addSeparator();
    toolbar->addWidget(demoButton_);

    toolbar->addSeparator();
    toolbar->addWidget(new QLabel(QStringLiteral(" 端口 ")));
    portSpin_ = new QSpinBox;
    portSpin_->setRange(1024, 65535);
    portSpin_->setValue(config_.port);
    toolbar->addWidget(portSpin_);

    toolbar->addWidget(new QLabel(QStringLiteral(" 地址 ")));
    hostEdit_ = new QLineEdit(config_.host);
    hostEdit_->setMaximumWidth(140);
    toolbar->addWidget(hostEdit_);
    toolbar->addWidget(connectButton_);

    connect(streamButton_, &QPushButton::clicked, this, [this] {
        if (workerThread_)
            stopStream();
        else
            startStream();
    });
    connect(demoButton_, &QPushButton::clicked, this, [this] { startLocalDemo(); });
    connect(connectButton_, &QPushButton::clicked, this, &MainWindow::toggleConnect);

    video_ = new VideoWidget;

    auto *side = new QWidget;
    auto *sideLayout = new QVBoxLayout(side);
    sideLayout->setContentsMargins(8, 8, 8, 8);

    auto *statsBox = new QGroupBox(QStringLiteral("五段耗时 / 状态"));
    auto *form = new QFormLayout(statsBox);
    captureLabel_ = new QLabel(QStringLiteral("-"));
    encodeLabel_ = new QLabel(QStringLiteral("-"));
    sendLabel_ = new QLabel(QStringLiteral("-"));
    decodeLabel_ = new QLabel(QStringLiteral("-"));
    latencyLabel_ = new QLabel(QStringLiteral("-"));
    stateLabel_ = new QLabel(QStringLiteral("空闲"));
    stateLabel_->setWordWrap(true);
    form->addRow(QStringLiteral("采集"), captureLabel_);
    form->addRow(QStringLiteral("编码"), encodeLabel_);
    form->addRow(QStringLiteral("发送"), sendLabel_);
    form->addRow(QStringLiteral("解码"), decodeLabel_);
    form->addRow(QStringLiteral("端到端"), latencyLabel_);
    form->addRow(QStringLiteral("状态"), stateLabel_);
    sideLayout->addWidget(statsBox);

    logView_ = new QPlainTextEdit;
    logView_->setReadOnly(true);
    logView_->setMaximumBlockCount(500);
    sideLayout->addWidget(logView_, 1);

    auto *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(video_);
    splitter->addWidget(side);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({ 900, 380 });
    setCentralWidget(splitter);

    statusBar()->showMessage(QStringLiteral("就绪"));
}

void MainWindow::setStreaming(bool on)
{
    streamButton_->setText(on ? QStringLiteral("停止推流") : QStringLiteral("开始推流"));
    streamButton_->setEnabled(true);
    demoButton_->setEnabled(!on);
}

void MainWindow::startStream()
{
    if (workerThread_)
        return;

    config_.port = static_cast<quint16>(portSpin_->value());
    config_.host = hostEdit_->text();
    config_.save();

    if (!server_->listen(config_.port)) {
        onLog(QStringLiteral("端口监听失败：%1").arg(config_.port));
        return;
    }

    workerThread_ = new QThread(this);
    worker_ = new CaptureWorker(config_);
    worker_->moveToThread(workerThread_);

    connect(workerThread_, &QThread::finished, worker_, &QObject::deleteLater);
    connect(worker_, &CaptureWorker::packetReady, this, &MainWindow::onPacket, Qt::QueuedConnection);
    connect(worker_, &CaptureWorker::started, this, &MainWindow::onWorkerStarted, Qt::QueuedConnection);
    connect(worker_, &CaptureWorker::statsUpdated, this, &MainWindow::onWorkerStats, Qt::QueuedConnection);
    connect(worker_, &CaptureWorker::errorOccurred, this, &MainWindow::onLog, Qt::QueuedConnection);

    workerThread_->start();
    QMetaObject::invokeMethod(worker_, "start", Qt::QueuedConnection);

    currentBitrate_ = config_.bitrateKbps;
    setStreaming(true);
    onLog(QStringLiteral("开始推流，端口 %1").arg(config_.port));
}

void MainWindow::stopStream()
{
    if (!workerThread_)
        return;

    QMetaObject::invokeMethod(worker_, "stop", Qt::BlockingQueuedConnection);
    workerThread_->quit();
    workerThread_->wait(3000);
    workerThread_ = nullptr;
    worker_ = nullptr;

    server_->close();
    setStreaming(false);
    stateLabel_->setText(QStringLiteral("空闲"));
    onLog(QStringLiteral("已停止推流"));
}

void MainWindow::toggleConnect()
{
    if (client_->isConnected()) {
        client_->disconnectFromHost();
        connectButton_->setText(QStringLiteral("连接"));
        return;
    }
    client_->connectTo(hostEdit_->text(), static_cast<quint16>(portSpin_->value()));
    connectButton_->setText(QStringLiteral("断开"));
}

void MainWindow::startLocalDemo()
{
    startLocalDemo(QStringLiteral("127.0.0.1"));
}

void MainWindow::startLocalDemo(const QString &host)
{
    if (!workerThread_)
        startStream();
    hostEdit_->setText(host);
    if (!client_->isConnected()) {
        client_->connectTo(host, static_cast<quint16>(portSpin_->value()));
        connectButton_->setText(QStringLiteral("断开"));
    }
    onLog(QStringLiteral("回环演示：同进程推流 + 拉流（%1）").arg(host));
}

void MainWindow::onPacket(sc::Packet packet, quint16 width, quint16 height)
{
    server_->sendPacket(packet, width, height);
}

void MainWindow::onFrame(sc::Frame frame)
{
    video_->setFrame(std::move(frame));
}

void MainWindow::onWorkerStarted(const QString &capturer, int width, int height)
{
    stateLabel_->setText(QStringLiteral("推流中 | %1 | %2x%3").arg(capturer).arg(width).arg(height));
    onLog(QStringLiteral("采集器：%1，编码分辨率 %2x%3").arg(capturer).arg(width).arg(height));
}

void MainWindow::onWorkerStats(int fps, double captureMs, double encodeMs, int bitrateKbps)
{
    lastFps_ = fps;
    captureLabel_->setText(QStringLiteral("%1 ms").arg(captureMs, 0, 'f', 2));
    encodeLabel_->setText(QStringLiteral("%1 ms").arg(encodeMs, 0, 'f', 2));
    sendLabel_->setText(QStringLiteral("%1 kbps").arg(bitrateKbps));
    statusBar()->showMessage(QStringLiteral("采集 %1 fps | 编码 %2 ms | 码率 %3 kbps | 客户端 %4")
                                 .arg(fps)
                                 .arg(encodeMs, 0, 'f', 1)
                                 .arg(bitrateKbps)
                                 .arg(server_->clientCount()));
}

void MainWindow::onClientsChanged(int count)
{
    if (count > 0)
        onLog(QStringLiteral("观看端数量：%1").arg(count));
}

void MainWindow::onLog(const QString &text)
{
    logView_->appendPlainText(QStringLiteral("[%1] %2")
                                  .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                                  .arg(text));
}

void MainWindow::refreshLatency()
{
    if (!client_->isConnected()) {
        latencyLabel_->setText(QStringLiteral("-"));
        return;
    }
    latencyLabel_->setText(QStringLiteral("%1 ms").arg(client_->lastLatencyMs(), 0, 'f', 1));
    decodeLabel_->setText(QStringLiteral("%1 ms").arg(client_->lastDecodeMs(), 0, 'f', 1));

    if (statLogClock_.elapsed() > 2000) {
        statLogClock_.restart();
        qInfo() << "STATS fps" << lastFps_ << "encode ms" << encodeLabel_->text()
                << "latency ms" << client_->lastLatencyMs();
    }
}

void MainWindow::adaptBitrate()
{
    if (!worker_ || server_->clientCount() == 0)
        return;

    const qint64 pending = server_->pendingBytes();
    int target = currentBitrate_;

    if (pending > 512 * 1024)
        target = qMax(1000, currentBitrate_ * 3 / 4);
    else if (pending < 64 * 1024)
        target = qMin(config_.bitrateKbps, currentBitrate_ * 5 / 4);

    if (target != currentBitrate_) {
        currentBitrate_ = target;
        QMetaObject::invokeMethod(worker_, "updateBitrate", Qt::QueuedConnection,
                                  Q_ARG(int, target));
        onLog(QStringLiteral("网络积压 %1 KB，码率调整为 %2 kbps")
                  .arg(pending / 1024)
                  .arg(target));
    }
}

} // namespace sc
