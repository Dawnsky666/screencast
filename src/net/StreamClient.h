#pragma once

#include "../codec/VideoDecoder.h"
#include "../core/Types.h"

#include <QByteArray>
#include <QObject>
#include <QTcpSocket>

namespace sc {

// 接收端：TCP 拆包 -> 解码 -> 甩给渲染控件
class StreamClient : public QObject {
    Q_OBJECT
public:
    explicit StreamClient(QObject *parent = nullptr);
    ~StreamClient() override;

    void connectTo(const QString &host, quint16 port);
    void disconnectFromHost();
    bool isConnected() const { return socket_.state() == QAbstractSocket::ConnectedState; }

    // 最近一帧的端到端延迟（采集开始 -> 解码完成，本机回环有效）
    double lastLatencyMs() const { return lastLatencyMs_; }
    // 最近一帧的解码耗时
    double lastDecodeMs() const { return lastDecodeMs_; }
    int decodedFrames() const { return decodedFrames_; }

signals:
    void connected();
    void disconnected();
    void frameReady(sc::Frame frame);
    void logMessage(const QString &text);
    void configReceived(int width, int height);

private slots:
    void onReadyRead();
    void onConnected();
    void onDisconnected();

private:
    void processBuffer();

    QTcpSocket socket_;
    QByteArray buffer_;
    VideoDecoder decoder_;
    int width_ = 0;
    int height_ = 0;
    int decodedFrames_ = 0;
    double lastLatencyMs_ = 0.0;
    double lastDecodeMs_ = 0.0;
};

} // namespace sc
