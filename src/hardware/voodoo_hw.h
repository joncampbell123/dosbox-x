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

#ifndef DOSBOX_VOODOO_HW_H
#define DOSBOX_VOODOO_HW_H

/* The hardware 3dfx emulation (v->ogl): rendered with OpenGL (with output=opengl), Vulkan (with
 * output=vulkan), Direct3D (with output=direct3d), Direct3D 11 (with output=direct3d11) or Metal
 * (with output=metal), as v->hw_api says. These go to that one. */

#include "voodoo.h"
#include "voodoo_opengl.h"
#include "voodoo_vulkan.h"
#include "voodoo_d3d.h"
#include "voodoo_d3d11.h"
#include "voodoo_metal.h"

#define VOODOO_HW_CALL(name, args) do { \
	if (v->hw_api == VOODOO_HW_API_VULKAN) voodoo_vk_##name args; \
	else if (v->hw_api == VOODOO_HW_API_DIRECT3D) voodoo_d3d_##name args; \
	else if (v->hw_api == VOODOO_HW_API_DIRECT3D11) voodoo_d3d11_##name args; \
	else if (v->hw_api == VOODOO_HW_API_METAL) voodoo_metal_##name args; \
	else voodoo_ogl_##name args; } while (0)

static inline void voodoo_hw_set_window(voodoo_state *vs) { VOODOO_HW_CALL(set_window, (vs)); }
static inline void voodoo_hw_present(void) { VOODOO_HW_CALL(present, ()); }
static inline void voodoo_hw_swap_buffer(void) { VOODOO_HW_CALL(swap_buffer, ()); }
static inline void voodoo_hw_vblank_flush(void) { VOODOO_HW_CALL(vblank_flush, ()); }
static inline void voodoo_hw_update_dimensions(void) { VOODOO_HW_CALL(update_dimensions, ()); }
static inline void voodoo_hw_follow_dimensions(void) { VOODOO_HW_CALL(follow_dimensions, ()); }
static inline void voodoo_hw_clear(void) { VOODOO_HW_CALL(clear, ()); }
static inline void voodoo_hw_load_framebuffers_from_ram(void) { VOODOO_HW_CALL(load_framebuffers_from_ram, ()); }
static inline void voodoo_hw_fastfill(void) { VOODOO_HW_CALL(fastfill, ()); }
static inline void voodoo_hw_clip_window(voodoo_state *vs) { VOODOO_HW_CALL(clip_window, (vs)); }
static inline void voodoo_hw_texture_clear(UINT32 address, int TMU) { VOODOO_HW_CALL(texture_clear, (address, TMU)); }
static inline void voodoo_hw_invalidate_paltex(void) { VOODOO_HW_CALL(invalidate_paltex, ()); }
static inline void voodoo_hw_draw_triangle(poly_extra_data *extra) { VOODOO_HW_CALL(draw_triangle, (extra)); }
static inline void voodoo_hw_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	VOODOO_HW_CALL(draw_pixel, (x, y, has_rgb, has_alpha, r, g, b, a));
}
static inline void voodoo_hw_draw_z(int x, int y, int z) { VOODOO_HW_CALL(draw_z, (x, y, z)); }
static inline void voodoo_hw_draw_pixel_pipeline(int x, int y, int r, int g, int b) { VOODOO_HW_CALL(draw_pixel_pipeline, (x, y, r, g, b)); }
static inline UINT32 voodoo_hw_read_pixel(int x, int y) {
	if (v->hw_api == VOODOO_HW_API_VULKAN) return voodoo_vk_read_pixel(x, y);
	if (v->hw_api == VOODOO_HW_API_DIRECT3D) return voodoo_d3d_read_pixel(x, y);
	if (v->hw_api == VOODOO_HW_API_DIRECT3D11) return voodoo_d3d11_read_pixel(x, y);
	if (v->hw_api == VOODOO_HW_API_METAL) return voodoo_metal_read_pixel(x, y);
	return voodoo_ogl_read_pixel(x, y);
}
static inline bool voodoo_hw_init(voodoo_state *vs) {
	if (v->hw_api == VOODOO_HW_API_VULKAN) return voodoo_vk_init(vs);
	if (v->hw_api == VOODOO_HW_API_DIRECT3D) return voodoo_d3d_init(vs);
	if (v->hw_api == VOODOO_HW_API_DIRECT3D11) return voodoo_d3d11_init(vs);
	if (v->hw_api == VOODOO_HW_API_METAL) return voodoo_metal_init(vs);
	return voodoo_ogl_init(vs);
}
static inline void voodoo_hw_leave(bool leavemode) { VOODOO_HW_CALL(leave, (leavemode)); }
static inline void voodoo_hw_shutdown(voodoo_state *vs) { VOODOO_HW_CALL(shutdown, (vs)); }

#undef VOODOO_HW_CALL

#endif
