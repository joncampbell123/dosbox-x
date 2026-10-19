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

#ifndef DOSBOX_VOODOO_D3D11_H
#define DOSBOX_VOODOO_D3D11_H

#include "dosbox.h"
#include "voodoo_types.h"

/* The 3dfx emulation rendered with Direct3D 11 (voodoo_card=direct3d11, or auto with output=direct3d11),
 * the same functions as the OpenGL one (voodoo_opengl.h) */

bool voodoo_d3d11_init(voodoo_state *v);
void voodoo_d3d11_leave(bool leavemode);
void voodoo_d3d11_shutdown(voodoo_state *v);

void voodoo_d3d11_set_window(voodoo_state *v);
void voodoo_d3d11_present(void);
void voodoo_d3d11_load_framebuffers_from_ram(void);
void voodoo_d3d11_follow_dimensions(void);
void voodoo_d3d11_swap_buffer(void);
void voodoo_d3d11_vblank_flush(void);

void voodoo_d3d11_update_dimensions(void);
void voodoo_d3d11_clear(void);
void voodoo_d3d11_fastfill(void);

void voodoo_d3d11_clip_window(voodoo_state *v);
void voodoo_d3d11_texture_clear(UINT32 address, int TMU);
void voodoo_d3d11_invalidate_paltex(void);

void voodoo_d3d11_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a);
void voodoo_d3d11_draw_z(int x, int y, int z);
void voodoo_d3d11_draw_pixel_pipeline(int x, int y, int r, int g, int b);
UINT32 voodoo_d3d11_read_pixel(int x, int y);

void voodoo_d3d11_draw_triangle(poly_extra_data *extra);

#endif
