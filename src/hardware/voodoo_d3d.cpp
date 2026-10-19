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

/* The 3dfx emulation rendered with Direct3D 9, on the Direct3D output's device (it only works with
 * output=direct3d). It does what the OpenGL one (voodoo_opengl.cpp) does, the same way where it can:
 *
 *  - It renders into textures of its own at the render resolution (voodoo_resolution): front and
 *    back color buffers and a depth/stencil buffer, top row first. At every vertical retrace the
 *    front buffer is copied to another texture, which the output draws in place of the emulated
 *    video card's frame.
 *  - Triangles use a pixel shader made for the rasterizer's register settings, like the GLSL ones
 *    the OpenGL emulation makes, but as ps_2_0 shader code put together here (no shader compiler
 *    is needed). The vertices are transformed already, so there is no vertex shader. The rest of
 *    the state (depth, blending, masks, stencil) is in render states.
 *  - Textures are converted on the CPU like the OpenGL emulation does, mipmaps too.
 *  - LFB writes are drawn as points or small rectangles, LFB reads read the buffers back (kept until
 *    the buffer is drawn to again).
 *  - The output's worker thread does not use the device while this does (CDirect3D::BeginExternal),
 *    and its device reset (for a window resize, or a lost device) keeps the color buffers. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <vector>

#include "dosbox.h"
#include "control.h"
#include "logging.h"
#include "render.h"

#include "paging.h"
#include "voodoo.h"
#include "voodoo_emu.h"
#include "voodoo_opengl.h"
#include "voodoo_d3d.h"

#if C_DIRECT3D

#include <output/output_direct3d.h>
#include "voodoo_def.h"

using namespace std;

/*************************************
 *  Pixel shaders
 *************************************/

/* Puts ps_2_0 shader code together. */
class VD3DShader {
public:
	enum { REG_TEMP = 0, REG_CONST = 2, REG_TEXTURE = 3, REG_COLOROUT = 8, REG_SAMPLER = 10 };
	enum { OP_MOV = 1, OP_ADD = 2, OP_MUL = 5, OP_DP3 = 8, OP_MAX = 11, OP_DCL = 31, OP_TEXKILL = 65, OP_TEX = 66, OP_DEF = 81, OP_CMP = 88 };
	enum { SWZ_XYZW = 0xE4, SWZ_XXXX = 0x00, SWZ_YYYY = 0x55, SWZ_ZZZZ = 0xAA, SWZ_WWWW = 0xFF };
	enum { MASK_RGB = 0x7, MASK_A = 0x8, MASK_ALL = 0xF, MASK_X = 0x1 };

	VD3DShader() : alu(0) { code.push_back(0xFFFF0200); }

	static uint32_t RegType(int type) { return ((uint32_t)(type & 7) << 28) | ((uint32_t)((type >> 3) & 3) << 11); }
	static uint32_t Dst(int type, int idx, uint32_t mask = MASK_ALL) { return 0x80000000u | RegType(type) | (uint32_t)idx | (mask << 16); }
	static uint32_t Src(int type, int idx, uint32_t swizzle = SWZ_XYZW, bool negate = false) {
		return 0x80000000u | RegType(type) | (uint32_t)idx | (swizzle << 16) | (negate ? (1u << 24) : 0u);
	}
	/* shorthands: rN, cN, -rN, rN.w etc. */
	static uint32_t R(int i, uint32_t swz = SWZ_XYZW) { return Src(REG_TEMP, i, swz); }
	static uint32_t NR(int i, uint32_t swz = SWZ_XYZW) { return Src(REG_TEMP, i, swz, true); }
	static uint32_t C(int i, uint32_t swz = SWZ_XYZW) { return Src(REG_CONST, i, swz); }
	static uint32_t NC(int i, uint32_t swz = SWZ_XYZW) { return Src(REG_CONST, i, swz, true); }
	static uint32_t DR(int i, uint32_t mask = MASK_ALL) { return Dst(REG_TEMP, i, mask); }

	void Ins(uint32_t opcode, const vector<uint32_t> &params, uint32_t control = 0) {
		code.push_back(opcode | (control << 16) | ((uint32_t)params.size() << 24));
		code.insert(code.end(), params.begin(), params.end());
		if (opcode != OP_DCL && opcode != OP_DEF && opcode != OP_TEX && opcode != OP_TEXKILL) alu++;
	}
	void Def(int c, float a, float b, float d, float e) {
		const float f[4] = { a, b, d, e };
		vector<uint32_t> p(1, Dst(REG_CONST, c));
		for (int i = 0; i < 4; i++) { uint32_t u; memcpy(&u, &f[i], 4); p.push_back(u); }
		Ins(OP_DEF, p);
	}
	void DclTexcoord(int i) { Ins(OP_DCL, {0x80000000u, Dst(REG_TEXTURE, i)}); }
	void DclSampler(int i) { Ins(OP_DCL, {0x80000000u | (2u << 27) /* 2D */, Dst(REG_SAMPLER, i)}); }
	void TexProj(int dst_reg, int texcoord, int sampler) {
		Ins(OP_TEX, {DR(dst_reg), Src(REG_TEXTURE, texcoord), Src(REG_SAMPLER, sampler)}, 1 /* project */);
	}
	void Tex(int dst_reg, int texcoord, int sampler) {
		Ins(OP_TEX, {DR(dst_reg), Src(REG_TEXTURE, texcoord), Src(REG_SAMPLER, sampler)});
	}
	void Kill(int reg) { Ins(OP_TEXKILL, {DR(reg)}); }	/* (texkill takes its register as a destination) */
	void Out(int reg) { Ins(OP_MOV, {Dst(REG_COLOROUT, 0), R(reg)}); }

	vector<uint32_t> Finish(void) {
		code.push_back(0x0000FFFF);
		return code;
	}

	vector<uint32_t> code;
	int alu;
};

/* what a triangle pixel shader depends on, as the Vulkan emulation's VvkFsKey */
struct VD3DPsKey {
	UINT32 texmode[2];	/* 0 if that TMU is not used */
	UINT32 fbzcp;
	UINT32 alphamode;	/* alpha test bits */
	UINT32 fogmode;		/* 0 without fog */
	UINT32 flags;
	bool operator<(const VD3DPsKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};
#define VD3D_PS_TMU0		0x01
#define VD3D_PS_TMU1		0x02
#define VD3D_PS_CHROMAKEY	0x04

/* constants, like the OpenGL emulation's uniforms (c0-c4), and fixed ones (c10, c11) */
struct VD3DConstants {
	float c0[4];		/* color0 */
	float c1[4];		/* color1 */
	float chromakey[4];
	float fogcolor[4];
	float alpharef[4];	/* alpha reference, zaColor */
};

/* Makes a triangle pixel shader like ogl_shaders() makes GLSL, with the same steps. Registers:
 * r0 clocal, r1 cother, r2 texel, r3 tt, r4 blend, r5 pixel, r6 r7 scratch, r8 the iterated color,
 * r9 fog blend (x) and LOD blends (y, z). Inputs: t0 color, t1 t2 texture coordinates (s*w, t*w, 0, w)
 * of TMU 0 and 1, t3 fog and LOD blends. */
class VD3DPixelShader : public VD3DShader {
public:
	vector<uint32_t> Build(const VD3DPsKey &k) {
		key = k;
		for (int i = 0; i < 4; i++) DclTexcoord(i);
		if (key.flags & VD3D_PS_TMU0) DclSampler(0);
		if (key.flags & VD3D_PS_TMU1) DclSampler(1);
		Def(10, 0.0f, 0.5f, 1.0f, 0.0001f);
		Def(11, 0.0001f * 0.0001f, -1.0f, 0.000001f, 0.0f);

		Ins(OP_MOV, {DR(8), Src(REG_TEXTURE, 0)});
		Ins(OP_MOV, {DR(9), Src(REG_TEXTURE, 3)});
		/* vec4 texel = vec4(1.0), cother = vec4(0.0) */
		Ins(OP_MOV, {DR(2), C(10, SWZ_ZZZZ)});
		Ins(OP_MOV, {DR(1), C(10, SWZ_XXXX)});

		if (key.flags & VD3D_PS_TMU1) {
			TexProj(0, 2, 1);
			TexCombine(1);
			Ins(OP_MOV, {DR(1), R(0)});
			Ins(OP_MOV, {DR(2), R(0)});
		}
		if (key.flags & VD3D_PS_TMU0) {
			TexProj(0, 1, 0);
			TexCombine(0);
			Ins(OP_MOV, {DR(2), R(0)});
		}

		ColorPath();
		if (FOGMODE_ENABLE_FOG(key.fogmode)) Fog();
		Out(5);
		return Finish();
	}

private:
	/* ogl_sh_tex_combine(): r0 clocal (the texel), r1 cother, result in r0 */
	void TexCombine(int tmu) {
		const UINT32 TEXMODE = key.texmode[tmu];
		const uint32_t lod = tmu == 0 ? SWZ_YYYY : SWZ_ZZZZ;

		Ins(OP_MOV, {DR(3, MASK_RGB), TEXMODE_TC_ZERO_OTHER(TEXMODE) ? C(10, SWZ_XXXX) : R(1)});
		Ins(OP_MOV, {DR(3, MASK_A), TEXMODE_TCA_ZERO_OTHER(TEXMODE) ? C(10, SWZ_XXXX) : R(1)});
		if (TEXMODE_TC_SUB_CLOCAL(TEXMODE)) Ins(OP_ADD, {DR(3, MASK_RGB), R(3), NR(0)});
		if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE)) Ins(OP_ADD, {DR(3, MASK_A), R(3), NR(0)});

		uint32_t b;
		switch (TEXMODE_TC_MSELECT(TEXMODE)) {
			default:
			case 0: b = C(10, SWZ_XXXX); break;
			case 1: b = R(0); break;
			case 2: b = R(1, SWZ_WWWW); break;
			case 3: b = R(0, SWZ_WWWW); break;
			case 4: /* detail */
			case 5: /* LOD */
				b = R(9, lod); break;
		}
		Ins(OP_MOV, {DR(4, MASK_RGB), b});
		switch (TEXMODE_TCA_MSELECT(TEXMODE)) {
			default:
			case 0: b = C(10, SWZ_XXXX); break;
			case 1: b = R(0); break;
			case 2: b = R(1); break;
			case 3: b = R(0); break;
			case 4: /* detail */
			case 5: /* LOD */
				b = R(9, lod); break;
		}
		Ins(OP_MOV, {DR(4, MASK_A), b});

		if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE)) Ins(OP_ADD, {DR(4, MASK_RGB), C(10, SWZ_ZZZZ), NR(4)});
		if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE)) Ins(OP_ADD, {DR(4, MASK_A), C(10, SWZ_ZZZZ), NR(4)});
		Ins(OP_MUL, {DR(3), R(3), R(4)});

		switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE)) {
			case 3:
			case 0: break;
			case 1: Ins(OP_ADD, {DR(3, MASK_RGB), R(3), R(0)}); break;
			case 2: Ins(OP_ADD, {DR(3, MASK_RGB), R(3), R(0, SWZ_WWWW)}); break;
		}
		if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE)) Ins(OP_ADD, {DR(3, MASK_A), R(3), R(0)});

		Ins(OP_MOV, {DR(0), R(3)});
		if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE)) Ins(OP_ADD, {DR(0, MASK_RGB), C(10, SWZ_ZZZZ), NR(3)});
		if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE)) Ins(OP_ADD, {DR(0, MASK_A), C(10, SWZ_ZZZZ), NR(3)});
	}

	/* ogl_sh_color_path(): result in r5 */
	void ColorPath(void) {
		const UINT32 FBZCOLORPATH = key.fbzcp;
		const UINT32 ALPHAMODE = key.alphamode;

		uint32_t s;
		switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH)) {
			case 0: s = R(8); break;
			case 1: s = R(2); break;
			case 2: s = C(1); break;
			default: s = C(10, SWZ_XXXX); break;
		}
		Ins(OP_MOV, {DR(1, MASK_RGB), s});

		if (key.flags & VD3D_PS_CHROMAKEY) {
			/* if (distance(cother.rgb, chromaKey.rgb) < 0.0001) discard; */
			Ins(OP_ADD, {DR(6, MASK_RGB), R(1), NC(2)});
			Ins(OP_DP3, {DR(6, MASK_X), R(6), R(6)});
			Ins(OP_ADD, {DR(6), R(6, SWZ_XXXX), NC(11, SWZ_XXXX)});
			Kill(6);
		}

		switch (FBZCP_CC_ASELECT(FBZCOLORPATH)) {
			case 0: s = R(8); break;
			case 1: s = R(2); break;
			case 2: s = C(1); break;
			default: s = C(10, SWZ_XXXX); break;
		}
		Ins(OP_MOV, {DR(1, MASK_A), s});

		/* alpha test: texkill discards where the value is negative */
		if (ALPHAMODE_ALPHATEST(ALPHAMODE)) {
			switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
				case 0:		/* never */
					Ins(OP_MOV, {DR(6), C(11, SWZ_YYYY)});
					Kill(6);
					break;
				case 1:		/* discard if a >= ref: ref - a - small */
					Ins(OP_ADD, {DR(6), C(4, SWZ_XXXX), NR(1, SWZ_WWWW)});
					Ins(OP_ADD, {DR(6), R(6), NC(11, SWZ_ZZZZ)});
					Kill(6);
					break;
				case 2:		/* discard if |a - ref| > 0.0001 */
					Ins(OP_ADD, {DR(6), R(1, SWZ_WWWW), NC(4, SWZ_XXXX)});
					Ins(OP_MAX, {DR(7), R(6), NR(6)});
					Ins(OP_ADD, {DR(6), C(10, SWZ_WWWW), NR(7)});
					Kill(6);
					break;
				case 3:		/* discard if a > ref */
					Ins(OP_ADD, {DR(6), C(4, SWZ_XXXX), NR(1, SWZ_WWWW)});
					Kill(6);
					break;
				case 4:		/* discard if a <= ref */
					Ins(OP_ADD, {DR(6), R(1, SWZ_WWWW), NC(4, SWZ_XXXX)});
					Ins(OP_ADD, {DR(6), R(6), NC(11, SWZ_ZZZZ)});
					Kill(6);
					break;
				case 5:		/* discard if |a - ref| < 0.0001 */
					Ins(OP_ADD, {DR(6), R(1, SWZ_WWWW), NC(4, SWZ_XXXX)});
					Ins(OP_MAX, {DR(7), R(6), NR(6)});
					Ins(OP_ADD, {DR(6), R(7), NC(10, SWZ_WWWW)});
					Kill(6);
					break;
				case 6:		/* discard if a < ref */
					Ins(OP_ADD, {DR(6), R(1, SWZ_WWWW), NC(4, SWZ_XXXX)});
					Kill(6);
					break;
				case 7:
					break;
			}
		}

		if (FBZCP_CC_LOCALSELECT_OVERRIDE(FBZCOLORPATH) == 0) {
			Ins(OP_MOV, {DR(0), FBZCP_CC_LOCALSELECT(FBZCOLORPATH) == 0 ? R(8) : C(0)});
		} else {
			/* texel.a < 0.5 ? gl_Color : color0 */
			Ins(OP_ADD, {DR(6), R(2, SWZ_WWWW), NC(10, SWZ_YYYY)});
			Ins(OP_CMP, {DR(0), R(6), C(0), R(8)});
		}

		switch (FBZCP_CCA_LOCALSELECT(FBZCOLORPATH)) {
			default:
			case 0: Ins(OP_MOV, {DR(0, MASK_A), R(8)}); break;
			case 1: Ins(OP_MOV, {DR(0, MASK_A), C(0)}); break;
			case 2: Ins(OP_MOV, {DR(0, MASK_A), R(8)}); break;	/* TODO CLAMPED_Z (as OpenGL) */
			case 3: break;					/* voodoo2 only */
		}

		Ins(OP_MOV, {DR(3, MASK_RGB), FBZCP_CC_ZERO_OTHER(FBZCOLORPATH) == 0 ? R(1) : C(10, SWZ_XXXX)});
		Ins(OP_MOV, {DR(3, MASK_A), FBZCP_CCA_ZERO_OTHER(FBZCOLORPATH) == 0 ? R(1) : C(10, SWZ_XXXX)});
		if (FBZCP_CC_SUB_CLOCAL(FBZCOLORPATH)) Ins(OP_ADD, {DR(3, MASK_RGB), R(3), NR(0)});
		if (FBZCP_CCA_SUB_CLOCAL(FBZCOLORPATH)) Ins(OP_ADD, {DR(3, MASK_A), R(3), NR(0)});

		switch (FBZCP_CC_MSELECT(FBZCOLORPATH)) {
			default:
			case 0: s = C(10, SWZ_XXXX); break;
			case 1: s = R(0); break;
			case 2: s = R(1, SWZ_WWWW); break;
			case 3: s = R(0, SWZ_WWWW); break;
			case 4: s = R(2, SWZ_WWWW); break;
			case 5: s = R(2); break;	/* voodoo2 only */
		}
		Ins(OP_MOV, {DR(4, MASK_RGB), s});
		switch (FBZCP_CCA_MSELECT(FBZCOLORPATH)) {
			default:
			case 0: s = C(10, SWZ_XXXX); break;
			case 1: s = R(0); break;
			case 2: s = R(1); break;
			case 3: s = R(0); break;
			case 4: s = R(2); break;
		}
		Ins(OP_MOV, {DR(4, MASK_A), s});

		if (!FBZCP_CC_REVERSE_BLEND(FBZCOLORPATH)) Ins(OP_ADD, {DR(4, MASK_RGB), C(10, SWZ_ZZZZ), NR(4)});
		if (!FBZCP_CCA_REVERSE_BLEND(FBZCOLORPATH)) Ins(OP_ADD, {DR(4, MASK_A), C(10, SWZ_ZZZZ), NR(4)});
		Ins(OP_MUL, {DR(3), R(3), R(4)});

		switch (FBZCP_CC_ADD_ACLOCAL(FBZCOLORPATH)) {
			case 3:
			case 0: break;
			case 1: Ins(OP_ADD, {DR(3, MASK_RGB), R(3), R(0)}); break;
			case 2: Ins(OP_ADD, {DR(3, MASK_RGB), R(3), R(0, SWZ_WWWW)}); break;
		}
		if (FBZCP_CCA_ADD_ACLOCAL(FBZCOLORPATH)) Ins(OP_ADD, {DR(3, MASK_A), R(3), R(0)});

		Ins(OP_MOV, {DR(5), R(3)});
		if (FBZCP_CC_INVERT_OUTPUT(FBZCOLORPATH)) Ins(OP_ADD, {DR(5, MASK_RGB), C(10, SWZ_ZZZZ), NR(3)});
		if (FBZCP_CCA_INVERT_OUTPUT(FBZCOLORPATH)) Ins(OP_ADD, {DR(5, MASK_A), C(10, SWZ_ZZZZ), NR(3)});
	}

	/* ogl_sh_fog() */
	void Fog(void) {
		const UINT32 FOGMODE = key.fogmode;
		/* constant fog bypasses everything else (as OpenGL: ff = fogColor, and nothing more) */
		if (FOGMODE_FOG_CONSTANT(FOGMODE)) return;

		/* if fog_add is zero, we start with the fog color */
		Ins(OP_MOV, {DR(6, MASK_RGB), FOGMODE_FOG_ADD(FOGMODE) == 0 ? C(3) : C(10, SWZ_XXXX)});
		/* if fog_mult is zero, we subtract the incoming color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0) Ins(OP_ADD, {DR(6, MASK_RGB), R(6), NR(5)});
		/* fog blending mode: the table, iterated Z and W are worked out per vertex */
		Ins(OP_MUL, {DR(6, MASK_RGB), R(6), FOGMODE_FOG_ZALPHA(FOGMODE) == 1 ? R(8, SWZ_WWWW) : R(9, SWZ_XXXX)});
		/* if fog_mult is 0, we add this to the original color, otherwise it is the new color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0) Ins(OP_ADD, {DR(5, MASK_RGB), R(5), R(6)});
		else Ins(OP_MOV, {DR(5, MASK_RGB), R(6)});
	}

	VD3DPsKey key;
};

/*************************************
 *  State
 *************************************/

/* vertex formats: triangles (position, color, two texture coordinates, fog and LOD blends), and
 * LFB pixels (position and color) */
#define VD3D_FVF_TRIANGLE	(D3DFVF_XYZRHW | D3DFVF_TEX4 | D3DFVF_TEXCOORDSIZE4(0) | D3DFVF_TEXCOORDSIZE4(1) | D3DFVF_TEXCOORDSIZE4(2) | D3DFVF_TEXCOORDSIZE4(3))
#define VD3D_FVF_FLAT		(D3DFVF_XYZRHW | D3DFVF_DIFFUSE)
#define VD3D_TRIANGLE_FLOATS	20
#define VD3D_FLAT_DWORDS	5

#define VD3D_STENCIL_WRITE	1
#define VD3D_STENCIL_TEST	2

/* all the state of a draw: draws with the same state are drawn together */
struct VD3DDrawState {
	int target;
	int kind;			/* 0: triangles with a pixel shader, 1: flat triangles, 2: flat points */
	IDirect3DPixelShader9 *shader;
	IDirect3DTexture9 *texture[2];
	UINT32 sampler[2];		/* VD3D_SAMPLER_... */
	BOOL blend;
	DWORD src, dst, src_alpha, dst_alpha;
	BOOL depth_test;
	DWORD depth_func;
	BOOL depth_write;
	DWORD color_mask;
	int stencil;
	VD3DConstants constants;
};
#define VD3D_SAMPLER_MIN_LINEAR		0x01
#define VD3D_SAMPLER_MAG_LINEAR		0x02
#define VD3D_SAMPLER_MIPMAPS		0x04
#define VD3D_SAMPLER_TRILINEAR		0x08
#define VD3D_SAMPLER_CLAMP_S		0x10
#define VD3D_SAMPLER_CLAMP_T		0x20

/* a texture, like the OpenGL emulation's ogl_texmap */
struct VD3DTexMap {
	bool valid_pal;
	UINT32 format, width, height, ilod, lodmask, ncc_table;
	IDirect3DTexture9 *current;
	map<UINT32, IDirect3DTexture9*> by_palette;	/* palette formats: one per palette */
};

struct VD3DDirtyRange {
	bool pending = false;
	UINT32 first = 0, last = 0;
};

static struct {
	bool ready;		/* the buffers exist */
	bool warned;
	bool saved;		/* the color buffers were saved for a device reset */
	bool lost;		/* the output destroyed its device (start over with its next one) */
	IDirect3DDevice9 *dev;
	UINT32 max_size;
	bool separate_alpha;
	bool intz;		/* the depth buffer is an INTZ texture (it can be read back) */

	/* the 3dfx card's buffers */
	IDirect3DTexture9 *color[2];
	IDirect3DSurface9 *color_surface[2];
	IDirect3DTexture9 *depth_texture;	/* INTZ */
	IDirect3DSurface9 *depth;
	IDirect3DTexture9 *present;
	IDirect3DSurface9 *present_surface;
	IDirect3DSurface9 *saved_color[2];	/* system memory, while the device is reset */
	UINT32 width, height;
	int front;

	map<VD3DPsKey, IDirect3DPixelShader9*> shaders;
	IDirect3DPixelShader9 *depth_copy_shader;

	/* draws not drawn yet */
	VD3DDrawState pending;
	bool have_pending;
	vector<uint32_t> vertices;
	UINT32 vertex_count;

	/* LFB reads: color buffers 0 and 1, and depth */
	IDirect3DSurface9 *readback;		/* system memory */
	IDirect3DTexture9 *depth_copy;		/* R32F render target */
	IDirect3DSurface9 *depth_copy_surface;
	IDirect3DSurface9 *depth_readback;	/* system memory, R32F */
	bool cache_valid[3];
	vector<UINT32> cache[3];		/* colors as 0xAARRGGBB, depth as 16 bits */
} vd3d;

static map<const UINT32, VD3DTexMap> vd3d_textures[2];
static VD3DDirtyRange vd3d_texture_dirty[2];

#define VD3D_FMT_INTZ ((D3DFORMAT)MAKEFOURCC('I','N','T','Z'))

static bool vd3d_check(HRESULT hr, const char *what) {
	if (FAILED(hr)) {
		LOG_MSG("VOODOO: Direct3D: %s failed (0x%08lx)", what, (unsigned long)hr);
		return false;
	}
	return true;
}

/* uses the device from here to the end of the block: the output's worker thread does not */
class VD3DScope {
public:
	VD3DScope() : ok(false) {
		if (d3d == NULL || d3d->GetDevice() == NULL) return;
		d3d->BeginExternal();
		locked = true;
		ok = vd3d.ready && vd3d.dev == d3d->GetDevice() && !d3d->IsDeviceLost();
		/* (not ready yet is not a reason to warn: the buffers are being made) */
		if (!ok && vd3d.ready && vd3d.dev != d3d->GetDevice() && !vd3d.warned) {
			LOG_MSG("VOODOO: Direct3D: not drawing, the 3dfx emulation needs the Direct3D output");
			vd3d.warned = true;
		}
	}
	~VD3DScope() { if (locked) d3d->EndExternal(); }
	bool ok;
private:
	bool locked = false;
};

/*************************************
 *  Drawing
 *************************************/

static D3DBLEND vd3d_src_factor(UINT32 f) {
	static const D3DBLEND t[16] = {
		D3DBLEND_ZERO, D3DBLEND_SRCALPHA, D3DBLEND_DESTCOLOR, D3DBLEND_DESTALPHA, D3DBLEND_ONE,
		D3DBLEND_INVSRCALPHA, D3DBLEND_INVDESTCOLOR, D3DBLEND_INVDESTALPHA, D3DBLEND_ZERO, D3DBLEND_ZERO,
		D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_SRCALPHASAT };
	return t[f & 15];
}

static D3DBLEND vd3d_dst_factor(UINT32 f) {
	static const D3DBLEND t[16] = {
		D3DBLEND_ZERO, D3DBLEND_SRCALPHA, D3DBLEND_SRCCOLOR, D3DBLEND_DESTALPHA, D3DBLEND_ONE,
		D3DBLEND_INVSRCALPHA, D3DBLEND_INVSRCCOLOR, D3DBLEND_INVDESTALPHA, D3DBLEND_ZERO, D3DBLEND_ZERO,
		D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO, D3DBLEND_ZERO,
		D3DBLEND_SRCCOLOR /* A_COLORBEFOREFOG */ };
	return t[f & 15];
}

static VD3DDrawState vd3d_default_state(int target, int kind) {
	VD3DDrawState st;
	memset(&st, 0, sizeof(st));
	st.target = target;
	st.kind = kind;
	st.src = st.src_alpha = D3DBLEND_ONE;
	st.dst = st.dst_alpha = D3DBLEND_ZERO;
	st.depth_func = D3DCMP_ALWAYS;
	st.color_mask = D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE | D3DCOLORWRITEENABLE_ALPHA;
	return st;
}

/* blending from alphaMode, as VOGL_SetAlphaMode() is called with */
static void vd3d_blend_state(VD3DDrawState &st, UINT32 ALPHAMODE) {
	st.blend = ALPHAMODE_ALPHABLEND(ALPHAMODE) ? TRUE : FALSE;
	if (!st.blend) return;
	st.src = vd3d_src_factor(ALPHAMODE_SRCRGBBLEND(ALPHAMODE));
	st.dst = vd3d_dst_factor(ALPHAMODE_DSTRGBBLEND(ALPHAMODE));
	st.src_alpha = ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4 ? D3DBLEND_ONE : D3DBLEND_ZERO;
	st.dst_alpha = ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4 ? D3DBLEND_ONE : D3DBLEND_ZERO;
}

static DWORD vd3d_color_mask(bool rgb, bool alpha) {
	return (rgb ? (D3DCOLORWRITEENABLE_RED | D3DCOLORWRITEENABLE_GREEN | D3DCOLORWRITEENABLE_BLUE) : 0) |
		(alpha ? D3DCOLORWRITEENABLE_ALPHA : 0);
}

static void vd3d_constants(VD3DConstants &c) {
	memset(&c, 0, sizeof(c));
	const voodoo_reg *r = v->reg;
	c.c0[0] = r[color0].rgb.r / 255.0f; c.c0[1] = r[color0].rgb.g / 255.0f;
	c.c0[2] = r[color0].rgb.b / 255.0f; c.c0[3] = r[color0].rgb.a / 255.0f;
	c.c1[0] = r[color1].rgb.r / 255.0f; c.c1[1] = r[color1].rgb.g / 255.0f;
	c.c1[2] = r[color1].rgb.b / 255.0f; c.c1[3] = r[color1].rgb.a / 255.0f;
	c.chromakey[0] = r[chromaKey].rgb.r / 255.0f; c.chromakey[1] = r[chromaKey].rgb.g / 255.0f;
	c.chromakey[2] = r[chromaKey].rgb.b / 255.0f;
	c.fogcolor[0] = r[fogColor].rgb.r / 255.0f; c.fogcolor[1] = r[fogColor].rgb.g / 255.0f;
	c.fogcolor[2] = r[fogColor].rgb.b / 255.0f; c.fogcolor[3] = 1.0f;
	c.alpharef[0] = r[alphaMode].rgb.a / 255.0f;
	c.alpharef[1] = (float)((UINT16)r[zaColor].u) / 65535.0f;
}

/* set up the device for a draw (all of it: the output changes it too) */
static void vd3d_apply_state(const VD3DDrawState &st) {
	IDirect3DDevice9 *dev = vd3d.dev;
	dev->SetRenderTarget(0, vd3d.color_surface[st.target]);
	dev->SetDepthStencilSurface(vd3d.depth);
	dev->SetVertexShader(NULL);
	if (st.kind == 0) {
		dev->SetFVF(VD3D_FVF_TRIANGLE);
		dev->SetPixelShader(st.shader);
		dev->SetPixelShaderConstantF(0, &st.constants.c0[0], 5);
		for (DWORD i = 0; i < 2; i++) {
			const UINT32 s = st.sampler[i];
			dev->SetTexture(i, st.texture[i]);
			dev->SetSamplerState(i, D3DSAMP_MINFILTER, (s & VD3D_SAMPLER_MIN_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
			dev->SetSamplerState(i, D3DSAMP_MAGFILTER, (s & VD3D_SAMPLER_MAG_LINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
			dev->SetSamplerState(i, D3DSAMP_MIPFILTER, !(s & VD3D_SAMPLER_MIPMAPS) ? D3DTEXF_NONE :
				(s & VD3D_SAMPLER_TRILINEAR) ? D3DTEXF_LINEAR : D3DTEXF_POINT);
			dev->SetSamplerState(i, D3DSAMP_ADDRESSU, (s & VD3D_SAMPLER_CLAMP_S) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
			dev->SetSamplerState(i, D3DSAMP_ADDRESSV, (s & VD3D_SAMPLER_CLAMP_T) ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
			dev->SetSamplerState(i, D3DSAMP_MAXMIPLEVEL, 0);
			dev->SetSamplerState(i, D3DSAMP_MIPMAPLODBIAS, 0);
		}
	} else {
		/* the color of the vertices, fixed function */
		dev->SetFVF(VD3D_FVF_FLAT);
		dev->SetPixelShader(NULL);
		dev->SetTexture(0, NULL);
		dev->SetTexture(1, NULL);
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
		dev->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
		dev->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
		dev->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
		float one = 1.0f;
		DWORD one_bits;
		memcpy(&one_bits, &one, 4);
		dev->SetRenderState(D3DRS_POINTSIZE, one_bits);
		dev->SetRenderState(D3DRS_POINTSCALEENABLE, FALSE);
		dev->SetRenderState(D3DRS_POINTSPRITEENABLE, FALSE);
	}
	dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
	dev->SetRenderState(D3DRS_LIGHTING, FALSE);
	dev->SetRenderState(D3DRS_FOGENABLE, FALSE);
	dev->SetRenderState(D3DRS_ALPHATESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	dev->SetRenderState(D3DRS_DITHERENABLE, FALSE);
	dev->SetRenderState(D3DRS_SHADEMODE, D3DSHADE_GOURAUD);
	dev->SetRenderState(D3DRS_ZENABLE, (st.depth_test || st.depth_write) ? D3DZB_TRUE : D3DZB_FALSE);
	dev->SetRenderState(D3DRS_ZFUNC, st.depth_test ? st.depth_func : (DWORD)D3DCMP_ALWAYS);
	dev->SetRenderState(D3DRS_ZWRITEENABLE, st.depth_write);
	dev->SetRenderState(D3DRS_ALPHABLENDENABLE, st.blend);
	dev->SetRenderState(D3DRS_BLENDOP, D3DBLENDOP_ADD);
	dev->SetRenderState(D3DRS_SRCBLEND, st.src);
	dev->SetRenderState(D3DRS_DESTBLEND, st.dst);
	dev->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, (st.blend && vd3d.separate_alpha) ? TRUE : FALSE);
	if (vd3d.separate_alpha) {
		dev->SetRenderState(D3DRS_SRCBLENDALPHA, st.src_alpha);
		dev->SetRenderState(D3DRS_DESTBLENDALPHA, st.dst_alpha);
		dev->SetRenderState(D3DRS_BLENDOPALPHA, D3DBLENDOP_ADD);
	}
	dev->SetRenderState(D3DRS_COLORWRITEENABLE, st.color_mask);
	dev->SetRenderState(D3DRS_STENCILENABLE, st.stencil != 0);
	if (st.stencil != 0) {
		dev->SetRenderState(D3DRS_STENCILREF, 1);
		dev->SetRenderState(D3DRS_STENCILMASK, 1);
		dev->SetRenderState(D3DRS_STENCILWRITEMASK, 1);
		dev->SetRenderState(D3DRS_STENCILFAIL, D3DSTENCILOP_KEEP);
		dev->SetRenderState(D3DRS_STENCILZFAIL, D3DSTENCILOP_KEEP);
		dev->SetRenderState(D3DRS_STENCILPASS, st.stencil == VD3D_STENCIL_WRITE ? D3DSTENCILOP_REPLACE : D3DSTENCILOP_KEEP);
		dev->SetRenderState(D3DRS_STENCILFUNC, st.stencil == VD3D_STENCIL_WRITE ? D3DCMP_ALWAYS : D3DCMP_EQUAL);
	}
}

/* draw what was queued */
static void vd3d_flush(void) {
	if (!vd3d.have_pending) return;
	vd3d.have_pending = false;
	if (vd3d.vertex_count == 0) return;
	const VD3DDrawState &st = vd3d.pending;
	vd3d_apply_state(st);
	IDirect3DDevice9 *dev = vd3d.dev;
	dev->BeginScene();
	if (st.kind == 0)
		dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vd3d.vertex_count / 3, vd3d.vertices.data(), VD3D_TRIANGLE_FLOATS * 4);
	else if (st.kind == 1)
		dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, vd3d.vertex_count / 3, vd3d.vertices.data(), VD3D_FLAT_DWORDS * 4);
	else
		dev->DrawPrimitiveUP(D3DPT_POINTLIST, vd3d.vertex_count, vd3d.vertices.data(), VD3D_FLAT_DWORDS * 4);
	dev->EndScene();
	vd3d.vertices.clear();
	vd3d.vertex_count = 0;
}

/* queue vertices, drawn with the ones before them if the state is the same */
static void vd3d_draw(const VD3DDrawState &st, const void *data, UINT32 count) {
	if (vd3d.have_pending && memcmp(&vd3d.pending, &st, sizeof(st)) != 0) vd3d_flush();
	if (!vd3d.have_pending) {
		vd3d.pending = st;
		vd3d.have_pending = true;
	}
	const size_t dwords = (size_t)count * (st.kind == 0 ? VD3D_TRIANGLE_FLOATS : VD3D_FLAT_DWORDS);
	const uint32_t *p = (const uint32_t *)data;
	vd3d.vertices.insert(vd3d.vertices.end(), p, p + dwords);
	vd3d.vertex_count += count;
	vd3d.cache_valid[st.target] = false;
	vd3d.cache_valid[2] = false;
	/* (DrawPrimitiveUP draws at most D3DMAXUSERPRIMITIVES at a time, well above this) */
	if (vd3d.vertex_count >= 30000) vd3d_flush();
}

static void vd3d_invalidate_reads(void) {
	vd3d.cache_valid[0] = vd3d.cache_valid[1] = vd3d.cache_valid[2] = false;
}

static int vd3d_target(bool front) {
	return front ? vd3d.front : (vd3d.front ^ 1);
}

/*************************************
 *  Buffers
 *************************************/

static void vd3d_release_targets(void) {
	for (int i = 0; i < 2; i++) {
		SAFE_RELEASE(vd3d.color_surface[i]);
		SAFE_RELEASE(vd3d.color[i]);
	}
	SAFE_RELEASE(vd3d.depth);
	SAFE_RELEASE(vd3d.depth_texture);
	SAFE_RELEASE(vd3d.present_surface);
	SAFE_RELEASE(vd3d.present);
	SAFE_RELEASE(vd3d.readback);
	SAFE_RELEASE(vd3d.depth_copy_surface);
	SAFE_RELEASE(vd3d.depth_copy);
	SAFE_RELEASE(vd3d.depth_readback);
	vd3d.width = vd3d.height = 0;
	vd3d_invalidate_reads();
}

static bool vd3d_render_target(UINT32 w, UINT32 h, D3DFORMAT format, IDirect3DTexture9 *&tex, IDirect3DSurface9 *&surface) {
	if (!vd3d_check(vd3d.dev->CreateTexture(w, h, 1, D3DUSAGE_RENDERTARGET, format, D3DPOOL_DEFAULT, &tex, NULL), "CreateTexture"))
		return false;
	return vd3d_check(tex->GetSurfaceLevel(0, &surface), "GetSurfaceLevel");
}

/* make the buffers, black (depth 1.0) */
static bool vd3d_create_targets(UINT32 w, UINT32 h) {
	IDirect3DDevice9 *dev = vd3d.dev;
	for (int i = 0; i < 2; i++)
		if (!vd3d_render_target(w, h, D3DFMT_A8R8G8B8, vd3d.color[i], vd3d.color_surface[i])) return false;
	if (!vd3d_render_target(w, h, D3DFMT_A8R8G8B8, vd3d.present, vd3d.present_surface)) return false;
	vd3d.intz = false;
	if (d3d->GetD3D()->CheckDeviceFormat(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DUSAGE_DEPTHSTENCIL,
	                                     D3DRTYPE_TEXTURE, VD3D_FMT_INTZ) == D3D_OK &&
	    SUCCEEDED(dev->CreateTexture(w, h, 1, D3DUSAGE_DEPTHSTENCIL, VD3D_FMT_INTZ, D3DPOOL_DEFAULT, &vd3d.depth_texture, NULL)) &&
	    SUCCEEDED(vd3d.depth_texture->GetSurfaceLevel(0, &vd3d.depth))) {
		vd3d.intz = true;
	} else {
		SAFE_RELEASE(vd3d.depth_texture);
		if (!vd3d_check(dev->CreateDepthStencilSurface(w, h, D3DFMT_D24S8, D3DMULTISAMPLE_NONE, 0, FALSE, &vd3d.depth, NULL),
		                "CreateDepthStencilSurface")) return false;
	}
	vd3d.width = w;
	vd3d.height = h;

	for (int i = 0; i < 2; i++) {
		dev->SetRenderTarget(0, vd3d.color_surface[i]);
		dev->SetDepthStencilSurface(vd3d.depth);
		dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
	}
	dev->SetRenderTarget(0, vd3d.present_surface);
	dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_ARGB(255, 0, 0, 0), 1.0f, 0);
	vd3d_invalidate_reads();
	return true;
}

/* change the render resolution, keeping what the color buffers show (scaled) */
static bool vd3d_resize_targets(UINT32 w, UINT32 h) {
	vd3d_flush();
	IDirect3DTexture9 *old_color[2] = { vd3d.color[0], vd3d.color[1] };
	IDirect3DSurface9 *old_surface[2] = { vd3d.color_surface[0], vd3d.color_surface[1] };
	vd3d.color[0] = vd3d.color[1] = NULL;
	vd3d.color_surface[0] = vd3d.color_surface[1] = NULL;
	d3d->SetExternalTexture(NULL, 0, 0);
	SAFE_RELEASE(vd3d.depth);
	SAFE_RELEASE(vd3d.depth_texture);
	SAFE_RELEASE(vd3d.present_surface);
	SAFE_RELEASE(vd3d.present);
	SAFE_RELEASE(vd3d.readback);
	SAFE_RELEASE(vd3d.depth_copy_surface);
	SAFE_RELEASE(vd3d.depth_copy);
	SAFE_RELEASE(vd3d.depth_readback);

	const bool ok = vd3d_create_targets(w, h);
	if (ok) {
		/* without filtering, the pixels keep their colors (for LFB reads). The depth buffer is
		 * cleared, Direct3D 9 can't scale it. */
		for (int i = 0; i < 2; i++)
			vd3d.dev->StretchRect(old_surface[i], NULL, vd3d.color_surface[i], NULL, D3DTEXF_POINT);
		d3d->SetExternalTexture(vd3d.present, w, h);
		LOG_MSG("VOODOO: Direct3D: render resolution %u:%u", (unsigned int)w, (unsigned int)h);
	} else {
		vd3d_release_targets();
		vd3d.ready = false;
	}
	for (int i = 0; i < 2; i++) {
		SAFE_RELEASE(old_surface[i]);
		SAFE_RELEASE(old_color[i]);
	}
	return ok;
}

/* follow the resolution setting and the display area, between frames */
static void vd3d_check_render_size(void) {
	Voodoo_HW_PollResolutionSetting();
	UINT32 w, h;
	Voodoo_HW_RenderSize(vd3d.max_size, w, h);
	if (vd3d.width != 0 && (w != vd3d.width || h != vd3d.height))
		vd3d_resize_targets(w, h);
}

/* the render resolution pixels [p0, p1) that 3dfx video mode pixel i covers, as the OpenGL
 * emulation's voodoo_ogl_lfb_span() */
static void vd3d_lfb_span(int i, UINT32 render_size, UINT32 mode_size, int &p0, int &p1) {
	const double d = (double)render_size / (double)mode_size;
	p0 = (int)ceil(i * d - 0.5);
	p1 = (int)ceil((i + 1) * d - 0.5);
	if (p1 <= p0) p1 = p0 + 1;
	if (p0 >= (int)render_size) p0 = (int)render_size - 1;
	if (p1 > (int)render_size) p1 = (int)render_size;
}

/* the output resets the device (for a window resize, or a lost device): it can't have objects in
 * D3DPOOL_DEFAULT then. The color buffers are kept in system memory meanwhile. */
void voodoo_d3d_device_reset(bool before) {
	if (before) {
		if (!vd3d.ready || vd3d.saved) return;
		vd3d_flush();
		for (int i = 0; i < 2; i++) {
			vd3d.saved_color[i] = NULL;
			if (vd3d.color_surface[i] != NULL &&
			    SUCCEEDED(vd3d.dev->CreateOffscreenPlainSurface(vd3d.width, vd3d.height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &vd3d.saved_color[i], NULL)) &&
			    FAILED(vd3d.dev->GetRenderTargetData(vd3d.color_surface[i], vd3d.saved_color[i])))
				SAFE_RELEASE(vd3d.saved_color[i]);
		}
		const UINT32 w = vd3d.width, h = vd3d.height;
		d3d->SetExternalTexture(NULL, 0, 0);
		vd3d_release_targets();
		vd3d.width = w;
		vd3d.height = h;
		vd3d.saved = true;
	} else {
		if (!vd3d.saved) return;
		vd3d.saved = false;
		const UINT32 w = vd3d.width, h = vd3d.height;
		if (d3d->IsDeviceLost() || !vd3d_create_targets(w, h)) {
			for (int i = 0; i < 2; i++) SAFE_RELEASE(vd3d.saved_color[i]);
			vd3d_release_targets();
			vd3d.ready = false;
			return;
		}
		for (int i = 0; i < 2; i++) {
			if (vd3d.saved_color[i] != NULL) vd3d.dev->UpdateSurface(vd3d.saved_color[i], NULL, vd3d.color_surface[i], NULL);
			SAFE_RELEASE(vd3d.saved_color[i]);
		}
		d3d->SetExternalTexture(vd3d.present, w, h);
	}
}

/*************************************
 *  Textures
 *************************************/

static void vd3d_destroy_texmap(VD3DTexMap &m) {
	if (!m.by_palette.empty()) {
		for (map<UINT32, IDirect3DTexture9*>::iterator i = m.by_palette.begin(); i != m.by_palette.end(); ++i)
			SAFE_RELEASE(i->second);
		m.by_palette.clear();
		m.current = NULL;
	} else {
		SAFE_RELEASE(m.current);
	}
}

static void vd3d_destroy_textures(void) {
	vd3d_flush();
	for (int j = 0; j < 2; j++) {
		for (map<const UINT32, VD3DTexMap>::iterator t = vd3d_textures[j].begin(); t != vd3d_textures[j].end(); ++t)
			vd3d_destroy_texmap(t->second);
		vd3d_textures[j].clear();
		vd3d_texture_dirty[j].pending = false;
	}
}

static bool vd3d_palette_format(UINT32 format) {
	return format == 1 || format == 9 || format == 5 || format == 14;
}

/* the palette (or NCC table) a texture is converted with */
static UINT32 vd3d_palette_sum(UINT32 tmunum, UINT32 texmode) {
	const UINT32 format = TEXMODE_FORMAT(texmode);
	const rgb_t *colors = (format & 7) == 1 ? v->tmu[tmunum].ncc[TEXMODE_NCC_TABLE_SELECT(texmode)].texel : v->tmu[tmunum].palette;
	UINT32 h = 2166136261u;
	for (int i = 0; i < 256; i++) h = (h ^ colors[i]) * 16777619u;
	return h;
}

/* drop textures that texture memory writes changed (before the next draw) */
static void vd3d_flush_texture_writes(int TMU) {
	VD3DDirtyRange &dirty = vd3d_texture_dirty[TMU];
	if (!dirty.pending) return;
	const UINT32 mask = v->tmu[TMU].mask;
	for (map<const UINT32, VD3DTexMap>::iterator t = vd3d_textures[TMU].begin(); t != vd3d_textures[TMU].end();) {
		const UINT32 size = t->second.width * t->second.height * (t->second.format < 8 ? 1 : 2);
		const UINT32 last = t->first + size - 1;
		const bool overlaps = last <= mask
			? t->first <= dirty.last && last >= dirty.first
			: t->first <= dirty.last || (last & mask) >= dirty.first;
		if (overlaps) {
			vd3d_flush();	/* (queued draws may use it) */
			vd3d_destroy_texmap(t->second);
			vd3d_textures[TMU].erase(t++);
		} else {
			++t;
		}
	}
	dirty.pending = false;
}

/* a texture from texture memory, with mipmaps (the OpenGL emulation's conversion) */
static IDirect3DTexture9 *vd3d_upload_texture(int j, UINT32 TEXMODE, UINT32 texbase, UINT32 width, UINT32 height) {
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	UINT32 levels = 1;
	while (((width | height) >> levels) != 0) levels++;

	IDirect3DTexture9 *tex = NULL;
	if (!vd3d_check(vd3d.dev->CreateTexture(width, height, levels, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &tex, NULL), "CreateTexture"))
		return NULL;

	vector<UINT32> level((size_t)width * height);
	UINT32 *texrgbp = level.data();
	UINT32 texboffset = texbase;
	const int count = (int)(width * height);
	if (format < 8) {
		for (int i = 0; i < count; i++) {
			*texrgbp++ = v->tmu[j].lookup[v->tmu[j].ram[texboffset & v->tmu[j].mask]];
			texboffset++;
		}
	} else if (format >= 10 && format <= 12) {
		for (int i = 0; i < count; i++) {
			*texrgbp++ = v->tmu[j].lookup[*(UINT16*)&v->tmu[j].ram[texboffset & v->tmu[j].mask]];
			texboffset += 2;
		}
	} else {
		for (int i = 0; i < count; i++) {
			const UINT16 texel = *(UINT16*)&v->tmu[j].ram[texboffset & v->tmu[j].mask];
			*texrgbp++ = (v->tmu[j].lookup[texel & 0xff] & 0xFFFFFF) | ((texel & 0xff00u) << 16u);
			texboffset += 2;
		}
	}

	/* each level is the one before at half the size (2x2 averages) */
	UINT32 lw = width, lh = height;
	for (UINT32 l = 0; l < levels; l++) {
		D3DLOCKED_RECT lr;
		if (SUCCEEDED(tex->LockRect(l, &lr, NULL, 0))) {
			for (UINT32 y = 0; y < lh; y++)
				memcpy((uint8_t*)lr.pBits + y * lr.Pitch, &level[(size_t)y * lw], lw * 4);
			tex->UnlockRect(l);
		}
		if (l + 1 == levels) break;
		const UINT32 nw = lw > 1 ? lw / 2 : 1, nh = lh > 1 ? lh / 2 : 1;
		vector<UINT32> next((size_t)nw * nh);
		for (UINT32 y = 0; y < nh; y++) {
			for (UINT32 x = 0; x < nw; x++) {
				const UINT32 x0 = x * 2 < lw ? x * 2 : lw - 1, x1 = x * 2 + 1 < lw ? x * 2 + 1 : lw - 1;
				const UINT32 y0 = y * 2 < lh ? y * 2 : lh - 1, y1 = y * 2 + 1 < lh ? y * 2 + 1 : lh - 1;
				const UINT32 p[4] = { level[y0 * lw + x0], level[y0 * lw + x1], level[y1 * lw + x0], level[y1 * lw + x1] };
				UINT32 out = 0;
				for (int c = 0; c < 32; c += 8) {
					UINT32 sum = 2;
					for (int k = 0; k < 4; k++) sum += (p[k] >> c) & 0xff;
					out |= (sum / 4) << c;
				}
				next[(size_t)y * nw + x] = out;
			}
		}
		level.swap(next);
		lw = nw;
		lh = nh;
	}
	return tex;
}

/* the texture a TMU uses, like ogl_cache_texture() */
static IDirect3DTexture9 *vd3d_cache_texture(const poly_extra_data *extra, UINT32 j) {
	vd3d_flush_texture_writes((int)j);
	const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	const UINT32 ncc_table = (format & 7) == 1 ? TEXMODE_NCC_TABLE_SELECT(TEXMODE) : 0;

	UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
	if (!((v->tmu[j].lodmask >> ilod) & 1)) ilod++;
	const UINT32 texbase = v->tmu[j].lodoffset[ilod];
	const UINT32 width = (v->tmu[j].wmask >> ilod) + 1;
	const UINT32 height = (v->tmu[j].hmask >> ilod) + 1;

	map<const UINT32, VD3DTexMap>::iterator t = vd3d_textures[j].find(texbase);
	/* a RAM address can be reused with a different texture layout */
	if (t != vd3d_textures[j].end() &&
	    (t->second.format != format || t->second.width != width || t->second.height != height ||
	     t->second.ilod != ilod || t->second.lodmask != v->tmu[j].lodmask || t->second.ncc_table != ncc_table)) {
		vd3d_flush();
		vd3d_destroy_texmap(t->second);
		vd3d_textures[j].erase(t);
		t = vd3d_textures[j].end();
	}

	UINT32 palsum = 0;
	if (t != vd3d_textures[j].end()) {
		if (t->second.valid_pal) return t->second.current;
		palsum = vd3d_palette_sum(j, TEXMODE);
		map<UINT32, IDirect3DTexture9*>::iterator u = t->second.by_palette.find(palsum);
		t->second.valid_pal = true;
		if (u != t->second.by_palette.end()) {
			t->second.current = u->second;
			return u->second;
		}
		if (t->second.by_palette.size() > 8) {
			vd3d_flush();
			for (u = t->second.by_palette.begin(); u != t->second.by_palette.end(); ++u)
				SAFE_RELEASE(u->second);
			t->second.by_palette.clear();
		}
	} else if (vd3d_palette_format(format)) {
		palsum = vd3d_palette_sum(j, TEXMODE);
	}

	IDirect3DTexture9 *tex = vd3d_upload_texture((int)j, TEXMODE, texbase, width, height);
	if (tex == NULL) return NULL;
	if (t == vd3d_textures[j].end()) {
		VD3DTexMap m;
		m.valid_pal = true;
		m.format = format;
		m.width = width;
		m.height = height;
		m.ilod = ilod;
		m.lodmask = v->tmu[j].lodmask;
		m.ncc_table = ncc_table;
		m.current = tex;
		if (vd3d_palette_format(format)) m.by_palette[palsum] = tex;
		vd3d_textures[j][texbase] = m;
	} else {
		t->second.current = tex;
		t->second.by_palette[palsum] = tex;
	}
	return tex;
}

/*************************************
 *  Setup
 *************************************/

static IDirect3DPixelShader9 *vd3d_create_shader(const vector<uint32_t> &code) {
	IDirect3DPixelShader9 *ps = NULL;
	if (!vd3d_check(vd3d.dev->CreatePixelShader((const DWORD*)code.data(), &ps), "CreatePixelShader")) return NULL;
	return ps;
}

static void vd3d_destroy_all(void) {
	if (vd3d.dev == NULL) return;
	vd3d_flush();
	if (d3d != NULL) d3d->SetExternalTexture(NULL, 0, 0);
	vd3d_destroy_textures();
	vd3d_release_targets();
	for (int i = 0; i < 2; i++) SAFE_RELEASE(vd3d.saved_color[i]);
	for (map<VD3DPsKey, IDirect3DPixelShader9*>::iterator i = vd3d.shaders.begin(); i != vd3d.shaders.end(); ++i)
		SAFE_RELEASE(i->second);
	vd3d.shaders.clear();
	SAFE_RELEASE(vd3d.depth_copy_shader);
	vd3d.vertices.clear();
	vd3d.vertex_count = 0;
	vd3d.have_pending = false;
	vd3d.ready = false;
	vd3d.saved = false;
	vd3d.dev = NULL;
}

/* (re)make the buffers for the 3dfx video mode, and show it in the output */
static bool vd3d_reset_videomode(void) {
	UINT32 w, h;
	{
		VD3DScope scope;
		if (vd3d.dev == NULL) return false;
		vd3d_flush();
		d3d->SetExternalTexture(NULL, 0, 0);
		vd3d_release_targets();
	}

	/* the output shows the 3dfx frame like any video mode (sized for the 3dfx video mode,
	 * whatever the render resolution is). This comes first, for the size of the display area. */
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);

	VD3DScope scope;
	if (vd3d.dev == NULL || d3d == NULL || d3d->GetDevice() != vd3d.dev) return false;
	Voodoo_HW_RenderSize(vd3d.max_size, w, h);
	if (!vd3d_create_targets(w, h)) {
		vd3d_release_targets();
		return false;
	}
	vd3d.front = 0;
	vd3d.ready = true;
	d3d->SetExternalTexture(vd3d.present, w, h);
	LOG_MSG("VOODOO: Direct3D: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, w, h);
	return true;
}

/*************************************
 *  The 3dfx emulation's functions
 *************************************/

bool voodoo_d3d_init(voodoo_state *vs) {
	(void)vs;
	if (vd3d.ready) return true;
	if (!OUTPUT_DIRECT3D_Active()) {
		LOG_MSG("VOODOO: Direct3D: the Direct3D 3dfx emulation needs output=direct3d");
		return false;
	}
	{
		d3d->BeginExternal();
		vd3d.dev = d3d->GetDevice();
		D3DCAPS9 caps;
		bool ok = SUCCEEDED(vd3d.dev->GetDeviceCaps(&caps));
		if (ok && caps.PixelShaderVersion < D3DPS_VERSION(2, 0)) {
			LOG_MSG("VOODOO: Direct3D: pixel shader 2.0 is not supported");
			ok = false;
		}
		if (ok) {
			vd3d.max_size = caps.MaxTextureWidth < caps.MaxTextureHeight ? caps.MaxTextureWidth : caps.MaxTextureHeight;
			vd3d.separate_alpha = (caps.PrimitiveMiscCaps & D3DPMISCCAPS_SEPARATEALPHABLEND) != 0;
			/* reads the INTZ depth buffer to a R32F render target, for LFB reads */
			VD3DShader s;
			s.DclTexcoord(0);
			s.DclSampler(0);
			s.Tex(0, 0, 0);
			s.Out(0);
			vd3d.depth_copy_shader = vd3d_create_shader(s.Finish());
		}
		vd3d.warned = false;
		d3d->EndExternal();
		if (!ok) {
			vd3d.dev = NULL;
			return false;
		}
	}
	Voodoo_HW_PollResolutionSetting();
	if (!vd3d_reset_videomode()) {
		VD3DScope scope;
		vd3d_destroy_all();
		return false;
	}
	LOG_MSG("VOODOO: Direct3D: rendering 3dfx graphics with Direct3D 9");
	return true;
}

void voodoo_d3d_device_lost(void) {
	/* (the output destroys its device: with its lock held already) */
	if (vd3d.dev == NULL) return;
	vd3d_destroy_all();
	vd3d.lost = true;
}

void voodoo_d3d_leave(bool leavemode) {
	if (leavemode) vd3d.lost = false;
	if (vd3d.dev == NULL || d3d == NULL) return;
	d3d->BeginExternal();
	if (leavemode) {
		LOG_MSG("VOODOO: Direct3D: quit");
		vd3d_destroy_all();
	} else {
		vd3d_destroy_textures();
	}
	d3d->EndExternal();
}

void voodoo_d3d_shutdown(voodoo_state *vs) {
	(void)vs;
	voodoo_d3d_leave(true);
}

void voodoo_d3d_set_window(voodoo_state *vs) {
	/* the projection is worked out for each triangle */
	(void)vs;
}

void voodoo_d3d_clip_window(voodoo_state *vs) {
	(void)vs;
}

void voodoo_d3d_vblank_flush(void) {
}

/* show the front buffer: copy it to the texture the output draws, and have the output draw it */
void voodoo_d3d_present(void) {
	if (!v->active) return;
	if (vd3d.dev == NULL) {
		/* the output was changed to another one and back: start over with its new device (what
		 * was drawn is gone) */
		if (vd3d.lost && OUTPUT_DIRECT3D_Active() && d3d != NULL && d3d->GetDevice() != NULL) {
			vd3d.lost = false;
			voodoo_d3d_init(v);
		}
		return;
	}
	{
		VD3DScope scope;
		if (!scope.ok || vd3d.width == 0) return;
		vd3d_check_render_size();
		if (!vd3d.ready) return;
		vd3d_flush();
		vd3d.dev->StretchRect(vd3d.color_surface[vd3d.front], NULL, vd3d.present_surface, NULL, D3DTEXF_POINT);
	}
	d3d->PresentExternal();
}

void voodoo_d3d_swap_buffer(void) {
	VD3DScope scope;
	if (!scope.ok) return;
	/* the back buffer becomes the front buffer (it is shown at the next vertical retrace) */
	vd3d_flush();
	vd3d.front ^= 1;
}

/* the 3dfx video mode changed: the buffers are resized right away (voodoo_d3d_follow_dimensions),
 * this tells the output, which changes the window etc. */
void voodoo_d3d_update_dimensions(void) {
	if (vd3d.dev == NULL) return;
	if (vd3d.width == 0) {
		vd3d_reset_videomode();
		return;
	}
	{
		VD3DScope scope;
		if (!scope.ok) return;
		vd3d_flush();
	}
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
	VD3DScope scope;
	if (!scope.ok) return;
	vd3d_check_render_size();
	d3d->SetExternalTexture(vd3d.present, vd3d.width, vd3d.height);
	LOG_MSG("VOODOO: Direct3D: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, vd3d.width, vd3d.height);
}

/* the 3dfx video mode changed while active: anything drawn from now on is drawn at the new size */
void voodoo_d3d_follow_dimensions(void) {
	VD3DScope scope;
	if (!scope.ok) return;
	vd3d_check_render_size();
}

/* both buffers black, depth 1.0 */
void voodoo_d3d_clear(void) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;
	vd3d_flush();
	for (int i = 0; i < 2; i++) {
		vd3d.dev->SetRenderTarget(0, vd3d.color_surface[i]);
		vd3d.dev->SetDepthStencilSurface(vd3d.depth);
		vd3d.dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL, 0, 1.0f, 0);
	}
	vd3d_invalidate_reads();
}

/* the 3dfx card was drawn to in software until the emulation became active (with the
 * buffers in its memory), so start from what it has there instead of black. Scaled to the
 * render resolution here, with the pixel spans LFB reads and writes use (so they agree). */
void voodoo_d3d_load_framebuffers_from_ram(void) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;
	const UINT32 fw = v->fbi.width, fh = v->fbi.height;
	if (fw == 0 || fh == 0) return;
	vd3d_flush();

	vector<UINT32> col(vd3d.width), row(vd3d.height);
	for (UINT32 i = 0; i < fw; i++) {
		int p0, p1;
		vd3d_lfb_span((int)i, vd3d.width, fw, p0, p1);
		for (int p = p0; p < p1; p++) col[(size_t)p] = i;
	}
	for (UINT32 i = 0; i < fh; i++) {
		int p0, p1;
		vd3d_lfb_span((int)i, vd3d.height, fh, p0, p1);
		for (int p = p0; p < p1; p++) row[(size_t)p] = i;
	}

	IDirect3DSurface9 *tmp = NULL;
	if (!vd3d_check(vd3d.dev->CreateOffscreenPlainSurface(vd3d.width, vd3d.height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &tmp, NULL),
	                "CreateOffscreenPlainSurface")) return;
	for (int b = 0; b < 2; b++) {
		const UINT32 offs = v->fbi.rgboffs[b == 0 ? v->fbi.frontbuf : v->fbi.backbuf];
		if (offs == (UINT32)(~0) || offs > v->fbi.mask) continue;
		const UINT16 *src = (const UINT16 *)(v->fbi.ram + offs);
		const UINT32 srcmax = (v->fbi.mask + 1 - offs) / 2;
		D3DLOCKED_RECT lr;
		if (FAILED(tmp->LockRect(&lr, NULL, 0))) break;
		/* RGB 565 to ARGB, alpha 0, top row first in both */
		for (UINT32 y = 0; y < vd3d.height; y++) {
			UINT32 *d = (UINT32 *)((uint8_t*)lr.pBits + y * lr.Pitch);
			for (UINT32 x = 0; x < vd3d.width; x++) {
				const UINT32 i = row[y] * v->fbi.rowpixels + col[x];
				const UINT32 p = (i < srcmax) ? src[i] : 0;
				const UINT32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, bl = p & 0x1f;
				d[x] = (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((bl << 3) | (bl >> 2));
			}
		}
		tmp->UnlockRect();
		vd3d.dev->UpdateSurface(tmp, NULL, vd3d.color_surface[vd3d_target(b == 0)], NULL);
	}
	tmp->Release();
	vd3d_invalidate_reads();
}

void voodoo_d3d_fastfill(void) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;

	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

	/* the clip rows are screen rows (0 at the top), counted from the bottom when the Y origin
	 * is (yorigin - y), like the software fastfill */
	const int fw = (int)v->fbi.width, fh = (int)v->fbi.height;
	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
		const int s0 = (int)v->fbi.yorigin - ey + 1;
		ey = (int)v->fbi.yorigin - sy + 1;
		sy = s0;
	}
	if (sx < 0) sx = 0;
	if (ex > fw) ex = fw;
	if (sy < 0) sy = 0;
	if (ey > fh) ey = fh;
	if (ex <= sx || ey <= sy) return;

	DWORD flags = 0;
	int target = vd3d.front ^ 1;
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u)) {
		flags |= D3DCLEAR_TARGET;
		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0) {
			target = vd3d.front;
			v->fbi.vblank_flush_pending = true;
		}
	}
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) flags |= D3DCLEAR_ZBUFFER;
	if (flags == 0) return;

	D3DRECT rect;
	rect.x1 = (LONG)((UINT32)sx * vd3d.width / (UINT32)fw);
	rect.y1 = (LONG)((UINT32)sy * vd3d.height / (UINT32)fh);
	rect.x2 = (LONG)((UINT32)ex * vd3d.width / (UINT32)fw);
	rect.y2 = (LONG)((UINT32)ey * vd3d.height / (UINT32)fh);
	if (rect.x2 <= rect.x1 || rect.y2 <= rect.y1) return;

	vd3d_flush();
	vd3d.dev->SetRenderTarget(0, vd3d.color_surface[target]);
	vd3d.dev->SetDepthStencilSurface(vd3d.depth);
	vd3d.dev->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
	const D3DCOLOR color = D3DCOLOR_ARGB(v->reg[color1].rgb.a, v->reg[color1].rgb.r, v->reg[color1].rgb.g, v->reg[color1].rgb.b);
	vd3d.dev->Clear(1, &rect, flags, color, (float)((UINT16)v->reg[zaColor].u) / 65535.0f, 0);
	vd3d_invalidate_reads();
}

void voodoo_d3d_texture_clear(UINT32 address, int TMU) {
	if (vd3d_textures[TMU].empty()) return;
	const UINT32 mask = v->tmu[TMU].mask;
	UINT32 first = address & mask;
	UINT32 last = (first + 3) & mask;
	/* a wrapped dword conservatively dirties the whole circular address space */
	if (last < first) {
		first = 0;
		last = mask;
	}
	VD3DDirtyRange &dirty = vd3d_texture_dirty[TMU];
	if (!dirty.pending) {
		dirty.first = first;
		dirty.last = last;
		dirty.pending = true;
	} else {
		if (first < dirty.first) dirty.first = first;
		if (last > dirty.last) dirty.last = last;
	}
}

void voodoo_d3d_invalidate_paltex(void) {
	for (int j = 0; j < 2; j++)
		for (map<const UINT32, VD3DTexMap>::iterator t = vd3d_textures[j].begin(); t != vd3d_textures[j].end(); ++t)
			if (vd3d_palette_format(t->second.format)) t->second.valid_pal = false;
}

/* an LFB pixel (x, y) in the 3dfx video mode, row 0 at the top: the render resolution pixels it covers,
 * a point if it is just one, otherwise a rectangle */
static void vd3d_draw_lfb(VD3DDrawState st, int x, int y, D3DCOLOR color, float z) {
	int x0, x1, y0, y1;
	vd3d_lfb_span(x, vd3d.width, v->fbi.width, x0, x1);
	vd3d_lfb_span(y, vd3d.height, v->fbi.height, y0, y1);
	uint32_t vtx[6][VD3D_FLAT_DWORDS];
	const float one = 1.0f;
	if (x1 - x0 == 1 && y1 - y0 == 1) {
		/* (Direct3D 9 pixel centers are at whole numbers) */
		const float p[3] = { (float)x0, (float)y0, z };
		memcpy(vtx[0], p, 12);
		memcpy(&vtx[0][3], &one, 4);
		vtx[0][4] = color;
		st.kind = 2;
		vd3d_draw(st, vtx, 1);
		return;
	}
	const float cx[6] = { (float)x0, (float)x1, (float)x0, (float)x1, (float)x1, (float)x0 };
	const float cy[6] = { (float)y0, (float)y0, (float)y1, (float)y0, (float)y1, (float)y1 };
	for (int i = 0; i < 6; i++) {
		const float p[3] = { cx[i] - 0.5f, cy[i] - 0.5f, z };
		memcpy(vtx[i], p, 12);
		memcpy(&vtx[i][3], &one, 4);
		vtx[i][4] = color;
	}
	st.kind = 1;
	vd3d_draw(st, vtx, 6);
}

void voodoo_d3d_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD3DDrawState st = vd3d_default_state(vd3d_target(front), 1);
	st.color_mask = vd3d_color_mask(has_rgb, has_alpha);
	vd3d_draw_lfb(st, x, y, D3DCOLOR_ARGB(a & 0xff, r & 0xff, g & 0xff, b & 0xff), 0.0f);
}

void voodoo_d3d_draw_z(int x, int y, int z) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD3DDrawState st = vd3d_default_state(vd3d_target(front), 1);
	st.color_mask = 0;
	st.depth_test = TRUE;
	st.depth_func = D3DCMP_ALWAYS;
	st.depth_write = TRUE;
	vd3d_draw_lfb(st, x, y, 0, (float)(z & 0xffff) / 65535.0f);
}

void voodoo_d3d_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD3DDrawState st = vd3d_default_state(vd3d_target(front), 1);
	st.color_mask = vd3d_color_mask(true, FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0);
	vd3d_blend_state(st, v->reg[alphaMode].u);
	vd3d_draw_lfb(st, x, y, D3DCOLOR_ARGB(255, r & 0xff, g & 0xff, b & 0xff), 0.0f);
}

/* read a buffer back (0 and 1: color, 2: depth), kept until it is drawn to again */
static bool vd3d_read_buffer(int which) {
	if (vd3d.cache_valid[which]) return true;
	vd3d_flush();
	IDirect3DDevice9 *dev = vd3d.dev;
	vector<UINT32> &c = vd3d.cache[which];
	c.resize((size_t)vd3d.width * vd3d.height);

	if (which < 2) {
		if (vd3d.readback == NULL &&
		    !vd3d_check(dev->CreateOffscreenPlainSurface(vd3d.width, vd3d.height, D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &vd3d.readback, NULL),
		                "CreateOffscreenPlainSurface")) return false;
		if (!vd3d_check(dev->GetRenderTargetData(vd3d.color_surface[which], vd3d.readback), "GetRenderTargetData")) return false;
		D3DLOCKED_RECT lr;
		if (FAILED(vd3d.readback->LockRect(&lr, NULL, D3DLOCK_READONLY))) return false;
		for (UINT32 y = 0; y < vd3d.height; y++)
			memcpy(&c[(size_t)y * vd3d.width], (uint8_t*)lr.pBits + y * lr.Pitch, vd3d.width * 4);
		vd3d.readback->UnlockRect();
	} else {
		/* the INTZ depth buffer, drawn to a R32F render target and read from there */
		if (!vd3d.intz || vd3d.depth_copy_shader == NULL) {
			std::fill(c.begin(), c.end(), 0xffff);
		} else {
			if (vd3d.depth_copy == NULL &&
			    (!vd3d_render_target(vd3d.width, vd3d.height, D3DFMT_R32F, vd3d.depth_copy, vd3d.depth_copy_surface) ||
			     !vd3d_check(dev->CreateOffscreenPlainSurface(vd3d.width, vd3d.height, D3DFMT_R32F, D3DPOOL_SYSTEMMEM, &vd3d.depth_readback, NULL),
			                 "CreateOffscreenPlainSurface"))) {
				SAFE_RELEASE(vd3d.depth_copy_surface);
				SAFE_RELEASE(vd3d.depth_copy);
				vd3d.depth_copy_shader->Release();
				vd3d.depth_copy_shader = NULL;
				std::fill(c.begin(), c.end(), 0xffff);
				vd3d.cache_valid[which] = true;
				return true;
			}
			const float w = (float)vd3d.width, h = (float)vd3d.height;
			struct { float x, y, z, rhw, u, v; } quad[4] = {
				{ -0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 0.0f }, { w - 0.5f, -0.5f, 0.0f, 1.0f, 1.0f, 0.0f },
				{ -0.5f, h - 0.5f, 0.0f, 1.0f, 0.0f, 1.0f }, { w - 0.5f, h - 0.5f, 0.0f, 1.0f, 1.0f, 1.0f } };
			dev->SetRenderTarget(0, vd3d.depth_copy_surface);
			dev->SetDepthStencilSurface(NULL);
			dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_TEX1);
			dev->SetPixelShader(vd3d.depth_copy_shader);
			dev->SetTexture(0, vd3d.depth_texture);
			dev->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			dev->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
			dev->SetSamplerState(0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
			dev->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
			dev->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			dev->SetRenderState(D3DRS_STENCILENABLE, FALSE);
			dev->SetRenderState(D3DRS_COLORWRITEENABLE, 0xF);
			dev->BeginScene();
			dev->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, 2, quad, sizeof(quad[0]));
			dev->EndScene();
			dev->SetTexture(0, NULL);
			if (!vd3d_check(dev->GetRenderTargetData(vd3d.depth_copy_surface, vd3d.depth_readback), "GetRenderTargetData")) return false;
			D3DLOCKED_RECT lr;
			if (FAILED(vd3d.depth_readback->LockRect(&lr, NULL, D3DLOCK_READONLY))) return false;
			for (UINT32 y = 0; y < vd3d.height; y++) {
				const float *s = (const float *)((uint8_t*)lr.pBits + y * lr.Pitch);
				for (UINT32 x = 0; x < vd3d.width; x++) {
					float f = s[x];
					if (f < 0.0f) f = 0.0f;
					if (f > 1.0f) f = 1.0f;
					c[(size_t)y * vd3d.width + x] = (UINT32)(f * 65535.0f + 0.5f);
				}
			}
			vd3d.depth_readback->UnlockRect();
		}
	}
	vd3d.cache_valid[which] = true;
	return true;
}

/* two pixels from (x, y) in the 3dfx video mode, row 0 at the top. With a larger render
 * resolution, the render pixel in the middle of each one is read (as OpenGL). */
UINT32 voodoo_d3d_read_pixel(int x, int y) {
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return 0;

	int rx[2], ry, p0, p1;
	for (int i = 0; i < 2; i++) {
		vd3d_lfb_span(x + i, vd3d.width, v->fbi.width, p0, p1);
		rx[i] = (p0 + p1 - 1) / 2;
	}
	vd3d_lfb_span(y, vd3d.height, v->fbi.height, p0, p1);
	ry = (p0 + p1 - 1) / 2;

	int which;
	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0: which = vd3d.front; break;		/* front buffer */
		case 1: which = vd3d.front ^ 1; break;		/* back buffer */
		case 2: which = 2; break;			/* aux buffer */
		default:
			E_Exit("read from invalid buf %x", LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			return 0;
	}
	if (!vd3d_read_buffer(which)) return 0;
	const UINT32 *line = &vd3d.cache[which][(size_t)ry * vd3d.width];
	const UINT32 d0 = line[rx[0]], d1 = line[rx[1]];
	if (which == 2) return (d0 & 0xffff) | (d1 << 16);

	/* ARGB to RGB 565 */
	#define VD3D_565(p) ((((p) >> 19) & 0x1f) << 11 | (((p) >> 10) & 0x3f) << 5 | (((p) >> 3) & 0x1f))
	return VD3D_565(d0) | (VD3D_565(d1) << 16);
	#undef VD3D_565
}

/* the pixel shader for a rasterizer's settings */
static IDirect3DPixelShader9 *vd3d_pixel_shader(const poly_extra_data *extra, bool tmu0, bool tmu1) {
	VD3DPsKey key;
	memset(&key, 0, sizeof(key));
	key.texmode[0] = tmu0 ? (UINT32)extra->r_textureMode0 : 0;
	key.texmode[1] = tmu1 ? (UINT32)extra->r_textureMode1 : 0;
	key.fbzcp = v->reg[fbzColorPath].u;
	key.alphamode = v->reg[alphaMode].u & 0x0f;	/* alpha test and function */
	key.fogmode = FOGMODE_ENABLE_FOG(v->reg[fogMode].u) ? (v->reg[fogMode].u & 0x3f) : 0;
	key.flags = (tmu0 ? VD3D_PS_TMU0 : 0) | (tmu1 ? VD3D_PS_TMU1 : 0) |
		(FBZMODE_ENABLE_CHROMAKEY(v->reg[fbzMode].u) ? VD3D_PS_CHROMAKEY : 0);
	map<VD3DPsKey, IDirect3DPixelShader9*>::iterator i = vd3d.shaders.find(key);
	if (i != vd3d.shaders.end()) return i->second;
	VD3DPixelShader ps;
	vector<uint32_t> code = ps.Build(key);
	IDirect3DPixelShader9 *shader = vd3d_create_shader(code);
	if (shader == NULL) LOG_MSG("VOODOO: Direct3D: pixel shader with %d instructions not accepted", ps.alu);
	vd3d.shaders[key] = shader;
	return shader;
}

void voodoo_d3d_draw_triangle(poly_extra_data *extra) {
	v = extra->state;
	VD3DScope scope;
	if (!scope.ok || vd3d.width == 0) return;

	const UINT32 ALPHAMODE = extra->r_alphaMode;
	const UINT32 FBZMODE = extra->r_fbzMode;

	ogl_vertex_data vd[3];
	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);

	/* the textures */
	IDirect3DTexture9 *tex[2] = { NULL, NULL };
	bool use[2] = { false, false };
	for (UINT32 j = 0; j < 2; j++) {
		if (j >= extra->texcount || v->tmu[j].ram == NULL || v->tmu[j].lodmin >= (8 << 8)) continue;
		tex[j] = vd3d_cache_texture(extra, j);
		use[j] = tex[j] != NULL;
	}
	IDirect3DPixelShader9 *shader = vd3d_pixel_shader(extra, use[0], use[1]);
	if (shader == NULL) return;

	const bool front = FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0;
	const int target = vd3d_target(front);
	if (front) v->fbi.vblank_flush_pending = true;

	VD3DDrawState st = vd3d_default_state(target, 0);
	st.shader = shader;
	for (int j = 0; j < 2; j++) {
		const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
		st.texture[j] = tex[j];
		if (!use[j]) continue;
		st.sampler[j] = (TEXMODE_MINIFICATION_FILTER(TEXMODE) ? VD3D_SAMPLER_MIN_LINEAR : 0u) |
			(TEXMODE_MAGNIFICATION_FILTER(TEXMODE) ? VD3D_SAMPLER_MAG_LINEAR : 0u) |
			(v->tmu[j].lodmin != v->tmu[j].lodmax ? VD3D_SAMPLER_MIPMAPS : 0u) |
			(TEXMODE_TRILINEAR(TEXMODE) ? VD3D_SAMPLER_TRILINEAR : 0u) |
			(TEXMODE_CLAMP_S(TEXMODE) ? VD3D_SAMPLER_CLAMP_S : 0u) |
			(TEXMODE_CLAMP_T(TEXMODE) ? VD3D_SAMPLER_CLAMP_T : 0u);
	}
	vd3d_constants(st.constants);

	/* vertices: in render resolution pixels (Direct3D 9 pixel centers are at whole numbers, so
	 * half a pixel less), the Y origin at the top or bottom as the OpenGL emulation's projection */
	const bool y_bottom = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u) != 0;
	float vert[3][VD3D_TRIANGLE_FLOATS];
	for (int i = 0; i < 3; i++) {
		float *o = vert[i];
		const float x = vd[i].x * (float)vd3d.width / (float)v->fbi.width;
		const float y = vd[i].y * (float)vd3d.height / (float)v->fbi.height;
		o[0] = x - 0.5f;
		o[1] = (y_bottom ? (float)vd3d.height - y : y) - 0.5f;
		o[2] = vd[i].d;
		o[3] = 1.0f;
		/* (OpenGL clamps the colors of the fixed function vertex shader) */
		const float c[4] = { vd[i].r, vd[i].g, vd[i].b, vd[i].a };
		for (int k = 0; k < 4; k++) o[4 + k] = c[k] < 0.0f ? 0.0f : (c[k] > 1.0f ? 1.0f : c[k]);
		for (int t = 0; t < 2; t++) {
			o[8 + t * 4] = use[t] ? vd[i].m[t].sw : 0.0f;
			o[9 + t * 4] = use[t] ? vd[i].m[t].tw : 0.0f;
			o[10 + t * 4] = 0.0f;
			o[11 + t * 4] = use[t] ? vd[i].m[t].w : 1.0f;
		}
		o[16] = vd[i].fogblend;
		o[17] = use[0] ? vd[i].m[0].lodblend : 0.0f;
		o[18] = use[1] ? vd[i].m[1].lodblend : 0.0f;
		o[19] = 0.0f;
	}

	/* depth source compare: the stencil marks where the triangle passes the depth test
	 * against zaColor, and it is drawn there */
	const bool stencil_pass = FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) != 0;
	if (stencil_pass) {
		vd3d_flush();
		vd3d.dev->SetRenderTarget(0, vd3d.color_surface[target]);
		vd3d.dev->SetDepthStencilSurface(vd3d.depth);
		vd3d.dev->Clear(0, NULL, D3DCLEAR_STENCIL, 0, 1.0f, 0);
		VD3DDrawState sst = vd3d_default_state(target, 1);
		sst.depth_test = (FBZMODE_ENABLE_DEPTHBUF(FBZMODE) && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? TRUE : FALSE;
		sst.depth_func = (DWORD)(D3DCMP_NEVER + FBZMODE_DEPTH_FUNCTION(FBZMODE));
		sst.color_mask = 0;
		sst.stencil = VD3D_STENCIL_WRITE;
		const float depth = (float)(v->reg[zaColor].u & 0xffff) / (float)0xffff;
		uint32_t flat[3][VD3D_FLAT_DWORDS];
		for (int i = 0; i < 3; i++) {
			const float p[4] = { vert[i][0], vert[i][1], depth, 1.0f };
			memcpy(flat[i], p, 16);
			flat[i][4] = 0xffffffff;
		}
		vd3d_draw(sst, flat, 3);
	}

	if (stencil_pass) {
		st.stencil = VD3D_STENCIL_TEST;
		st.depth_test = FBZMODE_ENABLE_DEPTHBUF(FBZMODE) ? TRUE : FALSE;
		st.depth_func = D3DCMP_ALWAYS;
	} else if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			st.depth_test = TRUE;
			st.depth_func = (DWORD)(D3DCMP_NEVER + FBZMODE_DEPTH_FUNCTION(FBZMODE));
		} else if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
			st.depth_test = TRUE;
			st.depth_func = D3DCMP_ALWAYS;
		}
	} else {
		st.depth_test = TRUE;
		st.depth_func = D3DCMP_ALWAYS;
	}
	const bool aux = FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0;
	st.depth_write = (aux && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? TRUE : FALSE;
	st.color_mask = vd3d_color_mask(FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0, aux);
	vd3d_blend_state(st, ALPHAMODE);
	vd3d_draw(st, vert, 3);
}

#else

/* no Direct3D output */

bool voodoo_d3d_init(voodoo_state *vs) { (void)vs; return false; }
void voodoo_d3d_leave(bool leavemode) { (void)leavemode; }
void voodoo_d3d_shutdown(voodoo_state *vs) { (void)vs; }
void voodoo_d3d_set_window(voodoo_state *vs) { (void)vs; }
void voodoo_d3d_present(void) { }
void voodoo_d3d_load_framebuffers_from_ram(void) { }
void voodoo_d3d_follow_dimensions(void) { }
void voodoo_d3d_swap_buffer(void) { }
void voodoo_d3d_vblank_flush(void) { }
void voodoo_d3d_update_dimensions(void) { }
void voodoo_d3d_clear(void) { }
void voodoo_d3d_fastfill(void) { }
void voodoo_d3d_clip_window(voodoo_state *vs) { (void)vs; }
void voodoo_d3d_texture_clear(UINT32 address, int TMU) { (void)address; (void)TMU; }
void voodoo_d3d_invalidate_paltex(void) { }
void voodoo_d3d_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	(void)x; (void)y; (void)has_rgb; (void)has_alpha; (void)r; (void)g; (void)b; (void)a;
}
void voodoo_d3d_draw_z(int x, int y, int z) { (void)x; (void)y; (void)z; }
void voodoo_d3d_draw_pixel_pipeline(int x, int y, int r, int g, int b) { (void)x; (void)y; (void)r; (void)g; (void)b; }
UINT32 voodoo_d3d_read_pixel(int x, int y) { (void)x; (void)y; return 0; }
void voodoo_d3d_draw_triangle(poly_extra_data *extra) { (void)extra; }

#endif
