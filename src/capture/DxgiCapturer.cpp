#include "DxgiCapturer.h"

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

DxgiCapturer::~DxgiCapturer()
{
    stop();
}

void DxgiCapturer::release()
{
    if (duplication_ && frameAcquired_) {
        duplication_->ReleaseFrame();
        frameAcquired_ = false;
    }
    staging_.Reset();
    duplication_.Reset();
    context_.Reset();
    device_.Reset();
    width_ = height_ = 0;
}

bool DxgiCapturer::createStaging()
{
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = static_cast<UINT>(width_);
    desc.Height = static_cast<UINT>(height_);
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_STAGING;
    desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    return SUCCEEDED(device_->CreateTexture2D(&desc, nullptr, staging_.ReleaseAndGetAddressOf()));
}

bool DxgiCapturer::start()
{
    release();

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(factory.GetAddressOf()))))
        return false;

    ComPtr<IDXGIAdapter1> adapter;
    ComPtr<IDXGIOutput> output;

    for (UINT i = 0; factory->EnumAdapters1(i, adapter.GetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i) {
        if (SUCCEEDED(adapter->EnumOutputs(0, output.GetAddressOf()))) {
            DXGI_OUTPUT_DESC od{};
            if (SUCCEEDED(output->GetDesc(&od)) && od.AttachedToDesktop)
                break;
        }
        output.Reset();
    }
    if (!output)
        return false;

    HRESULT hr = D3D11CreateDevice(adapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr,
                                   D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0,
                                   D3D11_SDK_VERSION, device_.GetAddressOf(), nullptr,
                                   context_.GetAddressOf());
    if (FAILED(hr))
        return false;

    ComPtr<IDXGIOutput1> output1;
    if (FAILED(output.As(&output1)))
        return false;

    hr = output1->DuplicateOutput(device_.Get(), duplication_.GetAddressOf());
    if (FAILED(hr))
        return false;

    DXGI_OUTDUPL_DESC dd{};
    duplication_->GetDesc(&dd);
    width_ = static_cast<int>(dd.ModeDesc.Width);
    height_ = static_cast<int>(dd.ModeDesc.Height);

    if (width_ <= 0 || height_ <= 0 || !createStaging())
        return false;

    t0_ = nowUs();
    return true;
}

void DxgiCapturer::stop()
{
    release();
}

bool DxgiCapturer::capture(Frame &out, int timeoutMs)
{
    if (!duplication_)
        return false;

    DXGI_OUTDUPL_FRAME_INFO info{};
    ComPtr<IDXGIResource> resource;

    const HRESULT hr = duplication_->AcquireNextFrame(static_cast<UINT>(timeoutMs), &info,
                                                      resource.GetAddressOf());
    if (hr == DXGI_ERROR_WAIT_TIMEOUT)
        return false;                       // 桌面没变化，正常现象
    if (FAILED(hr)) {
        if (hr == DXGI_ERROR_ACCESS_LOST || hr == DXGI_ERROR_INVALID_CALL) {
            start();                        // 分辨率切换 / UAC 提权后重建
        }
        return false;
    }
    frameAcquired_ = true;

    ComPtr<ID3D11Texture2D> texture;
    if (FAILED(resource.As(&texture))) {
        duplication_->ReleaseFrame();
        frameAcquired_ = false;
        return false;
    }

    D3D11_TEXTURE2D_DESC td{};
    texture->GetDesc(&td);
    if (static_cast<int>(td.Width) != width_ || static_cast<int>(td.Height) != height_) {
        width_ = static_cast<int>(td.Width);
        height_ = static_cast<int>(td.Height);
        if (!createStaging()) {
            duplication_->ReleaseFrame();
            frameAcquired_ = false;
            return false;
        }
    }

    context_->CopyResource(staging_.Get(), texture.Get());

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (FAILED(context_->Map(staging_.Get(), 0, D3D11_MAP_READ, 0, &mapped))) {
        duplication_->ReleaseFrame();
        frameAcquired_ = false;
        return false;
    }

    const size_t rowBytes = static_cast<size_t>(width_) * 4;
    out.width = width_;
    out.height = height_;
    out.stride = static_cast<int>(rowBytes);
    out.format = PixelFormat::BGRA;
    out.pts = nowUs() - t0_;
    out.data.resize(rowBytes * static_cast<size_t>(height_));

    const uint8_t *src = static_cast<const uint8_t *>(mapped.pData);
    for (int y = 0; y < height_; ++y)
        std::memcpy(out.data.data() + y * rowBytes, src + y * mapped.RowPitch, rowBytes);

    context_->Unmap(staging_.Get(), 0);
    duplication_->ReleaseFrame();
    frameAcquired_ = false;
    return true;
}

} // namespace sc
