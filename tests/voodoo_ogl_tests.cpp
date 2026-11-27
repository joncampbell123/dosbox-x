/*
 *  The OpenGL Voodoo's clip: the clip registers' rectangle as the scissor box triangles and fast
 *  fills are cut to.
 */

#include "config.h"

#if C_OPENGL

#include <gtest/gtest.h>

#include "../src/hardware/voodoo_clip.h"

namespace {

/* 320x200 centred in 512x384, at 96, 92, its rows counted down: SCALED, in a 1024x768 view. */
const uint32_t SCREEN_X = 96u << 16 | 416u;
const uint32_t SCREEN_Y = 92u << 16 | 292u;
const ogl_box SCALED = {0, 0, 1024, 768};

/* Its rows counted down from the top: the box's from the window's bottom. Triangles were not cut at
   all, and a fast fill's rows were taken from fbiInit3's y origin, 0 unless a program sets it: a
   320x200 screen in 512x384 was drawn past its edges. */
TEST(VoodooOglClipBox, RowsCountedDown)
{
    const ogl_box box = voodoo_ogl_clip_box(SCREEN_X, 10u << 16 | 110u, false, 512, 384, SCALED);

    EXPECT_EQ(box.x, 192);
    EXPECT_EQ(box.w, 640);
    EXPECT_EQ(box.y, (384 - 110) * 2);
    EXPECT_EQ(box.h, 200);
}

/* With FBZMODE_Y_ORIGIN, rows counted up: the box's as they are. */
TEST(VoodooOglClipBox, RowsCountedUp)
{
    const ogl_box box = voodoo_ogl_clip_box(SCREEN_X, 10u << 16 | 110u, true, 512, 384, SCALED);

    EXPECT_EQ(box.y, 20);
    EXPECT_EQ(box.h, 200);
}

/* A viewport moved into the window, as fullscreen centres a small mode: the box moves with it. */
TEST(VoodooOglClipBox, ViewportOffset)
{
    const ogl_box view = {64, 48, 512, 384};
    const ogl_box box = voodoo_ogl_clip_box(SCREEN_X, SCREEN_Y, false, 512, 384, view);

    EXPECT_EQ(box.x, 64 + 96);
    EXPECT_EQ(box.y, 48 + 384 - 292);
    EXPECT_EQ(box.w, 320);
    EXPECT_EQ(box.h, 200);
}

/* An empty rectangle: an empty box, not a negative one GL refuses. */
TEST(VoodooOglClipBox, Empty)
{
    const ogl_box box = voodoo_ogl_clip_box(400u << 16 | 100u, 300u << 16 | 50u, false, 512, 384, SCALED);

    EXPECT_EQ(box.w, 0);
    EXPECT_EQ(box.h, 0);
}

}

#endif
