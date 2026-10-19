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

/* The 3dfx emulation rendered with Metal, on the Metal output's device and command queue (it only
 * works with output=metal). It does what the Direct3D 11 one (voodoo_d3d11.cpp) does, the same way
 * where it can:
 *
 *  - It renders into textures of its own at the render resolution (voodoo_resolution): front and
 *    back color buffers and a depth/stencil buffer, top row first. At every vertical retrace the
 *    front buffer is copied to another texture, which the output draws in place of the emulated
 *    video card's frame.
 *  - Triangles use a fragment shader made for the rasterizer's register settings: the GLSL the
 *    OpenGL emulation makes, written in the Metal shading language instead, compiled when first
 *    needed. Blending and masks are in pipeline states (one per shader and blend setting), depth
 *    and stencil in depth/stencil states, all kept for reuse.
 *  - Textures are converted on the CPU like the OpenGL emulation does, mipmaps too.
 *  - LFB writes are drawn as points or small rectangles, LFB reads read the buffers back (kept until
 *    the buffer is drawn to again).
 *  - Commands go to the output's queue, so the GPU runs them in the order they are committed: what
 *    is drawn here comes before the output draws it. The output draws on the same thread.
 *
 * This file manages its Objective-C objects itself (it is built without ARC, like the output). */

#include "config.h"

#if defined(MACOSX) && defined(C_SDL2) && C_METAL

#if __has_feature(objc_arc)
#error "voodoo_metal.mm retains and releases its Objective-C objects itself: build it without ARC"
#endif

#import <Metal/Metal.h>
#include <output/output_metal.h>

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
#include "voodoo_metal.h"

/* the GPU is done with a command buffer (before voodoo_def.h, which has a register called status) */
static bool vmtl_cmd_done(id<MTLCommandBuffer> cmd) {
	const MTLCommandBufferStatus st = [cmd status];
	return st == MTLCommandBufferStatusCompleted || st == MTLCommandBufferStatusError;
}

#include "voodoo_def.h"

using namespace std;

bool OUTPUT_Metal_Active(void);
CMetal* OUTPUT_Metal_Get(void);

/*************************************
 *  Shaders
 *************************************/

/* the vertices: positions in render resolution pixels (row 0 at the top), the iterated color,
 * texture coordinates (s*w, t*w, 0, w) of TMU 0 and 1, fog blend and the LOD blends of TMU 0 and 1 */
struct VMtlColor {
	float r, g, b, a;
};

struct VMtlVertex {
	float pos[4];
	float color[4];
	float tex0[4];
	float tex1[4];
	float extra[4];
};

/* every library has the vertex function, with the fragment functions it is used with */
static const char *vmtl_common_source =
	"#include <metal_stdlib>\n"
	"using namespace metal;\n"
	"struct VSIn { float4 pos; float4 col; float4 t0; float4 t1; float4 ex; };\n"
	"struct PSIn {\n"
	"  float4 pos [[position]];\n"
	"  float4 col [[user(col)]]; float4 t0 [[user(t0)]]; float4 t1 [[user(t1)]]; float4 ex [[user(ex)]];\n"
	"};\n"
	"struct VSOut {\n"
	"  float4 pos [[position]];\n"
	"  float4 col [[user(col)]]; float4 t0 [[user(t0)]]; float4 t1 [[user(t1)]]; float4 ex [[user(ex)]];\n"
	"  float psize [[point_size]];\n"
	"};\n"
	"vertex VSOut vs_main(uint vid [[vertex_id]], const device VSIn *vin [[buffer(0)]],\n"
	"                     constant float4 &scale [[buffer(1)]]) {\n"
	"  VSIn i = vin[vid];\n"
	"  VSOut o;\n"
	"  o.pos = float4(i.pos.x * scale.x - 1.0, 1.0 - i.pos.y * scale.y, i.pos.z, 1.0);\n"
	"  o.col = i.col; o.t0 = i.t0; o.t1 = i.t1; o.ex = i.ex;\n"
	"  o.psize = 1.0;\n"
	"  return o;\n"
	"}\n";

/* LFB writes and fills: the color of the vertices. A texture copied (scaled) to another. */
static const char *vmtl_builtin_source =
	"fragment float4 flat_main(PSIn i [[stage_in]]) { return i.col; }\n"
	"fragment float4 copy_main(PSIn i [[stage_in]], texture2d<float> tex0 [[texture(0)]],\n"
	"                          sampler smp0 [[sampler(0)]]) { return tex0.sample(smp0, i.t0.xy); }\n";

/* what a triangle fragment shader depends on, as the Vulkan emulation's VvkFsKey */
struct VMtlPsKey {
	UINT32 texmode[2];	/* 0 if that TMU is not used */
	UINT32 fbzcp;
	UINT32 alphamode;	/* alpha test bits */
	UINT32 fogmode;		/* 0 without fog */
	UINT32 flags;
	bool operator<(const VMtlPsKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};
#define VMTL_PS_TMU0		0x01
#define VMTL_PS_TMU1		0x02
#define VMTL_PS_CHROMAKEY	0x04

/* the triangle fragment shader constants, like the OpenGL emulation's uniforms */
struct VMtlConstants {
	float c0[4];		/* color0 */
	float c1[4];		/* color1 */
	float chromakey[4];
	float fogcolor[4];
	float misc[4];		/* alpha reference, zaColor */
};

/* ogl_sh_tex_combine(), in the Metal shading language */
static void vmtl_sh_tex_combine(string &s, int TMU, UINT32 TEXMODE) {
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

/* ogl_sh_color_path(), in the Metal shading language */
static void vmtl_sh_color_path(string &s, const VMtlPsKey &k) {
	const UINT32 FBZCOLORPATH = k.fbzcp;
	const UINT32 ALPHAMODE = k.alphamode;

	switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH)) {
		case 0: s += "  cother = i.col;\n"; break;
		case 1: s += "  cother = texel;\n"; break;
		case 2: s += "  cother = color1;\n"; break;
		default: s += "  cother = 0.0;\n"; break;
	}

	if (k.flags & VMTL_PS_CHROMAKEY)
		s += "  if (distance(cother.rgb, chromaKey.rgb) < 0.0001) discard_fragment();\n";

	switch (FBZCP_CC_ASELECT(FBZCOLORPATH)) {
		case 0: s += "  cother.a = i.col.a;\n"; break;
		case 1: s += "  cother.a = texel.a;\n"; break;
		case 2: s += "  cother.a = color1.a;\n"; break;
		default: s += "  cother.a = 0.0;\n"; break;
	}

	if (ALPHAMODE_ALPHATEST(ALPHAMODE)) {
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
			case 0: s += "  discard_fragment();\n"; break;
			case 1: s += "  if (cother.a >= misc.x) discard_fragment();\n"; break;
			case 2: s += "  if (abs(cother.a - misc.x) > 0.0001) discard_fragment();\n"; break;
			case 3: s += "  if (cother.a > misc.x) discard_fragment();\n"; break;
			case 4: s += "  if (cother.a <= misc.x) discard_fragment();\n"; break;
			case 5: s += "  if (abs(cother.a - misc.x) < 0.0001) discard_fragment();\n"; break;
			case 6: s += "  if (cother.a < misc.x) discard_fragment();\n"; break;
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

/* ogl_sh_fog(), in the Metal shading language */
static void vmtl_sh_fog(string &s, UINT32 FOGMODE) {
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

/* ogl_shaders(): the fragment shader, in the Metal shading language (only the textures it uses
 * are arguments) */
static string vmtl_triangle_ps_source(const VMtlPsKey &k) {
	string s = vmtl_common_source;
	s +=
		"struct PSConstants { float4 color0; float4 color1; float4 chromaKey; float4 fogColor; float4 misc; };\n"
		"fragment float4 ps_main(PSIn i [[stage_in]], constant PSConstants &k [[buffer(0)]]";
	if (k.flags & VMTL_PS_TMU0) s += ",\n  texture2d<float> tex0 [[texture(0)]], sampler smp0 [[sampler(0)]]";
	if (k.flags & VMTL_PS_TMU1) s += ",\n  texture2d<float> tex1 [[texture(1)]], sampler smp1 [[sampler(1)]]";
	s +=
		") {\n"
		"  const float4 color0 = k.color0, color1 = k.color1, chromaKey = k.chromaKey;\n"
		"  const float4 fogColor = k.fogColor, misc = k.misc;\n"
		"  float4 pixel = 0.0;\n"
		"  float4 texel = 1.0;\n"
		"  float4 clocal = 1.0;\n"
		"  float4 cother = 0.0;\n"
		"  float4 tt = 0.0;\n"
		"  float4 blend = 0.0;\n";

	if (k.flags & VMTL_PS_TMU1) {
		s += "  clocal = tex1.sample(smp1, i.t1.xy / i.t1.w);\n";
		vmtl_sh_tex_combine(s, 1, k.texmode[1]);
		s += "  cother = clocal;\n";
		s += "  texel = clocal;\n";
	}
	if (k.flags & VMTL_PS_TMU0) {
		s += "  clocal = tex0.sample(smp0, i.t0.xy / i.t0.w);\n";
		vmtl_sh_tex_combine(s, 0, k.texmode[0]);
		s += "  texel = clocal;\n";
	}

	vmtl_sh_color_path(s, k);
	if (FOGMODE_ENABLE_FOG(k.fogmode)) vmtl_sh_fog(s, k.fogmode);
	s += "  return pixel;\n}\n";
	return s;
}

/*************************************
 *  State
 *************************************/

#define VMTL_COLOR_FORMAT	MTLPixelFormatRGBA8Unorm
#define VMTL_DEPTH_FORMAT	MTLPixelFormatDepth32Float_Stencil8

#define VMTL_STENCIL_WRITE	1
#define VMTL_STENCIL_TEST	2
#define VMTL_STENCIL_CLEAR	3

#define VMTL_SAMPLER_MIN_LINEAR		0x01
#define VMTL_SAMPLER_MAG_LINEAR		0x02
#define VMTL_SAMPLER_MIPMAPS		0x04
#define VMTL_SAMPLER_TRILINEAR		0x08
#define VMTL_SAMPLER_CLAMP_S		0x10
#define VMTL_SAMPLER_CLAMP_T		0x20

/* render pass clears */
#define VMTL_CLEAR_COLOR		0x01
#define VMTL_CLEAR_DEPTH		0x02
#define VMTL_CLEAR_STENCIL		0x04

struct VMtlBlendKey {
	UINT32 blend, src, dst, src_alpha, dst_alpha, mask;	/* MTLBlendFactor, MTLColorWriteMask */
	bool operator<(const VMtlBlendKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};

struct VMtlDepthKey {
	UINT32 test, func, write, stencil;			/* func: MTLCompareFunction */
	bool operator<(const VMtlDepthKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};

/* a library's vertex and fragment functions, and the pipeline states made with them (one for
 * each blend setting) */
struct VMtlShader {
	id<MTLLibrary> lib;
	id<MTLFunction> vs, fs;
	map<VMtlBlendKey, id<MTLRenderPipelineState> > pipelines;
};

/* all the state of a draw: draws with the same state are drawn together */
struct VMtlDrawState {
	int target;
	int kind;			/* 0: triangles with a fragment shader, 1: flat triangles, 2: flat points */
	VMtlShader *shader;
	id<MTLTexture> texture[2];
	UINT32 sampler[2];		/* VMTL_SAMPLER_... */
	VMtlBlendKey blend;
	VMtlDepthKey depth;
	VMtlConstants constants;
};

/* a texture, like the OpenGL emulation's ogl_texmap */
struct VMtlTexMap {
	bool valid_pal;
	UINT32 format, width, height, ilod, lodmask, ncc_table;
	id<MTLTexture> current;
	map<UINT32, id<MTLTexture> > by_palette;	/* palette formats: one per palette */
};

struct VMtlDirtyRange {
	bool pending = false;
	UINT32 first = 0, last = 0;
};

/* vertices are put in buffers of this size, which are used again once the commands that use
 * them are done */
#define VMTL_CHUNK_BYTES	(4u << 20)
#define VMTL_MAX_VERTICES	32768		/* drawn at once (less than a chunk) */

struct VMtlInFlight {
	id<MTLCommandBuffer> cmd;
	vector<id<MTLBuffer> > chunks;
};

static struct {
	bool ready;		/* the buffers exist */
	bool lost;		/* the output destroyed its device (start over with its next one) */
	id<MTLDevice> dev;
	id<MTLCommandQueue> queue;	/* the output's */
	UINT32 max_size;

	/* the 3dfx card's buffers */
	id<MTLTexture> color[2];
	id<MTLTexture> present;
	id<MTLTexture> depth;
	UINT32 width, height;
	int front;

	/* shaders, state objects */
	VMtlShader *flat, *copy;
	map<VMtlPsKey, VMtlShader*> shaders;
	map<VMtlDepthKey, id<MTLDepthStencilState> > depth_states;
	map<UINT32, id<MTLSamplerState> > samplers;

	/* the commands being put together, and the render pass they are in */
	id<MTLCommandBuffer> cmd;
	id<MTLRenderCommandEncoder> enc;
	id<MTLTexture> enc_target;

	/* vertex memory: the chunk being filled, the ones the commands use, the ones the GPU may
	 * still read, the ones free to fill */
	id<MTLBuffer> chunk;
	UINT32 chunk_used;
	vector<id<MTLBuffer> > cmd_chunks;
	vector<VMtlInFlight> in_flight;
	vector<id<MTLBuffer> > free_chunks;

	/* draws not drawn yet */
	VMtlDrawState pending;
	bool have_pending;
	vector<VMtlVertex> vertices;

	/* LFB reads: color buffers 0 and 1, and depth */
	id<MTLBuffer> readback;
	bool cache_valid[3];
	vector<UINT32> cache[3];		/* colors as 0xAARRGGBB, depth as 16 bits */
} vmtl;

static map<const UINT32, VMtlTexMap> vmtl_textures[2];
static VMtlDirtyRange vmtl_texture_dirty[2];

#define VMTL_RELEASE(x) do { if ((x) != nil) { [(x) release]; (x) = nil; } } while (0)

/* the device is there, and is the one the output uses */
static bool vmtl_usable(void) {
	CMetal *out = OUTPUT_Metal_Get();
	return vmtl.ready && vmtl.width != 0 && out != NULL && out->GetDevice() == vmtl.dev;
}

/*************************************
 *  Commands
 *************************************/

/* vertex memory the GPU is done with can be filled again */
static void vmtl_recycle_chunks(void) {
	for (size_t i = 0; i < vmtl.in_flight.size();) {
		VMtlInFlight &f = vmtl.in_flight[i];
		if (vmtl_cmd_done(f.cmd)) {
			vmtl.free_chunks.insert(vmtl.free_chunks.end(), f.chunks.begin(), f.chunks.end());
			[f.cmd release];
			vmtl.in_flight.erase(vmtl.in_flight.begin() + (ptrdiff_t)i);
		} else {
			i++;
		}
	}
}

static id<MTLCommandBuffer> vmtl_cmd(void) {
	if (vmtl.cmd == nil) vmtl.cmd = [[vmtl.queue commandBuffer] retain];
	return vmtl.cmd;
}

static void vmtl_end_pass(void) {
	if (vmtl.enc == nil) return;
	[vmtl.enc endEncoding];
	VMTL_RELEASE(vmtl.enc);
	vmtl.enc_target = nil;
}

/* send the commands so far to the GPU, and wait until it has run them if asked to */
static void vmtl_commit(bool wait) {
	vmtl_end_pass();
	if (vmtl.cmd == nil) return;
	[vmtl.cmd commit];
	if (wait) [vmtl.cmd waitUntilCompleted];
	VMtlInFlight f;
	f.cmd = vmtl.cmd;
	f.chunks.swap(vmtl.cmd_chunks);
	vmtl.in_flight.push_back(f);
	vmtl.cmd = nil;
	/* the next commands fill other vertex memory */
	vmtl.chunk = nil;
	vmtl.chunk_used = 0;
}

/* a render pass drawing into a color buffer and the depth/stencil buffer: the one the commands
 * are in if it is the same and nothing is cleared, otherwise a new one */
static id<MTLRenderCommandEncoder> vmtl_pass(id<MTLTexture> target, unsigned int clear, float clear_alpha) {
	if (vmtl.enc != nil && vmtl.enc_target == target && clear == 0) return vmtl.enc;
	vmtl_end_pass();
	MTLRenderPassDescriptor *p = [MTLRenderPassDescriptor renderPassDescriptor];
	p.colorAttachments[0].texture = target;
	p.colorAttachments[0].loadAction = (clear & VMTL_CLEAR_COLOR) ? MTLLoadActionClear : MTLLoadActionLoad;
	p.colorAttachments[0].clearColor = MTLClearColorMake(0.0, 0.0, 0.0, clear_alpha);
	p.colorAttachments[0].storeAction = MTLStoreActionStore;
	p.depthAttachment.texture = vmtl.depth;
	p.depthAttachment.loadAction = (clear & VMTL_CLEAR_DEPTH) ? MTLLoadActionClear : MTLLoadActionLoad;
	p.depthAttachment.clearDepth = 1.0;
	p.depthAttachment.storeAction = MTLStoreActionStore;
	p.stencilAttachment.texture = vmtl.depth;
	p.stencilAttachment.loadAction = (clear & VMTL_CLEAR_STENCIL) ? MTLLoadActionClear : MTLLoadActionLoad;
	p.stencilAttachment.clearStencil = 0;
	p.stencilAttachment.storeAction = MTLStoreActionStore;
	id<MTLRenderCommandEncoder> enc = [[vmtl_cmd() renderCommandEncoderWithDescriptor:p] retain];
	if (enc == nil) return nil;
	MTLViewport vp = { 0.0, 0.0, (double)[target width], (double)[target height], 0.0, 1.0 };
	[enc setViewport:vp];
	[enc setCullMode:MTLCullModeNone];
	[enc setDepthClipMode:MTLDepthClipModeClamp];
	[enc setStencilReferenceValue:1];
	vmtl.enc = enc;
	vmtl.enc_target = target;
	return enc;
}

/* put vertices in vertex memory: the buffer and offset they are at */
static bool vmtl_upload_vertices(const VMtlVertex *data, UINT32 count, id<MTLBuffer> &buf, NSUInteger &offset) {
	const UINT32 bytes = count * (UINT32)sizeof(VMtlVertex);
	if (bytes > VMTL_CHUNK_BYTES) return false;
	if (vmtl.chunk == nil || vmtl.chunk_used + bytes > VMTL_CHUNK_BYTES) {
		vmtl_recycle_chunks();
		if (!vmtl.free_chunks.empty()) {
			vmtl.chunk = vmtl.free_chunks.back();
			vmtl.free_chunks.pop_back();
		} else {
			vmtl.chunk = [vmtl.dev newBufferWithLength:VMTL_CHUNK_BYTES
				options:MTLResourceStorageModeShared | MTLResourceCPUCacheModeWriteCombined];
			if (vmtl.chunk == nil) {
				LOG_MSG("VOODOO: Metal: out of memory for vertices");
				return false;
			}
		}
		vmtl.cmd_chunks.push_back(vmtl.chunk);
		vmtl.chunk_used = 0;
	}
	memcpy((uint8_t*)[vmtl.chunk contents] + vmtl.chunk_used, data, bytes);
	buf = vmtl.chunk;
	offset = vmtl.chunk_used;
	vmtl.chunk_used += (bytes + 255u) & ~255u;
	return true;
}

/*************************************
 *  State objects
 *************************************/

static UINT32 vmtl_src_factor(UINT32 f) {
	static const MTLBlendFactor t[16] = {
		MTLBlendFactorZero, MTLBlendFactorSourceAlpha, MTLBlendFactorDestinationColor, MTLBlendFactorDestinationAlpha,
		MTLBlendFactorOne, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorOneMinusDestinationColor,
		MTLBlendFactorOneMinusDestinationAlpha, MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero,
		MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorSourceAlphaSaturated };
	return (UINT32)t[f & 15];
}

static UINT32 vmtl_dst_factor(UINT32 f) {
	static const MTLBlendFactor t[16] = {
		MTLBlendFactorZero, MTLBlendFactorSourceAlpha, MTLBlendFactorSourceColor, MTLBlendFactorDestinationAlpha,
		MTLBlendFactorOne, MTLBlendFactorOneMinusSourceAlpha, MTLBlendFactorOneMinusSourceColor,
		MTLBlendFactorOneMinusDestinationAlpha, MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero,
		MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero, MTLBlendFactorZero,
		MTLBlendFactorSourceColor /* A_COLORBEFOREFOG */ };
	return (UINT32)t[f & 15];
}

/* a shader's pipeline state for a blend setting */
static id<MTLRenderPipelineState> vmtl_pipeline(VMtlShader *sh, const VMtlBlendKey &k) {
	map<VMtlBlendKey, id<MTLRenderPipelineState> >::iterator i = sh->pipelines.find(k);
	if (i != sh->pipelines.end()) return i->second;
	MTLRenderPipelineDescriptor *d = [[MTLRenderPipelineDescriptor alloc] init];
	d.vertexFunction = sh->vs;
	d.fragmentFunction = sh->fs;
	MTLRenderPipelineColorAttachmentDescriptor *c = d.colorAttachments[0];
	c.pixelFormat = VMTL_COLOR_FORMAT;
	c.blendingEnabled = k.blend ? YES : NO;
	c.sourceRGBBlendFactor = (MTLBlendFactor)k.src;
	c.destinationRGBBlendFactor = (MTLBlendFactor)k.dst;
	c.rgbBlendOperation = MTLBlendOperationAdd;
	c.sourceAlphaBlendFactor = (MTLBlendFactor)k.src_alpha;
	c.destinationAlphaBlendFactor = (MTLBlendFactor)k.dst_alpha;
	c.alphaBlendOperation = MTLBlendOperationAdd;
	c.writeMask = (MTLColorWriteMask)k.mask;
	d.depthAttachmentPixelFormat = VMTL_DEPTH_FORMAT;
	d.stencilAttachmentPixelFormat = VMTL_DEPTH_FORMAT;
	NSError *err = nil;
	id<MTLRenderPipelineState> st = [vmtl.dev newRenderPipelineStateWithDescriptor:d error:&err];
	[d release];
	if (st == nil)
		LOG_MSG("VOODOO: Metal: pipeline state failed: %s", err != nil ? [[err localizedDescription] UTF8String] : "");
	sh->pipelines[k] = st;
	return st;
}

static id<MTLDepthStencilState> vmtl_depth_state(const VMtlDepthKey &k) {
	map<VMtlDepthKey, id<MTLDepthStencilState> >::iterator i = vmtl.depth_states.find(k);
	if (i != vmtl.depth_states.end()) return i->second;
	MTLDepthStencilDescriptor *d = [[MTLDepthStencilDescriptor alloc] init];
	d.depthCompareFunction = k.test ? (MTLCompareFunction)k.func : MTLCompareFunctionAlways;
	d.depthWriteEnabled = k.write ? YES : NO;
	if (k.stencil != 0) {
		MTLStencilDescriptor *s = [[MTLStencilDescriptor alloc] init];
		s.stencilFailureOperation = MTLStencilOperationKeep;
		s.depthFailureOperation = MTLStencilOperationKeep;
		s.depthStencilPassOperation = k.stencil == VMTL_STENCIL_WRITE ? MTLStencilOperationReplace :
			(k.stencil == VMTL_STENCIL_CLEAR ? MTLStencilOperationZero : MTLStencilOperationKeep);
		s.stencilCompareFunction = k.stencil == VMTL_STENCIL_TEST ? MTLCompareFunctionEqual : MTLCompareFunctionAlways;
		s.readMask = 1;
		s.writeMask = 1;
		d.frontFaceStencil = s;
		d.backFaceStencil = s;
		[s release];
	}
	id<MTLDepthStencilState> st = [vmtl.dev newDepthStencilStateWithDescriptor:d];
	[d release];
	if (st == nil) LOG_MSG("VOODOO: Metal: depth/stencil state failed");
	vmtl.depth_states[k] = st;
	return st;
}

static id<MTLSamplerState> vmtl_sampler(UINT32 k) {
	map<UINT32, id<MTLSamplerState> >::iterator i = vmtl.samplers.find(k);
	if (i != vmtl.samplers.end()) return i->second;
	MTLSamplerDescriptor *d = [[MTLSamplerDescriptor alloc] init];
	d.minFilter = (k & VMTL_SAMPLER_MIN_LINEAR) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	d.magFilter = (k & VMTL_SAMPLER_MAG_LINEAR) ? MTLSamplerMinMagFilterLinear : MTLSamplerMinMagFilterNearest;
	d.mipFilter = !(k & VMTL_SAMPLER_MIPMAPS) ? MTLSamplerMipFilterNotMipmapped :
		((k & VMTL_SAMPLER_TRILINEAR) ? MTLSamplerMipFilterLinear : MTLSamplerMipFilterNearest);
	d.sAddressMode = (k & VMTL_SAMPLER_CLAMP_S) ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
	d.tAddressMode = (k & VMTL_SAMPLER_CLAMP_T) ? MTLSamplerAddressModeClampToEdge : MTLSamplerAddressModeRepeat;
	d.rAddressMode = MTLSamplerAddressModeClampToEdge;
	id<MTLSamplerState> st = [vmtl.dev newSamplerStateWithDescriptor:d];
	[d release];
	if (st == nil) LOG_MSG("VOODOO: Metal: sampler state failed");
	vmtl.samplers[k] = st;
	return st;
}

/* the vertex shader's scale for render targets of w x h pixels */
static void vmtl_set_target_size(id<MTLRenderCommandEncoder> enc, UINT32 w, UINT32 h) {
	const float scale[4] = { 2.0f / (float)w, 2.0f / (float)h, 0.0f, 0.0f };
	[enc setVertexBytes:scale length:sizeof(scale) atIndex:1];
}

/*************************************
 *  Drawing
 *************************************/

static VMtlDrawState vmtl_default_state(int target, int kind) {
	VMtlDrawState st;
	memset(&st, 0, sizeof(st));
	st.target = target;
	st.kind = kind;
	st.blend.src = st.blend.src_alpha = MTLBlendFactorOne;
	st.blend.dst = st.blend.dst_alpha = MTLBlendFactorZero;
	st.blend.mask = MTLColorWriteMaskAll;
	st.depth.func = MTLCompareFunctionAlways;
	return st;
}

/* blending from alphaMode, as VOGL_SetAlphaMode() is called with */
static void vmtl_blend_from(VMtlDrawState &st, UINT32 ALPHAMODE) {
	st.blend.blend = ALPHAMODE_ALPHABLEND(ALPHAMODE) ? 1 : 0;
	if (!st.blend.blend) return;
	st.blend.src = vmtl_src_factor(ALPHAMODE_SRCRGBBLEND(ALPHAMODE));
	st.blend.dst = vmtl_dst_factor(ALPHAMODE_DSTRGBBLEND(ALPHAMODE));
	st.blend.src_alpha = ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4 ? MTLBlendFactorOne : MTLBlendFactorZero;
	st.blend.dst_alpha = ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4 ? MTLBlendFactorOne : MTLBlendFactorZero;
}

static UINT32 vmtl_color_mask(bool rgb, bool alpha) {
	return (rgb ? (MTLColorWriteMaskRed | MTLColorWriteMaskGreen | MTLColorWriteMaskBlue) : 0) |
		(alpha ? MTLColorWriteMaskAlpha : 0);
}

static void vmtl_constants(VMtlConstants &c) {
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
static void vmtl_flush(void) {
	if (!vmtl.have_pending) return;
	vmtl.have_pending = false;
	const UINT32 count = (UINT32)vmtl.vertices.size();
	if (count == 0) return;
	const VMtlDrawState &st = vmtl.pending;

	id<MTLBuffer> buf = nil;
	NSUInteger offset = 0;
	const bool uploaded = vmtl_upload_vertices(vmtl.vertices.data(), count, buf, offset);
	vmtl.vertices.clear();
	if (!uploaded) return;

	id<MTLRenderPipelineState> pipeline = vmtl_pipeline(st.kind == 0 ? st.shader : vmtl.flat, st.blend);
	id<MTLDepthStencilState> depth = vmtl_depth_state(st.depth);
	if (pipeline == nil || depth == nil) return;
	id<MTLRenderCommandEncoder> enc = vmtl_pass(vmtl.color[st.target], 0, 0.0f);
	if (enc == nil) return;
	[enc setRenderPipelineState:pipeline];
	[enc setDepthStencilState:depth];
	[enc setVertexBuffer:buf offset:offset atIndex:0];
	vmtl_set_target_size(enc, vmtl.width, vmtl.height);
	if (st.kind == 0) {
		[enc setFragmentBytes:&st.constants length:sizeof(st.constants) atIndex:0];
		for (int j = 0; j < 2; j++) {
			if (st.texture[j] == nil) continue;
			[enc setFragmentTexture:st.texture[j] atIndex:(NSUInteger)j];
			[enc setFragmentSamplerState:vmtl_sampler(st.sampler[j]) atIndex:(NSUInteger)j];
		}
	}
	[enc drawPrimitives:(st.kind == 2 ? MTLPrimitiveTypePoint : MTLPrimitiveTypeTriangle) vertexStart:0 vertexCount:count];
}

/* queue vertices, drawn with the ones before them if the state is the same */
static void vmtl_draw(const VMtlDrawState &st, const VMtlVertex *data, UINT32 count) {
	if (vmtl.have_pending && memcmp(&vmtl.pending, &st, sizeof(st)) != 0) vmtl_flush();
	if (!vmtl.have_pending) {
		vmtl.pending = st;
		vmtl.have_pending = true;
	}
	vmtl.vertices.insert(vmtl.vertices.end(), data, data + count);
	vmtl.cache_valid[st.target] = false;
	vmtl.cache_valid[2] = false;
	if (vmtl.vertices.size() >= VMTL_MAX_VERTICES - 6) vmtl_flush();
}

static void vmtl_invalidate_reads(void) {
	vmtl.cache_valid[0] = vmtl.cache_valid[1] = vmtl.cache_valid[2] = false;
}

static int vmtl_target(bool front) {
	return front ? vmtl.front : (vmtl.front ^ 1);
}

static void vmtl_vertex(VMtlVertex &o, float x, float y, float z, VMtlColor c) {
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
static void vmtl_rect(const VMtlDrawState &st, float x0, float y0, float x1, float y1, float z, VMtlColor c) {
	VMtlVertex q[6];
	vmtl_vertex(q[0], x0, y0, z, c);
	vmtl_vertex(q[1], x1, y0, z, c);
	vmtl_vertex(q[2], x0, y1, z, c);
	vmtl_vertex(q[3], x1, y0, z, c);
	vmtl_vertex(q[4], x1, y1, z, c);
	vmtl_vertex(q[5], x0, y1, z, c);
	vmtl_draw(st, q, 6);
}

/*************************************
 *  Buffers
 *************************************/

static void vmtl_release_targets(void) {
	vmtl_end_pass();
	if (OUTPUT_Metal_Get() != NULL) OUTPUT_Metal_Get()->SetExternalTexture(nil);
	for (int i = 0; i < 2; i++) VMTL_RELEASE(vmtl.color[i]);
	VMTL_RELEASE(vmtl.present);
	VMTL_RELEASE(vmtl.depth);
	VMTL_RELEASE(vmtl.readback);
	vmtl.width = vmtl.height = 0;
	vmtl_invalidate_reads();
}

static id<MTLTexture> vmtl_create_target(UINT32 w, UINT32 h, MTLPixelFormat format, MTLTextureUsage usage) {
	MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:format width:w height:h mipmapped:NO];
	d.usage = usage;
	d.storageMode = MTLStorageModePrivate;
	id<MTLTexture> t = [vmtl.dev newTextureWithDescriptor:d];
	if (t == nil) LOG_MSG("VOODOO: Metal: could not make a %ux%u buffer", (unsigned int)w, (unsigned int)h);
	return t;
}

/* make the buffers, black (depth 1.0) */
static bool vmtl_create_targets(UINT32 w, UINT32 h) {
	const MTLTextureUsage usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
	for (int i = 0; i < 2; i++)
		if ((vmtl.color[i] = vmtl_create_target(w, h, VMTL_COLOR_FORMAT, usage)) == nil) return false;
	if ((vmtl.present = vmtl_create_target(w, h, VMTL_COLOR_FORMAT, usage)) == nil) return false;
	if ((vmtl.depth = vmtl_create_target(w, h, VMTL_DEPTH_FORMAT, MTLTextureUsageRenderTarget)) == nil) return false;

	vmtl.width = w;
	vmtl.height = h;
	vmtl_pass(vmtl.color[0], VMTL_CLEAR_COLOR | VMTL_CLEAR_DEPTH | VMTL_CLEAR_STENCIL, 0.0f);
	vmtl_pass(vmtl.color[1], VMTL_CLEAR_COLOR, 0.0f);
	vmtl_pass(vmtl.present, VMTL_CLEAR_COLOR, 1.0f);
	vmtl_end_pass();
	vmtl_invalidate_reads();
	return true;
}

/* draw a texture over the whole of a render target, without filtering */
static void vmtl_copy_scaled(id<MTLTexture> src, id<MTLTexture> dst, UINT32 w, UINT32 h) {
	VMtlVertex q[6];
	const VMtlColor none = { 0, 0, 0, 0 };
	const float x[6] = { 0, (float)w, 0, (float)w, (float)w, 0 }, y[6] = { 0, 0, (float)h, 0, (float)h, (float)h };
	for (int i = 0; i < 6; i++) {
		vmtl_vertex(q[i], x[i], y[i], 0.0f, none);
		q[i].tex0[0] = x[i] / (float)w;
		q[i].tex0[1] = y[i] / (float)h;
	}
	id<MTLBuffer> buf = nil;
	NSUInteger offset = 0;
	if (!vmtl_upload_vertices(q, 6, buf, offset)) return;
	VMtlBlendKey bk;
	memset(&bk, 0, sizeof(bk));
	bk.src = bk.src_alpha = MTLBlendFactorOne;
	bk.dst = bk.dst_alpha = MTLBlendFactorZero;
	bk.mask = MTLColorWriteMaskAll;
	VMtlDepthKey dk;
	memset(&dk, 0, sizeof(dk));
	id<MTLRenderPipelineState> pipeline = vmtl_pipeline(vmtl.copy, bk);
	id<MTLDepthStencilState> depth = vmtl_depth_state(dk);
	if (pipeline == nil || depth == nil) return;
	id<MTLRenderCommandEncoder> enc = vmtl_pass(dst, 0, 0.0f);
	if (enc == nil) return;
	[enc setRenderPipelineState:pipeline];
	[enc setDepthStencilState:depth];
	[enc setVertexBuffer:buf offset:offset atIndex:0];
	vmtl_set_target_size(enc, w, h);
	[enc setFragmentTexture:src atIndex:0];
	[enc setFragmentSamplerState:vmtl_sampler(VMTL_SAMPLER_CLAMP_S | VMTL_SAMPLER_CLAMP_T) atIndex:0];
	[enc drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
}

/* change the render resolution, keeping what the color buffers show (scaled) */
static bool vmtl_resize_targets(UINT32 w, UINT32 h) {
	vmtl_flush();
	id<MTLTexture> old[2] = { vmtl.color[0], vmtl.color[1] };
	vmtl.color[0] = vmtl.color[1] = nil;
	vmtl_release_targets();

	const bool ok = vmtl_create_targets(w, h);
	if (ok) {
		/* without filtering, the pixels keep their colors (for LFB reads). The depth buffer is
		 * cleared. */
		for (int i = 0; i < 2; i++)
			if (old[i] != nil) vmtl_copy_scaled(old[i], vmtl.color[i], w, h);
		vmtl_end_pass();
		OUTPUT_Metal_Get()->SetExternalTexture(vmtl.present);
		LOG_MSG("VOODOO: Metal: render resolution %u:%u", (unsigned int)w, (unsigned int)h);
	} else {
		vmtl_release_targets();
		vmtl.ready = false;
	}
	/* (the commands that use them keep them until they are done) */
	for (int i = 0; i < 2; i++) VMTL_RELEASE(old[i]);
	return ok;
}

/* follow the resolution setting and the display area, between frames */
static void vmtl_check_render_size(void) {
	Voodoo_HW_PollResolutionSetting();
	UINT32 w, h;
	Voodoo_HW_RenderSize(vmtl.max_size, w, h);
	if (vmtl.width != 0 && (w != vmtl.width || h != vmtl.height))
		vmtl_resize_targets(w, h);
}

/* the render resolution pixels [p0, p1) that 3dfx video mode pixel i covers, as the OpenGL
 * emulation's voodoo_ogl_lfb_span() */
static void vmtl_lfb_span(int i, UINT32 render_size, UINT32 mode_size, int &p0, int &p1) {
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

static void vmtl_destroy_texmap(VMtlTexMap &m) {
	if (!m.by_palette.empty()) {
		for (map<UINT32, id<MTLTexture> >::iterator i = m.by_palette.begin(); i != m.by_palette.end(); ++i)
			VMTL_RELEASE(i->second);
		m.by_palette.clear();
		m.current = nil;
	} else {
		VMTL_RELEASE(m.current);
	}
}

static void vmtl_destroy_textures(void) {
	vmtl_flush();
	for (int j = 0; j < 2; j++) {
		for (map<const UINT32, VMtlTexMap>::iterator t = vmtl_textures[j].begin(); t != vmtl_textures[j].end(); ++t)
			vmtl_destroy_texmap(t->second);
		vmtl_textures[j].clear();
		vmtl_texture_dirty[j].pending = false;
	}
}

static bool vmtl_palette_format(UINT32 format) {
	return format == 1 || format == 9 || format == 5 || format == 14;
}

/* the palette (or NCC table) a texture is converted with */
static UINT32 vmtl_palette_sum(UINT32 tmunum, UINT32 texmode) {
	const UINT32 format = TEXMODE_FORMAT(texmode);
	const rgb_t *colors = (format & 7) == 1 ? v->tmu[tmunum].ncc[TEXMODE_NCC_TABLE_SELECT(texmode)].texel : v->tmu[tmunum].palette;
	UINT32 h = 2166136261u;
	for (int i = 0; i < 256; i++) h = (h ^ colors[i]) * 16777619u;
	return h;
}

/* drop textures that texture memory writes changed (before the next draw) */
static void vmtl_flush_texture_writes(int TMU) {
	VMtlDirtyRange &dirty = vmtl_texture_dirty[TMU];
	if (!dirty.pending) return;
	const UINT32 mask = v->tmu[TMU].mask;
	for (map<const UINT32, VMtlTexMap>::iterator t = vmtl_textures[TMU].begin(); t != vmtl_textures[TMU].end();) {
		const UINT32 size = t->second.width * t->second.height * (t->second.format < 8 ? 1 : 2);
		const UINT32 last = t->first + size - 1;
		const bool overlaps = last <= mask
			? t->first <= dirty.last && last >= dirty.first
			: t->first <= dirty.last || (last & mask) >= dirty.first;
		if (overlaps) {
			vmtl_flush();	/* (queued draws may use it) */
			vmtl_destroy_texmap(t->second);
			vmtl_textures[TMU].erase(t++);
		} else {
			++t;
		}
	}
	dirty.pending = false;
}

/* a texture from texture memory, with mipmaps (the OpenGL emulation's conversion) */
static id<MTLTexture> vmtl_upload_texture(int j, UINT32 TEXMODE, UINT32 texbase, UINT32 width, UINT32 height) {
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
	UINT32 lw = width, lh = height;
	size_t at = 0;
	for (UINT32 l = 0; l < levels; l++) {
		offsets[l] = at;
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

	/* (0xAARRGGBB in memory is BGRA) */
	MTLTextureDescriptor *d = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
		width:width height:height mipmapped:NO];
	d.mipmapLevelCount = levels;
	d.usage = MTLTextureUsageShaderRead;
	d.storageMode = MTLStorageModeManaged;
	id<MTLTexture> tex = [vmtl.dev newTextureWithDescriptor:d];
	if (tex == nil) {
		LOG_MSG("VOODOO: Metal: could not make a %ux%u texture", (unsigned int)width, (unsigned int)height);
		return nil;
	}
	lw = width;
	lh = height;
	for (UINT32 l = 0; l < levels; l++) {
		[tex replaceRegion:MTLRegionMake2D(0, 0, lw, lh) mipmapLevel:l withBytes:&data[offsets[l]] bytesPerRow:lw * 4];
		lw = lw > 1 ? lw / 2 : 1;
		lh = lh > 1 ? lh / 2 : 1;
	}
	return tex;
}

/* the texture a TMU uses, like ogl_cache_texture() */
static id<MTLTexture> vmtl_cache_texture(const poly_extra_data *extra, UINT32 j) {
	vmtl_flush_texture_writes((int)j);
	const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	const UINT32 ncc_table = (format & 7) == 1 ? TEXMODE_NCC_TABLE_SELECT(TEXMODE) : 0;

	UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
	if (!((v->tmu[j].lodmask >> ilod) & 1)) ilod++;
	const UINT32 texbase = v->tmu[j].lodoffset[ilod];
	const UINT32 width = (v->tmu[j].wmask >> ilod) + 1;
	const UINT32 height = (v->tmu[j].hmask >> ilod) + 1;

	map<const UINT32, VMtlTexMap>::iterator t = vmtl_textures[j].find(texbase);
	/* a RAM address can be reused with a different texture layout */
	if (t != vmtl_textures[j].end() &&
	    (t->second.format != format || t->second.width != width || t->second.height != height ||
	     t->second.ilod != ilod || t->second.lodmask != v->tmu[j].lodmask || t->second.ncc_table != ncc_table)) {
		vmtl_flush();
		vmtl_destroy_texmap(t->second);
		vmtl_textures[j].erase(t);
		t = vmtl_textures[j].end();
	}

	UINT32 palsum = 0;
	if (t != vmtl_textures[j].end()) {
		if (t->second.valid_pal) return t->second.current;
		palsum = vmtl_palette_sum(j, TEXMODE);
		map<UINT32, id<MTLTexture> >::iterator u = t->second.by_palette.find(palsum);
		t->second.valid_pal = true;
		if (u != t->second.by_palette.end()) {
			t->second.current = u->second;
			return u->second;
		}
		if (t->second.by_palette.size() > 8) {
			vmtl_flush();
			for (u = t->second.by_palette.begin(); u != t->second.by_palette.end(); ++u)
				VMTL_RELEASE(u->second);
			t->second.by_palette.clear();
		}
	} else if (vmtl_palette_format(format)) {
		palsum = vmtl_palette_sum(j, TEXMODE);
	}

	id<MTLTexture> tex = vmtl_upload_texture((int)j, TEXMODE, texbase, width, height);
	if (tex == nil) return nil;
	if (t == vmtl_textures[j].end()) {
		VMtlTexMap m;
		m.valid_pal = true;
		m.format = format;
		m.width = width;
		m.height = height;
		m.ilod = ilod;
		m.lodmask = v->tmu[j].lodmask;
		m.ncc_table = ncc_table;
		m.current = tex;
		if (vmtl_palette_format(format)) m.by_palette[palsum] = tex;
		vmtl_textures[j][texbase] = m;
	} else {
		t->second.current = tex;
		t->second.by_palette[palsum] = tex;
	}
	return tex;
}

/*************************************
 *  Setup
 *************************************/

/* a library made from source, with its vertex function and a fragment function */
static VMtlShader *vmtl_create_shader(id<MTLLibrary> lib, const char *fs_name) {
	VMtlShader *sh = new VMtlShader();
	sh->lib = [lib retain];
	sh->vs = [lib newFunctionWithName:@"vs_main"];
	sh->fs = [lib newFunctionWithName:[NSString stringWithUTF8String:fs_name]];
	if (sh->vs == nil || sh->fs == nil) LOG_MSG("VOODOO: Metal: shader function %s missing", fs_name);
	return sh;
}

static void vmtl_destroy_shader(VMtlShader *sh) {
	if (sh == NULL) return;
	for (map<VMtlBlendKey, id<MTLRenderPipelineState> >::iterator i = sh->pipelines.begin(); i != sh->pipelines.end(); ++i)
		VMTL_RELEASE(i->second);
	VMTL_RELEASE(sh->vs);
	VMTL_RELEASE(sh->fs);
	VMTL_RELEASE(sh->lib);
	delete sh;
}

static id<MTLLibrary> vmtl_compile(const string &source) {
	NSError *err = nil;
	NSString *src = [NSString stringWithUTF8String:source.c_str()];
	id<MTLLibrary> lib = [vmtl.dev newLibraryWithSource:src options:nil error:&err];
	if (lib == nil)
		LOG_MSG("VOODOO: Metal: shader compile error: %s\n%s",
			err != nil ? [[err localizedDescription] UTF8String] : "", source.c_str());
	return lib;
}

static bool vmtl_create_device_objects(void) {
	id<MTLLibrary> lib = vmtl_compile(string(vmtl_common_source) + vmtl_builtin_source);
	if (lib == nil) return false;
	vmtl.flat = vmtl_create_shader(lib, "flat_main");
	vmtl.copy = vmtl_create_shader(lib, "copy_main");
	[lib release];
	return vmtl.flat->vs != nil && vmtl.flat->fs != nil && vmtl.copy->fs != nil;
}

static void vmtl_destroy_all(void) {
	if (vmtl.dev == nil) return;
	vmtl_flush();
	vmtl_destroy_textures();
	vmtl_release_targets();
	/* (what was sent keeps what it uses until the GPU is done with it) */
	vmtl_commit(false);
	for (map<VMtlPsKey, VMtlShader*>::iterator i = vmtl.shaders.begin(); i != vmtl.shaders.end(); ++i)
		vmtl_destroy_shader(i->second);
	vmtl.shaders.clear();
	vmtl_destroy_shader(vmtl.flat);
	vmtl_destroy_shader(vmtl.copy);
	vmtl.flat = vmtl.copy = NULL;
	for (map<VMtlDepthKey, id<MTLDepthStencilState> >::iterator i = vmtl.depth_states.begin(); i != vmtl.depth_states.end(); ++i)
		VMTL_RELEASE(i->second);
	vmtl.depth_states.clear();
	for (map<UINT32, id<MTLSamplerState> >::iterator i = vmtl.samplers.begin(); i != vmtl.samplers.end(); ++i)
		VMTL_RELEASE(i->second);
	vmtl.samplers.clear();
	for (size_t i = 0; i < vmtl.in_flight.size(); i++) {
		vmtl.free_chunks.insert(vmtl.free_chunks.end(), vmtl.in_flight[i].chunks.begin(), vmtl.in_flight[i].chunks.end());
		[vmtl.in_flight[i].cmd release];
	}
	vmtl.in_flight.clear();
	for (size_t i = 0; i < vmtl.free_chunks.size(); i++) [vmtl.free_chunks[i] release];
	vmtl.free_chunks.clear();
	vmtl.chunk = nil;
	vmtl.vertices.clear();
	vmtl.have_pending = false;
	vmtl.ready = false;
	VMTL_RELEASE(vmtl.queue);
	VMTL_RELEASE(vmtl.dev);
}

/* (re)make the buffers for the 3dfx video mode, and show it in the output */
static bool vmtl_reset_videomode(void) {
	if (vmtl.dev == nil) return false;
	vmtl_flush();
	vmtl_release_targets();

	/* the output shows the 3dfx frame like any video mode (sized for the 3dfx video mode,
	 * whatever the render resolution is). This comes first, for the size of the display area. */
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);

	CMetal *out = OUTPUT_Metal_Get();
	if (out == NULL || out->GetDevice() != vmtl.dev) return false;
	UINT32 w, h;
	Voodoo_HW_RenderSize(vmtl.max_size, w, h);
	if (!vmtl_create_targets(w, h)) {
		vmtl_release_targets();
		return false;
	}
	vmtl.front = 0;
	vmtl.ready = true;
	out->SetExternalTexture(vmtl.present);
	LOG_MSG("VOODOO: Metal: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, w, h);
	return true;
}

/*************************************
 *  The 3dfx emulation's functions
 *************************************/

bool voodoo_metal_init(voodoo_state *vs) {
	(void)vs;
	if (vmtl.ready) return true;
	if (!OUTPUT_Metal_Active()) {
		LOG_MSG("VOODOO: Metal: the Metal 3dfx emulation needs output=metal");
		return false;
	}
	@autoreleasepool {
		CMetal *out = OUTPUT_Metal_Get();
		vmtl.dev = [out->GetDevice() retain];
		vmtl.queue = [out->GetQueue() retain];
		/* (every Mac GPU with Metal has 16384 pixel textures) */
		vmtl.max_size = 16384;
		if (!vmtl_create_device_objects()) {
			vmtl_destroy_all();
			return false;
		}
		Voodoo_HW_PollResolutionSetting();
		if (!vmtl_reset_videomode()) {
			vmtl_destroy_all();
			return false;
		}
	}
	LOG_MSG("VOODOO: Metal: rendering 3dfx graphics with Metal");
	return true;
}

void voodoo_metal_device_lost(void) {
	if (vmtl.dev == nil) return;
	@autoreleasepool {
		vmtl_destroy_all();
	}
	vmtl.lost = true;
}

void voodoo_metal_leave(bool leavemode) {
	if (leavemode) vmtl.lost = false;
	if (vmtl.dev == nil) return;
	@autoreleasepool {
		if (leavemode) {
			LOG_MSG("VOODOO: Metal: quit");
			vmtl_destroy_all();
		} else {
			vmtl_destroy_textures();
		}
	}
}

void voodoo_metal_shutdown(voodoo_state *vs) {
	(void)vs;
	voodoo_metal_leave(true);
}

void voodoo_metal_set_window(voodoo_state *vs) {
	/* the projection is worked out for each triangle */
	(void)vs;
}

void voodoo_metal_clip_window(voodoo_state *vs) {
	(void)vs;
}

void voodoo_metal_vblank_flush(void) {
}

/* show the front buffer: copy it to the texture the output draws, and have the output draw it */
void voodoo_metal_present(void) {
	if (!v->active) return;
	CMetal *out = OUTPUT_Metal_Get();
	if (vmtl.dev == nil) {
		/* the output was changed to another one and back: start over with its new device (what
		 * was drawn is gone) */
		if (vmtl.lost && OUTPUT_Metal_Active()) {
			vmtl.lost = false;
			voodoo_metal_init(v);
		}
		return;
	}
	if (!vmtl_usable()) return;
	@autoreleasepool {
		vmtl_check_render_size();
		if (!vmtl.ready) return;
		vmtl_flush();
		vmtl_end_pass();
		id<MTLBlitCommandEncoder> blit = [vmtl_cmd() blitCommandEncoder];
		[blit copyFromTexture:vmtl.color[vmtl.front] sourceSlice:0 sourceLevel:0 sourceOrigin:MTLOriginMake(0, 0, 0)
			sourceSize:MTLSizeMake(vmtl.width, vmtl.height, 1)
			toTexture:vmtl.present destinationSlice:0 destinationLevel:0 destinationOrigin:MTLOriginMake(0, 0, 0)];
		[blit endEncoding];
		/* (on the same queue, before the output's commands) */
		vmtl_commit(false);
		vmtl_recycle_chunks();
		out->SetExternalTexture(vmtl.present);
		out->PresentExternal();
	}
}

void voodoo_metal_swap_buffer(void) {
	if (!vmtl_usable()) return;
	/* the back buffer becomes the front buffer (it is shown at the next vertical retrace) */
	@autoreleasepool {
		vmtl_flush();
	}
	vmtl.front ^= 1;
}

/* the 3dfx video mode changed: the buffers are resized right away (voodoo_metal_follow_dimensions),
 * this tells the output, which changes the window etc. */
void voodoo_metal_update_dimensions(void) {
	if (vmtl.dev == nil) return;
	@autoreleasepool {
		if (vmtl.width == 0) {
			vmtl_reset_videomode();
			return;
		}
		vmtl_flush();
		RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
		if (!vmtl_usable()) return;
		vmtl_check_render_size();
		if (!vmtl.ready) return;
		OUTPUT_Metal_Get()->SetExternalTexture(vmtl.present);
	}
	LOG_MSG("VOODOO: Metal: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, vmtl.width, vmtl.height);
}

/* the 3dfx video mode changed while active: anything drawn from now on is drawn at the new size */
void voodoo_metal_follow_dimensions(void) {
	if (!vmtl_usable()) return;
	@autoreleasepool {
		vmtl_check_render_size();
	}
}

/* both buffers black, depth 1.0 */
void voodoo_metal_clear(void) {
	if (!vmtl_usable()) return;
	@autoreleasepool {
		vmtl_flush();
		vmtl_pass(vmtl.color[0], VMTL_CLEAR_COLOR | VMTL_CLEAR_DEPTH | VMTL_CLEAR_STENCIL, 0.0f);
		vmtl_pass(vmtl.color[1], VMTL_CLEAR_COLOR, 0.0f);
	}
	vmtl_invalidate_reads();
}

/* the 3dfx card was drawn to in software until the emulation became active (with the
 * buffers in its memory), so start from what it has there instead of black. Scaled to the
 * render resolution here, with the pixel spans LFB reads and writes use (so they agree). */
void voodoo_metal_load_framebuffers_from_ram(void) {
	if (!vmtl_usable()) return;
	const UINT32 fw = v->fbi.width, fh = v->fbi.height;
	if (fw == 0 || fh == 0) return;
	@autoreleasepool {
		vmtl_flush();
		vmtl_end_pass();

		vector<UINT32> col(vmtl.width), row(vmtl.height);
		for (UINT32 i = 0; i < fw; i++) {
			int p0, p1;
			vmtl_lfb_span((int)i, vmtl.width, fw, p0, p1);
			for (int p = p0; p < p1; p++) col[(size_t)p] = i;
		}
		for (UINT32 i = 0; i < fh; i++) {
			int p0, p1;
			vmtl_lfb_span((int)i, vmtl.height, fh, p0, p1);
			for (int p = p0; p < p1; p++) row[(size_t)p] = i;
		}

		vector<UINT32> pixels((size_t)vmtl.width * vmtl.height);
		for (int b = 0; b < 2; b++) {
			const UINT32 offs = v->fbi.rgboffs[b == 0 ? v->fbi.frontbuf : v->fbi.backbuf];
			if (offs == (UINT32)(~0) || offs > v->fbi.mask) continue;
			const UINT16 *src = (const UINT16 *)(v->fbi.ram + offs);
			const UINT32 srcmax = (v->fbi.mask + 1 - offs) / 2;
			/* RGB 565 to RGBA (bytes R, G, B, A), alpha 0, top row first in both */
			for (UINT32 y = 0; y < vmtl.height; y++) {
				UINT32 *d = &pixels[(size_t)y * vmtl.width];
				for (UINT32 x = 0; x < vmtl.width; x++) {
					const UINT32 i = row[y] * v->fbi.rowpixels + col[x];
					const UINT32 p = (i < srcmax) ? src[i] : 0;
					const UINT32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, bl = p & 0x1f;
					d[x] = ((r << 3) | (r >> 2)) | (((g << 2) | (g >> 4)) << 8) | (((bl << 3) | (bl >> 2)) << 16);
				}
			}
			/* (the buffers are GPU memory: copied from a buffer, in order with the drawing) */
			const NSUInteger pitch = (NSUInteger)vmtl.width * 4;
			id<MTLBuffer> staging = [vmtl.dev newBufferWithBytes:pixels.data() length:pitch * vmtl.height
				options:MTLResourceStorageModeShared];
			if (staging == nil) continue;
			id<MTLBlitCommandEncoder> blit = [vmtl_cmd() blitCommandEncoder];
			[blit copyFromBuffer:staging sourceOffset:0 sourceBytesPerRow:pitch sourceBytesPerImage:pitch * vmtl.height
				sourceSize:MTLSizeMake(vmtl.width, vmtl.height, 1)
				toTexture:vmtl.color[vmtl_target(b == 0)] destinationSlice:0 destinationLevel:0
				destinationOrigin:MTLOriginMake(0, 0, 0)];
			[blit endEncoding];
			[staging release];
		}
	}
	vmtl_invalidate_reads();
}

void voodoo_metal_fastfill(void) {
	if (!vmtl_usable()) return;

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
	int target = vmtl.front ^ 1;
	if (rgb && FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0) {
		target = vmtl.front;
		v->fbi.vblank_flush_pending = true;
	}

	/* a rectangle, written as it is (no depth test or blending) */
	VMtlDrawState st = vmtl_default_state(target, 1);
	st.blend.mask = rgb ? MTLColorWriteMaskAll : MTLColorWriteMaskNone;
	st.depth.write = aux ? 1 : 0;
	const float x0 = (float)((UINT32)sx * vmtl.width / (UINT32)fw), x1 = (float)((UINT32)ex * vmtl.width / (UINT32)fw);
	const float y0 = (float)((UINT32)sy * vmtl.height / (UINT32)fh), y1 = (float)((UINT32)ey * vmtl.height / (UINT32)fh);
	if (x1 <= x0 || y1 <= y0) return;
	VMtlColor c;
	c.r = v->reg[color1].rgb.r / 255.0f;
	c.g = v->reg[color1].rgb.g / 255.0f;
	c.b = v->reg[color1].rgb.b / 255.0f;
	c.a = v->reg[color1].rgb.a / 255.0f;
	@autoreleasepool {
		vmtl_rect(st, x0, y0, x1, y1, (float)((UINT16)v->reg[zaColor].u) / 65535.0f, c);
	}
}

void voodoo_metal_texture_clear(UINT32 address, int TMU) {
	if (vmtl_textures[TMU].empty()) return;
	const UINT32 mask = v->tmu[TMU].mask;
	UINT32 first = address & mask;
	UINT32 last = (first + 3) & mask;
	/* a wrapped dword conservatively dirties the whole circular address space */
	if (last < first) {
		first = 0;
		last = mask;
	}
	VMtlDirtyRange &dirty = vmtl_texture_dirty[TMU];
	if (!dirty.pending) {
		dirty.first = first;
		dirty.last = last;
		dirty.pending = true;
	} else {
		if (first < dirty.first) dirty.first = first;
		if (last > dirty.last) dirty.last = last;
	}
}

void voodoo_metal_invalidate_paltex(void) {
	for (int j = 0; j < 2; j++)
		for (map<const UINT32, VMtlTexMap>::iterator t = vmtl_textures[j].begin(); t != vmtl_textures[j].end(); ++t)
			if (vmtl_palette_format(t->second.format)) t->second.valid_pal = false;
}

/* an LFB pixel (x, y) in the 3dfx video mode, row 0 at the top: the render resolution pixels it covers,
 * a point if it is just one, otherwise a rectangle */
static void vmtl_draw_lfb(VMtlDrawState st, int x, int y, VMtlColor c, float z) {
	int x0, x1, y0, y1;
	vmtl_lfb_span(x, vmtl.width, v->fbi.width, x0, x1);
	vmtl_lfb_span(y, vmtl.height, v->fbi.height, y0, y1);
	@autoreleasepool {
		if (x1 - x0 == 1 && y1 - y0 == 1) {
			VMtlVertex p;
			vmtl_vertex(p, (float)x0 + 0.5f, (float)y0 + 0.5f, z, c);
			st.kind = 2;
			vmtl_draw(st, &p, 1);
			return;
		}
		st.kind = 1;
		vmtl_rect(st, (float)x0, (float)y0, (float)x1, (float)y1, z, c);
	}
}

static VMtlColor vmtl_color(int r, int g, int b, int a) {
	VMtlColor c;
	c.r = (r & 0xff) / 255.0f;
	c.g = (g & 0xff) / 255.0f;
	c.b = (b & 0xff) / 255.0f;
	c.a = (a & 0xff) / 255.0f;
	return c;
}

void voodoo_metal_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	if (!vmtl_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VMtlDrawState st = vmtl_default_state(vmtl_target(front), 1);
	st.blend.mask = vmtl_color_mask(has_rgb, has_alpha);
	vmtl_draw_lfb(st, x, y, vmtl_color(r, g, b, a), 0.0f);
}

void voodoo_metal_draw_z(int x, int y, int z) {
	if (!vmtl_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VMtlDrawState st = vmtl_default_state(vmtl_target(front), 1);
	st.blend.mask = MTLColorWriteMaskNone;
	st.depth.write = 1;
	vmtl_draw_lfb(st, x, y, vmtl_color(0, 0, 0, 0), (float)(z & 0xffff) / 65535.0f);
}

void voodoo_metal_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
	if (!vmtl_usable()) return;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	VMtlDrawState st = vmtl_default_state(vmtl_target(front), 1);
	st.blend.mask = vmtl_color_mask(true, FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0);
	vmtl_blend_from(st, v->reg[alphaMode].u);
	vmtl_draw_lfb(st, x, y, vmtl_color(r, g, b, 255), 0.0f);
}

/* read a buffer back (0 and 1: color, 2: depth), kept until it is drawn to again */
static bool vmtl_read_buffer(int which) {
	if (vmtl.cache_valid[which]) return true;
	vmtl_flush();
	vmtl_end_pass();
	vector<UINT32> &c = vmtl.cache[which];
	c.resize((size_t)vmtl.width * vmtl.height);

	const NSUInteger pitch = (NSUInteger)vmtl.width * 4, size = pitch * vmtl.height;
	if (vmtl.readback == nil || [vmtl.readback length] < size) {
		VMTL_RELEASE(vmtl.readback);
		vmtl.readback = [vmtl.dev newBufferWithLength:size options:MTLResourceStorageModeShared];
		if (vmtl.readback == nil) return false;
	}
	id<MTLBlitCommandEncoder> blit = [vmtl_cmd() blitCommandEncoder];
	[blit copyFromTexture:(which < 2 ? vmtl.color[which] : vmtl.depth) sourceSlice:0 sourceLevel:0
		sourceOrigin:MTLOriginMake(0, 0, 0) sourceSize:MTLSizeMake(vmtl.width, vmtl.height, 1)
		toBuffer:vmtl.readback destinationOffset:0 destinationBytesPerRow:pitch destinationBytesPerImage:size
		options:(which < 2 ? MTLBlitOptionNone : MTLBlitOptionDepthFromDepthStencil)];
	[blit endEncoding];
	vmtl_commit(true);

	const uint8_t *data = (const uint8_t *)[vmtl.readback contents];
	for (UINT32 y = 0; y < vmtl.height; y++) {
		UINT32 *o = &c[(size_t)y * vmtl.width];
		if (which < 2) {
			/* RGBA bytes to 0xAARRGGBB */
			const UINT32 *s = (const UINT32 *)(data + (size_t)y * pitch);
			for (UINT32 x = 0; x < vmtl.width; x++)
				o[x] = (s[x] & 0xff00ff00u) | ((s[x] & 0xffu) << 16) | ((s[x] >> 16) & 0xffu);
		} else {
			/* float depth to 16 bits */
			const float *s = (const float *)(data + (size_t)y * pitch);
			for (UINT32 x = 0; x < vmtl.width; x++) {
				const float d = s[x] < 0.0f ? 0.0f : (s[x] > 1.0f ? 1.0f : s[x]);
				o[x] = (UINT32)(d * 65535.0f + 0.5f);
			}
		}
	}
	vmtl.cache_valid[which] = true;
	return true;
}

/* two pixels from (x, y) in the 3dfx video mode, row 0 at the top. With a larger render
 * resolution, the render pixel in the middle of each one is read (as OpenGL). */
UINT32 voodoo_metal_read_pixel(int x, int y) {
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;
	if (!vmtl_usable()) return 0;

	int rx[2], ry, p0, p1;
	for (int i = 0; i < 2; i++) {
		vmtl_lfb_span(x + i, vmtl.width, v->fbi.width, p0, p1);
		rx[i] = (p0 + p1 - 1) / 2;
	}
	vmtl_lfb_span(y, vmtl.height, v->fbi.height, p0, p1);
	ry = (p0 + p1 - 1) / 2;

	int which;
	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0: which = vmtl.front; break;		/* front buffer */
		case 1: which = vmtl.front ^ 1; break;		/* back buffer */
		case 2: which = 2; break;			/* aux buffer */
		default:
			E_Exit("read from invalid buf %x", LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			return 0;
	}
	bool ok;
	@autoreleasepool {
		ok = vmtl_read_buffer(which);
	}
	if (!ok) return 0;
	const UINT32 *line = &vmtl.cache[which][(size_t)ry * vmtl.width];
	const UINT32 d0 = line[rx[0]], d1 = line[rx[1]];
	if (which == 2) return (d0 & 0xffff) | (d1 << 16);

	/* ARGB to RGB 565 */
	#define VMTL_565(p) ((((p) >> 19) & 0x1f) << 11 | (((p) >> 10) & 0x3f) << 5 | (((p) >> 3) & 0x1f))
	return VMTL_565(d0) | (VMTL_565(d1) << 16);
	#undef VMTL_565
}

/* the fragment shader for a rasterizer's settings */
static VMtlShader *vmtl_triangle_shader(const poly_extra_data *extra, bool tmu0, bool tmu1) {
	VMtlPsKey key;
	memset(&key, 0, sizeof(key));
	key.texmode[0] = tmu0 ? (UINT32)extra->r_textureMode0 : 0;
	key.texmode[1] = tmu1 ? (UINT32)extra->r_textureMode1 : 0;
	key.fbzcp = v->reg[fbzColorPath].u;
	key.alphamode = v->reg[alphaMode].u & 0x0f;	/* alpha test and function */
	key.fogmode = FOGMODE_ENABLE_FOG(v->reg[fogMode].u) ? (v->reg[fogMode].u & 0x3f) : 0;
	key.flags = (tmu0 ? VMTL_PS_TMU0 : 0) | (tmu1 ? VMTL_PS_TMU1 : 0) |
		(FBZMODE_ENABLE_CHROMAKEY(v->reg[fbzMode].u) ? VMTL_PS_CHROMAKEY : 0);
	map<VMtlPsKey, VMtlShader*>::iterator i = vmtl.shaders.find(key);
	if (i != vmtl.shaders.end()) return i->second;
	VMtlShader *shader = NULL;
	id<MTLLibrary> lib = vmtl_compile(vmtl_triangle_ps_source(key));
	if (lib != nil) {
		shader = vmtl_create_shader(lib, "ps_main");
		[lib release];
		if (shader->vs == nil || shader->fs == nil) {
			vmtl_destroy_shader(shader);
			shader = NULL;
		}
	}
	/* (one that failed is not tried again) */
	vmtl.shaders[key] = shader;
	return shader;
}

void voodoo_metal_draw_triangle(poly_extra_data *extra) {
	v = extra->state;
	if (!vmtl_usable()) return;
	@autoreleasepool {

	const UINT32 ALPHAMODE = extra->r_alphaMode;
	const UINT32 FBZMODE = extra->r_fbzMode;

	ogl_vertex_data vd[3];
	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);

	/* the textures */
	id<MTLTexture> tex[2] = { nil, nil };
	bool use[2] = { false, false };
	for (UINT32 j = 0; j < 2; j++) {
		if (j >= extra->texcount || v->tmu[j].ram == NULL || v->tmu[j].lodmin >= (8 << 8)) continue;
		tex[j] = vmtl_cache_texture(extra, j);
		use[j] = tex[j] != nil;
	}
	VMtlShader *shader = vmtl_triangle_shader(extra, use[0], use[1]);
	if (shader == NULL) return;

	const bool front = FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0;
	const int target = vmtl_target(front);
	if (front) v->fbi.vblank_flush_pending = true;

	VMtlDrawState st = vmtl_default_state(target, 0);
	st.shader = shader;
	for (int j = 0; j < 2; j++) {
		const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
		st.texture[j] = tex[j];
		if (!use[j]) continue;
		st.sampler[j] = (TEXMODE_MINIFICATION_FILTER(TEXMODE) ? VMTL_SAMPLER_MIN_LINEAR : 0u) |
			(TEXMODE_MAGNIFICATION_FILTER(TEXMODE) ? VMTL_SAMPLER_MAG_LINEAR : 0u) |
			(v->tmu[j].lodmin != v->tmu[j].lodmax ? VMTL_SAMPLER_MIPMAPS : 0u) |
			(TEXMODE_TRILINEAR(TEXMODE) ? VMTL_SAMPLER_TRILINEAR : 0u) |
			(TEXMODE_CLAMP_S(TEXMODE) ? VMTL_SAMPLER_CLAMP_S : 0u) |
			(TEXMODE_CLAMP_T(TEXMODE) ? VMTL_SAMPLER_CLAMP_T : 0u);
	}
	vmtl_constants(st.constants);

	/* vertices: in render resolution pixels, the Y origin at the top or bottom as the OpenGL
	 * emulation's projection */
	const bool y_bottom = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u) != 0;
	VMtlVertex vert[3];
	for (int i = 0; i < 3; i++) {
		VMtlVertex &o = vert[i];
		memset(&o, 0, sizeof(o));
		const float x = vd[i].x * (float)vmtl.width / (float)v->fbi.width;
		const float y = vd[i].y * (float)vmtl.height / (float)v->fbi.height;
		o.pos[0] = x;
		o.pos[1] = y_bottom ? (float)vmtl.height - y : y;
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
	 * against zaColor, and it is drawn there. The stencil is cleared first, with a rectangle
	 * over the whole buffer (rather than another render pass). */
	const bool stencil_pass = FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) != 0;
	if (stencil_pass) {
		VMtlDrawState cst = vmtl_default_state(target, 1);
		cst.depth.stencil = VMTL_STENCIL_CLEAR;
		cst.blend.mask = MTLColorWriteMaskNone;
		vmtl_rect(cst, 0.0f, 0.0f, (float)vmtl.width, (float)vmtl.height, 0.0f, vmtl_color(0, 0, 0, 0));

		VMtlDrawState sst = vmtl_default_state(target, 1);
		sst.depth.test = (FBZMODE_ENABLE_DEPTHBUF(FBZMODE) && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? 1 : 0;
		sst.depth.func = (UINT32)FBZMODE_DEPTH_FUNCTION(FBZMODE);	/* (as MTLCompareFunction) */
		sst.depth.stencil = VMTL_STENCIL_WRITE;
		sst.blend.mask = MTLColorWriteMaskNone;
		VMtlVertex flat[3];
		const float depth = (float)(v->reg[zaColor].u & 0xffff) / (float)0xffff;
		for (int i = 0; i < 3; i++) {
			flat[i] = vert[i];
			flat[i].pos[2] = depth;
		}
		vmtl_draw(sst, flat, 3);
	}

	if (stencil_pass) {
		st.depth.stencil = VMTL_STENCIL_TEST;
		st.depth.test = FBZMODE_ENABLE_DEPTHBUF(FBZMODE) ? 1 : 0;
		st.depth.func = MTLCompareFunctionAlways;
	} else if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			st.depth.test = 1;
			st.depth.func = (UINT32)FBZMODE_DEPTH_FUNCTION(FBZMODE);
		} else if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
			st.depth.test = 1;
			st.depth.func = MTLCompareFunctionAlways;
		}
	} else {
		st.depth.test = 1;
		st.depth.func = MTLCompareFunctionAlways;
	}
	const bool aux = FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0;
	st.depth.write = (aux && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) ? 1 : 0;
	st.blend.mask = vmtl_color_mask(FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0, aux);
	vmtl_blend_from(st, ALPHAMODE);
	vmtl_draw(st, vert, 3);

	}
}

#endif
