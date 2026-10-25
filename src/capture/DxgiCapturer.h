#pragma once

#include "ICapturer.h"

#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

namespace sc {

// DXGI Desktop Duplication 采集：GPU 零拷贝路径，桌面静止时不重传
class DxgiCapturer : public ICapturer {
public:
    DxgiCapturer() = default;
    ~DxgiCapturer() override;

    bool start() override;
    void stop() override;
    bool capture(Frame &out, int timeoutMs) override;

    const char *name() const override { return "DXGI"; }
    int width() const override { return width_; }
    int height() const override { return height_; }

private:
    template <class T>
    using ComPtr = Microsoft::WRL::ComPtr<T>;

    bool createStaging();
    void release();

    ComPtr<ID3D11Device> device_;
    ComPtr<ID3D11DeviceContext> context_;
    ComPtr<IDXGIOutputDuplication> duplication_;
    ComPtr<ID3D11Texture2D> staging_;

    int width_ = 0;
    int height_ = 0;
    int64_t t0_ = 0;
    bool frameAcquired_ = false;
};

} // namespace sc
