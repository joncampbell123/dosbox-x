/*
 *  The OpenGL Voodoo's clip (voodoo_opengl.cpp): the clip registers' rectangle as a scissor box.
 */

#ifndef DOSBOX_VOODOO_CLIP_H
#define DOSBOX_VOODOO_CLIP_H

#include <stdint.h>

/* A box of the window's pixels: its corner from the bottom left, its size. */
struct ogl_box {
	int x,y,w,h;
};

/* The clip registers' rectangle as the scissor box of a view (the viewport) onto a width by height
   frame buffer. As 86Box clips, the rectangle is in the triangles' own coordinates, rows counted up
   with FBZMODE_Y_ORIGIN (y_up), else down: the projection voodoo_ogl_set_window sets. */
ogl_box voodoo_ogl_clip_box(uint32_t left_right, uint32_t low_high, bool y_up, int width, int height, ogl_box view);

#endif
