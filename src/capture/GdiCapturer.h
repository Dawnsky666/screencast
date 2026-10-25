#pragma once

#include "ICapturer.h"

#include <windows.h>

namespace sc {

// GDI BitBlt 采集：兼容性最好，DXGI 初始化失败时兜底
class GdiCapturer : public ICapturer {
public:
    GdiCapturer() = default;
    ~GdiCapturer() override;

    bool start() override;
    void stop() override;
    bool capture(Frame &out, int timeoutMs) override;

    const char *name() const override { return "GDI"; }
    int width() const override { return width_; }
    int height() const override { return height_; }

private:
    void release();

    int width_ = 0;
    int height_ = 0;
    HDC screenDc_ = nullptr;
    HDC memDc_ = nullptr;
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ oldBitmap_ = nullptr;
    uint8_t *bits_ = nullptr;
    BITMAPINFO bmi_{};
};

} // namespace sc
