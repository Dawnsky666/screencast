#pragma once

#include "../core/Types.h"

namespace sc {

// 采集器抽象：DXGI 与 GDI 两种实现，pipeline 只依赖这个接口
class ICapturer {
public:
    virtual ~ICapturer() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // timeoutMs 内没有新画面时返回 false（这不是错误，例如桌面静止）
    virtual bool capture(Frame &out, int timeoutMs = 50) = 0;

    virtual const char *name() const = 0;
    virtual int width() const = 0;
    virtual int height() const = 0;
};

} // namespace sc
