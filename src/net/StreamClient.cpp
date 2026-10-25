#include "StreamClient.h"

#include "Protocol.h"

#include <QDateTime>

#include <chrono>
#include <cstring>

namespace sc {

namespace {
int64_t nowUs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
} // namespace

StreamClient::StreamClient(QObject *parent)
    : QObject(parent)
{
    connect(&socket_, &QTcpSocket::readyRead, this, &StreamClient::onReadyRead);
    connect(&socket_, &QTcpSocket::connected, this, &StreamClient::onConnected);
    connect(&socket_, &QTcpSocket::disconnected, this, &StreamClient::onDisconnected);
    connect(&socket_, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit logMessage(QStringLiteral("连接错误：%1").arg(socket_.errorString()));
    });
}

StreamClient::~StreamClient()
{
    socket_.abort();
}

void StreamClient::connectTo(const QString &host, quint16 port)
{
    buffer_.clear();
    decoder_.close();
    socket_.abort();
    socket_.connectToHost(host, port);
}

void StreamClient::disconnectFromHost()
{
    socket_.abort();
    buffer_.clear();
}

void StreamClient::onConnected()
{
    socket_.setSocketOption(QAbstractSocket::LowDelayOption, 1);
    emit connected();
    emit logMessage(QStringLiteral("已连接服务器 %1:%2").arg(socket_.peerName()).arg(socket_.peerPort()));
}

void StreamClient::onDisconnected()
{
    emit disconnected();
    emit logMessage(QStringLiteral("与服务器断开"));
}

void StreamClient::onReadyRead()
{
    buffer_.append(socket_.readAll());
    processBuffer();
}

void StreamClient::processBuffer()
{
    // 「读头 -> 读体」状态机，天然处理粘包/半包
    while (buffer_.size() >= static_cast<int>(kHeaderSize)) {
        FrameHeader header;
        if (!decodeHeader(reinterpret_cast<const uint8_t *>(buffer_.constData()),
                          static_cast<size_t>(buffer_.size()), header)) {
            emit logMessage(QStringLiteral("协议错误，断开连接"));
            buffer_.clear();
            socket_.abort();
            return;
        }

        const int total = static_cast<int>(kHeaderSize + header.payloadSize);
        if (buffer_.size() < total)
            return;   // 半包，等下一次 readyRead

        const QByteArray payload = buffer_.mid(static_cast<int>(kHeaderSize),
                                               static_cast<int>(header.payloadSize));
        buffer_.remove(0, total);

        if (header.type == MsgType::VideoConfig) {
            width_ = header.width;
            height_ = header.height;
            emit configReceived(width_, height_);
            decoder_.open(false);
            continue;
        }

        if (header.type != MsgType::VideoFrame || payload.isEmpty())
            continue;

        if (!decoder_.isOpen())
            decoder_.open(false);

        Packet packet;
        packet.data.assign(reinterpret_cast<const uint8_t *>(payload.constData()),
                           reinterpret_cast<const uint8_t *>(payload.constData()) + payload.size());
        packet.pts = header.pts;
        packet.keyframe = (header.flags & FlagKeyFrame) != 0;

        const int64_t t0 = nowUs();
        std::vector<Frame> frames;
        if (!decoder_.decode(packet, frames))
            continue;

        for (Frame &frame : frames) {
            ++decodedFrames_;
            const double decodeMs = (nowUs() - t0) / 1000.0;
            lastDecodeMs_ = decodeMs;
            lastLatencyMs_ = decodeMs;
            if (header.pts > 0) {
                const double e2e = (nowUs() - header.pts) / 1000.0;
                if (e2e > 0 && e2e < 10000)
                    lastLatencyMs_ = e2e;
            }
            emit frameReady(std::move(frame));
        }
    }
}

} // namespace sc
