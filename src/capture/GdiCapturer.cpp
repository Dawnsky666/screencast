#include "GdiCapturer.h"

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

GdiCapturer::~GdiCapturer()
{
    stop();
}

bool GdiCapturer::start()
{
    release();

    screenDc_ = GetDC(nullptr);
    if (!screenDc_)
        return false;

    width_ = GetDeviceCaps(screenDc_, HORZRES);
    height_ = GetDeviceCaps(screenDc_, VERTRES);
    if (width_ <= 0 || height_ <= 0) {
        release();
        return false;
    }

    memDc_ = CreateCompatibleDC(screenDc_);
    if (!memDc_) {
        release();
        return false;
    }

    bmi_.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi_.bmiHeader.biWidth = width_;
    bmi_.bmiHeader.biHeight = -height_;   // 负值 = top-down，省一次翻转
    bmi_.bmiHeader.biPlanes = 1;
    bmi_.bmiHeader.biBitCount = 32;
    bmi_.bmiHeader.biCompression = BI_RGB;

    void *bits = nullptr;
    bitmap_ = CreateDIBSection(memDc_, &bmi_, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap_ || !bits) {
        release();
        return false;
    }

    oldBitmap_ = SelectObject(memDc_, bitmap_);
    bits_ = static_cast<uint8_t *>(bits);
    return true;
}

void GdiCapturer::release()
{
    if (memDc_) {
        if (oldBitmap_)
            SelectObject(memDc_, oldBitmap_);
        if (bitmap_)
            DeleteObject(bitmap_);
        DeleteDC(memDc_);
    }
    if (screenDc_)
        ReleaseDC(nullptr, screenDc_);

    memDc_ = nullptr;
    bitmap_ = nullptr;
    oldBitmap_ = nullptr;
    screenDc_ = nullptr;
    bits_ = nullptr;
    width_ = height_ = 0;
}

void GdiCapturer::stop()
{
    release();
}

bool GdiCapturer::capture(Frame &out, int timeoutMs)
{
    (void)timeoutMs;

    if (!screenDc_ || !memDc_ || !bits_)
        return false;

    if (!BitBlt(memDc_, 0, 0, width_, height_, screenDc_, 0, 0, SRCCOPY))
        return false;

    // 桌面锁定 / 切换分辨率时 BitBlt 可能成功但内容为空，交给上层判断
    const size_t stride = static_cast<size_t>(width_) * 4;
    out.width = width_;
    out.height = height_;
    out.stride = static_cast<int>(stride);
    out.format = PixelFormat::BGRA;
    out.pts = nowUs();
    out.data.resize(stride * static_cast<size_t>(height_));
    std::memcpy(out.data.data(), bits_, out.data.size());
    return true;
}

} // namespace sc
