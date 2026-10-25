#include "SelfTest.h"

#include "../capture/DxgiCapturer.h"
#include "../capture/GdiCapturer.h"
#include "../codec/VideoDecoder.h"
#include "../codec/VideoEncoder.h"

#include <chrono>
#include <cstdio>
#include <memory>

namespace sc {

namespace {
double msSince(const std::chrono::steady_clock::time_point &t0)
{
    using namespace std::chrono;
    return duration_cast<nanoseconds>(steady_clock::now() - t0).count() / 1e6;
}
} // namespace

int runSelfTest(int frames)
{
    std::unique_ptr<ICapturer> capturer;
    auto dxgi = std::make_unique<DxgiCapturer>();
    if (dxgi->start()) {
        capturer = std::move(dxgi);
    } else {
        std::printf("[selftest] DXGI unavailable, fallback to GDI\n");
        auto gdi = std::make_unique<GdiCapturer>();
        if (!gdi->start()) {
            std::printf("[selftest] capture init failed\n");
            return 2;
        }
        capturer = std::move(gdi);
    }
    std::printf("[selftest] capturer=%s %dx%d\n", capturer->name(), capturer->width(),
                capturer->height());

    VideoEncoder encoder;
    if (!encoder.open(1280, 720, 60, 4000, false, 60)) {
        std::printf("[selftest] encoder init failed (libx264?)\n");
        return 3;
    }

    VideoDecoder decoder;
    if (!decoder.open(false)) {
        std::printf("[selftest] decoder init failed\n");
        return 4;
    }

    Frame frame;
    int captured = 0;
    int packets = 0;
    int decodedFrames = 0;
    int64_t bytes = 0;
    double encodeTotal = 0.0;
    double decodeTotal = 0.0;

    const auto start = std::chrono::steady_clock::now();
    while (captured < frames) {
        if (!capturer->capture(frame, 200))
            continue;

        std::vector<Packet> out;
        const auto t0 = std::chrono::steady_clock::now();
        if (!encoder.encode(frame, out)) {
            std::printf("[selftest] encode failed at frame %d\n", captured);
            return 5;
        }
        encodeTotal += msSince(t0);

        const auto t1 = std::chrono::steady_clock::now();
        for (Packet &packet : out) {
            ++packets;
            bytes += static_cast<int64_t>(packet.data.size());
            std::vector<Frame> framesOut;
            decoder.decode(packet, framesOut);
            decodedFrames += static_cast<int>(framesOut.size());
        }
        decodeTotal += msSince(t1);

        ++captured;
    }
    const auto end = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() / 1000.0;

    std::printf("[selftest] frames=%d packets=%d decoded=%d\n", captured, packets, decodedFrames);
    std::printf("[selftest] avg encode=%.2f ms  avg decode=%.2f ms\n", encodeTotal / captured,
                decodeTotal / captured);
    std::printf("[selftest] stream=%.0f kB  bitrate=%.0f kbps  elapsed=%.1f s  fps=%.1f\n",
                bytes / 1024.0, bytes * 8.0 / 1000.0 / seconds, seconds, captured / seconds);

    return decodedFrames > 0 ? 0 : 6;
}

} // namespace sc
