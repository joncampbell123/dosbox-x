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

#ifndef DOSBOX_VOODOO_VULKAN_H
#define DOSBOX_VOODOO_VULKAN_H

#include "dosbox.h"
#include "voodoo_types.h"

/* The 3dfx emulation rendered with Vulkan (voodoo_card=vulkan, or auto with output=vulkan),
 * the same functions as the OpenGL one (voodoo_opengl.h) */

bool voodoo_vk_init(voodoo_state *v);
void voodoo_vk_leave(bool leavemode);
void voodoo_vk_shutdown(voodoo_state *v);

void voodoo_vk_set_window(voodoo_state *v);
void voodoo_vk_present(void);
void voodoo_vk_load_framebuffers_from_ram(void);
void voodoo_vk_follow_dimensions(void);
void voodoo_vk_swap_buffer(void);
void voodoo_vk_vblank_flush(void);

void voodoo_vk_update_dimensions(void);
void voodoo_vk_clear(void);
void voodoo_vk_fastfill(void);

void voodoo_vk_clip_window(voodoo_state *v);
void voodoo_vk_texture_clear(UINT32 address, int TMU);
void voodoo_vk_invalidate_paltex(void);

void voodoo_vk_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a);
void voodoo_vk_draw_z(int x, int y, int z);
void voodoo_vk_draw_pixel_pipeline(int x, int y, int r, int g, int b);
UINT32 voodoo_vk_read_pixel(int x, int y);

void voodoo_vk_draw_triangle(poly_extra_data *extra);

#endif
