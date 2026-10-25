#include "Config.h"

#include <QSettings>

namespace sc {

namespace {
const char *kDefaultPath = "config.ini";
}

Config Config::load(const QString &path)
{
    Config cfg;
    QSettings settings(path.isEmpty() ? QString::fromLatin1(kDefaultPath) : path,
                       QSettings::IniFormat);

    cfg.width        = settings.value("capture/width", cfg.width).toInt();
    cfg.height       = settings.value("capture/height", cfg.height).toInt();
    cfg.fps          = settings.value("capture/fps", cfg.fps).toInt();
    cfg.captureMode  = settings.value("capture/mode", cfg.captureMode).toString();

    cfg.bitrateKbps  = settings.value("encode/bitrateKbps", cfg.bitrateKbps).toInt();
    cfg.adaptiveBitrate = settings.value("encode/adaptive", cfg.adaptiveBitrate).toBool();
    cfg.encoder      = settings.value("encode/codec", cfg.encoder).toString();
    cfg.keyframeInterval = settings.value("encode/gop", cfg.keyframeInterval).toInt();

    cfg.port         = static_cast<quint16>(settings.value("net/port", cfg.port).toInt());
    cfg.host         = settings.value("net/host", cfg.host).toString();

    cfg.sanitize();
    return cfg;
}

void Config::save(const QString &path) const
{
    QSettings settings(path.isEmpty() ? QString::fromLatin1(kDefaultPath) : path,
                       QSettings::IniFormat);

    settings.setValue("capture/width", width);
    settings.setValue("capture/height", height);
    settings.setValue("capture/fps", fps);
    settings.setValue("capture/mode", captureMode);

    settings.setValue("encode/bitrateKbps", bitrateKbps);
    settings.setValue("encode/adaptive", adaptiveBitrate);
    settings.setValue("encode/codec", encoder);
    settings.setValue("encode/gop", keyframeInterval);

    settings.setValue("net/port", port);
    settings.setValue("net/host", host);
}

void Config::sanitize()
{
    width  = qBound(320, width, 7680);
    height = qBound(240, height, 4320);
    // 编码器要求宽高为偶数
    width  &= ~1;
    height &= ~1;

    fps            = qBound(1, fps, 120);
    bitrateKbps    = qBound(500, bitrateKbps, 100000);
    keyframeInterval = qBound(1, keyframeInterval, 600);
    if (port == 0)
        port = 9527;
    if (captureMode != "dxgi" && captureMode != "gdi")
        captureMode = "dxgi";
    if (encoder != "h264" && encoder != "h265")
        encoder = "h264";
}

} // namespace sc
