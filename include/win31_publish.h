/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Matthew Hiltner
 */

#ifndef DOSBOX_WIN31_PUBLISH_H
#define DOSBOX_WIN31_PUBLISH_H
#include <cstdint>
#include <cstddef>
#include <vector>

namespace Win31Publish
{

constexpr size_t CursorBytes = 16 + 16384 + 2;
constexpr uint32_t MaxFrameBytes = 16u * 1024u * 1024u;
inline bool range(const uint32_t base, const size_t size)
{
    return size && uint64_t(size) <= (uint64_t(1) << 32) - base;
}
inline uint16_t word(const uint8_t *p)
{
    return uint16_t(p[0] | (uint16_t(p[1]) << 8));
}
inline uint32_t dword(const uint8_t *p)
{
    return uint32_t(word(p)) | (uint32_t(word(p + 2)) << 16);
}
struct Shape
{
    uint32_t width, height, stride, bytes;

    Shape(uint32_t w = 0, uint32_t h = 0, uint32_t pitch = 0, uint32_t count = 0)
        : width(w), height(h), stride(pitch), bytes(count)
    {}
    bool operator==(const Shape &s) const
    {
        return width == s.width && height == s.height && stride == s.stride && bytes == s.bytes;
    }
};

inline bool valid_shape(const Shape &s)
{
    return s.width && s.height && s.width <= s.stride && s.bytes <= MaxFrameBytes &&
           uint64_t(s.stride) * s.height == s.bytes;
}
inline bool descriptor_valid(const uint32_t *d, Shape &shape)
{
    shape = Shape(d[7], d[8], d[6], d[5]);
    return d[0] == 40 && d[1] == 1 && d[3] == 0 && d[9] == 8 &&
           valid_shape(shape) && range(d[2], shape.bytes) && range(d[4], CursorBytes);
}
inline bool compose(std::vector<uint8_t> &frame, const Shape &s, const uint8_t *cursor)
{
    if (!valid_shape(s) || frame.size() != s.bytes)
        return false;
    if (!(word(cursor + 16 + 16384) & 4))
        return true;
    const uint32_t w = word(cursor + 4), h = word(cursor + 6);
    const uint32_t xc = word(cursor + 8), yc = word(cursor + 10);
    const uint32_t pos = dword(cursor + 12);
    if (!w || !h)
        return true;
    if (w > 32 || h > 32 || xc + w > 32 || yc + h > 32 || pos >= s.bytes ||
        pos % s.stride + w > s.width || pos / s.stride + h > s.height)
        return false;
    for (uint32_t y = 0; y < h; ++y) {
        const uint8_t *p = cursor + 16 + ((yc + y) * 32 + xc) * 2;
        for (uint32_t x = 0; x < w; ++x) {
            uint8_t &v = frame[pos + y * s.stride + x];
            v = uint8_t((v & p[x * 2]) ^ p[x * 2 + 1]);
        }
    }
    return true;
}

struct Pending
{
    std::vector<uint8_t> pixels;
    Shape shape;
    uint64_t generation = 0;
    void cancel()
    {
        pixels.clear();
    }
    bool matches(const Shape &s, uint64_t gen) const
    {
        return !pixels.empty() && shape == s && generation == gen;
    }
};
} // namespace Win31Publish

#endif
