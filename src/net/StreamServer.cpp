#include "StreamServer.h"

#include "Protocol.h"

#include <QTcpSocket>

namespace sc {

StreamServer::StreamServer(QObject *parent)
    : QObject(parent)
{
    connect(&server_, &QTcpServer::newConnection, this, &StreamServer::onNewConnection);
}

StreamServer::~StreamServer()
{
    close();
}

bool StreamServer::listen(quint16 port)
{
    if (server_.isListening())
        return true;
    return server_.listen(QHostAddress::Any, port);
}

void StreamServer::close()
{
    for (QTcpSocket *socket : clients_) {
        socket->disconnect(this);
        socket->abort();
        socket->deleteLater();
    }
    clients_.clear();
    server_.close();
    emit clientsChanged(0);
}

qint64 StreamServer::pendingBytes() const
{
    qint64 total = 0;
    for (QTcpSocket *socket : clients_)
        total += socket->bytesToWrite();
    return total;
}

void StreamServer::onNewConnection()
{
    while (QTcpSocket *socket = server_.nextPendingConnection()) {
        socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
        socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
        clients_.append(socket);
        connect(socket, &QTcpSocket::disconnected, this, &StreamServer::onDisconnected);
        emit clientsChanged(clients_.size());
        emit logMessage(QStringLiteral("客户端接入：%1").arg(socket->peerAddress().toString()));
    }
}

void StreamServer::onDisconnected()
{
    auto *socket = qobject_cast<QTcpSocket *>(sender());
    if (!socket)
        return;
    clients_.removeAll(socket);
    socket->deleteLater();
    emit clientsChanged(clients_.size());
    emit logMessage(QStringLiteral("客户端断开"));
}

void StreamServer::sendConfig(quint16 width, quint16 height)
{
    lastWidth_ = width;
    lastHeight_ = height;

    FrameHeader header;
    header.type = MsgType::VideoConfig;
    header.width = width;
    header.height = height;
    header.pts = 0;
    header.payloadSize = 0;

    const auto bytes = encodeHeader(header);
    for (QTcpSocket *socket : clients_)
        socket->write(reinterpret_cast<const char *>(bytes.data()), static_cast<qint64>(bytes.size()));
}

void StreamServer::sendPacket(const Packet &packet, quint16 width, quint16 height)
{
    if (clients_.isEmpty() || packet.data.empty())
        return;

    if (width != lastWidth_ || height != lastHeight_)
        sendConfig(width, height);

    FrameHeader header;
    header.type = MsgType::VideoFrame;
    header.flags = packet.keyframe ? FlagKeyFrame : 0;
    header.width = width;
    header.height = height;
    header.pts = packet.pts;
    header.payloadSize = static_cast<uint32_t>(packet.data.size());

    const auto head = encodeHeader(header);

    for (QTcpSocket *socket : clients_) {
        // 网络跟不上就丢帧，而不是无限堆积内存
        if (socket->bytesToWrite() > kHighWaterMark) {
            ++droppedFrames_;
            continue;
        }
        socket->write(reinterpret_cast<const char *>(head.data()), static_cast<qint64>(head.size()));
        socket->write(reinterpret_cast<const char *>(packet.data.data()),
                      static_cast<qint64>(packet.data.size()));
    }
}

} // namespace sc
