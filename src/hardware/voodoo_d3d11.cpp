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

/* The 3dfx emulation rendered with Direct3D 11, on the Direct3D 11 output's device (it only works
 * with output=direct3d11). It does what the OpenGL one (voodoo_opengl.cpp) does, the same way where
 * it can:
 *
 *  - It renders into textures of its own at the render resolution (voodoo_resolution): front and
 *    back color buffers and a depth/stencil buffer, top row first. At every vertical retrace the
 *    front buffer is copied to another texture, which the output draws in place of the emulated
 *    video card's frame.
 *  - Triangles use a pixel shader made for the rasterizer's register settings: the GLSL the OpenGL
 *    emulation makes, written in HLSL instead, compiled with the output's shader compiler. The rest
 *    of the state (depth, blending, masks, stencil) is in state objects, made as needed and kept.
 *  - Textures are converted on the CPU like the OpenGL emulation does, mipmaps too.
 *  - LFB writes are drawn as points or small rectangles, LFB reads read the buffers back (kept until
 *    the buffer is drawn to again).
 *  - The output draws on the same thread, and sets up what it needs itself. */

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <map>
#include <string>
#include <vector>

#include "dosbox.h"
#include "control.h"
#include "logging.h"
#include "render.h"

#include "paging.h"
#include "voodoo.h"
#include "voodoo_emu.h"
#include "voodoo_opengl.h"
#include "voodoo_d3d11.h"

#if C_DIRECT3D && defined(C_SDL2)

#include <output/output_direct3d11.h>
#include "voodoo_def.h"

using namespace std;

/*************************************
 *  Shaders
 *************************************/

/* the vertices: positions in render resolution pixels (row 0 at the top), the iterated color,
 * texture coordinates (s*w, t*w, 0, w) of TMU 0 and 1, fog blend and the LOD blends of TMU 0 and 1 */
struct VD11Color {
	float r, g, b, a;
};

struct VD11Vertex {
	float pos[4];
	float color[4];
	float tex0[4];
	float tex1[4];
	float extra[4];
};

static const char *vd11_vs_source =
	"cbuffer vs_constants : register(b0) { float4 scale; };\n"
	"struct VSIn {\n"
	"  float4 pos : POSITION; float4 col : COLOR;\n"
	"  float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 ex : TEXCOORD2;\n"
	"};\n"
	"struct PSIn {\n"
	"  float4 pos : SV_Position; float4 col : COLOR;\n"
	"  float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 ex : TEXCOORD2;\n"
	"};\n"
	"PSIn main(VSIn i) {\n"
	"  PSIn o;\n"
	"  o.pos = float4(i.pos.x * scale.x - 1.0, 1.0 - i.pos.y * scale.y, i.pos.z, 1.0);\n"
	"  o.col = i.col; o.t0 = i.t0; o.t1 = i.t1; o.ex = i.ex;\n"
	"  return o;\n"
	"}\n";

#define VD11_PS_INPUT \
	"struct PSIn {\n" \
	"  float4 pos : SV_Position; float4 col : COLOR;\n" \
	"  float4 t0 : TEXCOORD0; float4 t1 : TEXCOORD1; float4 ex : TEXCOORD2;\n" \
	"};\n"

/* LFB writes and fills: the color of the vertices */
static const char *vd11_flat_ps_source =
	VD11_PS_INPUT
	"float4 main(PSIn i) : SV_Target { return i.col; }\n";

/* a texture copied (scaled) to another */
static const char *vd11_copy_ps_source =
	VD11_PS_INPUT
	"Texture2D tex0 : register(t0);\n"
	"SamplerState smp0 : register(s0);\n"
	"float4 main(PSIn i) : SV_Target { return tex0.Sample(smp0, i.t0.xy); }\n";

/* what a triangle pixel shader depends on, as the Vulkan emulation's VvkFsKey */
struct VD11PsKey {
	UINT32 texmode[2];	/* 0 if that TMU is not used */
	UINT32 fbzcp;
	UINT32 alphamode;	/* alpha test bits */
	UINT32 fogmode;		/* 0 without fog */
	UINT32 flags;
	bool operator<(const VD11PsKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};
#define VD11_PS_TMU0		0x01
#define VD11_PS_TMU1		0x02
#define VD11_PS_CHROMAKEY	0x04

/* the triangle pixel shader constants, like the OpenGL emulation's uniforms */
struct VD11Constants {
	float c0[4];		/* color0 */
	float c1[4];		/* color1 */
	float chromakey[4];
	float fogcolor[4];
	float misc[4];		/* alpha reference, zaColor */
};

/* ogl_sh_tex_combine(), in HLSL */
static void vd11_sh_tex_combine(string &s, int TMU, UINT32 TEXMODE) {
	const char *lod = TMU == 0 ? "i.ex.y" : "i.ex.z";

	s += TEXMODE_TC_ZERO_OTHER(TEXMODE) ? "  tt.rgb = 0.0;\n" : "  tt.rgb = cother.rgb;\n";
	s += TEXMODE_TCA_ZERO_OTHER(TEXMODE) ? "  tt.a = 0.0;\n" : "  tt.a = cother.a;\n";
	if (TEXMODE_TC_SUB_CLOCAL(TEXMODE)) s += "  tt.rgb -= clocal.rgb;\n";
	if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE)) s += "  tt.a -= clocal.a;\n";

	switch (TEXMODE_TC_MSELECT(TEXMODE)) {
		default:
		case 0: s += "  blend.rgb = 0.0;\n"; break;
		case 1: s += "  blend.rgb = clocal.rgb;\n"; break;
		case 2: s += "  blend.rgb = cother.aaa;\n"; break;
		case 3: s += "  blend.rgb = clocal.aaa;\n"; break;
		case 4: /* detail */
		case 5: /* LOD */
			s += string("  blend.rgb = ") + lod + ";\n"; break;
	}
	switch (TEXMODE_TCA_MSELECT(TEXMODE)) {
		default:
		case 0: s += "  blend.a = 0.0;\n"; break;
		case 1: s += "  blend.a = clocal.a;\n"; break;
		case 2: s += "  blend.a = cother.a;\n"; break;
		case 3: s += "  blend.a = clocal.a;\n"; break;
		case 4: /* detail */
		case 5: /* LOD */
			s += string("  blend.a = ") + lod + ";\n"; break;
	}

	if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE)) s += "  blend.rgb = 1.0 - blend.rgb;\n";
	if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE)) s += "  blend.a = 1.0 - blend.a;\n";
	s += "  tt *= blend;\n";

	switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE)) {
		case 3:
		case 0: break;
		case 1: s += "  tt.rgb += clocal.rgb;\n"; break;
		case 2: s += "  tt.rgb += clocal.aaa;\n"; break;
	}
	if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE)) s += "  tt.a += clocal.a;\n";

	s += "  clocal = tt;\n";
	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE)) s += "  clocal.rgb = 1.0 - clocal.rgb;\n";
	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE)) s += "  clocal.a = 1.0 - clocal.a;\n";
}

/* ogl_sh_color_path(), in HLSL */
static void vd11_sh_color_path(string &s, const VD11PsKey &k) {
	const UINT32 FBZCOLORPATH = k.fbzcp;
	const UINT32 ALPHAMODE = k.alphamode;

	switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH)) {
		case 0: s += "  cother = i.col;\n"; break;
		case 1: s += "  cother = texel;\n"; break;
		case 2: s += "  cother = color1;\n"; break;
		default: s += "  cother = 0.0;\n"; break;
	}

	if (k.flags & VD11_PS_CHROMAKEY)
		s += "  if (distance(cother.rgb, chromaKey.rgb) < 0.0001) discard;\n";

	switch (FBZCP_CC_ASELECT(FBZCOLORPATH)) {
		case 0: s += "  cother.a = i.col.a;\n"; break;
		case 1: s += "  cother.a = texel.a;\n"; break;
		case 2: s += "  cother.a = color1.a;\n"; break;
		default: s += "  cother.a = 0.0;\n"; break;
	}

	if (ALPHAMODE_ALPHATEST(ALPHAMODE)) {
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
			case 0: s += "  discard;\n"; break;
			case 1: s += "  if (cother.a >= misc.x) discard;\n"; break;
			case 2: s += "  if (abs(cother.a - misc.x) > 0.0001) discard;\n"; break;
			case 3: s += "  if (cother.a > misc.x) discard;\n"; break;
			case 4: s += "  if (cother.a <= misc.x) discard;\n"; break;
			case 5: s += "  if (abs(cother.a - misc.x) < 0.0001) discard;\n"; break;
			case 6: s += "  if (cother.a < misc.x) discard;\n"; break;
			case 7: break;
		}
	}

	if (FBZCP_CC_LOCALSELECT_OVERRIDE(FBZCOLORPATH) == 0)
		s += FBZCP_CC_LOCALSELECT(FBZCOLORPATH) == 0 ? "  clocal = i.col;\n" : "  clocal = color0;\n";
	else
		s += "  clocal = texel.a < 0.5 ? i.col : color0;\n";

	switch (FBZCP_CCA_LOCALSELECT(FBZCOLORPATH)) {
		default:
		case 0: s += "  clocal.a = i.col.a;\n"; break;
		case 1: s += "  clocal.a = color0.a;\n"; break;
		case 2: s += "  clocal.a = i.col.a;\n"; break;	/* TODO CLAMPED_Z (as OpenGL) */
		case 3: break;					/* voodoo2 only */
	}

	s += FBZCP_CC_ZERO_OTHER(FBZCOLORPATH) == 0 ? "  tt.rgb = cother.rgb;\n" : "  tt.rgb = 0.0;\n";
	s += FBZCP_CCA_ZERO_OTHER(FBZCOLORPATH) == 0 ? "  tt.a = cother.a;\n" : "  tt.a = 0.0;\n";
	if (FBZCP_CC_SUB_CLOCAL(FBZCOLORPATH)) s += "  tt.rgb -= clocal.rgb;\n";
	if (FBZCP_CCA_SUB_CLOCAL(FBZCOLORPATH)) s += "  tt.a -= clocal.a;\n";

	switch (FBZCP_CC_MSELECT(FBZCOLORPATH)) {
		default:
		case 0: s += "  blend.rgb = 0.0;\n"; break;
		case 1: s += "  blend.rgb = clocal.rgb;\n"; break;
		case 2: s += "  blend.rgb = cother.aaa;\n"; break;
		case 3: s += "  blend.rgb = clocal.aaa;\n"; break;
		case 4: s += "  blend.rgb = texel.aaa;\n"; break;
		case 5: s += "  blend.rgb = texel.rgb;\n"; break;	/* voodoo2 only */
	}
	switch (FBZCP_CCA_MSELECT(FBZCOLORPATH)) {
		default:
		case 0: s += "  blend.a = 0.0;\n"; break;
		case 1: s += "  blend.a = clocal.a;\n"; break;
		case 2: s += "  blend.a = cother.a;\n"; break;
		case 3: s += "  blend.a = clocal.a;\n"; break;
		case 4: s += "  blend.a = texel.a;\n"; break;
	}

	if (!FBZCP_CC_REVERSE_BLEND(FBZCOLORPATH)) s += "  blend.rgb = 1.0 - blend.rgb;\n";
	if (!FBZCP_CCA_REVERSE_BLEND(FBZCOLORPATH)) s += "  blend.a = 1.0 - blend.a;\n";
	s += "  tt *= blend;\n";

	switch (FBZCP_CC_ADD_ACLOCAL(FBZCOLORPATH)) {
		case 3:
		case 0: break;
		case 1: s += "  tt.rgb += clocal.rgb;\n"; break;
		case 2: s += "  tt.rgb += clocal.aaa;\n"; break;
	}
	if (FBZCP_CCA_ADD_ACLOCAL(FBZCOLORPATH)) s += "  tt.a += clocal.a;\n";

	s += "  pixel = tt;\n";
	if (FBZCP_CC_INVERT_OUTPUT(FBZCOLORPATH)) s += "  pixel.rgb = 1.0 - tt.rgb;\n";
	if (FBZCP_CCA_INVERT_OUTPUT(FBZCOLORPATH)) s += "  pixel.a = 1.0 - tt.a;\n";
}

/* ogl_sh_fog(), in HLSL */
static void vd11_sh_fog(string &s, UINT32 FOGMODE) {
	s += "  float4 ff;\n";
	/* constant fog bypasses everything else (as OpenGL: ff = fogColor, and nothing more) */
	if (FOGMODE_FOG_CONSTANT(FOGMODE)) {
		s += "  ff = fogColor;\n";
		return;
	}
	/* if fog_add is zero, we start with the fog color */
	s += FOGMODE_FOG_ADD(FOGMODE) == 0 ? "  ff = fogColor;\n" : "  ff = 0.0;\n";
	/* if fog_mult is zero, we subtract the incoming color */
	if (FOGMODE_FOG_MULT(FOGMODE) == 0) s += "  ff -= pixel;\n";
	/* fog blending mode: the table, iterated Z and W are worked out per vertex */
	s += FOGMODE_FOG_ZALPHA(FOGMODE) == 1 ? "  ff *= i.col.a;\n" : "  ff *= i.ex.x;\n";
	/* if fog_mult is 0, we add this to the original color, otherwise it is the new color */
	s += FOGMODE_FOG_MULT(FOGMODE) == 0 ? "  pixel.rgb += ff.rgb;\n" : "  pixel.rgb = ff.rgb;\n";
}

/* ogl_shaders(): the fragment shader, in HLSL */
static string vd11_triangle_ps_source(const VD11PsKey &k) {
	string s =
		VD11_PS_INPUT
		"Texture2D tex0 : register(t0);\n"
		"Texture2D tex1 : register(t1);\n"
		"SamplerState smp0 : register(s0);\n"
		"SamplerState smp1 : register(s1);\n"
		"cbuffer ps_constants : register(b0) {\n"
		"  float4 color0; float4 color1; float4 chromaKey; float4 fogColor; float4 misc;\n"
		"};\n"
		"float4 main(PSIn i) : SV_Target {\n"
		"  float4 pixel = 0.0;\n"
		"  float4 texel = 1.0;\n"
		"  float4 clocal = 1.0;\n"
		"  float4 cother = 0.0;\n"
		"  float4 tt = 0.0;\n"
		"  float4 blend = 0.0;\n";

	if (k.flags & VD11_PS_TMU1) {
		s += "  clocal = tex1.Sample(smp1, i.t1.xy / i.t1.w);\n";
		vd11_sh_tex_combine(s, 1, k.texmode[1]);
		s += "  cother = clocal;\n";
		s += "  texel = clocal;\n";
	}
	if (k.flags & VD11_PS_TMU0) {
		s += "  clocal = tex0.Sample(smp0, i.t0.xy / i.t0.w);\n";
		vd11_sh_tex_combine(s, 0, k.texmode[0]);
		s += "  texel = clocal;\n";
	}

	vd11_sh_color_path(s, k);
	if (FOGMODE_ENABLE_FOG(k.fogmode)) vd11_sh_fog(s, k.fogmode);
	s += "  return pixel;\n}\n";
	return s;
}

/*************************************
 *  State
 *************************************/

#define VD11_STENCIL_WRITE	1
#define VD11_STENCIL_TEST	2

#define VD11_SAMPLER_MIN_LINEAR		0x01
#define VD11_SAMPLER_MAG_LINEAR		0x02
#define VD11_SAMPLER_MIPMAPS		0x04
#define VD11_SAMPLER_TRILINEAR		0x08
#define VD11_SAMPLER_CLAMP_S		0x10
#define VD11_SAMPLER_CLAMP_T		0x20

struct VD11BlendKey {
	UINT32 blend, src, dst, src_alpha, dst_alpha, mask;
	bool operator<(const VD11BlendKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};

struct VD11DepthKey {
	UINT32 test, func, write, stencil;
	bool operator<(const VD11DepthKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};

/* all the state of a draw: draws with the same state are drawn together */
struct VD11DrawState {
	int target;
	int kind;			/* 0: triangles with a pixel shader, 1: flat triangles, 2: flat points */
	ID3D11PixelShader *shader;
	ID3D11ShaderResourceView *texture[2];
	UINT32 sampler[2];		/* VD11_SAMPLER_... */
	VD11BlendKey blend;
	VD11DepthKey depth;
	VD11Constants constants;
};

/* a texture, like the OpenGL emulation's ogl_texmap */
struct VD11Texture {
	ID3D11Texture2D *tex = NULL;
	ID3D11ShaderResourceView *srv = NULL;
};
struct VD11TexMap {
	bool valid_pal;
	UINT32 format, width, height, ilod, lodmask, ncc_table;
	VD11Texture current;
	map<UINT32, VD11Texture> by_palette;	/* palette formats: one per palette */
};

struct VD11DirtyRange {
	bool pending = false;
	UINT32 first = 0, last = 0;
};

/* the color buffers: render targets, readable by shaders */
struct VD11Target {
	ID3D11Texture2D *tex = NULL;
	ID3D11RenderTargetView *rtv = NULL;
	ID3D11ShaderResourceView *srv = NULL;
};

#define VD11_VB_VERTICES	65536

static struct {
	bool ready;		/* the buffers exist */
	bool lost;		/* the output destroyed its device (start over with its next one) */
	ID3D11Device *dev;
	ID3D11DeviceContext *ctx;
	UINT32 max_size;

	/* the 3dfx card's buffers */
	VD11Target color[2];
	VD11Target present;
	ID3D11Texture2D *depth;
	ID3D11DepthStencilView *dsv;
	UINT32 width, height;
	int front;

	/* shaders, state objects */
	ID3D11VertexShader *vs;
	ID3D11InputLayout *layout;
	ID3D11PixelShader *flat_ps;
	ID3D11PixelShader *copy_ps;
	ID3D11RasterizerState *raster;
	ID3D11Buffer *vb;
	UINT32 vb_used;			/* vertices */
	ID3D11Buffer *vs_cb, *ps_cb;
	UINT32 vs_cb_w, vs_cb_h;
	map<VD11PsKey, ID3D11PixelShader*> shaders;
	map<VD11BlendKey, ID3D11BlendState*> blend_states;
	map<VD11DepthKey, ID3D11DepthStencilState*> depth_states;
	map<UINT32, ID3D11SamplerState*> samplers;

	/* draws not drawn yet */
	VD11DrawState pending;
	bool have_pending;
	vector<VD11Vertex> vertices;

	/* LFB reads: color buffers 0 and 1, and depth */
	ID3D11Texture2D *staging_color;
	ID3D11Texture2D *staging_depth;
	bool cache_valid[3];
	vector<UINT32> cache[3];		/* colors as 0xAARRGGBB, depth as 16 bits */
} vd11;

static map<const UINT32, VD11TexMap> vd11_textures[2];
static VD11DirtyRange vd11_texture_dirty[2];

static bool vd11_check(HRESULT hr, const char *what) {
	if (FAILED(hr)) {
		LOG_MSG("VOODOO: Direct3D 11: %s failed (0x%08lx)", what, (unsigned long)hr);
		return false;
	}
	return true;
}

/* the device is there, and is the one the output uses */
static bool vd11_usable(void) {
	CDirect3D11 *out = OUTPUT_DIRECT3D11_Get();
	return vd11.ready && vd11.width != 0 && out != NULL && out->GetDevice() == vd11.dev;
}

/*************************************
 *  State objects
 *************************************/

static D3D11_BLEND vd11_src_factor(UINT32 f) {
	static const D3D11_BLEND t[16] = {
		D3D11_BLEND_ZERO, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_DEST_COLOR, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_ONE,
		D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_DEST_COLOR, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO,
		D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_SRC_ALPHA_SAT };
	return t[f & 15];
}

static D3D11_BLEND vd11_dst_factor(UINT32 f) {
	static const D3D11_BLEND t[16] = {
		D3D11_BLEND_ZERO, D3D11_BLEND_SRC_ALPHA, D3D11_BLEND_SRC_COLOR, D3D11_BLEND_DEST_ALPHA, D3D11_BLEND_ONE,
		D3D11_BLEND_INV_SRC_ALPHA, D3D11_BLEND_INV_SRC_COLOR, D3D11_BLEND_INV_DEST_ALPHA, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO,
		D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO, D3D11_BLEND_ZERO,
		D3D11_BLEND_SRC_COLOR /* A_COLORBEFOREFOG */ };
	return t[f & 15];
}

static ID3D11BlendState *vd11_blend_state(const VD11BlendKey &k) {
	map<VD11BlendKey, ID3D11BlendState*>::iterator i = vd11.blend_states.find(k);
	if (i != vd11.blend_states.end()) return i->second;
	D3D11_BLEND_DESC d;
	memset(&d, 0, sizeof(d));
	D3D11_RENDER_TARGET_BLEND_DESC &rt = d.RenderTarget[0];
	rt.BlendEnable = k.blend ? TRUE : FALSE;
	rt.SrcBlend = (D3D11_BLEND)k.src;
	rt.DestBlend = (D3D11_BLEND)k.dst;
	rt.BlendOp = D3D11_BLEND_OP_ADD;
	rt.SrcBlendAlpha = (D3D11_BLEND)k.src_alpha;
	rt.DestBlendAlpha = (D3D11_BLEND)k.dst_alpha;
	rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	rt.RenderTargetWriteMask = (UINT8)k.mask;
	ID3D11BlendState *st = NULL;
	vd11_check(vd11.dev->CreateBlendState(&d, &st), "CreateBlendState");
	vd11.blend_states[k] = st;
	return st;
}

static ID3D11DepthStencilState *vd11_depth_state(const VD11DepthKey &k) {
	map<VD11DepthKey, ID3D11DepthStencilState*>::iterator i = vd11.depth_states.find(k);
	if (i != vd11.depth_states.end()) return i->second;
	D3D11_DEPTH_STENCIL_DESC d;
	memset(&d, 0, sizeof(d));
	d.DepthEnable = (k.test || k.write) ? TRUE : FALSE;
	d.DepthFunc = k.test ? (D3D11_COMPARISON_FUNC)k.func : D3D11_COMPARISON_ALWAYS;
	d.DepthWriteMask = k.write ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	d.StencilEnable = k.stencil != 0 ? TRUE : FALSE;
	d.StencilReadMask = 1;
	d.StencilWriteMask = 1;
	D3D11_DEPTH_STENCILOP_DESC op;
	op.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	op.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	op.StencilPassOp = k.stencil == VD11_STENCIL_WRITE ? D3D11_STENCIL_OP_REPLACE : D3D11_STENCIL_OP_KEEP;
	op.StencilFunc = k.stencil == VD11_STENCIL_WRITE ? D3D11_COMPARISON_ALWAYS : D3D11_COMPARISON_EQUAL;
	d.FrontFace = op;
	d.BackFace = op;
	ID3D11DepthStencilState *st = NULL;
	vd11_check(vd11.dev->CreateDepthStencilState(&d, &st), "CreateDepthStencilState");
	vd11.depth_states[k] = st;
	return st;
}

static ID3D11SamplerState *vd11_sampler(UINT32 k) {
	map<UINT32, ID3D11SamplerState*>::iterator i = vd11.samplers.find(k);
	if (i != vd11.samplers.end()) return i->second;
	D3D11_SAMPLER_DESC d;
	memset(&d, 0, sizeof(d));
	/* (the filter's bits: mip 0, mag 2, min 4) */
	d.Filter = (D3D11_FILTER)(((k & VD11_SAMPLER_MIN_LINEAR) ? 0x10 : 0) | ((k & VD11_SAMPLER_MAG_LINEAR) ? 0x04 : 0) |
		((k & VD11_SAMPLER_TRILINEAR) ? 0x01 : 0));
	d.AddressU = (k & VD11_SAMPLER_CLAMP_S) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
	d.AddressV = (k & VD11_SAMPLER_CLAMP_T) ? D3D11_TEXTURE_ADDRESS_CLAMP : D3D11_TEXTURE_ADDRESS_WRAP;
	d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	d.MaxAnisotropy = 1;
	d.ComparisonFunc = D3D11_COMPARISON_NEVER;
	d.MinLOD = 0.0f;
	d.MaxLOD = (k & VD11_SAMPLER_MIPMAPS) ? D3D11_FLOAT32_MAX : 0.0f;
	ID3D11SamplerState *st = NULL;
	vd11_check(vd11.dev->CreateSamplerState(&d, &st), "CreateSamplerState");
	vd11.samplers[k] = st;
	return st;
}

/* the vertex shader's scale for render targets of w x h pixels */
static void vd11_set_target_size(UINT32 w, UINT32 h) {
	if (vd11.vs_cb_w == w && vd11.vs_cb_h == h) return;
	D3D11_MAPPED_SUBRESOURCE m;
	if (FAILED(vd11.ctx->Map(vd11.vs_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) return;
	const float scale[4] = { 2.0f / (float)w, 2.0f / (float)h, 0.0f, 0.0f };
	memcpy(m.pData, scale, sizeof(scale));
	vd11.ctx->Unmap(vd11.vs_cb, 0);
	vd11.vs_cb_w = w;
	vd11.vs_cb_h = h;
}

/* the pipeline as the 3dfx emulation draws with it (the output changes it too) */
static void vd11_bind_pipeline(UINT32 w, UINT32 h) {
	ID3D11DeviceContext *ctx = vd11.ctx;
	vd11_set_target_size(w, h);
	D3D11_VIEWPORT vp;
	vp.TopLeftX = 0.0f;
	vp.TopLeftY = 0.0f;
	vp.Width = (FLOAT)w;
	vp.Height = (FLOAT)h;
	vp.MinDepth = 0.0f;
	vp.MaxDepth = 1.0f;
	ctx->RSSetViewports(1, &vp);
	ctx->RSSetState(vd11.raster);
	ctx->IASetInputLayout(vd11.layout);
	const UINT stride = sizeof(VD11Vertex), offset = 0;
	ctx->IASetVertexBuffers(0, 1, &vd11.vb, &stride, &offset);
	ctx->VSSetShader(vd11.vs, NULL, 0);
	ctx->VSSetConstantBuffers(0, 1, &vd11.vs_cb);
	ctx->GSSetShader(NULL, NULL, 0);
}

/* put vertices in the vertex buffer, return the first one's index (or -1) */
static int vd11_upload_vertices(const VD11Vertex *data, UINT32 count) {
	if (count > VD11_VB_VERTICES) return -1;
	D3D11_MAP mode = D3D11_MAP_WRITE_NO_OVERWRITE;
	if (vd11.vb_used + count > VD11_VB_VERTICES) {
		mode = D3D11_MAP_WRITE_DISCARD;
		vd11.vb_used = 0;
	}
	D3D11_MAPPED_SUBRESOURCE m;
	if (FAILED(vd11.ctx->Map(vd11.vb, 0, mode, 0, &m))) return -1;
	memcpy((VD11Vertex*)m.pData + vd11.vb_used, data, count * sizeof(VD11Vertex));
	vd11.ctx->Unmap(vd11.vb, 0);
	const int first = (int)vd11.vb_used;
	vd11.vb_used += count;
	return first;
}

/*************************************
 *  Drawing
 *************************************/

static VD11DrawState vd11_default_state(int target, int kind) {
	VD11DrawState st;
	memset(&st, 0, sizeof(st));
	st.target = target;
	st.kind = kind;
	st.blend.src = st.blend.src_alpha = D3D11_BLEND_ONE;
	st.blend.dst = st.blend.dst_alpha = D3D11_BLEND_ZERO;
	st.blend.mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	st.depth.func = D3D11_COMPARISON_ALWAYS;
	return st;
}

/* blending from alphaMode, as VOGL_SetAlphaMode() is called with */
static void vd11_blend_from(VD11DrawState &st, UINT32 ALPHAMODE) {
	st.blend.blend = ALPHAMODE_ALPHABLEND(ALPHAMODE) ? 1 : 0;
	if (!st.blend.blend) return;
	st.blend.src = vd11_src_factor(ALPHAMODE_SRCRGBBLEND(ALPHAMODE));
	st.blend.dst = vd11_dst_factor(ALPHAMODE_DSTRGBBLEND(ALPHAMODE));
	st.blend.src_alpha = ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4 ? D3D11_BLEND_ONE : D3D11_BLEND_ZERO;
	st.blend.dst_alpha = ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4 ? D3D11_BLEND_ONE : D3D11_BLEND_ZERO;
}

static UINT32 vd11_color_mask(bool rgb, bool alpha) {
	return (rgb ? (D3D11_COLOR_WRITE_ENABLE_RED | D3D11_COLOR_WRITE_ENABLE_GREEN | D3D11_COLOR_WRITE_ENABLE_BLUE) : 0) |
		(alpha ? D3D11_COLOR_WRITE_ENABLE_ALPHA : 0);
}

static void vd11_constants(VD11Constants &c) {
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
	c.misc[0] = r[alphaMode].rgb.a / 255.0f;
	c.misc[1] = (float)((UINT16)r[zaColor].u) / 65535.0f;
}

/* draw what was queued */
static void vd11_flush(void) {
	if (!vd11.have_pending) return;
	vd11.have_pending = false;
	const UINT32 count = (UINT32)vd11.vertices.size();
	if (count == 0) return;
	const VD11DrawState &st = vd11.pending;
	ID3D11DeviceContext *ctx = vd11.ctx;

	const int first = vd11_upload_vertices(vd11.vertices.data(), count);
	vd11.vertices.clear();
	if (first < 0) return;

	vd11_bind_pipeline(vd11.width, vd11.height);
	ctx->OMSetRenderTargets(1, &vd11.color[st.target].rtv, vd11.dsv);
	ctx->OMSetBlendState(vd11_blend_state(st.blend), NULL, 0xffffffff);
	ctx->OMSetDepthStencilState(vd11_depth_state(st.depth), 1);
	if (st.kind == 0) {
		D3D11_MAPPED_SUBRESOURCE m;
		if (SUCCEEDED(ctx->Map(vd11.ps_cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
			memcpy(m.pData, &st.constants, sizeof(st.constants));
			ctx->Unmap(vd11.ps_cb, 0);
		}
		ctx->PSSetShader(st.shader, NULL, 0);
		ctx->PSSetConstantBuffers(0, 1, &vd11.ps_cb);
		ID3D11ShaderResourceView *srv[2] = { st.texture[0], st.texture[1] };
		ID3D11SamplerState *smp[2] = { vd11_sampler(st.sampler[0]), vd11_sampler(st.sampler[1]) };
		ctx->PSSetShaderResources(0, 2, srv);
		ctx->PSSetSamplers(0, 2, smp);
	} else {
		ctx->PSSetShader(vd11.flat_ps, NULL, 0);
	}
	ctx->IASetPrimitiveTopology(st.kind == 2 ? D3D11_PRIMITIVE_TOPOLOGY_POINTLIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->Draw(count, (UINT)first);
	if (st.kind == 0) {
		ID3D11ShaderResourceView *none[2] = { NULL, NULL };
		ctx->PSSetShaderResources(0, 2, none);
	}
}

/* queue vertices, drawn with the ones before them if the state is the same */
static void vd11_draw(const VD11DrawState &st, const VD11Vertex *data, UINT32 count) {
	if (vd11.have_pending && memcmp(&vd11.pending, &st, sizeof(st)) != 0) vd11_flush();
	if (!vd11.have_pending) {
		vd11.pending = st;
		vd11.have_pending = true;
	}
	vd11.vertices.insert(vd11.vertices.end(), data, data + count);
	vd11.cache_valid[st.target] = false;
	vd11.cache_valid[2] = false;
	if (vd11.vertices.size() >= VD11_VB_VERTICES / 2) vd11_flush();
}

static void vd11_invalidate_reads(void) {
	vd11.cache_valid[0] = vd11.cache_valid[1] = vd11.cache_valid[2] = false;
}

static int vd11_target(bool front) {
	return front ? vd11.front : (vd11.front ^ 1);
}

static void vd11_vertex(VD11Vertex &o, float x, float y, float z, VD11Color c) {
	memset(&o, 0, sizeof(o));
	o.pos[0] = x;
	o.pos[1] = y;
	o.pos[2] = z;
	o.pos[3] = 1.0f;
	o.color[0] = c.r;
	o.color[1] = c.g;
	o.color[2] = c.b;
	o.color[3] = c.a;
}

/* a flat rectangle in render resolution pixels [x0, x1) x [y0, y1) */
static void vd11_rect(const VD11DrawState &st, float x0, float y0, float x1, float y1, float z, VD11Color c) {
	VD11Vertex q[6];
	vd11_vertex(q[0], x0, y0, z, c);
	vd11_vertex(q[1], x1, y0, z, c);
	vd11_vertex(q[2], x0, y1, z, c);
	vd11_vertex(q[3], x1, y0, z, c);
	vd11_vertex(q[4], x1, y1, z, c);
	vd11_vertex(q[5], x0, y1, z, c);
	vd11_draw(st, q, 6);
}

/*************************************
 *  Buffers
 *************************************/

static void vd11_release_target(VD11Target &t) {
	SAFE_RELEASE(t.srv);
	SAFE_RELEASE(t.rtv);
	SAFE_RELEASE(t.tex);
}

static void vd11_release_targets(void) {
	if (OUTPUT_DIRECT3D11_Get() != NULL) OUTPUT_DIRECT3D11_Get()->SetExternalTexture(NULL);
	for (int i = 0; i < 2; i++) vd11_release_target(vd11.color[i]);
	vd11_release_target(vd11.present);
	SAFE_RELEASE(vd11.dsv);
	SAFE_RELEASE(vd11.depth);
	SAFE_RELEASE(vd11.staging_color);
	SAFE_RELEASE(vd11.staging_depth);
	vd11.width = vd11.height = 0;
	vd11_invalidate_reads();
}

static bool vd11_create_target(UINT32 w, UINT32 h, VD11Target &t) {
	D3D11_TEXTURE2D_DESC d;
	memset(&d, 0, sizeof(d));
	d.Width = w;
	d.Height = h;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	return vd11_check(vd11.dev->CreateTexture2D(&d, NULL, &t.tex), "CreateTexture2D") &&
		vd11_check(vd11.dev->CreateRenderTargetView(t.tex, NULL, &t.rtv), "CreateRenderTargetView") &&
		vd11_check(vd11.dev->CreateShaderResourceView(t.tex, NULL, &t.srv), "CreateShaderResourceView");
}

/* make the buffers, black (depth 1.0) */
static bool vd11_create_targets(UINT32 w, UINT32 h) {
	for (int i = 0; i < 2; i++)
		if (!vd11_create_target(w, h, vd11.color[i])) return false;
	if (!vd11_create_target(w, h, vd11.present)) return false;

	D3D11_TEXTURE2D_DESC d;
	memset(&d, 0, sizeof(d));
	d.Width = w;
	d.Height = h;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_R24G8_TYPELESS;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_DEFAULT;
	d.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	if (!vd11_check(vd11.dev->CreateTexture2D(&d, NULL, &vd11.depth), "CreateTexture2D")) return false;
	D3D11_DEPTH_STENCIL_VIEW_DESC dv;
	memset(&dv, 0, sizeof(dv));
	dv.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	if (!vd11_check(vd11.dev->CreateDepthStencilView(vd11.depth, &dv, &vd11.dsv), "CreateDepthStencilView")) return false;

	vd11.width = w;
	vd11.height = h;
	const float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, opaque[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
	for (int i = 0; i < 2; i++) vd11.ctx->ClearRenderTargetView(vd11.color[i].rtv, black);
	vd11.ctx->ClearRenderTargetView(vd11.present.rtv, opaque);
	vd11.ctx->ClearDepthStencilView(vd11.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	vd11_invalidate_reads();
	return true;
}

/* draw a texture over the whole of a render target, without filtering */
static void vd11_copy_scaled(ID3D11ShaderResourceView *src, ID3D11RenderTargetView *dst, UINT32 w, UINT32 h) {
	ID3D11DeviceContext *ctx = vd11.ctx;
	VD11Vertex q[6];
	const VD11Color none = { 0, 0, 0, 0 };
	const float x[6] = { 0, (float)w, 0, (float)w, (float)w, 0 }, y[6] = { 0, 0, (float)h, 0, (float)h, (float)h };
	for (int i = 0; i < 6; i++) {
		vd11_vertex(q[i], x[i], y[i], 0.0f, none);
		q[i].tex0[0] = x[i] / (float)w;
		q[i].tex0[1] = y[i] / (float)h;
	}
	const int first = vd11_upload_vertices(q, 6);
	if (first < 0) return;
	VD11BlendKey bk;
	memset(&bk, 0, sizeof(bk));
	bk.src = bk.src_alpha = D3D11_BLEND_ONE;
	bk.dst = bk.dst_alpha = D3D11_BLEND_ZERO;
	bk.mask = D3D11_COLOR_WRITE_ENABLE_ALL;
	VD11DepthKey dk;
	memset(&dk, 0, sizeof(dk));
	vd11_bind_pipeline(w, h);
	ctx->OMSetRenderTargets(1, &dst, NULL);
	ctx->OMSetBlendState(vd11_blend_state(bk), NULL, 0xffffffff);
	ctx->OMSetDepthStencilState(vd11_depth_state(dk), 0);
	ctx->PSSetShader(vd11.copy_ps, NULL, 0);
	ID3D11SamplerState *smp = vd11_sampler(VD11_SAMPLER_CLAMP_S | VD11_SAMPLER_CLAMP_T);
	ctx->PSSetSamplers(0, 1, &smp);
	ctx->PSSetShaderResources(0, 1, &src);
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ctx->Draw(6, (UINT)first);
	ID3D11ShaderResourceView *nosrv = NULL;
	ctx->PSSetShaderResources(0, 1, &nosrv);
}

/* change the render resolution, keeping what the color buffers show (scaled) */
static bool vd11_resize_targets(UINT32 w, UINT32 h) {
	vd11_flush();
	VD11Target old[2] = { vd11.color[0], vd11.color[1] };
	vd11.color[0] = vd11.color[1] = VD11Target();
	vd11_release_targets();

	const bool ok = vd11_create_targets(w, h);
	if (ok) {
		/* without filtering, the pixels keep their colors (for LFB reads). The depth buffer is
		 * cleared. */
		for (int i = 0; i < 2; i++)
			if (old[i].srv != NULL) vd11_copy_scaled(old[i].srv, vd11.color[i].rtv, w, h);
		OUTPUT_DIRECT3D11_Get()->SetExternalTexture(vd11.present.srv);
		LOG_MSG("VOODOO: Direct3D 11: render resolution %u:%u", (unsigned int)w, (unsigned int)h);
	} else {
		vd11_release_targets();
		vd11.ready = false;
	}
	for (int i = 0; i < 2; i++) vd11_release_target(old[i]);
	return ok;
}

/* follow the resolution setting and the display area, between frames */
static void vd11_check_render_size(void) {
	Voodoo_HW_PollResolutionSetting();
	UINT32 w, h;
	Voodoo_HW_RenderSize(vd11.max_size, w, h);
	if (vd11.width != 0 && (w != vd11.width || h != vd11.height))
		vd11_resize_targets(w, h);
}

/* the render resolution pixels [p0, p1) that 3dfx video mode pixel i covers, as the OpenGL
 * emulation's voodoo_ogl_lfb_span() */
static void vd11_lfb_span(int i, UINT32 render_size, UINT32 mode_size, int &p0, int &p1) {
	const double d = (double)render_size / (double)mode_size;
	p0 = (int)ceil(i * d - 0.5);
	p1 = (int)ceil((i + 1) * d - 0.5);
	if (p1 <= p0) p1 = p0 + 1;
	if (p0 >= (int)render_size) p0 = (int)render_size - 1;
	if (p1 > (int)render_size) p1 = (int)render_size;
}

/*************************************
 *  Textures
 *************************************/

static void vd11_release_texture(VD11Texture &t) {
	SAFE_RELEASE(t.srv);
	SAFE_RELEASE(t.tex);
}

static void vd11_destroy_texmap(VD11TexMap &m) {
	if (!m.by_palette.empty()) {
		for (map<UINT32, VD11Texture>::iterator i = m.by_palette.begin(); i != m.by_palette.end(); ++i)
			vd11_release_texture(i->second);
		m.by_palette.clear();
		m.current = VD11Texture();
	} else {
		vd11_release_texture(m.current);
	}
}

static void vd11_destroy_textures(void) {
	vd11_flush();
	for (int j = 0; j < 2; j++) {
		for (map<const UINT32, VD11TexMap>::iterator t = vd11_textures[j].begin(); t != vd11_textures[j].end(); ++t)
			vd11_destroy_texmap(t->second);
		vd11_textures[j].clear();
		vd11_texture_dirty[j].pending = false;
	}
}

static bool vd11_palette_format(UINT32 format) {
	return format == 1 || format == 9 || format == 5 || format == 14;
}

/* the palette (or NCC table) a texture is converted with */
static UINT32 vd11_palette_sum(UINT32 tmunum, UINT32 texmode) {
	const UINT32 format = TEXMODE_FORMAT(texmode);
	const rgb_t *colors = (format & 7) == 1 ? v->tmu[tmunum].ncc[TEXMODE_NCC_TABLE_SELECT(texmode)].texel : v->tmu[tmunum].palette;
	UINT32 h = 2166136261u;
	for (int i = 0; i < 256; i++) h = (h ^ colors[i]) * 16777619u;
	return h;
}

/* drop textures that texture memory writes changed (before the next draw) */
static void vd11_flush_texture_writes(int TMU) {
	VD11DirtyRange &dirty = vd11_texture_dirty[TMU];
	if (!dirty.pending) return;
	const UINT32 mask = v->tmu[TMU].mask;
	for (map<const UINT32, VD11TexMap>::iterator t = vd11_textures[TMU].begin(); t != vd11_textures[TMU].end();) {
		const UINT32 size = t->second.width * t->second.height * (t->second.format < 8 ? 1 : 2);
		const UINT32 last = t->first + size - 1;
		const bool overlaps = last <= mask
			? t->first <= dirty.last && last >= dirty.first
			: t->first <= dirty.last || (last & mask) >= dirty.first;
		if (overlaps) {
			vd11_flush();	/* (queued draws may use it) */
			vd11_destroy_texmap(t->second);
			vd11_textures[TMU].erase(t++);
		} else {
			++t;
		}
	}
	dirty.pending = false;
}

/* a texture from texture memory, with mipmaps (the OpenGL emulation's conversion) */
static VD11Texture vd11_upload_texture(int j, UINT32 TEXMODE, UINT32 texbase, UINT32 width, UINT32 height) {
	VD11Texture result;
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	UINT32 levels = 1;
	while (((width | height) >> levels) != 0) levels++;

	/* all the levels, one after another: each is the one before at half the size (2x2 averages) */
	vector<UINT32> data((size_t)width * height);
	UINT32 *texrgbp = data.data();
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

	vector<size_t> offsets(levels);
	vector<D3D11_SUBRESOURCE_DATA> init(levels);
	UINT32 lw = width, lh = height;
	size_t at = 0;
	for (UINT32 l = 0; l < levels; l++) {
		offsets[l] = at;
		init[l].SysMemPitch = lw * 4;
		init[l].SysMemSlicePitch = 0;
		if (l + 1 == levels) break;
		const UINT32 nw = lw > 1 ? lw / 2 : 1, nh = lh > 1 ? lh / 2 : 1;
		const size_t next = at + (size_t)lw * lh;
		data.resize(next + (size_t)nw * nh);
		const UINT32 *level = &data[at];
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
				data[next + (size_t)y * nw + x] = out;
			}
		}
		at = next;
		lw = nw;
		lh = nh;
	}
	for (UINT32 l = 0; l < levels; l++) init[l].pSysMem = &data[offsets[l]];

	D3D11_TEXTURE2D_DESC d;
	memset(&d, 0, sizeof(d));
	d.Width = width;
	d.Height = height;
	d.MipLevels = levels;
	d.ArraySize = 1;
	d.Format = DXGI_FORMAT_B8G8R8A8_UNORM;	/* (0xAARRGGBB in memory) */
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_IMMUTABLE;
	d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	if (!vd11_check(vd11.dev->CreateTexture2D(&d, init.data(), &result.tex), "CreateTexture2D") ||
	    !vd11_check(vd11.dev->CreateShaderResourceView(result.tex, NULL, &result.srv), "CreateShaderResourceView"))
		vd11_release_texture(result);
	return result;
}

/* the texture a TMU uses, like ogl_cache_texture() */
static ID3D11ShaderResourceView *vd11_cache_texture(const poly_extra_data *extra, UINT32 j) {
	vd11_flush_texture_writes((int)j);
	const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	const UINT32 ncc_table = (format & 7) == 1 ? TEXMODE_NCC_TABLE_SELECT(TEXMODE) : 0;

	UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
	if (!((v->tmu[j].lodmask >> ilod) & 1)) ilod++;
	const UINT32 texbase = v->tmu[j].lodoffset[ilod];
	const UINT32 width = (v->tmu[j].wmask >> ilod) + 1;
	const UINT32 height = (v->tmu[j].hmask >> ilod) + 1;

	map<const UINT32, VD11TexMap>::iterator t = vd11_textures[j].find(texbase);
	/* a RAM address can be reused with a different texture layout */
	if (t != vd11_textures[j].end() &&
	    (t->second.format != format || t->second.width != width || t->second.height != height ||
	     t->second.ilod != ilod || t->second.lodmask != v->tmu[j].lodmask || t->second.ncc_table != ncc_table)) {
		vd11_flush();
		vd11_destroy_texmap(t->second);
		vd11_textures[j].erase(t);
		t = vd11_textures[j].end();
	}

	UINT32 palsum = 0;
	if (t != vd11_textures[j].end()) {
		if (t->second.valid_pal) return t->second.current.srv;
		palsum = vd11_palette_sum(j, TEXMODE);
		map<UINT32, VD11Texture>::iterator u = t->second.by_palette.find(palsum);
		t->second.valid_pal = true;
		if (u != t->second.by_palette.end()) {
			t->second.current = u->second;
			return u->second.srv;
		}
		if (t->second.by_palette.size() > 8) {
			vd11_flush();
			for (u = t->second.by_palette.begin(); u != t->second.by_palette.end(); ++u)
				vd11_release_texture(u->second);
			t->second.by_palette.clear();
		}
	} else if (vd11_palette_format(format)) {
		palsum = vd11_palette_sum(j, TEXMODE);
	}

	VD11Texture tex = vd11_upload_texture((int)j, TEXMODE, texbase, width, height);
	if (tex.srv == NULL) return NULL;
	if (t == vd11_textures[j].end()) {
		VD11TexMap m;
		m.valid_pal = true;
		m.format = format;
		m.width = width;
		m.height = height;
		m.ilod = ilod;
		m.lodmask = v->tmu[j].lodmask;
		m.ncc_table = ncc_table;
		m.current = tex;
		if (vd11_palette_format(format)) m.by_palette[palsum] = tex;
		vd11_textures[j][texbase] = m;
	} else {
		t->second.current = tex;
		t->second.by_palette[palsum] = tex;
	}
	return tex.srv;
}

/*************************************
 *  Setup
 *************************************/

static ID3D11PixelShader *vd11_create_ps(const char *source) {
	ID3DBlob *blob = NULL;
	if (FAILED(OUTPUT_DIRECT3D11_Compile(source, "main", "ps_4_0", &blob))) return NULL;
	ID3D11PixelShader *ps = NULL;
	vd11_check(vd11.dev->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &ps), "CreatePixelShader");
	blob->Release();
	return ps;
}

static bool vd11_create_device_objects(void) {
	ID3DBlob *blob = NULL;
	if (FAILED(OUTPUT_DIRECT3D11_Compile(vd11_vs_source, "main", "vs_4_0", &blob))) return false;
	bool ok = vd11_check(vd11.dev->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &vd11.vs), "CreateVertexShader");
	if (ok) {
		const D3D11_INPUT_ELEMENT_DESC layout[5] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 64, D3D11_INPUT_PER_VERTEX_DATA, 0 } };
		ok = vd11_check(vd11.dev->CreateInputLayout(layout, 5, blob->GetBufferPointer(), blob->GetBufferSize(), &vd11.layout), "CreateInputLayout");
	}
	blob->Release();
	if (!ok) return false;

	vd11.flat_ps = vd11_create_ps(vd11_flat_ps_source);
	vd11.copy_ps = vd11_create_ps(vd11_copy_ps_source);
	if (vd11.flat_ps == NULL || vd11.copy_ps == NULL) return false;

	D3D11_RASTERIZER_DESC r;
	memset(&r, 0, sizeof(r));
	r.FillMode = D3D11_FILL_SOLID;
	r.CullMode = D3D11_CULL_NONE;
	r.DepthClipEnable = FALSE;
	if (!vd11_check(vd11.dev->CreateRasterizerState(&r, &vd11.raster), "CreateRasterizerState")) return false;

	D3D11_BUFFER_DESC b;
	memset(&b, 0, sizeof(b));
	b.ByteWidth = VD11_VB_VERTICES * sizeof(VD11Vertex);
	b.Usage = D3D11_USAGE_DYNAMIC;
	b.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	b.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	if (!vd11_check(vd11.dev->CreateBuffer(&b, NULL, &vd11.vb), "CreateBuffer")) return false;
	vd11.vb_used = VD11_VB_VERTICES;	/* (the first map discards) */
	b.ByteWidth = 16;
	b.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	if (!vd11_check(vd11.dev->CreateBuffer(&b, NULL, &vd11.vs_cb), "CreateBuffer")) return false;
	vd11.vs_cb_w = vd11.vs_cb_h = 0;
	b.ByteWidth = sizeof(VD11Constants);
	return vd11_check(vd11.dev->CreateBuffer(&b, NULL, &vd11.ps_cb), "CreateBuffer");
}

static void vd11_destroy_all(void) {
	if (vd11.dev == NULL) return;
	vd11_flush();
	vd11_destroy_textures();
	vd11_release_targets();
	for (map<VD11PsKey, ID3D11PixelShader*>::iterator i = vd11.shaders.begin(); i != vd11.shaders.end(); ++i)
		SAFE_RELEASE(i->second);
	vd11.shaders.clear();
	for (map<VD11BlendKey, ID3D11BlendState*>::iterator i = vd11.blend_states.begin(); i != vd11.blend_states.end(); ++i)
		SAFE_RELEASE(i->second);
	vd11.blend_states.clear();
	for (map<VD11DepthKey, ID3D11DepthStencilState*>::iterator i = vd11.depth_states.begin(); i != vd11.depth_states.end(); ++i)
		SAFE_RELEASE(i->second);
	vd11.depth_states.clear();
	for (map<UINT32, ID3D11SamplerState*>::iterator i = vd11.samplers.begin(); i != vd11.samplers.end(); ++i)
		SAFE_RELEASE(i->second);
	vd11.samplers.clear();
	SAFE_RELEASE(vd11.vs);
	SAFE_RELEASE(vd11.layout);
	SAFE_RELEASE(vd11.flat_ps);
	SAFE_RELEASE(vd11.copy_ps);
	SAFE_RELEASE(vd11.raster);
	SAFE_RELEASE(vd11.vb);
	SAFE_RELEASE(vd11.vs_cb);
	SAFE_RELEASE(vd11.ps_cb);
	if (vd11.ctx != NULL) {
		/* nothing of ours stays bound */
		vd11.ctx->ClearState();
		vd11.ctx->Release();
		vd11.ctx = NULL;
	}
	vd11.vertices.clear();
	vd11.have_pending = false;
	vd11.ready = false;
	vd11.dev = NULL;
}

/* (re)make the buffers for the 3dfx video mode, and show it in the output */
static bool vd11_reset_videomode(void) {
	if (vd11.dev == NULL) return false;
	vd11_flush();
	vd11_release_targets();

	/* the output shows the 3dfx frame like any video mode (sized for the 3dfx video mode,
	 * whatever the render resolution is). This comes first, for the size of the display area. */
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);

	CDirect3D11 *out = OUTPUT_DIRECT3D11_Get();
	if (out == NULL || out->GetDevice() != vd11.dev) return false;
	UINT32 w, h;
	Voodoo_HW_RenderSize(vd11.max_size, w, h);
	if (!vd11_create_targets(w, h)) {
		vd11_release_targets();
		return false;
	}
	vd11.front = 0;
	vd11.ready = true;
	out->SetExternalTexture(vd11.present.srv);
	LOG_MSG("VOODOO: Direct3D 11: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, w, h);
	return true;
}

/*************************************
 *  The 3dfx emulation's functions
 *************************************/

bool voodoo_d3d11_init(voodoo_state *vs) {
	(void)vs;
	if (vd11.ready) return true;
	if (!OUTPUT_DIRECT3D11_Active()) {
		LOG_MSG("VOODOO: Direct3D 11: the Direct3D 11 3dfx emulation needs output=direct3d11");
		return false;
	}
	vd11.dev = OUTPUT_DIRECT3D11_Get()->GetDevice();
	vd11.dev->GetImmediateContext(&vd11.ctx);
	if (vd11.dev->GetFeatureLevel() < D3D_FEATURE_LEVEL_10_0) {
		LOG_MSG("VOODOO: Direct3D 11: needs feature level 10.0 (shader model 4)");
		vd11_destroy_all();
		return false;
	}
	vd11.max_size = vd11.dev->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0 ? 16384 : 8192;
	if (!vd11_create_device_objects()) {
		vd11_destroy_all();
		return false;
	}
	Voodoo_HW_PollResolutionSetting();
	if (!vd11_reset_videomode()) {
		vd11_destroy_all();
		return false;
	}
	LOG_MSG("VOODOO: Direct3D 11: rendering 3dfx graphics with Direct3D 11");
	return true;
}

void voodoo_d3d11_device_lost(void) {
	if (vd11.dev == NULL) return;
	vd11_destroy_all();
	vd11.lost = true;
}

void voodoo_d3d11_leave(bool leavemode) {
	if (leavemode) vd11.lost = false;
	if (vd11.dev == NULL) return;
	if (leavemode) {
		LOG_MSG("VOODOO: Direct3D 11: quit");
		vd11_destroy_all();
	} else {
		vd11_destroy_textures();
	}
}

void voodoo_d3d11_shutdown(voodoo_state *vs) {
	(void)vs;
	voodoo_d3d11_leave(true);
}

void voodoo_d3d11_set_window(voodoo_state *vs) {
	/* the projection is worked out for each triangle */
	(void)vs;
}

void voodoo_d3d11_clip_window(voodoo_state *vs) {
	(void)vs;
}

void voodoo_d3d11_vblank_flush(void) {
}

/* show the front buffer: copy it to the texture the output draws, and have the output draw it */
void voodoo_d3d11_present(void) {
	if (!v->active) return;
	CDirect3D11 *out = OUTPUT_DIRECT3D11_Get();
	if (vd11.dev == NULL) {
		/* the output was changed to another one and back: start over with its new device (what
		 * was drawn is gone) */
		if (vd11.lost && OUTPUT_DIRECT3D11_Active()) {
			vd11.lost = false;
			voodoo_d3d11_init(v);
		}
		return;
	}
	if (!vd11_usable()) return;
	vd11_check_render_size();
	if (!vd11.ready) return;
	vd11_flush();
	vd11.ctx->CopyResource(vd11.present.tex, vd11.color[vd11.front].tex);
	out->SetExternalTexture(vd11.present.srv);
	out->PresentExternal();
}

void voodoo_d3d11_swap_buffer(void) {
	if (!vd11_usable()) return;
	/* the back buffer becomes the front buffer (it is shown at the next vertical retrace) */
	vd11_flush();
	vd11.front ^= 1;
}

/* the 3dfx video mode changed: the buffers are resized right away (voodoo_d3d11_follow_dimensions),
 * this tells the output, which changes the window etc. */
void voodoo_d3d11_update_dimensions(void) {
	if (vd11.dev == NULL) return;
	if (vd11.width == 0) {
		vd11_reset_videomode();
		return;
	}
	vd11_flush();
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
	if (!vd11_usable()) return;
	vd11_check_render_size();
	if (!vd11.ready) return;
	OUTPUT_DIRECT3D11_Get()->SetExternalTexture(vd11.present.srv);
	LOG_MSG("VOODOO: Direct3D 11: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, vd11.width, vd11.height);
}

/* the 3dfx video mode changed while active: anything drawn from now on is drawn at the new size */
void voodoo_d3d11_follow_dimensions(void) {
	if (!vd11_usable()) return;
	vd11_check_render_size();
}

/* both buffers black, depth 1.0 */
void voodoo_d3d11_clear(void) {
	if (!vd11_usable()) return;
	vd11_flush();
	const float black[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	for (int i = 0; i < 2; i++) vd11.ctx->ClearRenderTargetView(vd11.color[i].rtv, black);
	vd11.ctx->ClearDepthStencilView(vd11.dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	vd11_invalidate_reads();
}

/* the 3dfx card was drawn to in software until the emulation became active (with the
 * buffers in its memory), so start from what it has there instead of black. Scaled to the
 * render resolution here, with the pixel spans LFB reads and writes use (so they agree). */
void voodoo_d3d11_load_framebuffers_from_ram(void) {
	if (!vd11_usable()) return;
	const UINT32 fw = v->fbi.width, fh = v->fbi.height;
	if (fw == 0 || fh == 0) return;
	vd11_flush();

	vector<UINT32> col(vd11.width), row(vd11.height);
	for (UINT32 i = 0; i < fw; i++) {
		int p0, p1;
		vd11_lfb_span((int)i, vd11.width, fw, p0, p1);
		for (int p = p0; p < p1; p++) col[(size_t)p] = i;
	}
	for (UINT32 i = 0; i < fh; i++) {
		int p0, p1;
		vd11_lfb_span((int)i, vd11.height, fh, p0, p1);
		for (int p = p0; p < p1; p++) row[(size_t)p] = i;
	}

	vector<UINT32> pixels((size_t)vd11.width * vd11.height);
	for (int b = 0; b < 2; b++) {
		const UINT32 offs = v->fbi.rgboffs[b == 0 ? v->fbi.frontbuf : v->fbi.backbuf];
		if (offs == (UINT32)(~0) || offs > v->fbi.mask) continue;
		const UINT16 *src = (const UINT16 *)(v->fbi.ram + offs);
		const UINT32 srcmax = (v->fbi.mask + 1 - offs) / 2;
		/* RGB 565 to RGBA (bytes R, G, B, A), alpha 0, top row first in both */
		for (UINT32 y = 0; y < vd11.height; y++) {
			UINT32 *d = &pixels[(size_t)y * vd11.width];
			for (UINT32 x = 0; x < vd11.width; x++) {
				const UINT32 i = row[y] * v->fbi.rowpixels + col[x];
				const UINT32 p = (i < srcmax) ? src[i] : 0;
				const UINT32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, bl = p & 0x1f;
				d[x] = ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) | (((bl << 3) | (bl >> 2)) << 16);
			}
		}
		vd11.ctx->UpdateSubresource(vd11.color[vd11_target(b == 0)].tex, 0, NULL, pixels.data(), vd11.width * 4, 0);
	}
	vd11_invalidate_reads();
}

void voodoo_d3d11_fastfill(void) {
	if (!vd11_usable()) return;

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

	const bool rgb = FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u) != 0;
	const bool aux = FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0);
	if (!rgb && !aux) return;
	int target = vd11.front ^ 1;
	if (rgb && FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0) {
		target = vd11.front;
		v->fbi.vblank_flush_pending = true;
	}

	/* a rectangle, written as it is (no depth test or blending) */
	VD11DrawState st = vd11_default_state(target, 1);
	st.blend.mask = rgb ? D3D11_COLOR_WRITE_ENABLE_ALL : 0;
	st.depth.write = aux ? 1 : 0;
	const float x0 = (float)((UINT32)sx * vd11.width / (UINT32)fw), x1 = (float)((UINT32)ex * vd11.width / (UINT32)fw);
	const float y0 = (float)((UINT32)sy * vd11.height / (UINT32)fh), y1 = (float)((UINT32)ey * vd11.height / (UINT32)fh);
	if (x1 <= x0 || y1 <= y0) return;
	VD11Color c;
	c.r = v->reg[color1].rgb.r / 255.0f;
	c.g = v->reg[color1].rgb.g / 255.0f;
	c.b = v->reg[color1].rgb.b / 255.0f;
	c.a = v->reg[color1].rgb.a / 255.0f;
	vd11_rect(st, x0, y0, x1, y1, (float)((UINT16)v->reg[zaColor].u) / 65535.0f, c);
}

void voodoo_d3d11_texture_clear(UINT32 address, int TMU) {
	if (vd11_textures[TMU].empty()) return;
	const UINT32 mask = v->tmu[TMU].mask;
	UINT32 first = address & mask;
	UINT32 last = (first + 3) & mask;
	/* a wrapped dword conservatively dirties the whole circular address space */
	if (last < first) {
		first = 0;
		last = mask;
	}
	VD11DirtyRange &dirty = vd11_texture_dirty[TMU];
	if (!dirty.pending) {
		dirty.first = first;
		dirty.last = last;
		dirty.pending = true;
	} else {
		if (first < dirty.first) dirty.first = first;
		if (last > dirty.last) dirty.last = last;
	}
}

void voodoo_d3d11_invalidate_paltex(void) {
	for (int j = 0; j < 2; j++)
		for (map<const UINT32, VD11TexMap>::iterator t = vd11_textures[j].begin(); t != vd11_textures[j].end(); ++t)
			if (vd11_palette_format(t->second.format)) t->second.valid_pal = false;
}

/* an LFB pixel (x, y) in the 3dfx video mode, row 0 at the top: the render resolution pixels it covers,
 * a point if it is just one, otherwise a rectangle */
static void vd11_draw_lfb(VD11DrawState st, int x, int y, VD11Color c, float z) {
	int x0, x1, y0, y1;
	vd11_lfb_span(x, vd11.width, v->fbi.width, x0, x1);
	vd11_lfb_span(y, vd11.height, v->fbi.height, y0, y1);
	if (x1 - x0 == 1 && y1 - y0 == 1) {
		VD11Vertex p;
		vd11_vertex(p, (float)x0 + 0.5f, (float)y0 + 0.5f, z, c);
		st.kind = 2;
		vd11_draw(st, &p, 1);
		return;
	}
	st.kind = 1;
	vd11_rect(st, (float)x0, (float)y0, (float)x1, (float)y1, z, c);
}

static VD11Color vd11_color(int r, int g, int b, int a) {
	VD11Color c;
	c.r = (r & 0xff) / 255.0f;
	c.g = (g & 0xff) / 255.0f;
	c.b = (b & 0xff) / 255.0f;
	c.a = (a & 0xff) / 255.0f;
	return c;
}

void voodoo_d3d11_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	if (!vd11_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD11DrawState st = vd11_default_state(vd11_target(front), 1);
	st.blend.mask = vd11_color_mask(has_rgb, has_alpha);
	vd11_draw_lfb(st, x, y, vd11_color(r, g, b, a), 0.0f);
}

void voodoo_d3d11_draw_z(int x, int y, int z) {
	if (!vd11_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD11DrawState st = vd11_default_state(vd11_target(front), 1);
	st.blend.mask = 0;
	st.depth.write = 1;
	vd11_draw_lfb(st, x, y, vd11_color(0, 0, 0, 0), (float)(z & 0xffff) / 65535.0f);
}

void voodoo_d3d11_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
	if (!vd11_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VD11DrawState st = vd11_default_state(vd11_target(front), 1);
	st.blend.mask = vd11_color_mask(true, FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0);
	vd11_blend_from(st, v->reg[alphaMode].u);
	vd11_draw_lfb(st, x, y, vd11_color(r, g, b, 255), 0.0f);
}

/* read a buffer back (0 and 1: color, 2: depth), kept until it is drawn to again */
static bool vd11_read_buffer(int which) {
	if (vd11.cache_valid[which]) return true;
	vd11_flush();
	vector<UINT32> &c = vd11.cache[which];
	c.resize((size_t)vd11.width * vd11.height);

	D3D11_TEXTURE2D_DESC d;
	memset(&d, 0, sizeof(d));
	d.Width = vd11.width;
	d.Height = vd11.height;
	d.MipLevels = 1;
	d.ArraySize = 1;
	d.SampleDesc.Count = 1;
	d.Usage = D3D11_USAGE_STAGING;
	d.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	ID3D11Texture2D *&staging = which < 2 ? vd11.staging_color : vd11.staging_depth;
	if (staging == NULL) {
		d.Format = which < 2 ? DXGI_FORMAT_R8G8B8A8_UNORM : DXGI_FORMAT_R24G8_TYPELESS;
		if (!vd11_check(vd11.dev->CreateTexture2D(&d, NULL, &staging), "CreateTexture2D")) return false;
	}
	vd11.ctx->CopyResource(staging, which < 2 ? (ID3D11Resource*)vd11.color[which].tex : (ID3D11Resource*)vd11.depth);
	D3D11_MAPPED_SUBRESOURCE m;
	if (!vd11_check(vd11.ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m), "Map")) return false;
	for (UINT32 y = 0; y < vd11.height; y++) {
		const UINT32 *s = (const UINT32 *)((const uint8_t*)m.pData + (size_t)y * m.RowPitch);
		UINT32 *o = &c[(size_t)y * vd11.width];
		if (which < 2) {
			/* RGBA bytes to 0xAARRGGBB */
			for (UINT32 x = 0; x < vd11.width; x++)
				o[x] = (s[x] & 0xff00ff00u) | ((s[x] & 0xffu) << 16) | ((s[x] >> 16) & 0xffu);
		} else {
			/* 24 bit depth in the low bits, to 16 bits */
			for (UINT32 x = 0; x < vd11.width; x++)
				o[x] = (UINT32)(((double)(s[x] & 0xffffffu) / 16777215.0) * 65535.0 + 0.5);
		}
	}
	vd11.ctx->Unmap(staging, 0);
	vd11.cache_valid[which] = true;
	return true;
}

/* two pixels from (x, y) in the 3dfx video mode, row 0 at the top. With a larger render
 * resolution, the render pixel in the middle of each one is read (as OpenGL). */
UINT32 voodoo_d3d11_read_pixel(int x, int y) {
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;
	if (!vd11_usable()) return 0;

	int rx[2], ry, p0, p1;
	for (int i = 0; i < 2; i++) {
		vd11_lfb_span(x + i, vd11.width, v->fbi.width, p0, p1);
		rx[i] = (p0 + p1 - 1) / 2;
	}
	vd11_lfb_span(y, vd11.height, v->fbi.height, p0, p1);
	ry = (p0 + p1 - 1) / 2;

	int which;
	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0: which = vd11.front; break;		/* front buffer */
		case 1: which = vd11.front ^ 1; break;		/* back buffer */
		case 2: which = 2; break;			/* aux buffer */
		default:
			E_Exit("read from invalid buf %x", LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			return 0;
	}
	if (!vd11_read_buffer(which)) return 0;
	const UINT32 *line = &vd11.cache[which][(size_t)ry * vd11.width];
	const UINT32 d0 = line[rx[0]], d1 = line[rx[1]];
	if (which == 2) return (d0 & 0xffff) | (d1 << 16);

	/* ARGB to RGB 565 */
	#define VD11_565(p) ((((p) >> 19) & 0x1f) << 11 | (((p) >> 10) & 0x3f) << 5 | (((p) >> 3) & 0x1f))
	return VD11_565(d0) | (VD11_565(d1) << 16);
	#undef VD11_565
}

/* the pixel shader for a rasterizer's settings */
static ID3D11PixelShader *vd11_pixel_shader(const poly_extra_data *extra, bool tmu0, bool tmu1) {
	VD11PsKey key;
	memset(&key, 0, sizeof(key));
	key.texmode[0] = tmu0 ? (UINT32)extra->r_textureMode0 : 0;
	key.texmode[1] = tmu1 ? (UINT32)extra->r_textureMode1 : 0;
	key.fbzcp = v->reg[fbzColorPath].u;
	key.alphamode = v->reg[alphaMode].u & 0x0f;	/* alpha test and function */
	key.fogmode = FOGMODE_ENABLE_FOG(v->reg[fogMode].u) ? (v->reg[fogMode].u & 0x3f) : 0;
	key.flags = (tmu0 ? VD11_PS_TMU0 : 0) | (tmu1 ? VD11_PS_TMU1 : 0) |
		(FBZMODE_ENABLE_CHROMAKEY(v->reg[fbzMode].u) ? VD11_PS_CHROMAKEY : 0);
	map<VD11PsKey, ID3D11PixelShader*>::iterator i = vd11.shaders.find(key);
	if (i != vd11.shaders.end()) return i->second;
	ID3D11PixelShader *shader = vd11_create_ps(vd11_triangle_ps_source(key).c_str());
	vd11.shaders[key] = shader;
	return shader;
}

void voodoo_d3d11_draw_triangle(poly_extra_data *extra) {
	v = extra->state;
	if (!vd11_usable()) return;

	const UINT32 ALPHAMODE = extra->r_alphaMode;
	const UINT32 FBZMODE = extra->r_fbzMode;

	ogl_vertex_data vd[3];
	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);

	/* the textures */
	ID3D11ShaderResourceView *tex[2] = { NULL, NULL };
	bool use[2] = { false, false };
	for (UINT32 j = 0; j < 2; j++) {
		if (j >= extra->texcount || v->tmu[j].ram == NULL || v->tmu[j].lodmin >= (8 << 8)) continue;
		tex[j] = vd11_cache_texture(extra, j);
		use[j] = tex[j] != NULL;
	}
	ID3D11PixelShader *shader = vd11_pixel_shader(extra, use[0], use[1]);
	if (shader == NULL) return;

	const bool front = FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0;
	const int target = vd11_target(front);
	if (front) v->fbi.vblank_flush_pending = true;

	VD11DrawState st = vd11_default_state(target, 0);
	st.shader = shader;
	for (int j = 0; j < 2; j++) {
		const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
		st.texture[j] = tex[j];
		if (!use[j]) continue;
		st.sampler[j] = (TEXMODE_MINIFICATION_FILTER(TEXMODE) ? VD11_SAMPLER_MIN_LINEAR : 0u) |
			(TEXMODE_MAGNIFICATION_FILTER(TEXMODE) ? VD11_SAMPLER_MAG_LINEAR : 0u) |
			(v->tmu[j].lodmin != v->tmu[j].lodmax ? VD11_SAMPLER_MIPMAPS : 0u) |
			(TEXMODE_TRILINEAR(TEXMODE) ? VD11_SAMPLER_TRILINEAR : 0u) |
			(TEXMODE_CLAMP_S(TEXMODE) ? VD11_SAMPLER_CLAMP_S : 0u) |
			(TEXMODE_CLAMP_T(TEXMODE) ? VD11_SAMPLER_CLAMP_T : 0u);
	}
	vd11_constants(st.constants);

	/* vertices: in render resolution pixels, the Y origin at the top or bottom as the OpenGL
	 * emulation's projection */
	const bool y_bottom = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u) != 0;
	VD11Vertex vert[3];
	for (int i = 0; i < 3; i++) {
		VD11Vertex &o = vert[i];
		memset(&o, 0, sizeof(o));
		const float x = vd[i].x * (float)vd11.width / (float)v->fbi.width;
		const float y = vd[i].y * (float)vd11.height / (float)v->fbi.height;
		o.pos[0] = x;
		o.pos[1] = y_bottom ? (float)vd11.height - y : y;
		o.pos[2] = vd[i].d;
		o.pos[3] = 1.0f;
		/* (OpenGL clamps the colors of the fixed function vertex shader) */
		const float c[4] = { vd[i].r, vd[i].g, vd[i].b, vd[i].a };
		for (int k = 0; k < 4; k++) o.color[k] = c[k] < 0.0f ? 0.0f : (c[k] > 1.0f ? 1.0f : c[k]);
		float *t[2] = { o.tex0, o.tex1 };
		for (int k = 0; k < 2; k++) {
			t[k][0] = use[k] ? vd[i].m[k].sw : 0.0f;
			t[k][1] = use[k] ? vd[i].m[k].tw : 0.0f;
			t[k][3] = use[k] ? vd[i].m[k].w : 1.0f;
		}
		o.extra[0] = vd[i].fogblend;
		o.extra[1] = use[0] ? vd[i].m[0].lodblend : 0.0f;
		o.extra[2] = use[1] ? vd[i].m[1].lodblend : 0.0f;
	}

	/* depth source compare: the stencil marks where the triangle passes the depth test
	 * against zaColor, and it is drawn there */
	const bool stencil_pass = FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) != 0;
	if (stencil_pass) {
		vd11_flush();
		vd11.ctx->ClearDepthStencilView(vd11.dsv, D3D11_CLEAR_STENCIL, 1.0f, 0);
		VD11DrawState sst = vd11_default_state(target, 1);
		sst.depth.test = (FBZMODE_ENABLE_DEPTHBUF(FBZMODE) && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? 1 : 0;
		sst.depth.func = (UINT32)(D3D11_COMPARISON_NEVER + FBZMODE_DEPTH_FUNCTION(FBZMODE));
		sst.depth.stencil = VD11_STENCIL_WRITE;
		sst.blend.mask = 0;
		VD11Vertex flat[3];
		const float depth = (float)(v->reg[zaColor].u & 0xffff) / (float)0xffff;
		for (int i = 0; i < 3; i++) {
			flat[i] = vert[i];
			flat[i].pos[2] = depth;
		}
		vd11_draw(sst, flat, 3);
	}

	if (stencil_pass) {
		st.depth.stencil = VD11_STENCIL_TEST;
		st.depth.test = FBZMODE_ENABLE_DEPTHBUF(FBZMODE) ? 1 : 0;
		st.depth.func = D3D11_COMPARISON_ALWAYS;
	} else if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			st.depth.test = 1;
			st.depth.func = (UINT32)(D3D11_COMPARISON_NEVER + FBZMODE_DEPTH_FUNCTION(FBZMODE));
		} else if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
			st.depth.test = 1;
			st.depth.func = D3D11_COMPARISON_ALWAYS;
		}
	} else {
		st.depth.test = 1;
		st.depth.func = D3D11_COMPARISON_ALWAYS;
	}
	const bool aux = FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0;
	st.depth.write = (aux && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? 1 : 0;
	st.blend.mask = vd11_color_mask(FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0, aux);
	vd11_blend_from(st, ALPHAMODE);
	vd11_draw(st, vert, 3);
}

#else

/* no Direct3D 11 output */

bool voodoo_d3d11_init(voodoo_state *vs) { (void)vs; return false; }
void voodoo_d3d11_leave(bool leavemode) { (void)leavemode; }
void voodoo_d3d11_shutdown(voodoo_state *vs) { (void)vs; }
void voodoo_d3d11_set_window(voodoo_state *vs) { (void)vs; }
void voodoo_d3d11_present(void) { }
void voodoo_d3d11_load_framebuffers_from_ram(void) { }
void voodoo_d3d11_follow_dimensions(void) { }
void voodoo_d3d11_swap_buffer(void) { }
void voodoo_d3d11_vblank_flush(void) { }
void voodoo_d3d11_update_dimensions(void) { }
void voodoo_d3d11_clear(void) { }
void voodoo_d3d11_fastfill(void) { }
void voodoo_d3d11_clip_window(voodoo_state *vs) { (void)vs; }
void voodoo_d3d11_texture_clear(UINT32 address, int TMU) { (void)address; (void)TMU; }
void voodoo_d3d11_invalidate_paltex(void) { }
void voodoo_d3d11_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	(void)x; (void)y; (void)has_rgb; (void)has_alpha; (void)r; (void)g; (void)b; (void)a;
}
void voodoo_d3d11_draw_z(int x, int y, int z) { (void)x; (void)y; (void)z; }
void voodoo_d3d11_draw_pixel_pipeline(int x, int y, int r, int g, int b) { (void)x; (void)y; (void)r; (void)g; (void)b; }
UINT32 voodoo_d3d11_read_pixel(int x, int y) { (void)x; (void)y; return 0; }
void voodoo_d3d11_draw_triangle(poly_extra_data *extra) { (void)extra; }

#endif
