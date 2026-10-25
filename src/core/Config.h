#pragma once

#include <QString>

namespace sc {

// 全局配置：参数全部有默认值，可通过 ini 覆盖
struct Config {
    int width = 1920;
    int height = 1080;
    int fps = 30;
    int bitrateKbps = 8000;
    bool adaptiveBitrate = true;
    quint16 port = 9527;
    QString captureMode = QStringLiteral("dxgi");   // dxgi | gdi
    QString encoder = QStringLiteral("h264");       // h264 | h265
    int keyframeInterval = 60;                      // GOP，帧数
    QString host = QStringLiteral("127.0.0.1");

    static Config load(const QString &path = QString());
    void save(const QString &path = QString()) const;

    // 简单合法性修正，避免 0 或越界值把编码器搞崩
    void sanitize();
};

} // namespace sc
