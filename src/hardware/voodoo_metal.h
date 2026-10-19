/*
 *  Copyright (C) 2002-2021  The DOSBox Team
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

#ifndef DOSBOX_VOODOO_METAL_H
#define DOSBOX_VOODOO_METAL_H

#include "dosbox.h"
#include "voodoo_types.h"

/* The 3dfx emulation rendered with Metal (voodoo_card=metal, with output=metal; macOS, SDL2 builds),
 * the same functions as the OpenGL one (voodoo_opengl.h). Without Metal, these do nothing. */

#if defined(MACOSX) && defined(C_SDL2) && C_METAL

bool voodoo_metal_init(voodoo_state *v);
void voodoo_metal_leave(bool leavemode);
void voodoo_metal_shutdown(voodoo_state *v);

void voodoo_metal_set_window(voodoo_state *v);
void voodoo_metal_present(void);
void voodoo_metal_load_framebuffers_from_ram(void);
void voodoo_metal_follow_dimensions(void);
void voodoo_metal_swap_buffer(void);
void voodoo_metal_vblank_flush(void);

void voodoo_metal_update_dimensions(void);
void voodoo_metal_clear(void);
void voodoo_metal_fastfill(void);

void voodoo_metal_clip_window(voodoo_state *v);
void voodoo_metal_texture_clear(UINT32 address, int TMU);
void voodoo_metal_invalidate_paltex(void);

void voodoo_metal_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a);
void voodoo_metal_draw_z(int x, int y, int z);
void voodoo_metal_draw_pixel_pipeline(int x, int y, int r, int g, int b);
UINT32 voodoo_metal_read_pixel(int x, int y);

void voodoo_metal_draw_triangle(poly_extra_data *extra);

#else

static inline bool voodoo_metal_init(voodoo_state *vs) { (void)vs; return false; }
static inline void voodoo_metal_leave(bool leavemode) { (void)leavemode; }
static inline void voodoo_metal_shutdown(voodoo_state *vs) { (void)vs; }
static inline void voodoo_metal_set_window(voodoo_state *vs) { (void)vs; }
static inline void voodoo_metal_present(void) { }
static inline void voodoo_metal_load_framebuffers_from_ram(void) { }
static inline void voodoo_metal_follow_dimensions(void) { }
static inline void voodoo_metal_swap_buffer(void) { }
static inline void voodoo_metal_vblank_flush(void) { }
static inline void voodoo_metal_update_dimensions(void) { }
static inline void voodoo_metal_clear(void) { }
static inline void voodoo_metal_fastfill(void) { }
static inline void voodoo_metal_clip_window(voodoo_state *vs) { (void)vs; }
static inline void voodoo_metal_texture_clear(UINT32 address, int TMU) { (void)address; (void)TMU; }
static inline void voodoo_metal_invalidate_paltex(void) { }
static inline void voodoo_metal_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	(void)x; (void)y; (void)has_rgb; (void)has_alpha; (void)r; (void)g; (void)b; (void)a;
}
static inline void voodoo_metal_draw_z(int x, int y, int z) { (void)x; (void)y; (void)z; }
static inline void voodoo_metal_draw_pixel_pipeline(int x, int y, int r, int g, int b) { (void)x; (void)y; (void)r; (void)g; (void)b; }
static inline UINT32 voodoo_metal_read_pixel(int x, int y) { (void)x; (void)y; return 0; }
static inline void voodoo_metal_draw_triangle(poly_extra_data *extra) { (void)extra; }

#endif

#endif
