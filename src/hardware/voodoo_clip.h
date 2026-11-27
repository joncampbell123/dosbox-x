/*
 *  Copyright (C) 2026  The DOSBox-X Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef DOSBOX_VOODOO_CLIP_H
#define DOSBOX_VOODOO_CLIP_H

#include <stdint.h>

/** A box of window pixels: its corner from the bottom left, and its size. */
struct ogl_box
{
    int x;
    int y;
    int w;
    int h;
};

/**
 * @brief The clip registers' rectangle as a scissor box in a view of the frame buffer.
 *
 * As 86Box clips, the rectangle is in the triangles' own coordinates: rows count up
 * with FBZMODE_Y_ORIGIN, down otherwise, as the projection voodoo_ogl_set_window sets.
 *
 * @param left_right The clipLeftRight register.
 * @param low_high   The clipLowYHighY register.
 * @param y_up       Whether FBZMODE_Y_ORIGIN is set.
 * @param width      The frame buffer's width.
 * @param height     The frame buffer's height.
 * @param view       The viewport the frame buffer is drawn into.
 * @return The scissor box; empty, never negative, for an empty rectangle.
 */
ogl_box voodoo_ogl_clip_box(uint32_t left_right, uint32_t low_high, bool y_up, int width, int height,
                            ogl_box view);

#endif
