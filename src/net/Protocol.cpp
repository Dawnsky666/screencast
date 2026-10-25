#include "Protocol.h"

#include <cstring>

namespace sc {

namespace {

inline void putU8(uint8_t *p, uint8_t v) { *p = v; }

inline void putU16(uint8_t *p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void putU32(uint8_t *p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline void putI64(uint8_t *p, int64_t v)
{
    const uint64_t u = static_cast<uint64_t>(v);
    for (int i = 0; i < 8; ++i)
        p[i] = static_cast<uint8_t>((u >> (8 * i)) & 0xFF);
}

inline uint8_t  getU8(const uint8_t *p)  { return p[0]; }
inline uint16_t getU16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
inline uint32_t getU32(const uint8_t *p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}
inline int64_t getI64(const uint8_t *p)
{
    uint64_t u = 0;
    for (int i = 7; i >= 0; --i)
        u = (u << 8) | p[i];
    return static_cast<int64_t>(u);
}

} // namespace

std::vector<uint8_t> encodeHeader(const FrameHeader &h)
{
    std::vector<uint8_t> buf(kHeaderSize, 0);
    uint8_t *p = buf.data();

    putU32(p, kMagic);
    putU8(p + 4, h.version);
    putU8(p + 5, static_cast<uint8_t>(h.type));
    putU8(p + 6, h.flags);
    putU8(p + 7, 0);
    putU16(p + 8, h.width);
    putU16(p + 10, h.height);
    putI64(p + 12, h.pts);
    putU32(p + 20, h.payloadSize);
    return buf;
}

bool decodeHeader(const uint8_t *buf, size_t len, FrameHeader &out)
{
    if (!buf || len < kHeaderSize)
        return false;
    if (getU32(buf) != kMagic)
        return false;
    if (getU8(buf + 4) != kProtocolVersion)
        return false;

    const uint32_t payload = getU32(buf + 20);
    if (payload > kMaxPayloadSize)
        return false;

    out.version     = getU8(buf + 4);
    out.type        = static_cast<MsgType>(getU8(buf + 5));
    out.flags       = getU8(buf + 6);
    out.width       = getU16(buf + 8);
    out.height      = getU16(buf + 10);
    out.pts         = getI64(buf + 12);
    out.payloadSize = payload;
    return true;
}

} // namespace sc
