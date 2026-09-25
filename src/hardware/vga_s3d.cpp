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

/* S3 ViRGE / ViRGE/VX "S3d Engine" 3D functions: 3D lines and 3D triangles.
 *
 * This is a synchronous software rasterizer. A 3D command is rendered completely,
 * directly into emulated video memory, at the moment the guest writes the register
 * that starts it (CMD_SET with autoexecute off, or 3YCNT/TY01_Y12 with autoexecute on).
 * The engine therefore never reports itself busy and needs no FIFO, thread or timer.
 * This matches how the ViRGE 2D engine is already emulated in vga_xga.cpp.
 *
 * References:
 *   [DB]    S3 ViRGE Integrated 3D Accelerator databook (DB019-B, August 1996),
 *           Section 15.4.5-15.4.8 (3D drawing) and Section 19.4 (3D registers).
 *   [VX]    S3 ViRGE/VX databook, same sections (register layout is identical).
 *   [86Box] 86Box src/video/vid_s3_virge.c (GPL-2.0-or-later; Sarah Walker, Miran Grca
 *           and the 86Box contributors). The triangle edge walking, span setup, sub-pixel
 *           attribute pre-step, texture addressing and perspective division below follow
 *           the structure of 86Box's tri()/tex_sample_*() functions, which have been
 *           tested against a large amount of real software.
 *   [Mesa]  Mesa 7.0 src/mesa/drivers/dri/s3v (Max Lingua), a driver written against real
 *           ViRGE hardware; used to confirm register value encodings.
 *   [S3DTK] S3D ToolKit 2.6 headers and samples.
 *
 * Deliberate deviations from 86Box:
 *   - Z compare mode 000b with Z buffering mode 00b: [DB] 15.4.6 says Z buffering is only
 *     enabled when the compare mode is not 000b ("if the operator is set to never pass,
 *     z-buffering is effectively disabled"), so such pixels are drawn without a Z test.
 *     86Box discards every pixel instead.
 *   - Z interpolation uses S16.15 for the start value, the X delta and the Y delta, as all
 *     three registers are documented ([DB] 19.4) and as [Mesa] programs them. 86Box
 *     doubles the start value but not the X delta, which halves the effective dZ/dX.
 *   - 8 bits/pixel destinations are implemented (86Box leaves them unimplemented). The
 *     databook does not say what an 8bpp destination receives for non-palettized colors;
 *     we write the blue channel, following the 2D engine's convention that the DAC CLUT
 *     index occupies the blue ("DATA 1") byte of every color register ([DB] 19.3).
 *   - Palettized, Blend4 and Alpha4/Blend4 texel formats are implemented from [DB]
 *     15.4.8.1-15.4.8.2 (86Box decodes them as ARGB1555).
 *   - M2TPP and M8TPP interpolate between two mipmap levels as described in [DB] 15.4.8.1
 *     (86Box treats them as M1TPP and M4TPP). V2TPP is a 2-texel vertical filter ([DB]
 *     figure 15-8, texels 1 and 3).
 *   - Non-mipmapped (1TPP/4TPP/V2TPP) textures use the SOURCE STRIDE field of DEST_SRC_STR
 *     for their row pitch when it is non-zero ([DB] 19.4: "byte offset of vertically
 *     adjacent pixels for a flat (not mipmapped) texture map"). 86Box always uses 2^s.
 *   - Fog is not applied to Gouraud shaded triangles ([DB] 19.4, CMD_SET bit 17: "Fogging
 *     is not available for Gouraud shaded triangles").
 *   - Robustness: every video memory access is bounds checked, and pixels outside the
 *     11-bit coordinate space (including negative coordinates when hardware clipping is
 *     off) are discarded instead of wrapping around video memory. Loop counts are bounded.
 *     Real hardware would wrap, but that would only ever corrupt textures and offscreen
 *     surfaces, never produce a picture a game depends on.
 */

#include <string.h>
#include <stdint.h>

#include "dosbox.h"
#include "logging.h"
#include "vga.h"
#include "mem.h"

/* do not issue CPU-side I/O here -- this code emulates functions that the GDC itself carries out, not on the CPU */
#include "cpu_io_is_forbidden.h"

void S3_ViRGE_SetSubsysStatus(uint32_t bits);

/* Command Set register (MMB100, MMB500) fields [DB] 19.4 */
#define S3D_CMD_AE              (1u << 0u)      /* autoexecute */
#define S3D_CMD_HC              (1u << 1u)      /* hardware clipping enable */
#define S3D_CMD_DEST_FMT(c)     (((c) >> 2u) & 7u)
#define S3D_CMD_TEX_FMT(c)      (((c) >> 5u) & 7u)
#define S3D_CMD_MIP_SIZE(c)     (((c) >> 8u) & 15u)
#define S3D_CMD_FILTER(c)       (((c) >> 12u) & 7u)
#define S3D_CMD_TEX_BLEND(c)    (((c) >> 15u) & 3u)
#define S3D_CMD_FE              (1u << 17u)     /* fog enable */
#define S3D_CMD_ABC(c)          (((c) >> 18u) & 3u)
#define S3D_CMD_ZB_COMP(c)      (((c) >> 20u) & 7u)
#define S3D_CMD_ZUP             (1u << 23u)
#define S3D_CMD_ZB_MODE(c)      (((c) >> 24u) & 3u)
#define S3D_CMD_TWE             (1u << 26u)     /* texture wrap enable */
#define S3D_CMD_COMMAND(c)      (((c) >> 27u) & 15u)
#define S3D_CMD_3D              (1u << 31u)

enum {
	S3D_GOURAUD_TRI = 0,
	S3D_LIT_TEX_TRI = 1,
	S3D_UNLIT_TEX_TRI = 2,
	S3D_LIT_TEX_TRI_PERSP = 5,
	S3D_UNLIT_TEX_TRI_PERSP = 6,
	S3D_LINE = 8,
	S3D_NOP = 15
};

enum {
	S3D_TEX_ARGB8888 = 0,
	S3D_TEX_ARGB4444 = 1,
	S3D_TEX_ARGB1555 = 2,
	S3D_TEX_ALPHA4BLEND4 = 3,
	S3D_TEX_BLEND4_LO = 4,
	S3D_TEX_BLEND4_HI = 5,
	S3D_TEX_PAL8 = 6,
	S3D_TEX_YUV = 7
};

/* Raw register file. Index = (MMIO offset & 0x1FC) >> 2 within the 3D Line (B000-B1FF)
 * or 3D Triangle (B400-B5FF) block. Keeping the raw values makes read back and save
 * states trivial; everything is decoded when a command executes. */
struct S3DState {
	uint32_t line[0x80];
	uint32_t tri[0x80];
};

static S3DState s3d;

/* Registers with the same mnemonic in the 3D Line and 3D Triangle columns of [DB] table
 * 19-1 are one physical register with two addresses: Z_BASE, DEST_BASE, CLIP_L_R, CLIP_T_B,
 * DEST_SRC_STR, Z_STRIDE, FOG_CLR and CMD_SET. */
static inline bool S3D_IsSharedReg(unsigned int off) {
	switch (off) {
		case 0xD4: case 0xD8: case 0xDC: case 0xE0: case 0xE4: case 0xE8: case 0xF4: case 0x100:
			return true;
		default:
			break;
	}
	return false;
}

#define TREG(off) (s3d.tri[((off) & 0x1FCu) >> 2u])
#define LREG(off) (s3d.line[((off) & 0x1FCu) >> 2u])

struct S3DColor {
	int r, g, b, a;
};

/* Everything a pixel needs, decoded once per command */
struct S3DContext {
	uint8_t*    vram;
	uint32_t    memsize;
	uint32_t    memmask;

	uint32_t    cmd;
	unsigned    command;
	unsigned    bypp;           /* destination bytes per pixel (1, 2, 3) */
	uint32_t    dest_base, dest_stride;
	uint32_t    z_base, z_stride;
	bool        hc;
	int         clip_l, clip_r, clip_t, clip_b;

	bool        textured, lit, persp, palettized;
	unsigned    tex_blend;
	bool        fog;
	unsigned    abc;            /* 2 = texture (stage) alpha, 3 = source alpha, else off */
	unsigned    zmode;          /* 0 = normal, 1 = MUX Z pass, 2 = MUX draw pass, 3 = none */
	unsigned    zcomp;
	bool        zup;
	bool        ztest;
	uint8_t     fog_r, fog_g, fog_b;

	/* texture */
	unsigned    tex_fmt;
	unsigned    tex_bypp;
	unsigned    filter;
	int         max_d;
	bool        wrap;
	uint32_t    tex_level_base[10];
	uint32_t    flat_stride;    /* 0 = use 2^level * bytes per texel */
	int32_t     tbu, tbv;
	uint32_t    tex_bdr_clr;
	S3DColor    color0, color1;
};

/* per pixel interpolated values */
struct S3DAttr {
	int32_t r, g, b, a;         /* S8.7 */
	int32_t z;                  /* S16.15 */
	int32_t u, v, w, d;
};

static inline int S3D_Clamp8(int x) {
	return (x < 0) ? 0 : ((x > 255) ? 255 : x);
}

/* sign extend a 16-bit S8.7 color delta packed into a 32-bit register */
static inline int32_t S3D_Lo16s(uint32_t v) { return (int32_t)((int16_t)(v & 0xFFFFu)); }
static inline int32_t S3D_Hi16s(uint32_t v) { return (int32_t)((int16_t)(v >> 16u)); }

static inline uint32_t S3D_BaseMask(void) {
	/* bits 21-3 for 4MB parts; allow bit 22 on configurations with more memory */
	return (vga.mem.memsize > (4u << 20u)) ? 0x7FFFF8u : 0x3FFFF8u;
}

static bool S3D_SetupCommon(S3DContext &c, uint32_t cmd, const uint32_t *regs) {
	c.vram = vga.mem.linear;
	c.memsize = vga.mem.memsize;
	c.memmask = vga.mem.memmask;
	if (c.vram == NULL || c.memsize == 0) return false;

	c.cmd = cmd;
	c.command = S3D_CMD_COMMAND(cmd);

	switch (S3D_CMD_DEST_FMT(cmd)) {
		case 0: c.bypp = 1; break;
		case 1: c.bypp = 2; break;
		case 2: c.bypp = 3; break;
		default:
			LOG(LOG_VGA,LOG_WARN)("S3D: reserved destination format %u, command ignored",(unsigned int)S3D_CMD_DEST_FMT(cmd));
			return false;
	}

	const uint32_t bmask = S3D_BaseMask();
	c.z_base      = regs[0xD4 >> 2] & bmask;
	c.dest_base   = regs[0xD8 >> 2] & bmask;
	c.clip_r      = (int)(regs[0xDC >> 2] & 0x7FFu);
	c.clip_l      = (int)((regs[0xDC >> 2] >> 16u) & 0x7FFu);
	c.clip_b      = (int)(regs[0xE0 >> 2] & 0x7FFu);
	c.clip_t      = (int)((regs[0xE0 >> 2] >> 16u) & 0x7FFu);
	c.dest_stride = (regs[0xE4 >> 2] >> 16u) & 0xFF8u;
	c.z_stride    = regs[0xE8 >> 2] & 0xFF8u;
	c.hc          = (cmd & S3D_CMD_HC) != 0;

	const uint32_t fog = regs[0xF4 >> 2];
	c.fog_b = (uint8_t)fog;
	c.fog_g = (uint8_t)(fog >> 8u);
	c.fog_r = (uint8_t)(fog >> 16u);

	c.zmode = S3D_CMD_ZB_MODE(cmd);
	c.zcomp = S3D_CMD_ZB_COMP(cmd);
	c.zup   = (cmd & S3D_CMD_ZUP) != 0;
	/* [DB] 15.4.6: Z buffering is enabled when ZB MODE = 00b and ZB COMP != 000b */
	c.ztest = (c.zmode == 0 && c.zcomp != 0);
	if ((c.zmode == 1 || c.zmode == 2) && c.bypp != 2) {
		/* [DB] 15.4.7: MUX buffering requires a 16 bits/pixel destination */
		c.zmode = 3;
	}

	c.abc = S3D_CMD_ABC(cmd);
	c.textured = c.lit = c.persp = c.palettized = false;
	c.tex_blend = 2;
	c.fog = false;
	return true;
}

static void S3D_SetupTexture(S3DContext &c) {
	static const unsigned tex_bytes[8] = {
		4, /* ARGB8888 */
		2, /* ARGB4444 */
		2, /* ARGB1555 */
		1, /* Alpha4/Blend4 */
		1, /* Blend4 low nibble (one texel per byte, [DB] 15.4.8.2) */
		1, /* Blend4 high nibble */
		1, /* palettized */
		2  /* YU/YV (16 bits/pixel equivalent) */
	};

	c.tex_fmt  = S3D_CMD_TEX_FMT(c.cmd);
	c.tex_bypp = tex_bytes[c.tex_fmt];
	c.filter   = S3D_CMD_FILTER(c.cmd);
	c.max_d    = (int)S3D_CMD_MIP_SIZE(c.cmd);
	if (c.max_d > 9) c.max_d = 9; /* largest allowed is 9 (512x512) [DB] 19.4 */
	c.wrap     = (c.cmd & S3D_CMD_TWE) != 0;
	c.palettized = (c.tex_fmt == S3D_TEX_PAL8);

	/* [DB] 15.4.8.1: palettized texels can only be used unfiltered */
	if (c.palettized) {
		if (c.filter < 4) c.filter = 0; /* M1TPP */
		else c.filter = 4; /* 1TPP */
	}
	if (c.filter == 7) c.filter = 4; /* reserved: treat as 1TPP */

	/* Mipmap levels are stored largest first, each level immediately following the
	 * previous one (same layout as 86Box). Level n is (2^n x 2^n) texels. */
	uint32_t base = TREG(0xEC) & S3D_BaseMask();
	for (int lv = 9; lv >= 0; lv--) {
		c.tex_level_base[lv] = base;
		if (lv <= c.max_d) base += ((uint32_t)1u << (2u * (unsigned)lv)) * c.tex_bypp;
	}

	/* flat (non-mipmapped) textures may have their own row pitch */
	c.flat_stride = 0;
	if (c.filter >= 4) c.flat_stride = TREG(0xE4) & 0xFF8u;

	/* TBU/TBV are (4+s).(16-s); the U/V accumulators are (4+s).(27-s) */
	c.tbu = (int32_t)((TREG(0x108) & 0xFFFFFu) << 11u);
	c.tbv = (int32_t)((TREG(0x104) & 0xFFFFFu) << 11u);
	c.tex_bdr_clr = TREG(0xF0) & 0xFFFFFFu;

	const uint32_t c0 = TREG(0xF8), c1 = TREG(0xFC);
	c.color0.b = (int)(c0 & 0xFFu); c.color0.g = (int)((c0 >> 8u) & 0xFFu); c.color0.r = (int)((c0 >> 16u) & 0xFFu); c.color0.a = 255;
	c.color1.b = (int)(c1 & 0xFFu); c.color1.g = (int)((c1 >> 8u) & 0xFFu); c.color1.r = (int)((c1 >> 16u) & 0xFFu); c.color1.a = 255;
}

/* Blend4: the 4-bit texel value interpolates between COLOR0 (0) and COLOR1 (15) [DB] 15.4.8.2 */
static inline void S3D_Blend4(const S3DContext &c, unsigned t, S3DColor &o) {
	o.r = c.color0.r + (((c.color1.r - c.color0.r) * (int)t) / 15);
	o.g = c.color0.g + (((c.color1.g - c.color0.g) * (int)t) / 15);
	o.b = c.color0.b + (((c.color1.b - c.color0.b) * (int)t) / 15);
}

static void S3D_DecodeTexel(const S3DContext &c, uint32_t val, uint32_t pairval, bool odd, S3DColor &o) {
	switch (c.tex_fmt) {
		case S3D_TEX_ARGB8888:
			o.b = (int)(val & 0xFFu);
			o.g = (int)((val >> 8u) & 0xFFu);
			o.r = (int)((val >> 16u) & 0xFFu);
			o.a = (int)((val >> 24u) & 0xFFu);
			break;
		case S3D_TEX_ARGB4444:
			o.b = (int)((val & 0x000Fu) * 0x11u);
			o.g = (int)(((val >> 4u) & 0xFu) * 0x11u);
			o.r = (int)(((val >> 8u) & 0xFu) * 0x11u);
			o.a = (int)(((val >> 12u) & 0xFu) * 0x11u);
			break;
		case S3D_TEX_ARGB1555:
			o.b = (int)(((val & 0x001Fu) << 3u) | ((val & 0x001Cu) >> 2u));
			o.g = (int)(((val & 0x03E0u) >> 2u) | ((val & 0x0380u) >> 7u));
			o.r = (int)(((val & 0x7C00u) >> 7u) | ((val & 0x7000u) >> 12u));
			o.a = (val & 0x8000u) ? 255 : 0;
			break;
		case S3D_TEX_ALPHA4BLEND4:
			/* [DB] does not say which nibble holds alpha. The high nibble is assumed, matching
			 * the placement of alpha in every other ViRGE texel format. */
			S3D_Blend4(c, val & 0xFu, o);
			o.a = (int)(((val >> 4u) & 0xFu) * 0x11u);
			break;
		case S3D_TEX_BLEND4_LO:
			S3D_Blend4(c, val & 0xFu, o);
			o.a = 255;
			break;
		case S3D_TEX_BLEND4_HI:
			S3D_Blend4(c, (val >> 4u) & 0xFu, o);
			o.a = 255;
			break;
		case S3D_TEX_PAL8: {
			/* The CLUT index is carried in the blue channel so it survives to an 8bpp
			 * destination unchanged. For other destinations (which [DB] does not allow)
			 * we look the index up in the DAC so the result is at least a sensible color. */
			const unsigned idx = val & 0xFFu;
			if (c.bypp == 1) {
				o.r = o.g = 0; o.b = (int)idx;
			}
			else {
				const RGBEntry &e = vga.dac.rgb[idx];
				if (vga.dac.bits == 8) { o.r = e.red; o.g = e.green; o.b = e.blue; }
				else { o.r = (e.red << 2) | (e.red >> 4); o.g = (e.green << 2) | (e.green >> 4); o.b = (e.blue << 2) | (e.blue >> 4); }
			}
			o.a = 255;
			break; }
		case S3D_TEX_YUV: default: {
			/* YU/YV: each texel carries Y in its low byte and U (even texel) or V (odd
			 * texel) in its high byte. Converted with the ITU-R BT.601 equations. */
			const int y  = (int)(val & 0xFFu) - 16;
			const int uu = (int)(((odd ? pairval : val) >> 8u) & 0xFFu) - 128;
			const int vv = (int)(((odd ? val : pairval) >> 8u) & 0xFFu) - 128;
			o.r = S3D_Clamp8((298 * y + 409 * vv + 128) >> 8);
			o.g = S3D_Clamp8((298 * y - 100 * uu - 208 * vv + 128) >> 8);
			o.b = S3D_Clamp8((298 * y + 516 * uu + 128) >> 8);
			o.a = 255;
			break; }
	}
}

/* Fetch one texel. ui/vi are integer texel coordinates in mipmap level 'level'. */
static void S3D_FetchTexel(const S3DContext &c, int level, int ui, int vi, S3DColor &o) {
	const int size = 1 << level;
	ui &= size - 1;
	vi &= size - 1;

	uint32_t row;
	if (c.flat_stride != 0) row = c.flat_stride;
	else row = (uint32_t)size * c.tex_bypp;

	const uint32_t addr = (c.tex_level_base[level] + ((uint32_t)vi * row) + ((uint32_t)ui * c.tex_bypp)) & c.memmask;
	/* NTS: vga.mem.linear is allocated with 32 bytes of slack, so a 4-byte read at memmask is safe */
	uint32_t val, pairval = 0;
	switch (c.tex_bypp) {
		case 4: val = host_readd(c.vram + addr); break;
		case 2:
			val = host_readw(c.vram + addr);
			if (c.tex_fmt == S3D_TEX_YUV) pairval = host_readw(c.vram + ((addr ^ 2u) & c.memmask));
			break;
		default: val = c.vram[addr]; break;
	}
	S3D_DecodeTexel(c, val, pairval, (ui & 1) != 0, o);
}

static void S3D_BorderTexel(const S3DContext &c, S3DColor &o) {
	/* The border color is stored in the texel format ([DB] 19.4 TEX_BDR_CLR) */
	S3D_DecodeTexel(c, c.tex_bdr_clr, c.tex_bdr_clr, false, o);
}

/* Sample a texture level at normalized coordinates u,v (1.0 = 1 << 27).
 * taps: 1 = nearest, 2 = vertical pair (V2TPP), 4 = bilinear */
static void S3D_SampleLevel(const S3DContext &c, int level, int32_t u, int32_t v, unsigned taps, S3DColor &o) {
	const int shift = 27 - level;

	if (!c.wrap && ((u | v) & (int32_t)0xF8000000)) {
		/* [DB] 15.4.8.3: without wrapping, texels beyond the texture use the border color */
		S3D_BorderTexel(c, o);
		return;
	}

	const int ui = (int)(u >> shift);
	const int vi = (int)(v >> shift);

	if (taps == 1) {
		S3D_FetchTexel(c, level, ui, vi, o);
		return;
	}

	/* 8 filter bits below the integer texel position */
	const int du = (int)((u >> (shift - 8)) & 0xFF);
	const int dv = (int)((v >> (shift - 8)) & 0xFF);
	const int last = (1 << level) - 1;
	/* Without wrapping, neighbours past the right/bottom edge repeat the edge texel
	 * rather than bleeding the border color into the last row/column. */
	const int ui1 = (c.wrap || ui < last) ? ui + 1 : ui;
	const int vi1 = (c.wrap || vi < last) ? vi + 1 : vi;

	S3DColor t0, t1, t2, t3;
	if (taps == 2) {
		S3D_FetchTexel(c, level, ui, vi,  t0);
		S3D_FetchTexel(c, level, ui, vi1, t2);
		o.r = (t0.r * (256 - dv) + t2.r * dv) >> 8;
		o.g = (t0.g * (256 - dv) + t2.g * dv) >> 8;
		o.b = (t0.b * (256 - dv) + t2.b * dv) >> 8;
		o.a = (t0.a * (256 - dv) + t2.a * dv) >> 8;
		return;
	}

	S3D_FetchTexel(c, level, ui,  vi,  t0);
	S3D_FetchTexel(c, level, ui1, vi,  t1);
	S3D_FetchTexel(c, level, ui,  vi1, t2);
	S3D_FetchTexel(c, level, ui1, vi1, t3);

	const int w0 = (256 - du) * (256 - dv);
	const int w1 = du * (256 - dv);
	const int w2 = (256 - du) * dv;
	const int w3 = du * dv;
	o.r = (t0.r * w0 + t1.r * w1 + t2.r * w2 + t3.r * w3) >> 16;
	o.g = (t0.g * w0 + t1.g * w1 + t2.g * w2 + t3.g * w3) >> 16;
	o.b = (t0.b * w0 + t1.b * w1 + t2.b * w2 + t3.b * w3) >> 16;
	o.a = (t0.a * w0 + t1.a * w1 + t2.a * w2 + t3.a * w3) >> 16;
}

static void S3D_SampleTexture(const S3DContext &c, const S3DAttr &at, S3DColor &o) {
	int32_t u, v;

	if (c.persp) {
		/* U and V are premultiplied by W. The shift matches the original ViRGE/ViRGE/VX
		 * (86Box uses a shift of 8 instead of 12 for ViRGE/DX and later). */
		int64_t w = 0;
		if (at.w > 0) {
			w = ((int64_t)1 << 46) / (int64_t)at.w;
			if (w > 0x7FFFFFFF) w = 0x7FFFFFFF;
		}
		u = (int32_t)(((int64_t)at.u * w) >> (12 + c.max_d)) + c.tbu;
		v = (int32_t)(((int64_t)at.v * w) >> (12 + c.max_d)) + c.tbv;
	}
	else {
		u = at.u + c.tbu;
		v = at.v + c.tbv;
	}

	/* mipmap level from the integer part of D (S4.27) */
	int level = c.max_d;
	int dfrac = 0;
	if (c.filter < 4 && at.d >= 0) {
		level = c.max_d - (int)((at.d >> 27) & 0xF);
		dfrac = (int)((at.d >> 19) & 0xFF);
		if (level < 0) { level = 0; dfrac = 0; }
	}

	switch (c.filter) {
		case 0: /* M1TPP: nearest texel of level D */
		case 4: /* 1TPP */
			S3D_SampleLevel(c, level, u, v, 1, o);
			break;
		case 5: /* V2TPP */
			S3D_SampleLevel(c, level, u, v, 2, o);
			break;
		case 2: /* M4TPP: bilinear within level D */
		case 6: /* 4TPP */
			S3D_SampleLevel(c, level, u, v, 4, o);
			break;
		case 1: /* M2TPP: nearest texel of levels D and D+1, interpolated */
		case 3: /* M8TPP: bilinear in levels D and D+1, interpolated */ {
			const unsigned taps = (c.filter == 1) ? 1u : 4u;
			S3D_SampleLevel(c, level, u, v, taps, o);
			if (level > 0 && dfrac != 0) {
				S3DColor o2;
				S3D_SampleLevel(c, level - 1, u, v, taps, o2);
				o.r += ((o2.r - o.r) * dfrac) >> 8;
				o.g += ((o2.g - o.g) * dfrac) >> 8;
				o.b += ((o2.b - o.b) * dfrac) >> 8;
				o.a += ((o2.a - o.a) * dfrac) >> 8;
			}
			break; }
		default:
			S3D_SampleLevel(c, level, u, v, 1, o);
			break;
	}
}

static inline bool S3D_ZPass(unsigned comp, uint32_t zs, uint32_t zzb) {
	switch (comp) {
		case 0: return false;
		case 1: return zs >  zzb;
		case 2: return zs == zzb;
		case 3: return zs >= zzb;
		case 4: return zs <  zzb;
		case 5: return zs != zzb;
		case 6: return zs <= zzb;
		default: return true;
	}
}

static inline uint32_t S3D_Z16(int32_t z) {
	/* S16.15 -> 16-bit depth value */
	if (z < 0) return 0;
	const uint32_t zz = (uint32_t)z >> 15u;
	return (zz > 0xFFFFu) ? 0xFFFFu : zz;
}

/* Render one pixel through the [DB] figure 15-7 pipeline */
static void S3D_Pixel(const S3DContext &c, int x, int y, const S3DAttr &at) {
	/* coordinates are 11 bits unsigned; discard anything outside (see header comment) */
	if ((unsigned)x > 2047u || (unsigned)y > 2047u) return;
	if (c.hc && (x < c.clip_l || x > c.clip_r || y < c.clip_t || y > c.clip_b)) return;

	const uint32_t dest_addr = c.dest_base + ((uint32_t)y * c.dest_stride) + ((uint32_t)x * c.bypp);
	if (dest_addr + c.bypp > c.memsize) return;

	const uint32_t zs = S3D_Z16(at.z);
	uint32_t z_addr = 0;

	if (c.ztest) {
		z_addr = c.z_base + ((uint32_t)y * c.z_stride) + ((uint32_t)x * 2u);
		if (z_addr + 2u > c.memsize) return;
		if (!S3D_ZPass(c.zcomp, zs, host_readw(c.vram + z_addr))) return;
	}
	else if (c.zmode == 1) {
		/* MUX buffering, Z buffer pass [DB] 15.4.7: bit 15 set = the word holds a Z value */
		const uint16_t d = host_readw(c.vram + dest_addr);
		const uint32_t zs15 = zs >> 1u;
		if ((d & 0x8000u) && !S3D_ZPass(c.zcomp, zs15, d & 0x7FFFu)) return;
		host_writew(c.vram + dest_addr, (uint16_t)(zs15 | 0x8000u));
		return;
	}
	else if (c.zmode == 2) {
		/* MUX buffering, draw buffer pass: only replace Z values that match */
		const uint16_t d = host_readw(c.vram + dest_addr);
		if (!(d & 0x8000u)) return;
		if ((zs >> 1u) != (uint32_t)(d & 0x7FFFu)) return;
	}

	/* source (vertex) color */
	S3DColor src;
	src.r = S3D_Clamp8(at.r >> 7);
	src.g = S3D_Clamp8(at.g >> 7);
	src.b = S3D_Clamp8(at.b >> 7);
	src.a = S3D_Clamp8(at.a >> 7);

	S3DColor col;
	if (c.textured) {
		S3D_SampleTexture(c, at, col);
		if (c.lit && !c.palettized) {
			switch (c.tex_blend) {
				case 0: /* complex reflection: add, saturate */
					col.r = S3D_Clamp8(col.r + src.r);
					col.g = S3D_Clamp8(col.g + src.g);
					col.b = S3D_Clamp8(col.b + src.b);
					break;
				case 1: /* modulate */
					col.r = (col.r * src.r) >> 8;
					col.g = (col.g * src.g) >> 8;
					col.b = (col.b * src.b) >> 8;
					break;
				default: /* decal (and reserved 11b) */
					break;
			}
		}
	}
	else {
		col = src;
	}

	if (!c.palettized) {
		if (c.fog) {
			/* [DB] 15.4.8.4: the source alpha interpolates between pixel and fog color */
			const int a = src.a;
			col.r = (col.r * a + c.fog_r * (255 - a)) / 255;
			col.g = (col.g * a + c.fog_g * (255 - a)) / 255;
			col.b = (col.b * a + c.fog_b * (255 - a)) / 255;
		}

		if ((c.abc == 2 || c.abc == 3) && c.bypp != 1) {
			/* [DB] 15.4.8.5: 10b = alpha at this stage, 11b = source alpha before texturing */
			const int a = (c.abc == 3) ? src.a : S3D_Clamp8(col.a);
			int dr, dg, db;
			if (c.bypp == 2) {
				const uint32_t d = host_readw(c.vram + dest_addr);
				db = (int)(((d & 0x001Fu) << 3u) | ((d & 0x001Cu) >> 2u));
				dg = (int)(((d & 0x03E0u) >> 2u) | ((d & 0x0380u) >> 7u));
				dr = (int)(((d & 0x7C00u) >> 7u) | ((d & 0x7000u) >> 12u));
			}
			else {
				db = c.vram[dest_addr];
				dg = c.vram[dest_addr + 1u];
				dr = c.vram[dest_addr + 2u];
			}
			col.r = (col.r * a + dr * (255 - a)) / 255;
			col.g = (col.g * a + dg * (255 - a)) / 255;
			col.b = (col.b * a + db * (255 - a)) / 255;
		}
	}

	switch (c.bypp) {
		case 1:
			c.vram[dest_addr] = (uint8_t)S3D_Clamp8(col.b);
			break;
		case 2:
			/* ZRGB1555: bit 15 is the MUX buffering Z flag and is written as 0 */
			host_writew(c.vram + dest_addr, (uint16_t)(
				((unsigned)S3D_Clamp8(col.b) >> 3u) |
				(((unsigned)S3D_Clamp8(col.g) >> 3u) << 5u) |
				(((unsigned)S3D_Clamp8(col.r) >> 3u) << 10u)));
			break;
		default:
			c.vram[dest_addr + 0u] = (uint8_t)S3D_Clamp8(col.b);
			c.vram[dest_addr + 1u] = (uint8_t)S3D_Clamp8(col.g);
			c.vram[dest_addr + 2u] = (uint8_t)S3D_Clamp8(col.r);
			break;
	}

	if (c.ztest && c.zup) host_writew(c.vram + z_addr, (uint16_t)zs);
}

/* ---------------------------------------------------------------------------------------- */
/* 3D triangle                                                                              */
/* ---------------------------------------------------------------------------------------- */

struct S3DTriDeltas {
	int32_t dRdX, dGdX, dBdX, dAdX, dZdX, dUdX, dVdX, dWdX, dDdX;
	int32_t dRdY, dGdY, dBdY, dAdY, dZdY, dUdY, dVdY, dWdY, dDdY;
};

/* Render one of the two parts of a triangle (bottom part along side 01, top part along side 12).
 * Walks upward (decreasing y) as the hardware does [DB] 15.4.5.2. */
static void S3D_TriPart(const S3DContext &c, const S3DTriDeltas &dt, int xdir, int &y, int32_t &x1, int32_t x2,
	int32_t dx1, int32_t dx2, int ycount, S3DAttr &base) {

	if (ycount > 2048) ycount = 2048;

	for (; ycount > 0; ycount--) {
		const bool line_visible = (unsigned)y <= 2047u && (!c.hc || (y >= c.clip_t && y <= c.clip_b));

		if (line_visible) {
			int x  = (int)((x1 + ((1 << 20) - 1)) >> 20);
			int xe = (int)((x2 + ((1 << 20) - 1)) >> 20);

			if (xdir < 0) {
				x--;
				xe--;
			}

			if (x != xe && ((xdir > 0 && x < xe) || (xdir < 0 && x > xe))) {
				/* sub-pixel pre-step from the edge to the first pixel, in 1/32 pixel units */
				int dx = (xdir > 0) ? ((31 - ((x1 - 1) >> 15)) & 0x1F) : (((x1 - 1) >> 15) & 0x1F);
				if (xdir > 0) dx += 1;

				S3DAttr at;
				at.r = base.r + ((dt.dRdX * dx) >> 5);
				at.g = base.g + ((dt.dGdX * dx) >> 5);
				at.b = base.b + ((dt.dBdX * dx) >> 5);
				at.a = base.a + ((dt.dAdX * dx) >> 5);
				at.z = base.z + ((dt.dZdX * dx) >> 5);
				at.u = base.u + ((dt.dUdX * dx) >> 5);
				at.v = base.v + ((dt.dVdX * dx) >> 5);
				at.w = base.w + ((dt.dWdX * dx) >> 5);
				at.d = base.d + ((dt.dDdX * dx) >> 5);

				bool draw = true;
				if (c.hc) {
					int skip = 0;
					if (xdir > 0) {
						if (x > c.clip_r || xe <= c.clip_l) draw = false;
						else {
							if (xe > c.clip_r + 1) xe = c.clip_r + 1;
							if (x < c.clip_l) { skip = c.clip_l - x; x = c.clip_l; }
						}
					}
					else {
						if (x < c.clip_l || xe >= c.clip_r) draw = false;
						else {
							if (xe < c.clip_l - 1) xe = c.clip_l - 1;
							if (x > c.clip_r) { skip = x - c.clip_r; x = c.clip_r; }
						}
					}
					if (skip > 0) {
						at.r += dt.dRdX * skip; at.g += dt.dGdX * skip; at.b += dt.dBdX * skip; at.a += dt.dAdX * skip;
						at.z += dt.dZdX * skip; at.u += dt.dUdX * skip; at.v += dt.dVdX * skip;
						at.w += dt.dWdX * skip; at.d += dt.dDdX * skip;
					}
				}

				if (draw) {
					int count = (xdir > 0) ? (xe - x) : (x - xe);
					if (count > 4096) count = 4096;
					for (; count > 0; count--) {
						S3D_Pixel(c, x, y, at);
						at.r += dt.dRdX; at.g += dt.dGdX; at.b += dt.dBdX; at.a += dt.dAdX;
						at.z += dt.dZdX; at.u += dt.dUdX; at.v += dt.dVdX; at.w += dt.dWdX; at.d += dt.dDdX;
						x += xdir;
					}
				}
			}
		}

		x1 += dx1;
		x2 += dx2;
		base.r += dt.dRdY; base.g += dt.dGdY; base.b += dt.dBdY; base.a += dt.dAdY;
		base.z += dt.dZdY; base.u += dt.dUdY; base.v += dt.dVdY; base.w += dt.dWdY; base.d += dt.dDdY;
		y--;
	}
}

static void S3D_Triangle(void) {
	S3DContext c;
	const uint32_t cmd = TREG(0x100);

	if (!S3D_SetupCommon(c, cmd, s3d.tri)) return;

	switch (c.command) {
		case S3D_GOURAUD_TRI:
			break;
		case S3D_LIT_TEX_TRI:
		case S3D_LIT_TEX_TRI_PERSP:
			c.textured = c.lit = true;
			break;
		case S3D_UNLIT_TEX_TRI:
		case S3D_UNLIT_TEX_TRI_PERSP:
			c.textured = true;
			break;
		default:
			return;
	}

	if (c.textured) {
		c.persp = (c.command == S3D_LIT_TEX_TRI_PERSP || c.command == S3D_UNLIT_TEX_TRI_PERSP);
		c.tex_blend = S3D_CMD_TEX_BLEND(cmd);
		S3D_SetupTexture(c);
		c.fog = (cmd & S3D_CMD_FE) != 0;
	}

	S3DTriDeltas dt;
	dt.dBdX = S3D_Lo16s(TREG(0x13C)); dt.dGdX = S3D_Hi16s(TREG(0x13C));
	dt.dRdX = S3D_Lo16s(TREG(0x140)); dt.dAdX = S3D_Hi16s(TREG(0x140));
	dt.dBdY = S3D_Lo16s(TREG(0x144)); dt.dGdY = S3D_Hi16s(TREG(0x144));
	dt.dRdY = S3D_Lo16s(TREG(0x148)); dt.dAdY = S3D_Hi16s(TREG(0x148));
	dt.dWdX = (int32_t)TREG(0x10C);   dt.dWdY = (int32_t)TREG(0x110);
	dt.dDdX = (int32_t)TREG(0x118);   dt.dDdY = (int32_t)TREG(0x124);
	dt.dVdX = (int32_t)TREG(0x11C);   dt.dVdY = (int32_t)TREG(0x128);
	dt.dUdX = (int32_t)TREG(0x120);   dt.dUdY = (int32_t)TREG(0x12C);
	dt.dZdX = (int32_t)TREG(0x154);   dt.dZdY = (int32_t)TREG(0x158);

	S3DAttr base;
	base.b = (int32_t)(TREG(0x14C) & 0xFFFFu);
	base.g = (int32_t)(TREG(0x14C) >> 16u);
	base.r = (int32_t)(TREG(0x150) & 0xFFFFu);
	base.a = (int32_t)(TREG(0x150) >> 16u);
	base.w = (int32_t)TREG(0x114);
	base.d = (int32_t)TREG(0x130);
	base.v = (int32_t)TREG(0x134);
	base.u = (int32_t)TREG(0x138);
	base.z = (int32_t)TREG(0x15C);

	const uint32_t ycnt = TREG(0x17C);
	const int ty12 = (int)(ycnt & 0x7FFu);
	const int ty01 = (int)((ycnt >> 16u) & 0x7FFu);
	const int xdir = (ycnt & 0x80000000u) ? 1 : -1;

	int y = (int)(TREG(0x178) & 0x7FFu);
	int32_t x1 = (int32_t)TREG(0x174);                  /* along side 02 */

	S3D_TriPart(c, dt, xdir, y, x1, (int32_t)TREG(0x16C), (int32_t)TREG(0x170), (int32_t)TREG(0x168), ty01, base);
	S3D_TriPart(c, dt, xdir, y, x1, (int32_t)TREG(0x164), (int32_t)TREG(0x170), (int32_t)TREG(0x160), ty12, base);
}

/* ---------------------------------------------------------------------------------------- */
/* 3D line                                                                                  */
/* ---------------------------------------------------------------------------------------- */

/* 3D lines use the same X start/delta/end semantics as 2D lines ([DB] 15.4.5.1 "3D line drawing
 * is very similar to 2D line drawing"), drawn bottom-up. Z and color deltas are applied once per
 * pixel along the major axis, which is how [Mesa] computes them. */
static void S3D_Line(void) {
	S3DContext c;
	const uint32_t cmd = LREG(0x100);

	if (!S3D_SetupCommon(c, cmd, s3d.line)) return;
	c.fog = (cmd & S3D_CMD_FE) != 0;

	const int32_t dB = S3D_Lo16s(LREG(0x144)), dG = S3D_Hi16s(LREG(0x144));
	const int32_t dR = S3D_Lo16s(LREG(0x148)), dA = S3D_Hi16s(LREG(0x148));
	const int32_t dZ = (int32_t)LREG(0x158);

	S3DAttr at;
	memset(&at, 0, sizeof(at));
	at.b = (int32_t)(LREG(0x14C) & 0xFFFFu);
	at.g = (int32_t)(LREG(0x14C) >> 16u);
	at.r = (int32_t)(LREG(0x150) & 0xFFFFu);
	at.a = (int32_t)(LREG(0x150) >> 16u);
	at.z = (int32_t)LREG(0x15C);

	const int end1 = (int)((int16_t)(LREG(0x16C) & 0xFFFFu));   /* last pixel, top scanline */
	const int end0 = (int)((int16_t)(LREG(0x16C) >> 16u));      /* first pixel, bottom scanline */
	const int32_t xdelta = (int32_t)LREG(0x170);
	int32_t xf = (int32_t)LREG(0x174);
	int y = (int)(LREG(0x178) & 0x7FFu);
	int ycount = (int)(LREG(0x17C) & 0x7FFu);
	const int xdir = (LREG(0x17C) & 0x80000000u) ? 1 : -1;
	const int lo = (end0 < end1) ? end0 : end1;
	const int hi = (end0 < end1) ? end1 : end0;

#define S3D_LINE_STEP() do { at.r += dR; at.g += dG; at.b += dB; at.a += dA; at.z += dZ; } while (0)

	if (ycount <= 1) {
		/* single scanline: END0 through END1 */
		int x = end0;
		int guard = 4096;
		while (guard-- > 0) {
			S3D_Pixel(c, x, y, at);
			S3D_LINE_STEP();
			if (x == end1) break;
			x += (end1 > x) ? 1 : -1;
		}
	}
	else if (xdelta >= -(1 << 20) && xdelta <= (1 << 20)) {
		/* Y major: one pixel per scanline */
		for (; ycount > 0; ycount--) {
			const int x = (int)(xf >> 20);
			if (x >= lo && x <= hi) {
				S3D_Pixel(c, x, y, at);
				S3D_LINE_STEP();
			}
			xf += xdelta;
			y--;
		}
	}
	else {
		/* X major: a run of pixels per scanline, from the current x up to the accumulator */
		int x = end0;
		for (; ycount > 0; ycount--) {
			const int xto = (ycount == 1) ? end1 : (int)(xf >> 20);
			int guard = 4096;
			while (guard-- > 0 && ((xdir > 0) ? (x <= xto) : (x >= xto))) {
				if (x >= lo && x <= hi) {
					S3D_Pixel(c, x, y, at);
					S3D_LINE_STEP();
				}
				x += xdir;
			}
			xf += xdelta;
			y--;
		}
	}

#undef S3D_LINE_STEP
}

/* ---------------------------------------------------------------------------------------- */
/* Register interface                                                                       */
/* ---------------------------------------------------------------------------------------- */

static void S3D_Execute(uint32_t cmd, bool from_line_block) {
	if (!(cmd & S3D_CMD_3D)) return;

	const unsigned command = S3D_CMD_COMMAND(cmd);
	switch (command) {
		case S3D_NOP:
			return;
		case S3D_LINE:
			S3D_Line();
			break;
		case S3D_GOURAUD_TRI:
		case S3D_LIT_TEX_TRI:
		case S3D_UNLIT_TEX_TRI:
		case S3D_LIT_TEX_TRI_PERSP:
		case S3D_UNLIT_TEX_TRI_PERSP:
			S3D_Triangle();
			break;
		default:
			LOG(LOG_VGA,LOG_DEBUG)("S3D: reserved 3D command %u (cmd=%08x, %s block)",
				command,(unsigned int)cmd,from_line_block ? "line" : "triangle");
			return;
	}

	/* the engine finishes synchronously: flag S3d Engine Done */
	S3_ViRGE_SetSubsysStatus(0x02u);
}

/* Handle a write in the 3D register blocks (MMIO offsets B000-B1FF and B400-B5FF).
 * Returns false if the port is not an S3D 3D register. */
bool S3D_Write(Bitu port, uint32_t val, Bitu len) {
	const unsigned int blk = (unsigned int)port & 0xFE00u;
	if (blk != 0xB000u && blk != 0xB400u) return false;

	const bool is_line = (blk == 0xB000u);
	const unsigned int off = (unsigned int)port & 0x1FFu;
	uint32_t *regs = is_line ? s3d.line : s3d.tri;
	uint32_t &reg = regs[off >> 2u];

	/* byte and word writes merge into the containing doubleword */
	if (len == 4 && (off & 3u) == 0) {
		reg = val;
	}
	else {
		const unsigned int sh = (off & 3u) * 8u;
		const uint32_t msk = ((len >= 4) ? 0xFFFFFFFFu : ((1u << (len * 8u)) - 1u)) << sh;
		reg = (reg & ~msk) | ((val << sh) & msk);
	}

	const unsigned int aoff = off & 0x1FCu;
	if (S3D_IsSharedReg(aoff)) {
		s3d.line[aoff >> 2u] = reg;
		s3d.tri[aoff >> 2u] = reg;
	}

	/* only act once the last byte of the register has been written */
	if (((off & 3u) + len) < 4u) return true;

	if (aoff == 0x100u) {
		if (!(reg & S3D_CMD_AE)) S3D_Execute(reg, is_line);
	}
	else if (aoff == 0x17Cu) {
		const uint32_t cmd = regs[0x100u >> 2u];
		if (cmd & S3D_CMD_AE) {
			const unsigned command = S3D_CMD_COMMAND(cmd);
			if (is_line ? (command == S3D_LINE) : (command != S3D_LINE)) S3D_Execute(cmd, is_line);
		}
	}

	return true;
}

bool S3D_Read(Bitu port, Bitu len, uint32_t &val) {
	const unsigned int blk = (unsigned int)port & 0xFE00u;
	if (blk != 0xB000u && blk != 0xB400u) return false;

	const unsigned int off = (unsigned int)port & 0x1FFu;
	const uint32_t *regs = (blk == 0xB000u) ? s3d.line : s3d.tri;
	val = regs[off >> 2u] >> ((off & 3u) * 8u);
	if (len < 4) val &= (1u << (len * 8u)) - 1u;
	return true;
}

void S3D_Reset(void) {
	memset(&s3d, 0, sizeof(s3d));
}

void POD_Save_VGA_S3D(std::ostream& stream) {
	WRITE_POD(&s3d, s3d);
}

void POD_Load_VGA_S3D(std::istream& stream) {
	READ_POD(&s3d, s3d);
}
