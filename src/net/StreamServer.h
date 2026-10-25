#pragma once

#include "../core/Types.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QTcpServer>

class QTcpSocket;

namespace sc {

// 发送端：监听端口，把编码包广播给所有客户端
class StreamServer : public QObject {
    Q_OBJECT
public:
    explicit StreamServer(QObject *parent = nullptr);
    ~StreamServer() override;

    bool listen(quint16 port);
    void close();
    bool isListening() const { return server_.isListening(); }
    quint16 port() const { return server_.serverPort(); }

    int clientCount() const { return clients_.size(); }
    qint64 pendingBytes() const;

    void sendConfig(quint16 width, quint16 height);
    void sendPacket(const Packet &packet, quint16 width, quint16 height);

signals:
    void clientsChanged(int count);
    void logMessage(const QString &text);

private slots:
    void onNewConnection();
    void onDisconnected();

private:
    static constexpr qint64 kHighWaterMark = 1 * 1024 * 1024;   // 单连接积压上限

    QTcpServer server_;
    QList<QTcpSocket *> clients_;
    quint16 lastWidth_ = 0;
    quint16 lastHeight_ = 0;
    int droppedFrames_ = 0;
};

} // namespace sc
