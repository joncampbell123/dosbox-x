/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright (C) 2026 Matthew Hiltner
 */

#include "win31_publish.h"
#include <gtest/gtest.h>

namespace {

using namespace Win31Publish;

void put_word(uint8_t *p, uint16_t value)
{
    p[0] = uint8_t(value);
    p[1] = uint8_t(value >> 8);
}

TEST(Win31Publish, RejectsInvalidRangesAndShapes)
{
    EXPECT_TRUE(range(0xfffff000u, 4096));
    EXPECT_FALSE(range(0xfffff000u, 4097));
    EXPECT_FALSE(range(0, 0));
    EXPECT_FALSE(range(2, size_t(-1)));
    EXPECT_TRUE(range(1, 0xffffffffu));
    EXPECT_FALSE(range(2, 0xffffffffu));
    EXPECT_FALSE(valid_shape(Shape(1280, 720, 1000, 720000)));
    EXPECT_FALSE(valid_shape(Shape(1, 0xffffffffu, 0xffffffffu, 1)));
    EXPECT_FALSE(valid_shape(Shape(1, MaxFrameBytes + 1, 1, MaxFrameBytes + 1)));
    EXPECT_TRUE(valid_shape(Shape(8, 4, 10, 40)));
}

TEST(Win31Publish, ValidatesDescriptorVersionGeometryAndAddresses)
{
    uint32_t descriptor[10] = {40, 1, 0x1000, 0, 0x3000, 32, 8, 8, 4, 8};
    Shape shape;
    EXPECT_TRUE(descriptor_valid(descriptor, shape));
    const unsigned fields[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    const uint32_t invalid[] = {36, 2, 0xfffffff0u, 1, 0xfffff000u, 33, 7, 9, 5, 16};
    for (unsigned i = 0; i < 10; ++i) {
        const unsigned field = fields[i];
        const uint32_t saved = descriptor[field];
        descriptor[field] = invalid[i];
        EXPECT_FALSE(descriptor_valid(descriptor, shape)) << "field " << field;
        descriptor[field] = saved;
    }
}

TEST(Win31Publish, ComposesClippedCursorAndRejectsOutOfBounds)
{
    const Shape shape(8, 4, 8, 32);
    std::vector<uint8_t> frame(32, 0x55);
    uint8_t cursor[CursorBytes] = {};
    EXPECT_TRUE(compose(frame, shape, cursor)); // Hidden cursor.
    put_word(cursor + CursorBytes - 2, 4);
    put_word(cursor + 4, 2);
    put_word(cursor + 6, 1);
    put_word(cursor + 8, 30);
    put_word(cursor + 10, 31);
    put_word(cursor + 12, 30);
    cursor[16 + (31 * 32 + 30) * 2] = 0xff;
    cursor[16 + (31 * 32 + 30) * 2 + 1] = 0xff;
    cursor[16 + (31 * 32 + 31) * 2] = 0;
    cursor[16 + (31 * 32 + 31) * 2 + 1] = 0x42;
    ASSERT_TRUE(compose(frame, shape, cursor));
    EXPECT_EQ(0xaa, frame[30]); // Invert.
    EXPECT_EQ(0x42, frame[31]); // Replace.
    EXPECT_EQ(0x55, frame[29]);
    put_word(cursor + 4, 3);
    EXPECT_FALSE(compose(frame, shape, cursor)); // Cursor source runs past right edge.
    put_word(cursor + 4, 2);
    put_word(cursor + 12, 31);
    EXPECT_FALSE(compose(frame, shape, cursor)); // Screen destination runs past right edge.
    put_word(cursor + 12, 32);
    EXPECT_FALSE(compose(frame, shape, cursor)); // Screen destination below bottom.
    put_word(cursor + 12, 30);
    put_word(cursor + 6, 2);
    EXPECT_FALSE(compose(frame, shape, cursor)); // Cursor source below bottom.
    put_word(cursor + 10, 0);
    EXPECT_FALSE(compose(frame, shape, cursor)); // Screen destination below bottom.
    put_word(cursor + 4, 0);
    EXPECT_TRUE(compose(frame, shape, cursor)); // Entirely clipped cursor.
    frame.pop_back();
    EXPECT_FALSE(compose(frame, shape, cursor));
}

TEST(Win31Publish, OwnsSnapshotAndRejectsChangedGenerationOrShape)
{
    const Shape shape(8, 4, 8, 32);
    std::vector<uint8_t> frame(32, 0x55);
    Pending pending;
    pending.shape = shape;
    pending.generation = 7;
    pending.pixels = frame;
    frame[0] = 0;
    EXPECT_EQ(0x55, pending.pixels[0]);
    EXPECT_TRUE(pending.matches(shape, 7));
    EXPECT_FALSE(pending.matches(shape, 8));
    EXPECT_FALSE(pending.matches(Shape(4, 8, 4, 32), 7));
    pending.cancel();
    EXPECT_FALSE(pending.matches(shape, 7));
}

} // namespace
