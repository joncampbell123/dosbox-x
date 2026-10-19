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

/* The 3dfx emulation rendered with Vulkan, on the Vulkan output's device (it only works with
 * output=vulkan). It does what the OpenGL one (voodoo_opengl.cpp) does, the same way where it
 * can:
 *
 *  - It renders into images of its own at the render resolution (voodoo_resolution): front and
 *    back color buffers and a depth/stencil buffer, top row first. At every vertical retrace the
 *    front buffer is copied to another image, which the output draws in place of the emulated
 *    video card's frame. Everything is submitted to the output's queue, in order.
 *  - Triangles use a fragment shader made for the rasterizer's register settings, like the
 *    GLSL ones the OpenGL emulation makes, but as SPIR-V, put together here (DOSBox-X has no
 *    shader compiler). The rest of the state (depth, blending, masks, stencil) is in the
 *    pipeline, which are kept for reuse.
 *  - Textures are converted on the CPU like the OpenGL emulation does, and have mipmaps.
 *  - LFB writes are drawn as small rectangles, LFB reads read the buffers back (kept until
 *    the buffer is drawn to again).
 *  - Commands go to a few command buffers in turn, so the CPU can work on the next one while
 *    the GPU draws. */

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
#include "voodoo_vulkan.h"

#if C_VULKAN_OUTPUT

#include <output/output_vulkan_device.h>
#include "voodoo_def.h"

using namespace std;

/*************************************
 *  Vulkan functions
 *************************************/

#define VVK_INSTANCE_FUNCS \
	VKF(vkGetDeviceProcAddr) \
	VKF(vkGetPhysicalDeviceProperties) \
	VKF(vkGetPhysicalDeviceMemoryProperties) \
	VKF(vkGetPhysicalDeviceFormatProperties)

#define VVK_DEVICE_FUNCS \
	VKF(vkDeviceWaitIdle) \
	VKF(vkQueueSubmit) \
	VKF(vkCreateImage) \
	VKF(vkDestroyImage) \
	VKF(vkGetImageMemoryRequirements) \
	VKF(vkBindImageMemory) \
	VKF(vkCreateImageView) \
	VKF(vkDestroyImageView) \
	VKF(vkCreateBuffer) \
	VKF(vkDestroyBuffer) \
	VKF(vkGetBufferMemoryRequirements) \
	VKF(vkBindBufferMemory) \
	VKF(vkAllocateMemory) \
	VKF(vkFreeMemory) \
	VKF(vkMapMemory) \
	VKF(vkUnmapMemory) \
	VKF(vkCreateRenderPass) \
	VKF(vkDestroyRenderPass) \
	VKF(vkCreateFramebuffer) \
	VKF(vkDestroyFramebuffer) \
	VKF(vkCreateShaderModule) \
	VKF(vkDestroyShaderModule) \
	VKF(vkCreatePipelineLayout) \
	VKF(vkDestroyPipelineLayout) \
	VKF(vkCreateGraphicsPipelines) \
	VKF(vkDestroyPipeline) \
	VKF(vkCreateDescriptorSetLayout) \
	VKF(vkDestroyDescriptorSetLayout) \
	VKF(vkCreateDescriptorPool) \
	VKF(vkDestroyDescriptorPool) \
	VKF(vkResetDescriptorPool) \
	VKF(vkAllocateDescriptorSets) \
	VKF(vkUpdateDescriptorSets) \
	VKF(vkCreateSampler) \
	VKF(vkDestroySampler) \
	VKF(vkCreateCommandPool) \
	VKF(vkDestroyCommandPool) \
	VKF(vkAllocateCommandBuffers) \
	VKF(vkResetCommandBuffer) \
	VKF(vkBeginCommandBuffer) \
	VKF(vkEndCommandBuffer) \
	VKF(vkCreateFence) \
	VKF(vkDestroyFence) \
	VKF(vkWaitForFences) \
	VKF(vkResetFences) \
	VKF(vkCmdPipelineBarrier) \
	VKF(vkCmdCopyBufferToImage) \
	VKF(vkCmdCopyImageToBuffer) \
	VKF(vkCmdCopyImage) \
	VKF(vkCmdBlitImage) \
	VKF(vkCmdClearColorImage) \
	VKF(vkCmdClearDepthStencilImage) \
	VKF(vkCmdClearAttachments) \
	VKF(vkCmdBeginRenderPass) \
	VKF(vkCmdEndRenderPass) \
	VKF(vkCmdBindPipeline) \
	VKF(vkCmdBindDescriptorSets) \
	VKF(vkCmdBindVertexBuffers) \
	VKF(vkCmdPushConstants) \
	VKF(vkCmdSetViewport) \
	VKF(vkCmdSetScissor) \
	VKF(vkCmdDraw)

#define VKF(name) static PFN_##name name = NULL;
VVK_INSTANCE_FUNCS
VVK_DEVICE_FUNCS
#undef VKF

/*************************************
 *  SPIR-V
 *************************************/

/* Puts SPIR-V 1.0 shaders together, an instruction at a time. Values are SSA ids. */
class SpvBuilder {
public:
	typedef uint32_t Id;

	SpvBuilder() : bound(1) {}

	Id NewId() { return bound++; }

	/* types, made once each */
	Id TypeVoid() { return Type(19, {}); }
	Id TypeBool() { return Type(20, {}); }
	Id TypeInt() { return Type(21, {32, 1}); }
	Id TypeFloat() { return Type(22, {32}); }
	Id TypeVec(Id comp, uint32_t n) { return Type(23, {comp, n}); }
	Id TypePointer(uint32_t storage, Id type) { return Type(32, {storage, type}); }
	Id TypeFunction(Id ret) { return Type(33, {ret}); }
	Id TypeImage2D() { return Type(25, {TypeFloat(), 1 /*2D*/, 0, 0, 0, 1 /*sampled*/, 0 /*unknown format*/}); }
	Id TypeSampledImage(Id image) { return Type(27, {image}); }
	Id TypeStruct(const vector<Id> &members) {
		const Id id = NewId();
		vector<uint32_t> w(1, id);
		w.insert(w.end(), members.begin(), members.end());
		Emit(types, 30, w);
		return id;
	}

	/* constants */
	Id ConstFloat(float f) {
		uint32_t bits;
		memcpy(&bits, &f, 4);
		map<uint32_t, Id>::iterator i = float_consts.find(bits);
		if (i != float_consts.end()) return i->second;
		const Id id = NewId();
		Emit(types, 43, {TypeFloat(), id, bits});
		float_consts[bits] = id;
		return id;
	}
	Id ConstTrue() {
		if (const_true == 0) {
			const_true = NewId();
			Emit(types, 41, {TypeBool(), const_true});
		}
		return const_true;
	}
	Id ConstInt(int32_t v) {
		map<int32_t, Id>::iterator i = int_consts.find(v);
		if (i != int_consts.end()) return i->second;
		const Id id = NewId();
		Emit(types, 43, {TypeInt(), id, (uint32_t)v});
		int_consts[v] = id;
		return id;
	}

	/* global variables, the input and output ones are the entry point's interface */
	Id Variable(uint32_t storage, Id type) {
		const Id id = NewId();
		Emit(types, 59, {TypePointer(storage, type), id, storage});
		if (storage == 1 /*Input*/ || storage == 3 /*Output*/) interface_vars.push_back(id);
		return id;
	}
	void Decorate(Id target, uint32_t decoration, int value = -1) {
		if (value < 0) Emit(annotations, 71, {target, decoration});
		else Emit(annotations, 71, {target, decoration, (uint32_t)value});
	}
	void MemberDecorate(Id type, uint32_t member, uint32_t decoration, uint32_t value) {
		Emit(annotations, 72, {type, member, decoration, value});
	}

	/* the function's code */
	void BeginMain() {
		main_id = NewId();
		Emit(code, 54, {TypeVoid(), main_id, 0, TypeFunction(TypeVoid())});
		Label(NewId());
	}
	void Label(Id id) { Emit(code, 248, {id}); }
	Id Op(uint16_t opcode, Id type, const vector<uint32_t> &operands) {
		const Id id = NewId();
		vector<uint32_t> w;
		w.push_back(type);
		w.push_back(id);
		w.insert(w.end(), operands.begin(), operands.end());
		Emit(code, opcode, w);
		return id;
	}
	Id Load(Id type, Id pointer) { return Op(61, type, {pointer}); }
	void Store(Id pointer, Id value) { Emit(code, 62, {pointer, value}); }
	Id AccessChain(Id ptr_type, Id base, Id index) { return Op(65, ptr_type, {base, index}); }

	/* fragment shaders: if (cond) discard; */
	void KillIf(Id cond) {
		const Id kill = NewId(), merge = NewId();
		Emit(code, 247, {merge, 0});
		Emit(code, 250, {cond, kill, merge});
		Label(kill);
		Emit(code, 252, {});
		Label(merge);
	}

	vector<uint32_t> Finish(uint32_t execution_model) {
		Emit(code, 253, {});	/* OpReturn */
		Emit(code, 56, {});	/* OpFunctionEnd */

		vector<uint32_t> out;
		out.push_back(0x07230203);
		out.push_back(0x00010000);
		out.push_back(0);
		out.push_back(bound);
		out.push_back(0);
		Emit(out, 17, {1});	/* OpCapability Shader */
		Emit(out, 14, {0, 1});	/* OpMemoryModel Logical GLSL450 */
		vector<uint32_t> ep;
		ep.push_back(execution_model);
		ep.push_back(main_id);
		ep.push_back(0x6e69616d);	/* "main" */
		ep.push_back(0);
		ep.insert(ep.end(), interface_vars.begin(), interface_vars.end());
		Emit(out, 15, ep);
		if (execution_model == 4) Emit(out, 16, {main_id, 7});	/* OriginUpperLeft */
		out.insert(out.end(), annotations.begin(), annotations.end());
		out.insert(out.end(), types.begin(), types.end());
		out.insert(out.end(), code.begin(), code.end());
		return out;
	}

private:
	static void Emit(vector<uint32_t> &section, uint16_t opcode, const vector<uint32_t> &operands) {
		section.push_back(((uint32_t)(operands.size() + 1) << 16) | opcode);
		section.insert(section.end(), operands.begin(), operands.end());
	}
	Id Type(uint16_t opcode, const vector<uint32_t> &operands) {
		vector<uint32_t> key(operands);
		key.insert(key.begin(), opcode);
		map<vector<uint32_t>, Id>::iterator i = type_ids.find(key);
		if (i != type_ids.end()) return i->second;
		const Id id = NewId();
		vector<uint32_t> w(1, id);
		w.insert(w.end(), operands.begin(), operands.end());
		Emit(types, opcode, w);
		type_ids[key] = id;
		return id;
	}

	Id bound;
	Id main_id = 0;
	Id const_true = 0;
	vector<uint32_t> annotations, types, code;
	vector<Id> interface_vars;
	map<vector<uint32_t>, Id> type_ids;
	map<uint32_t, Id> float_consts;
	map<int32_t, Id> int_consts;
};

enum {
	SPV_DECORATION_BLOCK = 2,
	SPV_DECORATION_BUILTIN = 11,
	SPV_DECORATION_LOCATION = 30,
	SPV_DECORATION_BINDING = 33,
	SPV_DECORATION_DESCRIPTORSET = 34,
	SPV_DECORATION_OFFSET = 35,
	SPV_BUILTIN_POSITION = 0,
	SPV_BUILTIN_VERTEXINDEX = 42,
	SPV_STORAGE_UNIFORMCONSTANT = 0,
	SPV_STORAGE_INPUT = 1,
	SPV_STORAGE_OUTPUT = 3,
	SPV_STORAGE_PUSHCONSTANT = 9,
	SPV_MODEL_VERTEX = 0,
	SPV_MODEL_FRAGMENT = 4
};

/* The triangle vertex shader:
 *   in vec3 pos (normalized device coordinates and depth), vec4 color, vec3 tc0, vec3 tc1 (s*w, t*w, w),
 *   vec3 extra (fog blend, LOD blend 0 and 1); passes them on */
static vector<uint32_t> vvk_build_triangle_vs(void) {
	SpvBuilder b;
	const SpvBuilder::Id f = b.TypeFloat(), v3 = b.TypeVec(f, 3), v4 = b.TypeVec(f, 4);
	const SpvBuilder::Id in_pos = b.Variable(SPV_STORAGE_INPUT, v3);
	const SpvBuilder::Id in_attr[4] = { b.Variable(SPV_STORAGE_INPUT, v4), b.Variable(SPV_STORAGE_INPUT, v3),
		b.Variable(SPV_STORAGE_INPUT, v3), b.Variable(SPV_STORAGE_INPUT, v3) };
	const SpvBuilder::Id out_attr[4] = { b.Variable(SPV_STORAGE_OUTPUT, v4), b.Variable(SPV_STORAGE_OUTPUT, v3),
		b.Variable(SPV_STORAGE_OUTPUT, v3), b.Variable(SPV_STORAGE_OUTPUT, v3) };
	const SpvBuilder::Id out_pos = b.Variable(SPV_STORAGE_OUTPUT, v4);
	b.Decorate(in_pos, SPV_DECORATION_LOCATION, 0);
	for (int i = 0; i < 4; i++) {
		b.Decorate(in_attr[i], SPV_DECORATION_LOCATION, i + 1);
		b.Decorate(out_attr[i], SPV_DECORATION_LOCATION, i);
	}
	b.Decorate(out_pos, SPV_DECORATION_BUILTIN, SPV_BUILTIN_POSITION);

	b.BeginMain();
	const SpvBuilder::Id pos = b.Load(v3, in_pos);
	b.Store(out_pos, b.Op(80, v4, {b.Op(81, f, {pos, 0}), b.Op(81, f, {pos, 1}), b.Op(81, f, {pos, 2}), b.ConstFloat(1.0f)}));
	for (int i = 0; i < 4; i++) b.Store(out_attr[i], b.Load(i == 0 ? v4 : v3, in_attr[i]));
	return b.Finish(SPV_MODEL_VERTEX);
}

/* The rectangle (LFB pixel) vertex shader: a triangle strip of 4 vertices per instance
 *   in vec4 rect (x0, y0, x1, y1), vec4 color, float depth */
static vector<uint32_t> vvk_build_rect_vs(void) {
	SpvBuilder b;
	const SpvBuilder::Id f = b.TypeFloat(), i32 = b.TypeInt(), v4 = b.TypeVec(f, 4);
	const SpvBuilder::Id in_rect = b.Variable(SPV_STORAGE_INPUT, v4);
	const SpvBuilder::Id in_color = b.Variable(SPV_STORAGE_INPUT, v4);
	const SpvBuilder::Id in_z = b.Variable(SPV_STORAGE_INPUT, f);
	const SpvBuilder::Id in_index = b.Variable(SPV_STORAGE_INPUT, i32);
	const SpvBuilder::Id out_color = b.Variable(SPV_STORAGE_OUTPUT, v4);
	const SpvBuilder::Id out_pos = b.Variable(SPV_STORAGE_OUTPUT, v4);
	b.Decorate(in_rect, SPV_DECORATION_LOCATION, 0);
	b.Decorate(in_color, SPV_DECORATION_LOCATION, 1);
	b.Decorate(in_z, SPV_DECORATION_LOCATION, 2);
	b.Decorate(in_index, SPV_DECORATION_BUILTIN, SPV_BUILTIN_VERTEXINDEX);
	b.Decorate(out_color, SPV_DECORATION_LOCATION, 0);
	b.Decorate(out_pos, SPV_DECORATION_BUILTIN, SPV_BUILTIN_POSITION);

	b.BeginMain();
	const SpvBuilder::Id index = b.Load(i32, in_index);
	const SpvBuilder::Id cx = b.Op(111, f, {b.Op(199, i32, {index, b.ConstInt(1)})});	/* float(index & 1) */
	const SpvBuilder::Id cy = b.Op(111, f, {b.Op(195, i32, {index, b.ConstInt(1)})});	/* float(index >> 1) */
	const SpvBuilder::Id rect = b.Load(v4, in_rect);
	SpvBuilder::Id c[4];
	for (int i = 0; i < 4; i++) c[i] = b.Op(81, f, {rect, (uint32_t)i});
	/* x0 + (x1 - x0) * cx, y0 + (y1 - y0) * cy */
	const SpvBuilder::Id x = b.Op(129, f, {c[0], b.Op(133, f, {b.Op(131, f, {c[2], c[0]}), cx})});
	const SpvBuilder::Id y = b.Op(129, f, {c[1], b.Op(133, f, {b.Op(131, f, {c[3], c[1]}), cy})});
	b.Store(out_pos, b.Op(80, v4, {x, y, b.Load(f, in_z), b.ConstFloat(1.0f)}));
	b.Store(out_color, b.Load(v4, in_color));
	return b.Finish(SPV_MODEL_VERTEX);
}

/* The fragment shader with just the color (LFB writes, the stencil pass of triangles) */
static vector<uint32_t> vvk_build_flat_fs(void) {
	SpvBuilder b;
	const SpvBuilder::Id f = b.TypeFloat(), v4 = b.TypeVec(f, 4);
	const SpvBuilder::Id in_color = b.Variable(SPV_STORAGE_INPUT, v4);
	const SpvBuilder::Id out_color = b.Variable(SPV_STORAGE_OUTPUT, v4);
	b.Decorate(in_color, SPV_DECORATION_LOCATION, 0);
	b.Decorate(out_color, SPV_DECORATION_LOCATION, 0);
	b.BeginMain();
	b.Store(out_color, b.Load(v4, in_color));
	return b.Finish(SPV_MODEL_FRAGMENT);
}

/* what a triangle fragment shader depends on */
struct VvkFsKey {
	UINT32 texmode[2];	/* 0 if that TMU is not used */
	UINT32 fbzcp;
	UINT32 alphamode;	/* alpha test bits */
	UINT32 fogmode;		/* 0 without fog */
	UINT32 flags;
	bool operator<(const VvkFsKey &o) const { return memcmp(this, &o, sizeof(*this)) < 0; }
};
#define VVK_FS_TMU0		0x01
#define VVK_FS_TMU1		0x02
#define VVK_FS_CHROMAKEY	0x04

/* push constants (fragment shader), like the OpenGL emulation's uniforms */
struct VvkPushConstants {
	float c0[4];	/* color0 */
	float c1[4];	/* color1 */
	float chromakey[4];
	float fogcolor[4];
	float alpharef;
	float zacolor;
};

/* Builds a triangle fragment shader like ogl_shaders() builds GLSL, with the same steps.
 * Colors are kept as rgb (vec3) and alpha (float) values. */
class VvkFragmentShader {
public:
	typedef SpvBuilder::Id Id;
	struct Color { Id rgb, a; };

	vector<uint32_t> Build(const VvkFsKey &k) {
		key = k;
		f = b.TypeFloat();
		v3 = b.TypeVec(f, 3);
		v4 = b.TypeVec(f, 4);
		boolean = b.TypeBool();
		bv3 = b.TypeVec(boolean, 3);

		const Id in_color = b.Variable(SPV_STORAGE_INPUT, v4);
		const Id in_tc[2] = { b.Variable(SPV_STORAGE_INPUT, v3), b.Variable(SPV_STORAGE_INPUT, v3) };
		const Id in_extra = b.Variable(SPV_STORAGE_INPUT, v3);
		const Id out_color = b.Variable(SPV_STORAGE_OUTPUT, v4);
		b.Decorate(in_color, SPV_DECORATION_LOCATION, 0);
		b.Decorate(in_tc[0], SPV_DECORATION_LOCATION, 1);
		b.Decorate(in_tc[1], SPV_DECORATION_LOCATION, 2);
		b.Decorate(in_extra, SPV_DECORATION_LOCATION, 3);
		b.Decorate(out_color, SPV_DECORATION_LOCATION, 0);

		const Id sampled = b.TypeSampledImage(b.TypeImage2D());
		Id tex[2] = { 0, 0 };
		for (int i = 0; i < 2; i++) {
			if (!(key.flags & (i ? VVK_FS_TMU1 : VVK_FS_TMU0))) continue;
			tex[i] = b.Variable(SPV_STORAGE_UNIFORMCONSTANT, sampled);
			b.Decorate(tex[i], SPV_DECORATION_DESCRIPTORSET, 0);
			b.Decorate(tex[i], SPV_DECORATION_BINDING, i);
		}

		const Id pc_struct = b.TypeStruct({v4, v4, v4, v4, f, f});
		b.Decorate(pc_struct, SPV_DECORATION_BLOCK);
		const uint32_t offsets[6] = { 0, 16, 32, 48, 64, 68 };
		for (uint32_t i = 0; i < 6; i++) b.MemberDecorate(pc_struct, i, SPV_DECORATION_OFFSET, offsets[i]);
		pc = b.Variable(SPV_STORAGE_PUSHCONSTANT, pc_struct);
		pc_v4 = b.TypePointer(SPV_STORAGE_PUSHCONSTANT, v4);
		pc_f = b.TypePointer(SPV_STORAGE_PUSHCONSTANT, f);

		b.BeginMain();
		gl_color = Split(b.Load(v4, in_color));
		const Id extra = b.Load(v3, in_extra);
		fogblend_in = b.Op(81, f, {extra, 0});
		lodblend[0] = b.Op(81, f, {extra, 1});
		lodblend[1] = b.Op(81, f, {extra, 2});

		/* vec4 pixel = vec4(0.0), texel = vec4(1.0), clocal = vec4(1.0), cother = vec4(0.0) */
		Color pixel = Splat(0.0f), texel = Splat(1.0f), clocal = Splat(1.0f), cother = Splat(0.0f);

		if (key.flags & VVK_FS_TMU1) {
			clocal = Split(b.Op(91, v4, {b.Load(sampled, tex[1]), b.Load(v3, in_tc[1])}));
			clocal = TexCombine(1, clocal, cother);
			cother = clocal;
			texel = clocal;
		}
		if (key.flags & VVK_FS_TMU0) {
			clocal = Split(b.Op(91, v4, {b.Load(sampled, tex[0]), b.Load(v3, in_tc[0])}));
			clocal = TexCombine(0, clocal, cother);
			texel = clocal;
		}

		pixel = ColorPath(texel);
		if (FOGMODE_ENABLE_FOG(key.fogmode)) pixel = Fog(pixel);

		b.Store(out_color, b.Op(80, v4, {b.Op(81, f, {pixel.rgb, 0}), b.Op(81, f, {pixel.rgb, 1}),
			b.Op(81, f, {pixel.rgb, 2}), pixel.a}));
		return b.Finish(SPV_MODEL_FRAGMENT);
	}

private:
	Id C(float x) { return b.ConstFloat(x); }
	Id Vec3(Id x) { return b.Op(80, v3, {x, x, x}); }
	Color Splat(float x) { Color c = { Vec3(C(x)), C(x) }; return c; }
	Color Split(Id rgba) { Color c = { b.Op(79, v3, {rgba, rgba, 0, 1, 2}), b.Op(81, f, {rgba, 3}) }; return c; }
	Id Add(Id t, Id x, Id y) { return b.Op(129, t, {x, y}); }
	Id Sub(Id t, Id x, Id y) { return b.Op(131, t, {x, y}); }
	Id Mul(Id t, Id x, Id y) { return b.Op(133, t, {x, y}); }
	Id OneMinus3(Id x) { return Sub(v3, Vec3(C(1.0f)), x); }
	Id OneMinus(Id x) { return Sub(f, C(1.0f), x); }
	Id PushV4(int member) { return Split(b.Load(v4, b.AccessChain(pc_v4, pc, b.ConstInt(member)))).rgb; }
	Id PushA(int member) { return b.Op(81, f, {b.Load(v4, b.AccessChain(pc_v4, pc, b.ConstInt(member))), 3}); }
	Id PushF(int member) { return b.Load(f, b.AccessChain(pc_f, pc, b.ConstInt(member))); }
	Color Push(int member) { Color c = { PushV4(member), PushA(member) }; return c; }

	/* ogl_sh_tex_combine() */
	Color TexCombine(int tmu, Color clocal, Color cother) {
		const UINT32 TEXMODE = key.texmode[tmu];
		Color tt, blend;

		tt.rgb = TEXMODE_TC_ZERO_OTHER(TEXMODE) ? Vec3(C(0.0f)) : cother.rgb;
		tt.a = TEXMODE_TCA_ZERO_OTHER(TEXMODE) ? C(0.0f) : cother.a;

		if (TEXMODE_TC_SUB_CLOCAL(TEXMODE)) tt.rgb = Sub(v3, tt.rgb, clocal.rgb);
		if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE)) tt.a = Sub(f, tt.a, clocal.a);

		switch (TEXMODE_TC_MSELECT(TEXMODE)) {
			default:
			case 0: blend.rgb = Vec3(C(0.0f)); break;
			case 1: blend.rgb = clocal.rgb; break;
			case 2: blend.rgb = Vec3(cother.a); break;
			case 3: blend.rgb = Vec3(clocal.a); break;
			case 4: /* detail */
			case 5: /* LOD */
				blend.rgb = Vec3(lodblend[tmu]); break;
		}
		switch (TEXMODE_TCA_MSELECT(TEXMODE)) {
			default:
			case 0: blend.a = C(0.0f); break;
			case 1: blend.a = clocal.a; break;
			case 2: blend.a = cother.a; break;
			case 3: blend.a = clocal.a; break;
			case 4: /* detail */
			case 5: /* LOD */
				blend.a = lodblend[tmu]; break;
		}

		if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE)) blend.rgb = OneMinus3(blend.rgb);
		if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE)) blend.a = OneMinus(blend.a);

		tt.rgb = Mul(v3, tt.rgb, blend.rgb);
		tt.a = Mul(f, tt.a, blend.a);

		switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE)) {
			case 3:
			case 0: break;
			case 1: tt.rgb = Add(v3, tt.rgb, clocal.rgb); break;
			case 2: tt.rgb = Add(v3, tt.rgb, Vec3(clocal.a)); break;
		}
		if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE)) tt.a = Add(f, tt.a, clocal.a);

		clocal = tt;
		if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE)) clocal.rgb = OneMinus3(clocal.rgb);
		if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE)) clocal.a = OneMinus(clocal.a);
		return clocal;
	}

	/* ogl_sh_color_path() */
	Color ColorPath(Color texel) {
		const UINT32 FBZCOLORPATH = key.fbzcp;
		const UINT32 ALPHAMODE = key.alphamode;
		Color cother, clocal, tt, blend, pixel;

		switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH)) {
			case 0: cother.rgb = gl_color.rgb; break;
			case 1: cother.rgb = texel.rgb; break;
			case 2: cother.rgb = PushV4(1); break;	/* color1 */
			default: cother.rgb = Vec3(C(0.0f)); break;
		}

		if (key.flags & VVK_FS_CHROMAKEY) {
			/* if (distance(cother.rgb, chromaKey.rgb) < 0.0001) discard; */
			const Id d = Sub(v3, cother.rgb, PushV4(2));
			b.KillIf(b.Op(184, boolean, {b.Op(148, f, {d, d}), C(0.0001f * 0.0001f)}));
		}

		switch (FBZCP_CC_ASELECT(FBZCOLORPATH)) {
			case 0: cother.a = gl_color.a; break;
			case 1: cother.a = texel.a; break;
			case 2: cother.a = PushA(1); break;
			default: cother.a = C(0.0f); break;
		}

		if (ALPHAMODE_ALPHATEST(ALPHAMODE)) {
			const Id ref = PushF(4);
			const Id diff = Sub(f, cother.a, ref);
			const Id dist2 = Mul(f, diff, diff);
			switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
				case 0: b.KillIf(b.ConstTrue()); break;			/* never */
				case 1: b.KillIf(b.Op(190, boolean, {cother.a, ref})); break;		/* >= */
				case 2: b.KillIf(b.Op(186, boolean, {dist2, C(0.0001f * 0.0001f)})); break;	/* != */
				case 3: b.KillIf(b.Op(186, boolean, {cother.a, ref})); break;		/* > */
				case 4: b.KillIf(b.Op(188, boolean, {cother.a, ref})); break;		/* <= */
				case 5: b.KillIf(b.Op(184, boolean, {dist2, C(0.0001f * 0.0001f)})); break;	/* == */
				case 6: b.KillIf(b.Op(184, boolean, {cother.a, ref})); break;		/* < */
				case 7: break;
			}
		}

		const Color c0 = Push(0);
		if (FBZCP_CC_LOCALSELECT_OVERRIDE(FBZCOLORPATH) == 0) {
			clocal = (FBZCP_CC_LOCALSELECT(FBZCOLORPATH) == 0) ? gl_color : c0;
		} else {
			/* texel.a < 0.5 ? gl_Color : c0 */
			const Id c = b.Op(184, boolean, {texel.a, C(0.5f)});
			clocal.rgb = b.Op(169, v3, {b.Op(80, bv3, {c, c, c}), gl_color.rgb, c0.rgb});
			clocal.a = b.Op(169, f, {c, gl_color.a, c0.a});
		}

		switch (FBZCP_CCA_LOCALSELECT(FBZCOLORPATH)) {
			default:
			case 0: clocal.a = gl_color.a; break;
			case 1: clocal.a = c0.a; break;
			case 2: clocal.a = gl_color.a; break;	/* TODO CLAMPED_Z (as OpenGL) */
			case 3: break;				/* voodoo2 only */
		}

		tt.rgb = FBZCP_CC_ZERO_OTHER(FBZCOLORPATH) == 0 ? cother.rgb : Vec3(C(0.0f));
		tt.a = FBZCP_CCA_ZERO_OTHER(FBZCOLORPATH) == 0 ? cother.a : C(0.0f);

		if (FBZCP_CC_SUB_CLOCAL(FBZCOLORPATH)) tt.rgb = Sub(v3, tt.rgb, clocal.rgb);
		if (FBZCP_CCA_SUB_CLOCAL(FBZCOLORPATH)) tt.a = Sub(f, tt.a, clocal.a);

		switch (FBZCP_CC_MSELECT(FBZCOLORPATH)) {
			default:
			case 0: blend.rgb = Vec3(C(0.0f)); break;
			case 1: blend.rgb = clocal.rgb; break;
			case 2: blend.rgb = Vec3(cother.a); break;
			case 3: blend.rgb = Vec3(clocal.a); break;
			case 4: blend.rgb = Vec3(texel.a); break;
			case 5: blend.rgb = texel.rgb; break;	/* voodoo2 only */
		}
		switch (FBZCP_CCA_MSELECT(FBZCOLORPATH)) {
			default:
			case 0: blend.a = C(0.0f); break;
			case 1: blend.a = clocal.a; break;
			case 2: blend.a = cother.a; break;
			case 3: blend.a = clocal.a; break;
			case 4: blend.a = texel.a; break;
		}

		if (!FBZCP_CC_REVERSE_BLEND(FBZCOLORPATH)) blend.rgb = OneMinus3(blend.rgb);
		if (!FBZCP_CCA_REVERSE_BLEND(FBZCOLORPATH)) blend.a = OneMinus(blend.a);

		tt.rgb = Mul(v3, tt.rgb, blend.rgb);
		tt.a = Mul(f, tt.a, blend.a);

		switch (FBZCP_CC_ADD_ACLOCAL(FBZCOLORPATH)) {
			case 3:
			case 0: break;
			case 1: tt.rgb = Add(v3, tt.rgb, clocal.rgb); break;
			case 2: tt.rgb = Add(v3, tt.rgb, Vec3(clocal.a)); break;
		}
		if (FBZCP_CCA_ADD_ACLOCAL(FBZCOLORPATH)) tt.a = Add(f, tt.a, clocal.a);

		pixel = tt;
		if (FBZCP_CC_INVERT_OUTPUT(FBZCOLORPATH)) pixel.rgb = OneMinus3(tt.rgb);
		if (FBZCP_CCA_INVERT_OUTPUT(FBZCOLORPATH)) pixel.a = OneMinus(tt.a);
		return pixel;
	}

	/* ogl_sh_fog() */
	Color Fog(Color pixel) {
		const UINT32 FOGMODE = key.fogmode;
		const Id fogcolor = PushV4(3);

		/* constant fog bypasses everything else */
		if (FOGMODE_FOG_CONSTANT(FOGMODE)) {
			/* (as OpenGL: ff = fogColor, and nothing more) */
			return pixel;
		}

		/* if fog_add is zero, we start with the fog color */
		Id ff = FOGMODE_FOG_ADD(FOGMODE) == 0 ? fogcolor : Vec3(C(0.0f));
		/* if fog_mult is zero, we subtract the incoming color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0) ff = Sub(v3, ff, pixel.rgb);

		/* fog blending mode: the table, iterated Z and W are worked out per vertex */
		const Id fogblend = FOGMODE_FOG_ZALPHA(FOGMODE) == 1 ? gl_color.a : fogblend_in;
		ff = Mul(v3, ff, Vec3(fogblend));

		/* if fog_mult is 0, we add this to the original color, otherwise it is the new color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0) pixel.rgb = Add(v3, pixel.rgb, ff);
		else pixel.rgb = ff;
		return pixel;
	}

	SpvBuilder b;
	VvkFsKey key;
	Id f, v3, v4, boolean, bv3;
	Id pc, pc_v4, pc_f;
	Color gl_color;
	Id fogblend_in, lodblend[2];
};

/*************************************
 *  State
 *************************************/

struct VvkImage {
	VkImage image = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	VkImageView view = VK_NULL_HANDLE;
};

/* a buffer that vertices and texture uploads are put in, mapped */
struct VvkArena {
	VkBuffer buffer = VK_NULL_HANDLE;
	VkDeviceMemory memory = VK_NULL_HANDLE;
	uint8_t *ptr = NULL;
	VkDeviceSize size = 0, used = 0;
};

/* what is destroyed when the GPU is done with a command buffer */
struct VvkTrash {
	VvkImage image;
	VkFramebuffer framebuffer = VK_NULL_HANDLE;
};

/* a command buffer and what its commands use */
struct VvkSlot {
	VkCommandBuffer cmd = VK_NULL_HANDLE;
	VkFence fence = VK_NULL_HANDLE;
	bool submitted = false;
	vector<VvkArena> arenas;
	size_t arena = 0;
	vector<VkDescriptorPool> pools;
	size_t pool = 0;
	map<vector<uint64_t>, VkDescriptorSet> sets;
	vector<VvkTrash> trash;
};

#define VVK_SLOTS 3

/* a texture, like the OpenGL emulation's ogl_texmap */
struct VvkTexture {
	VvkImage image;
	UINT32 width, height;
};
struct VvkTexMap {
	bool valid_pal;
	UINT32 format, width, height, ilod, lodmask, ncc_table;
	VvkTexture *current;
	map<UINT32, VvkTexture*> by_palette;	/* palette formats: one per palette */
};

struct VvkDirtyRange {
	bool pending = false;
	UINT32 first = 0, last = 0;
};

/* a draw, and the ones after it with the same state are added to it */
struct VvkBatch {
	bool active = false;
	bool rect = false;
	VkPipeline pipeline = VK_NULL_HANDLE;
	VkDescriptorSet set = VK_NULL_HANDLE;
	VvkPushConstants pc;
	VkBuffer buffer = VK_NULL_HANDLE;
	uint32_t first = 0, count = 0;	/* vertices (triangles) or instances (rectangles) */
};

static struct {
	bool ready;
	bool warned;
	OUTPUT_VULKAN_Device dev;
	VkPhysicalDeviceMemoryProperties memory_properties;
	UINT32 max_size;
	VkFormat depth_format;
	bool has_stencil;
	bool depth_blit;

	VkRenderPass render_pass;
	VkDescriptorSetLayout set_layout;
	VkPipelineLayout pipeline_layout;
	VkShaderModule vs_triangle, vs_rect, fs_flat;
	map<VvkFsKey, VkShaderModule> fs_cache;
	map<vector<uint64_t>, VkPipeline> pipelines;
	map<UINT32, VkSampler> samplers;
	VkCommandPool command_pool;
	VvkSlot slots[VVK_SLOTS];
	int slot;
	bool recording;

	/* the 3dfx card's buffers */
	VvkImage color[2], depth, present;
	VkFramebuffer framebuffer[2];
	UINT32 width, height;
	int front;
	VvkTexture white;

	/* while recording */
	int pass_target;	/* -1: not in a render pass */
	VkPipeline bound_pipeline;
	VkDescriptorSet bound_set;
	VkBuffer bound_buffer;
	bool pc_valid;
	VvkPushConstants bound_pc;
	VvkBatch batch;

	/* LFB reads: color buffers 0 and 1, and depth */
	VkBuffer readback;
	VkDeviceMemory readback_memory;
	uint8_t *readback_ptr;
	VkDeviceSize readback_size;
	bool cache_valid[3];
	vector<UINT32> cache[3];
} vvk;

static map<const UINT32, VvkTexMap> vvk_textures[2];
static VvkDirtyRange vvk_texture_dirty[2];

static bool vvk_check(VkResult res, const char *what) {
	if (res != VK_SUCCESS) {
		LOG_MSG("VOODOO: Vulkan: %s failed (VkResult %d)", what, (int)res);
		return false;
	}
	return true;
}

template <typename T> static uint64_t vvk_handle(T h) {
	uint64_t u = 0;
	memcpy(&u, &h, sizeof(h) < sizeof(u) ? sizeof(h) : sizeof(u));
	return u;
}

static int vvk_memory_type(uint32_t type_bits, VkMemoryPropertyFlags want) {
	for (uint32_t i = 0; i < vvk.memory_properties.memoryTypeCount; i++)
		if ((type_bits & (1u << i)) && (vvk.memory_properties.memoryTypes[i].propertyFlags & want) == want)
			return (int)i;
	return -1;
}

static bool vvk_allocate(VkMemoryRequirements req, VkMemoryPropertyFlags want, VkMemoryPropertyFlags fallback, VkDeviceMemory &mem) {
	int type = vvk_memory_type(req.memoryTypeBits, want);
	if (type < 0) type = vvk_memory_type(req.memoryTypeBits, fallback);
	if (type < 0) return false;
	VkMemoryAllocateInfo mai = {};
	mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	mai.allocationSize = req.size;
	mai.memoryTypeIndex = (uint32_t)type;
	return vvk_check(vkAllocateMemory(vvk.dev.device, &mai, NULL, &mem), "vkAllocateMemory");
}

static void vvk_destroy_image(VvkImage &img) {
	if (img.view != VK_NULL_HANDLE) vkDestroyImageView(vvk.dev.device, img.view, NULL);
	if (img.image != VK_NULL_HANDLE) vkDestroyImage(vvk.dev.device, img.image, NULL);
	if (img.memory != VK_NULL_HANDLE) vkFreeMemory(vvk.dev.device, img.memory, NULL);
	img = VvkImage();
}

static bool vvk_create_image(VvkImage &img, VkFormat format, UINT32 w, UINT32 h, uint32_t mips,
	VkImageUsageFlags usage, VkImageAspectFlags aspect) {
	VkImageCreateInfo ici = {};
	ici.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	ici.imageType = VK_IMAGE_TYPE_2D;
	ici.format = format;
	ici.extent.width = w;
	ici.extent.height = h;
	ici.extent.depth = 1;
	ici.mipLevels = mips;
	ici.arrayLayers = 1;
	ici.samples = VK_SAMPLE_COUNT_1_BIT;
	ici.tiling = VK_IMAGE_TILING_OPTIMAL;
	ici.usage = usage;
	ici.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	ici.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	if (!vvk_check(vkCreateImage(vvk.dev.device, &ici, NULL, &img.image), "vkCreateImage")) return false;

	VkMemoryRequirements req;
	vkGetImageMemoryRequirements(vvk.dev.device, img.image, &req);
	if (!vvk_allocate(req, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 0, img.memory) ||
	    !vvk_check(vkBindImageMemory(vvk.dev.device, img.image, img.memory, 0), "vkBindImageMemory")) {
		vvk_destroy_image(img);
		return false;
	}

	VkImageViewCreateInfo vci = {};
	vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	vci.image = img.image;
	vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
	vci.format = format;
	vci.subresourceRange.aspectMask = aspect;
	vci.subresourceRange.levelCount = mips;
	vci.subresourceRange.layerCount = 1;
	if (!vvk_check(vkCreateImageView(vvk.dev.device, &vci, NULL, &img.view), "vkCreateImageView")) {
		vvk_destroy_image(img);
		return false;
	}
	return true;
}

static void vvk_destroy_arena(VvkArena &a) {
	if (a.ptr != NULL) vkUnmapMemory(vvk.dev.device, a.memory);
	if (a.buffer != VK_NULL_HANDLE) vkDestroyBuffer(vvk.dev.device, a.buffer, NULL);
	if (a.memory != VK_NULL_HANDLE) vkFreeMemory(vvk.dev.device, a.memory, NULL);
	a = VvkArena();
}

static bool vvk_create_buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkBuffer &buf, VkDeviceMemory &mem, uint8_t *&ptr) {
	VkBufferCreateInfo bci = {};
	bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bci.size = size;
	bci.usage = usage;
	bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	if (!vvk_check(vkCreateBuffer(vvk.dev.device, &bci, NULL, &buf), "vkCreateBuffer")) return false;
	VkMemoryRequirements req;
	vkGetBufferMemoryRequirements(vvk.dev.device, buf, &req);
	void *p = NULL;
	if (!vvk_allocate(req, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_CACHED_BIT,
	                  VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, mem) ||
	    !vvk_check(vkBindBufferMemory(vvk.dev.device, buf, mem, 0), "vkBindBufferMemory") ||
	    !vvk_check(vkMapMemory(vvk.dev.device, mem, 0, VK_WHOLE_SIZE, 0, &p), "vkMapMemory")) {
		vkDestroyBuffer(vvk.dev.device, buf, NULL);
		if (mem != VK_NULL_HANDLE) vkFreeMemory(vvk.dev.device, mem, NULL);
		buf = VK_NULL_HANDLE;
		mem = VK_NULL_HANDLE;
		return false;
	}
	ptr = (uint8_t*)p;
	return true;
}

static void vvk_begin(void);

/* destroyed when the GPU is done with the command buffer being recorded (and the ones before) */
static void vvk_trash_image(VvkImage &img) {
	if (img.image == VK_NULL_HANDLE) return;
	vvk_begin();
	VvkTrash t;
	t.image = img;
	vvk.slots[vvk.slot].trash.push_back(t);
	img = VvkImage();
}

static void vvk_empty_trash(VvkSlot &s) {
	for (size_t i = 0; i < s.trash.size(); i++) {
		vvk_destroy_image(s.trash[i].image);
		if (s.trash[i].framebuffer != VK_NULL_HANDLE) vkDestroyFramebuffer(vvk.dev.device, s.trash[i].framebuffer, NULL);
	}
	s.trash.clear();
}

/*************************************
 *  Command buffers
 *************************************/

static void vvk_barrier(VkImage image, VkImageAspectFlags aspect, uint32_t base_mip, uint32_t mips,
	VkImageLayout from, VkImageLayout to, VkAccessFlags src_access, VkAccessFlags dst_access,
	VkPipelineStageFlags src_stage, VkPipelineStageFlags dst_stage) {
	VkImageMemoryBarrier b = {};
	b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	b.srcAccessMask = src_access;
	b.dstAccessMask = dst_access;
	b.oldLayout = from;
	b.newLayout = to;
	b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	b.image = image;
	b.subresourceRange.aspectMask = aspect;
	b.subresourceRange.baseMipLevel = base_mip;
	b.subresourceRange.levelCount = mips;
	b.subresourceRange.layerCount = 1;
	vkCmdPipelineBarrier(vvk.slots[vvk.slot].cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &b);
}

#define VVK_COLOR_STAGES	(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT)
#define VVK_COLOR_ACCESS	(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)
#define VVK_DEPTH_STAGES	(VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT)
#define VVK_DEPTH_ACCESS	(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)

/* the color and depth buffers stay in their attachment layouts, except while copied */
static void vvk_color_to(VkImage image, bool transfer_src, bool to_transfer) {
	const VkImageLayout t = transfer_src ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	const VkAccessFlags ta = transfer_src ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
	if (to_transfer)
		vvk_barrier(image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, t,
			VVK_COLOR_ACCESS, ta, VVK_COLOR_STAGES, VK_PIPELINE_STAGE_TRANSFER_BIT);
	else
		vvk_barrier(image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, t, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			ta, VVK_COLOR_ACCESS, VK_PIPELINE_STAGE_TRANSFER_BIT, VVK_COLOR_STAGES);
}

static VkImageAspectFlags vvk_depth_aspect(void) {
	return VK_IMAGE_ASPECT_DEPTH_BIT | (vvk.has_stencil ? VK_IMAGE_ASPECT_STENCIL_BIT : 0);
}

static void vvk_depth_to(VkImage image, bool transfer_src, bool to_transfer) {
	const VkImageLayout t = transfer_src ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
	const VkAccessFlags ta = transfer_src ? VK_ACCESS_TRANSFER_READ_BIT : VK_ACCESS_TRANSFER_WRITE_BIT;
	if (to_transfer)
		vvk_barrier(image, vvk_depth_aspect(), 0, 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, t,
			VVK_DEPTH_ACCESS, ta, VVK_DEPTH_STAGES, VK_PIPELINE_STAGE_TRANSFER_BIT);
	else
		vvk_barrier(image, vvk_depth_aspect(), 0, 1, t, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			ta, VVK_DEPTH_ACCESS, VK_PIPELINE_STAGE_TRANSFER_BIT, VVK_DEPTH_STAGES);
}

/* start recording a command buffer, once the GPU is done with what it was used for before */
static void vvk_begin(void) {
	if (vvk.recording) return;
	VvkSlot &s = vvk.slots[vvk.slot];
	if (s.submitted) {
		vkWaitForFences(vvk.dev.device, 1, &s.fence, VK_TRUE, UINT64_MAX);
		s.submitted = false;
	}
	vvk_empty_trash(s);
	for (size_t i = 0; i < s.pools.size(); i++) vkResetDescriptorPool(vvk.dev.device, s.pools[i], 0);
	s.pool = 0;
	s.sets.clear();
	for (size_t i = 0; i < s.arenas.size(); i++) s.arenas[i].used = 0;
	s.arena = 0;

	vkResetCommandBuffer(s.cmd, 0);
	VkCommandBufferBeginInfo bi = {};
	bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	vkBeginCommandBuffer(s.cmd, &bi);
	vvk.recording = true;
	vvk.pass_target = -1;
	vvk.bound_pipeline = VK_NULL_HANDLE;
	vvk.bound_set = VK_NULL_HANDLE;
	vvk.bound_buffer = VK_NULL_HANDLE;
	vvk.pc_valid = false;
	vvk.batch.active = false;
}

static void vvk_flush_batch(void) {
	VvkBatch &b = vvk.batch;
	if (!b.active) return;
	b.active = false;
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	if (vvk.bound_pipeline != b.pipeline) {
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, b.pipeline);
		vvk.bound_pipeline = b.pipeline;
	}
	if (vvk.bound_set != b.set) {
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, vvk.pipeline_layout, 0, 1, &b.set, 0, NULL);
		vvk.bound_set = b.set;
	}
	if (!vvk.pc_valid || memcmp(&vvk.bound_pc, &b.pc, sizeof(b.pc))) {
		vkCmdPushConstants(cmd, vvk.pipeline_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(b.pc), &b.pc);
		vvk.bound_pc = b.pc;
		vvk.pc_valid = true;
	}
	if (vvk.bound_buffer != b.buffer) {
		const VkDeviceSize offset = 0;
		vkCmdBindVertexBuffers(cmd, 0, 1, &b.buffer, &offset);
		vvk.bound_buffer = b.buffer;
	}
	if (b.rect) vkCmdDraw(cmd, 4, b.count, 0, b.first);
	else vkCmdDraw(cmd, b.count, 1, b.first, 0);
}

static void vvk_end_pass(void) {
	vvk_flush_batch();
	if (vvk.pass_target >= 0) {
		vkCmdEndRenderPass(vvk.slots[vvk.slot].cmd);
		vvk.pass_target = -1;
	}
}

/* drawing to color buffer 0 or 1 (and the depth buffer) */
static void vvk_begin_pass(int target) {
	vvk_begin();
	if (vvk.pass_target == target) return;
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	VkRenderPassBeginInfo rpbi = {};
	rpbi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	rpbi.renderPass = vvk.render_pass;
	rpbi.framebuffer = vvk.framebuffer[target];
	rpbi.renderArea.extent.width = vvk.width;
	rpbi.renderArea.extent.height = vvk.height;
	vkCmdBeginRenderPass(cmd, &rpbi, VK_SUBPASS_CONTENTS_INLINE);
	VkViewport vp = {};
	vp.width = (float)vvk.width;
	vp.height = (float)vvk.height;
	vp.maxDepth = 1.0f;
	VkRect2D sc = {};
	sc.extent.width = vvk.width;
	sc.extent.height = vvk.height;
	vkCmdSetViewport(cmd, 0, 1, &vp);
	vkCmdSetScissor(cmd, 0, 1, &sc);
	vvk.pass_target = target;
}

/* submit what was recorded, and wait for the GPU to finish it if asked to */
static void vvk_submit(bool wait) {
	if (!vvk.recording) return;
	vvk_end_pass();
	VvkSlot &s = vvk.slots[vvk.slot];
	vkEndCommandBuffer(s.cmd);
	VkSubmitInfo si = {};
	si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	si.commandBufferCount = 1;
	si.pCommandBuffers = &s.cmd;
	vkResetFences(vvk.dev.device, 1, &s.fence);
	vvk.recording = false;
	if (!vvk_check(vkQueueSubmit(vvk.dev.queue, 1, &si, s.fence), "vkQueueSubmit")) {
		vkDeviceWaitIdle(vvk.dev.device);
		s.submitted = false;
	} else {
		s.submitted = true;
		if (wait) {
			vkWaitForFences(vvk.dev.device, 1, &s.fence, VK_TRUE, UINT64_MAX);
			s.submitted = false;
		}
	}
	vvk.slot = (vvk.slot + 1) % VVK_SLOTS;
}

/* room in the current command buffer's memory, for vertices or a texture upload */
static uint8_t *vvk_alloc(VkDeviceSize size, VkDeviceSize align, VkBuffer &buffer, VkDeviceSize &offset) {
	vvk_begin();
	VvkSlot &s = vvk.slots[vvk.slot];
	for (;;) {
		if (s.arena < s.arenas.size()) {
			VvkArena &a = s.arenas[s.arena];
			const VkDeviceSize o = (a.used + align - 1) / align * align;
			if (o + size <= a.size) {
				a.used = o + size;
				buffer = a.buffer;
				offset = o;
				return a.ptr + o;
			}
			s.arena++;
			continue;
		}
		VvkArena a;
		a.size = size * 2 > 4u << 20 ? size * 2 : 4u << 20;
		if (!vvk_create_buffer(a.size, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
		                       a.buffer, a.memory, a.ptr))
			return NULL;
		s.arenas.push_back(a);
	}
}

/*************************************
 *  Pipelines
 *************************************/

static VkShaderModule vvk_shader(const vector<uint32_t> &code) {
	VkShaderModuleCreateInfo ci = {};
	ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	ci.codeSize = code.size() * 4;
	ci.pCode = code.data();
	VkShaderModule mod = VK_NULL_HANDLE;
	vvk_check(vkCreateShaderModule(vvk.dev.device, &ci, NULL, &mod), "vkCreateShaderModule");
	return mod;
}

/* what a pipeline has besides its shaders */
struct VvkPipelineState {
	bool rect;
	bool blend;
	VkBlendFactor src, dst, src_alpha, dst_alpha;
	bool depth_test;
	VkCompareOp depth_func;
	bool depth_write;
	VkColorComponentFlags color_mask;
	int stencil;	/* 0: none, 1: write 1, 2: pass where 1 */
};

#define VVK_STENCIL_WRITE	1
#define VVK_STENCIL_TEST	2

static VkPipeline vvk_pipeline(VkShaderModule fs, const VvkPipelineState &st) {
	const VkShaderModule vs = st.rect ? vvk.vs_rect : vvk.vs_triangle;
	vector<uint64_t> key;
	key.push_back(vvk_handle(vs));
	key.push_back(vvk_handle(fs));
	key.push_back((uint64_t)st.blend | ((uint64_t)st.src << 8) | ((uint64_t)st.dst << 16) |
		((uint64_t)st.src_alpha << 24) | ((uint64_t)st.dst_alpha << 32));
	key.push_back((uint64_t)st.depth_test | ((uint64_t)st.depth_func << 8) | ((uint64_t)st.depth_write << 16) |
		((uint64_t)st.color_mask << 24) | ((uint64_t)st.stencil << 32));
	map<vector<uint64_t>, VkPipeline>::iterator i = vvk.pipelines.find(key);
	if (i != vvk.pipelines.end()) return i->second;

	VkPipelineShaderStageCreateInfo stages[2] = {};
	stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	stages[0].module = vs;
	stages[0].pName = "main";
	stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	stages[1].module = fs;
	stages[1].pName = "main";

	VkVertexInputBindingDescription vb = {};
	VkVertexInputAttributeDescription va[5] = {};
	VkPipelineVertexInputStateCreateInfo vin = {};
	vin.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vin.vertexBindingDescriptionCount = 1;
	vin.pVertexBindingDescriptions = &vb;
	vin.pVertexAttributeDescriptions = va;
	if (st.rect) {
		/* per instance: vec4 rect, vec4 color, float depth */
		vb.stride = 9 * 4;
		vb.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
		va[0].location = 0; va[0].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[0].offset = 0;
		va[1].location = 1; va[1].format = VK_FORMAT_R32G32B32A32_SFLOAT; va[1].offset = 16;
		va[2].location = 2; va[2].format = VK_FORMAT_R32_SFLOAT; va[2].offset = 32;
		vin.vertexAttributeDescriptionCount = 3;
	} else {
		/* vec3 position, vec4 color, vec3 tc0, vec3 tc1, vec3 extra */
		vb.stride = 16 * 4;
		vb.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		const uint32_t offs[5] = { 0, 12, 28, 40, 52 };
		for (uint32_t a = 0; a < 5; a++) {
			va[a].location = a;
			va[a].format = a == 1 ? VK_FORMAT_R32G32B32A32_SFLOAT : VK_FORMAT_R32G32B32_SFLOAT;
			va[a].offset = offs[a];
		}
		vin.vertexAttributeDescriptionCount = 5;
	}

	VkPipelineInputAssemblyStateCreateInfo ia = {};
	ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	ia.topology = st.rect ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	VkPipelineViewportStateCreateInfo vps = {};
	vps.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	vps.viewportCount = 1;
	vps.scissorCount = 1;
	VkPipelineRasterizationStateCreateInfo rs = {};
	rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rs.polygonMode = VK_POLYGON_MODE_FILL;
	rs.cullMode = VK_CULL_MODE_NONE;
	rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rs.lineWidth = 1.0f;
	VkPipelineMultisampleStateCreateInfo ms = {};
	ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	VkPipelineDepthStencilStateCreateInfo ds = {};
	ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	ds.depthTestEnable = st.depth_test ? VK_TRUE : VK_FALSE;
	ds.depthWriteEnable = st.depth_write ? VK_TRUE : VK_FALSE;
	ds.depthCompareOp = st.depth_func;
	if (st.stencil != 0) {
		ds.stencilTestEnable = VK_TRUE;
		ds.front.failOp = VK_STENCIL_OP_KEEP;
		ds.front.depthFailOp = VK_STENCIL_OP_KEEP;
		ds.front.passOp = st.stencil == VVK_STENCIL_WRITE ? VK_STENCIL_OP_REPLACE : VK_STENCIL_OP_KEEP;
		ds.front.compareOp = st.stencil == VVK_STENCIL_WRITE ? VK_COMPARE_OP_ALWAYS : VK_COMPARE_OP_EQUAL;
		ds.front.compareMask = 1;
		ds.front.writeMask = 1;
		ds.front.reference = 1;
		ds.back = ds.front;
	}
	VkPipelineColorBlendAttachmentState cba = {};
	cba.colorWriteMask = st.color_mask;
	if (st.blend) {
		cba.blendEnable = VK_TRUE;
		cba.srcColorBlendFactor = st.src;
		cba.dstColorBlendFactor = st.dst;
		cba.colorBlendOp = VK_BLEND_OP_ADD;
		cba.srcAlphaBlendFactor = st.src_alpha;
		cba.dstAlphaBlendFactor = st.dst_alpha;
		cba.alphaBlendOp = VK_BLEND_OP_ADD;
	}
	VkPipelineColorBlendStateCreateInfo cb = {};
	cb.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	cb.attachmentCount = 1;
	cb.pAttachments = &cba;
	const VkDynamicState dyn_states[2] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	VkPipelineDynamicStateCreateInfo dyn = {};
	dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dyn.dynamicStateCount = 2;
	dyn.pDynamicStates = dyn_states;

	VkGraphicsPipelineCreateInfo gpci = {};
	gpci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	gpci.stageCount = 2;
	gpci.pStages = stages;
	gpci.pVertexInputState = &vin;
	gpci.pInputAssemblyState = &ia;
	gpci.pViewportState = &vps;
	gpci.pRasterizationState = &rs;
	gpci.pMultisampleState = &ms;
	gpci.pDepthStencilState = &ds;
	gpci.pColorBlendState = &cb;
	gpci.pDynamicState = &dyn;
	gpci.layout = vvk.pipeline_layout;
	gpci.renderPass = vvk.render_pass;
	VkPipeline p = VK_NULL_HANDLE;
	if (!vvk_check(vkCreateGraphicsPipelines(vvk.dev.device, VK_NULL_HANDLE, 1, &gpci, NULL, &p), "vkCreateGraphicsPipelines"))
		return VK_NULL_HANDLE;
	vvk.pipelines[key] = p;
	return p;
}

static VkBlendFactor vvk_src_factor(UINT32 f) {
	static const VkBlendFactor t[16] = {
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_DST_COLOR, VK_BLEND_FACTOR_DST_ALPHA,
		VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_SRC_ALPHA_SATURATE };
	return t[f & 15];
}

static VkBlendFactor vvk_dst_factor(UINT32 f) {
	static const VkBlendFactor t[16] = {
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_SRC_ALPHA, VK_BLEND_FACTOR_SRC_COLOR, VK_BLEND_FACTOR_DST_ALPHA,
		VK_BLEND_FACTOR_ONE, VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA, VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
		VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO, VK_BLEND_FACTOR_ZERO,
		VK_BLEND_FACTOR_SRC_COLOR /* A_COLORBEFOREFOG */ };
	return t[f & 15];
}

/* blending from alphaMode, as VOGL_SetAlphaMode() is called with */
static void vvk_blend_state(VvkPipelineState &st, UINT32 ALPHAMODE) {
	st.blend = ALPHAMODE_ALPHABLEND(ALPHAMODE) != 0;
	st.src = st.blend ? vvk_src_factor(ALPHAMODE_SRCRGBBLEND(ALPHAMODE)) : VK_BLEND_FACTOR_ONE;
	st.dst = st.blend ? vvk_dst_factor(ALPHAMODE_DSTRGBBLEND(ALPHAMODE)) : VK_BLEND_FACTOR_ZERO;
	st.src_alpha = (st.blend && ALPHAMODE_SRCALPHABLEND(ALPHAMODE) == 4) ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
	st.dst_alpha = (st.blend && ALPHAMODE_DSTALPHABLEND(ALPHAMODE) == 4) ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
	if (!st.blend) {
		st.src_alpha = VK_BLEND_FACTOR_ONE;
		st.dst_alpha = VK_BLEND_FACTOR_ZERO;
	}
}

static VvkPipelineState vvk_default_state(void) {
	VvkPipelineState st;
	memset(&st, 0, sizeof(st));
	st.src = st.src_alpha = VK_BLEND_FACTOR_ONE;
	st.dst = st.dst_alpha = VK_BLEND_FACTOR_ZERO;
	st.depth_func = VK_COMPARE_OP_ALWAYS;
	st.color_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	return st;
}

static VkColorComponentFlags vvk_color_mask(bool rgb, bool alpha) {
	return (rgb ? (VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT) : 0) |
		(alpha ? VK_COLOR_COMPONENT_A_BIT : 0);
}

/* the push constants from the registers, as ogl_shaders() sets the uniforms */
static void vvk_push_constants(VvkPushConstants &pc) {
	memset(&pc, 0, sizeof(pc));
	const voodoo_reg *r = v->reg;
	pc.c0[0] = r[color0].rgb.r / 255.0f; pc.c0[1] = r[color0].rgb.g / 255.0f;
	pc.c0[2] = r[color0].rgb.b / 255.0f; pc.c0[3] = r[color0].rgb.a / 255.0f;
	pc.c1[0] = r[color1].rgb.r / 255.0f; pc.c1[1] = r[color1].rgb.g / 255.0f;
	pc.c1[2] = r[color1].rgb.b / 255.0f; pc.c1[3] = r[color1].rgb.a / 255.0f;
	pc.chromakey[0] = r[chromaKey].rgb.r / 255.0f; pc.chromakey[1] = r[chromaKey].rgb.g / 255.0f;
	pc.chromakey[2] = r[chromaKey].rgb.b / 255.0f;
	pc.fogcolor[0] = r[fogColor].rgb.r / 255.0f; pc.fogcolor[1] = r[fogColor].rgb.g / 255.0f;
	pc.fogcolor[2] = r[fogColor].rgb.b / 255.0f; pc.fogcolor[3] = 1.0f;
	pc.alpharef = r[alphaMode].rgb.a / 255.0f;
	pc.zacolor = (float)((UINT16)r[zaColor].u) / 65535.0f;
}

/* add a draw (vertices or rectangle instances) to the render pass, with the ones before
 * it if the state is the same */
static void vvk_draw(int target, VkPipeline pipeline, VkDescriptorSet set, const VvkPushConstants &pc,
	bool rect, const void *data, uint32_t count) {
	if (pipeline == VK_NULL_HANDLE) return;
	vvk_begin_pass(target);
	const VkDeviceSize stride = rect ? 9 * 4 : 16 * 4;
	VkBuffer buffer;
	VkDeviceSize offset;
	uint8_t *p = vvk_alloc(stride * count, stride, buffer, offset);
	if (p == NULL) return;
	memcpy(p, data, (size_t)(stride * count));
	const uint32_t first = (uint32_t)(offset / stride);

	VvkBatch &b = vvk.batch;
	if (b.active && b.rect == rect && b.pipeline == pipeline && b.set == set && b.buffer == buffer &&
	    b.first + b.count == first && !memcmp(&b.pc, &pc, sizeof(pc))) {
		b.count += count;
		return;
	}
	vvk_flush_batch();
	b.active = true;
	b.rect = rect;
	b.pipeline = pipeline;
	b.set = set;
	b.pc = pc;
	b.buffer = buffer;
	b.first = first;
	b.count = count;
}

/*************************************
 *  Buffers
 *************************************/

static void vvk_invalidate_reads(void) {
	vvk.cache_valid[0] = vvk.cache_valid[1] = vvk.cache_valid[2] = false;
}

static void vvk_destroy_targets(void) {
	for (int i = 0; i < 2; i++) {
		if (vvk.framebuffer[i] != VK_NULL_HANDLE) vkDestroyFramebuffer(vvk.dev.device, vvk.framebuffer[i], NULL);
		vvk.framebuffer[i] = VK_NULL_HANDLE;
		vvk_destroy_image(vvk.color[i]);
	}
	vvk_destroy_image(vvk.depth);
	vvk_destroy_image(vvk.present);
	vvk.width = vvk.height = 0;
}

/* make the buffers (none may exist), the GPU must be idle */
static bool vvk_create_targets(UINT32 w, UINT32 h) {
	for (int i = 0; i < 2; i++)
		if (!vvk_create_image(vvk.color[i], VK_FORMAT_R8G8B8A8_UNORM, w, h, 1,
			VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT)) return false;
	if (!vvk_create_image(vvk.depth, vvk.depth_format, w, h, 1,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
		vvk_depth_aspect())) return false;
	if (!vvk_create_image(vvk.present, VK_FORMAT_R8G8B8A8_UNORM, w, h, 1,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT)) return false;
	for (int i = 0; i < 2; i++) {
		const VkImageView views[2] = { vvk.color[i].view, vvk.depth.view };
		VkFramebufferCreateInfo fci = {};
		fci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		fci.renderPass = vvk.render_pass;
		fci.attachmentCount = 2;
		fci.pAttachments = views;
		fci.width = w;
		fci.height = h;
		fci.layers = 1;
		if (!vvk_check(vkCreateFramebuffer(vvk.dev.device, &fci, NULL, &vvk.framebuffer[i]), "vkCreateFramebuffer")) return false;
	}
	vvk.width = w;
	vvk.height = h;
	vvk_invalidate_reads();
	return true;
}

/* new buffers are black (depth 1.0), and move to the layouts they stay in */
static void vvk_init_targets(bool colors, bool depth) {
	vvk_begin();
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	VkImageSubresourceRange cr = {};
	cr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cr.levelCount = 1;
	cr.layerCount = 1;
	VkClearColorValue black = {};
	for (int i = 0; colors && i < 2; i++) {
		vvk_barrier(vvk.color[i].image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdClearColorImage(cmd, vvk.color[i].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &black, 1, &cr);
		vvk_color_to(vvk.color[i].image, false, false);
	}
	if (depth) {
		VkImageSubresourceRange dr = cr;
		dr.aspectMask = vvk_depth_aspect();
		VkClearDepthStencilValue one = { 1.0f, 0 };
		vvk_barrier(vvk.depth.image, vvk_depth_aspect(), 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
			VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		vkCmdClearDepthStencilImage(cmd, vvk.depth.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &one, 1, &dr);
		vvk_depth_to(vvk.depth.image, false, false);
	}
	/* the image the output shows */
	VkClearColorValue opaque = {};
	opaque.float32[3] = 1.0f;
	vvk_barrier(vvk.present.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vkCmdClearColorImage(cmd, vvk.present.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &opaque, 1, &cr);
	vvk_barrier(vvk.present.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
}

/* change the render resolution, keeping what the buffers show (scaled) */
static bool vvk_resize_targets(UINT32 w, UINT32 h) {
	vvk_submit(true);
	/* the output may still be drawing the old image */
	vkDeviceWaitIdle(vvk.dev.device);

	VvkImage old_color[2] = { vvk.color[0], vvk.color[1] };
	VvkImage old_depth = vvk.depth, old_present = vvk.present;
	VkFramebuffer old_fb[2] = { vvk.framebuffer[0], vvk.framebuffer[1] };
	const UINT32 old_w = vvk.width, old_h = vvk.height;
	vvk.color[0] = vvk.color[1] = vvk.depth = vvk.present = VvkImage();
	vvk.framebuffer[0] = vvk.framebuffer[1] = VK_NULL_HANDLE;

	bool ok = vvk_create_targets(w, h);
	if (ok) {
		vvk_init_targets(false, !vvk.depth_blit);
		const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
		VkImageBlit blit = {};
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[1].x = (int32_t)old_w;
		blit.srcOffsets[1].y = (int32_t)old_h;
		blit.srcOffsets[1].z = 1;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[1].x = (int32_t)w;
		blit.dstOffsets[1].y = (int32_t)h;
		blit.dstOffsets[1].z = 1;
		/* without filtering, the pixels keep their colors (for LFB reads) */
		for (int i = 0; i < 2; i++) {
			blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			vvk_color_to(old_color[i].image, true, true);
			vvk_barrier(vvk.color[i].image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vkCmdBlitImage(cmd, old_color[i].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vvk.color[i].image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
			vvk_color_to(vvk.color[i].image, false, false);
		}
		if (vvk.depth_blit) {
			/* depth (and stencil) can only be copied without filtering */
			blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = vvk_depth_aspect();
			vvk_depth_to(old_depth.image, true, true);
			vvk_barrier(vvk.depth.image, vvk_depth_aspect(), 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
				VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
			vkCmdBlitImage(cmd, old_depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vvk.depth.image,
				VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
			vvk_depth_to(vvk.depth.image, false, false);
		}
		vvk_submit(true);
	} else {
		vvk_destroy_targets();
		vvk.color[0] = old_color[0];
		vvk.color[1] = old_color[1];
		vvk.depth = old_depth;
		vvk.present = old_present;
		vvk.framebuffer[0] = old_fb[0];
		vvk.framebuffer[1] = old_fb[1];
		vvk.width = old_w;
		vvk.height = old_h;
		return false;
	}

	for (int i = 0; i < 2; i++) {
		vkDestroyFramebuffer(vvk.dev.device, old_fb[i], NULL);
		vvk_destroy_image(old_color[i]);
	}
	vvk_destroy_image(old_depth);
	vvk_destroy_image(old_present);
	OUTPUT_VULKAN_SetExternalImage(vvk.present.view, w, h);
	LOG_MSG("VOODOO: Vulkan: render resolution %u:%u", (unsigned int)w, (unsigned int)h);
	return true;
}

/* follow the resolution setting and the display area, between frames */
static void vvk_check_render_size(void) {
	Voodoo_HW_PollResolutionSetting();
	UINT32 w, h;
	Voodoo_HW_RenderSize(vvk.max_size, w, h);
	if (vvk.width != 0 && (w != vvk.width || h != vvk.height))
		vvk_resize_targets(w, h);
}

/* the render resolution pixels [p0, p1) that 3dfx video mode pixel i covers, as the OpenGL
 * emulation's voodoo_ogl_lfb_span() */
static void vvk_lfb_span(int i, UINT32 render_size, UINT32 mode_size, int &p0, int &p1) {
	const double d = (double)render_size / (double)mode_size;
	p0 = (int)ceil(i * d - 0.5);
	p1 = (int)ceil((i + 1) * d - 0.5);
	if (p1 <= p0) p1 = p0 + 1;
	if (p0 >= (int)render_size) p0 = (int)render_size - 1;
	if (p1 > (int)render_size) p1 = (int)render_size;
}

static int vvk_target(bool front) {
	return front ? vvk.front : (vvk.front ^ 1);
}

/*************************************
 *  Textures
 *************************************/

static void vvk_destroy_texture(VvkTexture *t) {
	if (t == NULL) return;
	vvk_trash_image(t->image);
	delete t;
}

static void vvk_destroy_texmap(VvkTexMap &m) {
	if (!m.by_palette.empty()) {
		for (map<UINT32, VvkTexture*>::iterator i = m.by_palette.begin(); i != m.by_palette.end(); ++i)
			vvk_destroy_texture(i->second);
		m.by_palette.clear();
	} else {
		vvk_destroy_texture(m.current);
	}
	m.current = NULL;
}

static void vvk_destroy_textures(void) {
	for (int j = 0; j < 2; j++) {
		for (map<const UINT32, VvkTexMap>::iterator t = vvk_textures[j].begin(); t != vvk_textures[j].end(); ++t)
			vvk_destroy_texmap(t->second);
		vvk_textures[j].clear();
		vvk_texture_dirty[j].pending = false;
	}
}

static bool vvk_palette_format(UINT32 format) {
	return format == 1 || format == 9 || format == 5 || format == 14;
}

/* the palette (or NCC table) a texture is converted with, the OpenGL emulation uses a CRC */
static UINT32 vvk_palette_sum(UINT32 tmunum, UINT32 texmode) {
	const UINT32 format = TEXMODE_FORMAT(texmode);
	const rgb_t *colors = (format & 7) == 1 ? v->tmu[tmunum].ncc[TEXMODE_NCC_TABLE_SELECT(texmode)].texel : v->tmu[tmunum].palette;
	UINT32 h = 2166136261u;
	for (int i = 0; i < 256; i++) h = (h ^ colors[i]) * 16777619u;
	return h;
}

/* drop textures that texture memory writes changed (before the next draw) */
static void vvk_flush_texture_writes(int TMU) {
	VvkDirtyRange &dirty = vvk_texture_dirty[TMU];
	if (!dirty.pending) return;
	const UINT32 mask = v->tmu[TMU].mask;
	for (map<const UINT32, VvkTexMap>::iterator t = vvk_textures[TMU].begin(); t != vvk_textures[TMU].end();) {
		const UINT32 size = t->second.width * t->second.height * (t->second.format < 8 ? 1 : 2);
		const UINT32 last = t->first + size - 1;
		const bool overlaps = last <= mask
			? t->first <= dirty.last && last >= dirty.first
			: t->first <= dirty.last || (last & mask) >= dirty.first;
		if (overlaps) {
			vvk_destroy_texmap(t->second);
			vvk_textures[TMU].erase(t++);
		} else {
			++t;
		}
	}
	dirty.pending = false;
}

/* a texture image from texture memory, with mipmaps (the OpenGL emulation's conversion) */
static VvkTexture *vvk_upload_texture(int j, UINT32 TEXMODE, UINT32 texbase, UINT32 width, UINT32 height) {
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	uint32_t mips = 1;
	while (((width | height) >> mips) != 0) mips++;

	VvkTexture *t = new VvkTexture;
	t->width = width;
	t->height = height;
	if (!vvk_create_image(t->image, VK_FORMAT_B8G8R8A8_UNORM, width, height, mips,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
		VK_IMAGE_ASPECT_COLOR_BIT)) {
		delete t;
		return NULL;
	}

	VkBuffer buffer;
	VkDeviceSize offset;
	UINT32 *texrgbp = (UINT32*)vvk_alloc((VkDeviceSize)width * height * 4, 16, buffer, offset);
	if (texrgbp == NULL) {
		vvk_destroy_image(t->image);
		delete t;
		return NULL;
	}
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

	/* outside of a render pass: copy to the first level, and make the others from it */
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	vvk_barrier(t->image.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mips, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	VkBufferImageCopy copy = {};
	copy.bufferOffset = offset;
	copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = width;
	copy.imageExtent.height = height;
	copy.imageExtent.depth = 1;
	vkCmdCopyBufferToImage(cmd, buffer, t->image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	int32_t mw = (int32_t)width, mh = (int32_t)height;
	for (uint32_t m = 1; m < mips; m++) {
		vvk_barrier(t->image.image, VK_IMAGE_ASPECT_COLOR_BIT, m - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
		VkImageBlit blit = {};
		blit.srcSubresource.aspectMask = blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blit.srcSubresource.mipLevel = m - 1;
		blit.srcSubresource.layerCount = 1;
		blit.srcOffsets[1].x = mw;
		blit.srcOffsets[1].y = mh;
		blit.srcOffsets[1].z = 1;
		mw = mw > 1 ? mw / 2 : 1;
		mh = mh > 1 ? mh / 2 : 1;
		blit.dstSubresource.mipLevel = m;
		blit.dstSubresource.layerCount = 1;
		blit.dstOffsets[1].x = mw;
		blit.dstOffsets[1].y = mh;
		blit.dstOffsets[1].z = 1;
		vkCmdBlitImage(cmd, t->image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, t->image.image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);
	}
	if (mips > 1)
		vvk_barrier(t->image.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, mips - 1, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT, VK_ACCESS_SHADER_READ_BIT,
			VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	vvk_barrier(t->image.image, VK_IMAGE_ASPECT_COLOR_BIT, mips - 1, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	return t;
}

/* the texture a TMU uses, like ogl_cache_texture() */
static VvkTexture *vvk_cache_texture(const poly_extra_data *extra, UINT32 j) {
	vvk_flush_texture_writes((int)j);
	const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
	const UINT32 format = TEXMODE_FORMAT(TEXMODE);
	const UINT32 ncc_table = (format & 7) == 1 ? TEXMODE_NCC_TABLE_SELECT(TEXMODE) : 0;

	UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
	if (!((v->tmu[j].lodmask >> ilod) & 1)) ilod++;
	const UINT32 texbase = v->tmu[j].lodoffset[ilod];
	const UINT32 width = (v->tmu[j].wmask >> ilod) + 1;
	const UINT32 height = (v->tmu[j].hmask >> ilod) + 1;

	map<const UINT32, VvkTexMap>::iterator t = vvk_textures[j].find(texbase);
	/* a RAM address can be reused with a different texture layout */
	if (t != vvk_textures[j].end() &&
	    (t->second.format != format || t->second.width != width || t->second.height != height ||
	     t->second.ilod != ilod || t->second.lodmask != v->tmu[j].lodmask || t->second.ncc_table != ncc_table)) {
		vvk_destroy_texmap(t->second);
		vvk_textures[j].erase(t);
		t = vvk_textures[j].end();
	}

	UINT32 palsum = 0;
	if (t != vvk_textures[j].end()) {
		if (t->second.valid_pal) return t->second.current;
		palsum = vvk_palette_sum(j, TEXMODE);
		map<UINT32, VvkTexture*>::iterator u = t->second.by_palette.find(palsum);
		t->second.valid_pal = true;
		if (u != t->second.by_palette.end()) {
			t->second.current = u->second;
			return u->second;
		}
		if (t->second.by_palette.size() > 8) {
			for (u = t->second.by_palette.begin(); u != t->second.by_palette.end(); ++u)
				vvk_destroy_texture(u->second);
			t->second.by_palette.clear();
		}
	} else if (vvk_palette_format(format)) {
		palsum = vvk_palette_sum(j, TEXMODE);
	}

	VvkTexture *tex = vvk_upload_texture((int)j, TEXMODE, texbase, width, height);
	if (tex == NULL) return NULL;
	if (t == vvk_textures[j].end()) {
		VvkTexMap m;
		m.valid_pal = true;
		m.format = format;
		m.width = width;
		m.height = height;
		m.ilod = ilod;
		m.lodmask = v->tmu[j].lodmask;
		m.ncc_table = ncc_table;
		m.current = tex;
		if (vvk_palette_format(format)) m.by_palette[palsum] = tex;
		vvk_textures[j][texbase] = m;
	} else {
		t->second.current = tex;
		t->second.by_palette[palsum] = tex;
	}
	return tex;
}

/* the sampler for a TMU's filtering and clamping, like voodoo_ogl_draw_triangle() sets them */
static VkSampler vvk_sampler(UINT32 TEXMODE, bool mipmaps) {
	const UINT32 key = (TEXMODE_MINIFICATION_FILTER(TEXMODE) ? 1u : 0u) | (TEXMODE_MAGNIFICATION_FILTER(TEXMODE) ? 2u : 0u) |
		(mipmaps ? 4u : 0u) | (TEXMODE_TRILINEAR(TEXMODE) ? 8u : 0u) |
		(TEXMODE_CLAMP_S(TEXMODE) ? 16u : 0u) | (TEXMODE_CLAMP_T(TEXMODE) ? 32u : 0u);
	map<UINT32, VkSampler>::iterator i = vvk.samplers.find(key);
	if (i != vvk.samplers.end()) return i->second;
	VkSamplerCreateInfo sci = {};
	sci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sci.minFilter = (key & 1) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	sci.magFilter = (key & 2) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
	sci.mipmapMode = (key & 8) ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;
	sci.addressModeU = (key & 16) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sci.addressModeV = (key & 32) ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sci.minLod = 0.0f;
	sci.maxLod = (key & 4) ? 1000.0f : 0.0f;
	VkSampler s = VK_NULL_HANDLE;
	if (!vvk_check(vkCreateSampler(vvk.dev.device, &sci, NULL, &s), "vkCreateSampler")) return VK_NULL_HANDLE;
	vvk.samplers[key] = s;
	return s;
}

/* the descriptor set with these two textures, made once per command buffer */
static VkDescriptorSet vvk_descriptor_set(VkImageView view0, VkSampler s0, VkImageView view1, VkSampler s1) {
	vvk_begin();	/* (which resets the descriptor pools of the command buffer it starts) */
	VvkSlot &slot = vvk.slots[vvk.slot];
	vector<uint64_t> key;
	key.push_back(vvk_handle(view0));
	key.push_back(vvk_handle(s0));
	key.push_back(vvk_handle(view1));
	key.push_back(vvk_handle(s1));
	map<vector<uint64_t>, VkDescriptorSet>::iterator i = slot.sets.find(key);
	if (i != slot.sets.end()) return i->second;

	VkDescriptorSet set = VK_NULL_HANDLE;
	for (;;) {
		if (slot.pool >= slot.pools.size()) {
			VkDescriptorPoolSize ps = {};
			ps.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			ps.descriptorCount = 1024;
			VkDescriptorPoolCreateInfo dpci = {};
			dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			dpci.maxSets = 512;
			dpci.poolSizeCount = 1;
			dpci.pPoolSizes = &ps;
			VkDescriptorPool pool = VK_NULL_HANDLE;
			if (!vvk_check(vkCreateDescriptorPool(vvk.dev.device, &dpci, NULL, &pool), "vkCreateDescriptorPool"))
				return VK_NULL_HANDLE;
			slot.pools.push_back(pool);
		}
		VkDescriptorSetAllocateInfo dsai = {};
		dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		dsai.descriptorPool = slot.pools[slot.pool];
		dsai.descriptorSetCount = 1;
		dsai.pSetLayouts = &vvk.set_layout;
		if (vkAllocateDescriptorSets(vvk.dev.device, &dsai, &set) == VK_SUCCESS) break;
		slot.pool++;	/* full, the next one */
	}

	VkDescriptorImageInfo dii[2] = {};
	dii[0].sampler = s0;
	dii[0].imageView = view0;
	dii[0].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	dii[1].sampler = s1;
	dii[1].imageView = view1;
	dii[1].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	VkWriteDescriptorSet w = {};
	w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	w.dstSet = set;
	w.dstBinding = 0;
	w.descriptorCount = 2;
	w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	w.pImageInfo = dii;
	vkUpdateDescriptorSets(vvk.dev.device, 1, &w, 0, NULL);
	slot.sets[key] = set;
	return set;
}

static VkDescriptorSet vvk_no_textures(void) {
	VkSampler s = vvk_sampler(0, false);
	return vvk_descriptor_set(vvk.white.image.view, s, vvk.white.image.view, s);
}

/*************************************
 *  Setup
 *************************************/

static bool vvk_load_functions(void) {
#define VKF(name) name = (PFN_##name)vvk.dev.GetInstanceProcAddr(vvk.dev.instance, #name); if (name == NULL) { LOG_MSG("VOODOO: Vulkan: missing " #name); return false; }
	VVK_INSTANCE_FUNCS
#undef VKF
#define VKF(name) name = (PFN_##name)vkGetDeviceProcAddr(vvk.dev.device, #name); if (name == NULL) { LOG_MSG("VOODOO: Vulkan: missing " #name); return false; }
	VVK_DEVICE_FUNCS
#undef VKF
	return true;
}

static bool vvk_choose_formats(void) {
	static const VkFormat with_stencil[] = { VK_FORMAT_D24_UNORM_S8_UINT, VK_FORMAT_D32_SFLOAT_S8_UINT };
	static const VkFormat depth_only[] = { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D16_UNORM };
	vvk.depth_format = VK_FORMAT_UNDEFINED;
	for (int pass = 0; pass < 2 && vvk.depth_format == VK_FORMAT_UNDEFINED; pass++) {
		const VkFormat *list = pass == 0 ? with_stencil : depth_only;
		for (int i = 0; i < 2; i++) {
			VkFormatProperties fp;
			vkGetPhysicalDeviceFormatProperties(vvk.dev.physical, list[i], &fp);
			if (fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
				vvk.depth_format = list[i];
				vvk.has_stencil = pass == 0;
				const VkFormatFeatureFlags blit = VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT;
				vvk.depth_blit = (fp.optimalTilingFeatures & blit) == blit;
				break;
			}
		}
	}
	if (vvk.depth_format == VK_FORMAT_UNDEFINED) {
		LOG_MSG("VOODOO: Vulkan: no depth buffer format");
		return false;
	}
	if (!vvk.has_stencil) LOG_MSG("VOODOO: Vulkan: no stencil buffer");
	return true;
}

static bool vvk_create_device_objects(void) {
	/* one render pass for everything: load and keep what is there */
	VkAttachmentDescription att[2] = {};
	att[0].format = VK_FORMAT_R8G8B8A8_UNORM;
	att[0].samples = VK_SAMPLE_COUNT_1_BIT;
	att[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	att[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	att[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	att[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	att[1] = att[0];
	att[1].format = vvk.depth_format;
	att[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
	att[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE;
	att[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	att[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	VkAttachmentReference cref = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
	VkAttachmentReference dref = { 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
	VkSubpassDescription sub = {};
	sub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	sub.colorAttachmentCount = 1;
	sub.pColorAttachments = &cref;
	sub.pDepthStencilAttachment = &dref;
	/* render passes one after another, and copies and texture uploads in between */
	VkSubpassDependency dep[2] = {};
	dep[0].srcSubpass = VK_SUBPASS_EXTERNAL;
	dep[0].dstSubpass = 0;
	dep[0].srcStageMask = VVK_COLOR_STAGES | VVK_DEPTH_STAGES | VK_PIPELINE_STAGE_TRANSFER_BIT;
	dep[0].dstStageMask = VVK_COLOR_STAGES | VVK_DEPTH_STAGES | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
	dep[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	dep[0].dstAccessMask = VVK_COLOR_ACCESS | VVK_DEPTH_ACCESS | VK_ACCESS_SHADER_READ_BIT;
	dep[1].srcSubpass = 0;
	dep[1].dstSubpass = VK_SUBPASS_EXTERNAL;
	dep[1].srcStageMask = VVK_COLOR_STAGES | VVK_DEPTH_STAGES;
	dep[1].dstStageMask = VVK_COLOR_STAGES | VVK_DEPTH_STAGES | VK_PIPELINE_STAGE_TRANSFER_BIT;
	dep[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	dep[1].dstAccessMask = VVK_COLOR_ACCESS | VVK_DEPTH_ACCESS | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT;
	VkRenderPassCreateInfo rpci = {};
	rpci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	rpci.attachmentCount = 2;
	rpci.pAttachments = att;
	rpci.subpassCount = 1;
	rpci.pSubpasses = &sub;
	rpci.dependencyCount = 2;
	rpci.pDependencies = dep;
	if (!vvk_check(vkCreateRenderPass(vvk.dev.device, &rpci, NULL, &vvk.render_pass), "vkCreateRenderPass")) return false;

	VkDescriptorSetLayoutBinding bind[2] = {};
	for (uint32_t i = 0; i < 2; i++) {
		bind[i].binding = i;
		bind[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		bind[i].descriptorCount = 1;
		bind[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	}
	VkDescriptorSetLayoutCreateInfo dslci = {};
	dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	dslci.bindingCount = 2;
	dslci.pBindings = bind;
	if (!vvk_check(vkCreateDescriptorSetLayout(vvk.dev.device, &dslci, NULL, &vvk.set_layout), "vkCreateDescriptorSetLayout")) return false;

	VkPushConstantRange pcr = {};
	pcr.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	pcr.size = sizeof(VvkPushConstants);
	VkPipelineLayoutCreateInfo plci = {};
	plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	plci.setLayoutCount = 1;
	plci.pSetLayouts = &vvk.set_layout;
	plci.pushConstantRangeCount = 1;
	plci.pPushConstantRanges = &pcr;
	if (!vvk_check(vkCreatePipelineLayout(vvk.dev.device, &plci, NULL, &vvk.pipeline_layout), "vkCreatePipelineLayout")) return false;

	vvk.vs_triangle = vvk_shader(vvk_build_triangle_vs());
	vvk.vs_rect = vvk_shader(vvk_build_rect_vs());
	vvk.fs_flat = vvk_shader(vvk_build_flat_fs());
	if (vvk.vs_triangle == VK_NULL_HANDLE || vvk.vs_rect == VK_NULL_HANDLE || vvk.fs_flat == VK_NULL_HANDLE) return false;

	VkCommandPoolCreateInfo cpci = {};
	cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	cpci.queueFamilyIndex = vvk.dev.queue_family;
	if (!vvk_check(vkCreateCommandPool(vvk.dev.device, &cpci, NULL, &vvk.command_pool), "vkCreateCommandPool")) return false;
	for (int i = 0; i < VVK_SLOTS; i++) {
		VkCommandBufferAllocateInfo cbai = {};
		cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cbai.commandPool = vvk.command_pool;
		cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cbai.commandBufferCount = 1;
		if (!vvk_check(vkAllocateCommandBuffers(vvk.dev.device, &cbai, &vvk.slots[i].cmd), "vkAllocateCommandBuffers")) return false;
		VkFenceCreateInfo fci = {};
		fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		if (!vvk_check(vkCreateFence(vvk.dev.device, &fci, NULL, &vvk.slots[i].fence), "vkCreateFence")) return false;
		vvk.slots[i].submitted = false;
	}
	vvk.slot = 0;
	vvk.recording = false;

	/* sampled where a TMU is not used */
	if (!vvk_create_image(vvk.white.image, VK_FORMAT_B8G8R8A8_UNORM, 1, 1, 1,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT)) return false;
	vvk.white.width = vvk.white.height = 1;
	vvk_begin();
	VkImageSubresourceRange cr = {};
	cr.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	cr.levelCount = 1;
	cr.layerCount = 1;
	VkClearColorValue white;
	white.float32[0] = white.float32[1] = white.float32[2] = white.float32[3] = 1.0f;
	vvk_barrier(vvk.white.image.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 0, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	vkCmdClearColorImage(vvk.slots[vvk.slot].cmd, vvk.white.image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &white, 1, &cr);
	vvk_barrier(vvk.white.image.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	return true;
}

/* everything, when the 3dfx output turns off or the output's device goes away */
static void vvk_destroy_all(void) {
	if (vvk.dev.device == VK_NULL_HANDLE) return;
	if (vvk.recording) vvk_submit(false);
	vkDeviceWaitIdle(vvk.dev.device);
	OUTPUT_VULKAN_SetExternalImage(VK_NULL_HANDLE, 0, 0);

	vvk_destroy_textures();
	for (int i = 0; i < VVK_SLOTS; i++) {
		VvkSlot &s = vvk.slots[i];
		vvk_empty_trash(s);
		for (size_t a = 0; a < s.arenas.size(); a++) vvk_destroy_arena(s.arenas[a]);
		for (size_t p = 0; p < s.pools.size(); p++) vkDestroyDescriptorPool(vvk.dev.device, s.pools[p], NULL);
		if (s.fence != VK_NULL_HANDLE) vkDestroyFence(vvk.dev.device, s.fence, NULL);
		s = VvkSlot();
	}
	vvk_destroy_targets();
	vvk_destroy_image(vvk.white.image);
	if (vvk.readback != VK_NULL_HANDLE) {
		vkUnmapMemory(vvk.dev.device, vvk.readback_memory);
		vkDestroyBuffer(vvk.dev.device, vvk.readback, NULL);
		vkFreeMemory(vvk.dev.device, vvk.readback_memory, NULL);
	}
	vvk.readback = VK_NULL_HANDLE;
	vvk.readback_memory = VK_NULL_HANDLE;
	vvk.readback_ptr = NULL;
	vvk.readback_size = 0;
	for (map<vector<uint64_t>, VkPipeline>::iterator i = vvk.pipelines.begin(); i != vvk.pipelines.end(); ++i)
		vkDestroyPipeline(vvk.dev.device, i->second, NULL);
	vvk.pipelines.clear();
	for (map<VvkFsKey, VkShaderModule>::iterator i = vvk.fs_cache.begin(); i != vvk.fs_cache.end(); ++i)
		vkDestroyShaderModule(vvk.dev.device, i->second, NULL);
	vvk.fs_cache.clear();
	for (map<UINT32, VkSampler>::iterator i = vvk.samplers.begin(); i != vvk.samplers.end(); ++i)
		vkDestroySampler(vvk.dev.device, i->second, NULL);
	vvk.samplers.clear();
	if (vvk.vs_triangle != VK_NULL_HANDLE) vkDestroyShaderModule(vvk.dev.device, vvk.vs_triangle, NULL);
	if (vvk.vs_rect != VK_NULL_HANDLE) vkDestroyShaderModule(vvk.dev.device, vvk.vs_rect, NULL);
	if (vvk.fs_flat != VK_NULL_HANDLE) vkDestroyShaderModule(vvk.dev.device, vvk.fs_flat, NULL);
	if (vvk.command_pool != VK_NULL_HANDLE) vkDestroyCommandPool(vvk.dev.device, vvk.command_pool, NULL);
	if (vvk.pipeline_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vvk.dev.device, vvk.pipeline_layout, NULL);
	if (vvk.set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vvk.dev.device, vvk.set_layout, NULL);
	if (vvk.render_pass != VK_NULL_HANDLE) vkDestroyRenderPass(vvk.dev.device, vvk.render_pass, NULL);
	vvk.vs_triangle = vvk.vs_rect = vvk.fs_flat = VK_NULL_HANDLE;
	vvk.command_pool = VK_NULL_HANDLE;
	vvk.pipeline_layout = VK_NULL_HANDLE;
	vvk.set_layout = VK_NULL_HANDLE;
	vvk.render_pass = VK_NULL_HANDLE;
	vvk.recording = false;
	vvk.ready = false;
	memset(&vvk.dev, 0, sizeof(vvk.dev));
}

/* the 3dfx emulation's functions do nothing unless it can draw */
static bool vvk_enter(void) {
	if (vvk.ready) return true;
	if (!vvk.warned) {
		LOG_MSG("VOODOO: Vulkan: not drawing, the 3dfx emulation needs the Vulkan output");
		vvk.warned = true;
	}
	return false;
}

/* (re)make the buffers for the 3dfx video mode, and show it in the output */
static bool vvk_reset_videomode(void) {
	OUTPUT_VULKAN_SetExternalImage(VK_NULL_HANDLE, 0, 0);
	vvk_submit(true);
	vkDeviceWaitIdle(vvk.dev.device);
	vvk_destroy_targets();

	/* the output shows the 3dfx frame like any video mode (sized for the 3dfx video mode,
	 * whatever the render resolution is). This comes first, for the size of the display area. */
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);

	UINT32 w, h;
	Voodoo_HW_RenderSize(vvk.max_size, w, h);
	if (!vvk_create_targets(w, h)) {
		vvk_destroy_targets();
		return false;
	}
	vvk_init_targets(true, true);
	vvk_submit(true);
	vvk.front = 0;
	OUTPUT_VULKAN_SetExternalImage(vvk.present.view, w, h);
	LOG_MSG("VOODOO: Vulkan: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, w, h);
	return true;
}

/*************************************
 *  The 3dfx emulation's functions
 *************************************/

bool voodoo_vk_init(voodoo_state *vs) {
	(void)vs;
	if (vvk.ready) return true;
	memset(&vvk.dev, 0, sizeof(vvk.dev));
	if (!OUTPUT_VULKAN_GetDevice(vvk.dev)) {
		LOG_MSG("VOODOO: Vulkan: the Vulkan 3dfx emulation needs output=vulkan");
		return false;
	}
	if (!vvk_load_functions() || !vvk_choose_formats()) {
		memset(&vvk.dev, 0, sizeof(vvk.dev));
		return false;
	}
	VkPhysicalDeviceProperties props;
	vkGetPhysicalDeviceProperties(vvk.dev.physical, &props);
	vkGetPhysicalDeviceMemoryProperties(vvk.dev.physical, &vvk.memory_properties);
	vvk.max_size = props.limits.maxImageDimension2D;
	if (props.limits.maxFramebufferWidth < vvk.max_size) vvk.max_size = props.limits.maxFramebufferWidth;
	if (props.limits.maxFramebufferHeight < vvk.max_size) vvk.max_size = props.limits.maxFramebufferHeight;

	vvk.ready = true;
	vvk.warned = false;
	Voodoo_HW_PollResolutionSetting();
	if (!vvk_create_device_objects() || !vvk_reset_videomode()) {
		vvk_destroy_all();
		return false;
	}
	LOG_MSG("VOODOO: Vulkan: rendering 3dfx graphics with Vulkan on %s", props.deviceName);
	return true;
}

void voodoo_vk_device_lost(void) {
	if (!vvk.ready) return;
	vvk_destroy_all();
}

void voodoo_vk_leave(bool leavemode) {
	if (!vvk.ready) return;
	if (leavemode) {
		LOG_MSG("VOODOO: Vulkan: quit");
		vvk_destroy_all();
	} else {
		vvk_submit(true);
		vvk_destroy_textures();
	}
}

void voodoo_vk_shutdown(voodoo_state *vs) {
	(void)vs;
	voodoo_vk_leave(true);
}

void voodoo_vk_set_window(voodoo_state *vs) {
	/* the projection is worked out for each triangle */
	(void)vs;
}

void voodoo_vk_clip_window(voodoo_state *vs) {
	(void)vs;
}

void voodoo_vk_vblank_flush(void) {
	/* submitted at every vertical retrace anyway (voodoo_vk_present) */
}

/* show the front buffer: copy it to the image the output draws, and have the output draw it */
void voodoo_vk_present(void) {
	if (!v->active || !vvk_enter() || vvk.width == 0) return;
	vvk_check_render_size();

	vvk_begin();
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	VkImage front = vvk.color[vvk.front].image;
	vvk_color_to(front, true, true);
	vvk_barrier(vvk.present.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
		VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT);
	VkImageCopy copy = {};
	copy.srcSubresource.aspectMask = copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	copy.srcSubresource.layerCount = copy.dstSubresource.layerCount = 1;
	copy.extent.width = vvk.width;
	copy.extent.height = vvk.height;
	copy.extent.depth = 1;
	vkCmdCopyImage(cmd, front, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vvk.present.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
	vvk_color_to(front, true, false);
	vvk_barrier(vvk.present.image, VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
		VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
	vvk_submit(false);

	OUTPUT_VULKAN_PresentExternal();
}

void voodoo_vk_swap_buffer(void) {
	if (!vvk_enter()) return;
	/* the back buffer becomes the front buffer (it is shown at the next vertical retrace) */
	vvk_flush_batch();
	vvk.front ^= 1;
}

/* the 3dfx video mode changed: the buffers are resized right away (voodoo_vk_follow_dimensions),
 * this tells the output, which changes the window etc. */
void voodoo_vk_update_dimensions(void) {
	if (!vvk_enter()) return;
	if (vvk.width == 0) {
		vvk_reset_videomode();
		return;
	}
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
	if (!vvk_enter()) return;
	vvk_check_render_size();
	OUTPUT_VULKAN_SetExternalImage(vvk.present.view, vvk.width, vvk.height);
	LOG_MSG("VOODOO: Vulkan: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, vvk.width, vvk.height);
}

/* the 3dfx video mode changed while active: anything drawn from now on is drawn at the new size */
void voodoo_vk_follow_dimensions(void) {
	if (!vvk_enter()) return;
	vvk_check_render_size();
}

/* both buffers black, depth 1.0 */
void voodoo_vk_clear(void) {
	if (!vvk_enter() || vvk.width == 0) return;
	VkClearAttachment ca[2] = {};
	ca[0].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	ca[1].aspectMask = vvk_depth_aspect();
	ca[1].clearValue.depthStencil.depth = 1.0f;
	VkClearRect rect = {};
	rect.rect.extent.width = vvk.width;
	rect.rect.extent.height = vvk.height;
	rect.layerCount = 1;
	for (int i = 0; i < 2; i++) {
		vvk_begin_pass(i);
		vvk_flush_batch();
		vkCmdClearAttachments(vvk.slots[vvk.slot].cmd, 2, ca, 1, &rect);
	}
	vvk_invalidate_reads();
}

/* the 3dfx card was drawn to in software until the emulation became active (with the
 * buffers in its memory), so start from what it has there instead of black. Scaled to the
 * render resolution here, with the pixel spans LFB reads and writes use (so they agree). */
void voodoo_vk_load_framebuffers_from_ram(void) {
	if (!vvk_enter() || vvk.width == 0) return;
	const UINT32 fw = v->fbi.width, fh = v->fbi.height;
	if (fw == 0 || fh == 0) return;

	/* the 3dfx video mode pixel for each render resolution column and row */
	vector<UINT32> col(vvk.width), row(vvk.height);
	for (UINT32 i = 0; i < fw; i++) {
		int p0, p1;
		vvk_lfb_span((int)i, vvk.width, fw, p0, p1);
		for (int p = p0; p < p1; p++) col[(size_t)p] = i;
	}
	for (UINT32 i = 0; i < fh; i++) {
		int p0, p1;
		vvk_lfb_span((int)i, vvk.height, fh, p0, p1);
		for (int p = p0; p < p1; p++) row[(size_t)p] = i;
	}

	vvk_begin();
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	for (int b = 0; b < 2; b++) {
		const UINT32 offs = v->fbi.rgboffs[b == 0 ? v->fbi.frontbuf : v->fbi.backbuf];
		if (offs == (UINT32)(~0) || offs > v->fbi.mask) continue;
		const UINT16 *src = (const UINT16 *)(v->fbi.ram + offs);
		const UINT32 srcmax = (v->fbi.mask + 1 - offs) / 2;

		VkBuffer buffer;
		VkDeviceSize offset;
		UINT8 *d = vvk_alloc((VkDeviceSize)vvk.width * vvk.height * 4, 16, buffer, offset);
		if (d == NULL) break;
		/* RGB 565 to RGBA, top row first in both */
		for (UINT32 y = 0; y < vvk.height; y++) {
			for (UINT32 x = 0; x < vvk.width; x++, d += 4) {
				const UINT32 i = row[y] * v->fbi.rowpixels + col[x];
				const UINT32 p = (i < srcmax) ? src[i] : 0;
				const UINT32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, bl = p & 0x1f;
				d[0] = (UINT8)((r << 3) | (r >> 2));
				d[1] = (UINT8)((g << 2) | (g >> 4));
				d[2] = (UINT8)((bl << 3) | (bl >> 2));
				d[3] = 0;
			}
		}

		const VkImage target = vvk.color[vvk_target(b == 0)].image;
		vvk_color_to(target, false, true);
		VkBufferImageCopy copy = {};
		copy.bufferOffset = offset;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.layerCount = 1;
		copy.imageExtent.width = vvk.width;
		copy.imageExtent.height = vvk.height;
		copy.imageExtent.depth = 1;
		vkCmdCopyBufferToImage(cmd, buffer, target, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
		vvk_color_to(target, false, false);
	}
	vvk_invalidate_reads();
}

void voodoo_vk_fastfill(void) {
	if (!vvk_enter() || vvk.width == 0) return;

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

	VkClearAttachment ca[2] = {};
	uint32_t n = 0;
	int target = vvk.front ^ 1;
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u)) {
		ca[n].aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		ca[n].colorAttachment = 0;
		ca[n].clearValue.color.float32[0] = v->reg[color1].rgb.r / 255.0f;
		ca[n].clearValue.color.float32[1] = v->reg[color1].rgb.g / 255.0f;
		ca[n].clearValue.color.float32[2] = v->reg[color1].rgb.b / 255.0f;
		ca[n].clearValue.color.float32[3] = v->reg[color1].rgb.a / 255.0f;
		n++;
		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0) {
			target = vvk.front;
			v->fbi.vblank_flush_pending = true;
		}
	}
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) {
		ca[n].aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		ca[n].clearValue.depthStencil.depth = (float)((UINT16)v->reg[zaColor].u) / 65535.0f;
		n++;
	}
	if (n == 0) return;

	VkClearRect rect = {};
	rect.rect.offset.x = (int32_t)((UINT32)sx * vvk.width / (UINT32)fw);
	rect.rect.offset.y = (int32_t)((UINT32)sy * vvk.height / (UINT32)fh);
	rect.rect.extent.width = (UINT32)ex * vvk.width / (UINT32)fw - (UINT32)rect.rect.offset.x;
	rect.rect.extent.height = (UINT32)ey * vvk.height / (UINT32)fh - (UINT32)rect.rect.offset.y;
	rect.layerCount = 1;
	if (rect.rect.extent.width == 0 || rect.rect.extent.height == 0) return;
	vvk_begin_pass(target);
	vvk_flush_batch();
	vkCmdClearAttachments(vvk.slots[vvk.slot].cmd, n, ca, 1, &rect);
	vvk_invalidate_reads();
}

void voodoo_vk_texture_clear(UINT32 address, int TMU) {
	if (vvk_textures[TMU].empty()) return;
	const UINT32 mask = v->tmu[TMU].mask;
	UINT32 first = address & mask;
	UINT32 last = (first + 3) & mask;
	/* a wrapped dword conservatively dirties the whole circular address space */
	if (last < first) {
		first = 0;
		last = mask;
	}
	VvkDirtyRange &dirty = vvk_texture_dirty[TMU];
	if (!dirty.pending) {
		dirty.first = first;
		dirty.last = last;
		dirty.pending = true;
	} else {
		if (first < dirty.first) dirty.first = first;
		if (last > dirty.last) dirty.last = last;
	}
}

void voodoo_vk_invalidate_paltex(void) {
	for (int j = 0; j < 2; j++)
		for (map<const UINT32, VvkTexMap>::iterator t = vvk_textures[j].begin(); t != vvk_textures[j].end(); ++t)
			if (vvk_palette_format(t->second.format)) t->second.valid_pal = false;
}

/* an LFB pixel (x, y) in the 3dfx video mode, row 0 at the top: a rectangle over the render
 * resolution pixels it covers */
static void vvk_lfb_rect(int x, int y, float *inst) {
	int x0, x1, y0, y1;
	vvk_lfb_span(x, vvk.width, v->fbi.width, x0, x1);
	vvk_lfb_span(y, vvk.height, v->fbi.height, y0, y1);
	inst[0] = 2.0f * (float)x0 / (float)vvk.width - 1.0f;
	inst[1] = 2.0f * (float)y0 / (float)vvk.height - 1.0f;
	inst[2] = 2.0f * (float)x1 / (float)vvk.width - 1.0f;
	inst[3] = 2.0f * (float)y1 / (float)vvk.height - 1.0f;
}

static void vvk_draw_rect(int target, VvkPipelineState st, int x, int y, float r, float g, float b, float a, float z) {
	st.rect = true;
	float inst[9];
	vvk_lfb_rect(x, y, inst);
	inst[4] = r;
	inst[5] = g;
	inst[6] = b;
	inst[7] = a;
	inst[8] = z;
	VvkPushConstants pc;
	memset(&pc, 0, sizeof(pc));
	vvk_draw(target, vvk_pipeline(vvk.fs_flat, st), vvk_no_textures(), pc, true, inst, 1);
	vvk.cache_valid[target] = false;
	vvk.cache_valid[2] = false;
}

void voodoo_vk_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	if (!vvk_enter() || vvk.width == 0) return;
	VvkPipelineState st = vvk_default_state();
	st.color_mask = vvk_color_mask(has_rgb, has_alpha);
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	vvk_draw_rect(vvk_target(front), st, x, y, (r & 0xff) / 255.0f, (g & 0xff) / 255.0f, (b & 0xff) / 255.0f, (a & 0xff) / 255.0f, 0.0f);
}

void voodoo_vk_draw_z(int x, int y, int z) {
	if (!vvk_enter() || vvk.width == 0) return;
	VvkPipelineState st = vvk_default_state();
	st.color_mask = 0;
	st.depth_test = true;
	st.depth_func = VK_COMPARE_OP_ALWAYS;
	st.depth_write = true;
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	vvk_draw_rect(vvk_target(front), st, x, y, 0.0f, 0.0f, 0.0f, 0.0f, (float)(z & 0xffff) / 65535.0f);
}

void voodoo_vk_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
	if (!vvk_enter() || vvk.width == 0) return;
	VvkPipelineState st = vvk_default_state();
	st.color_mask = vvk_color_mask(true, FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0);
	vvk_blend_state(st, v->reg[alphaMode].u);
	const bool front = LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u) == 0;
	if (front) v->fbi.vblank_flush_pending = true;
	vvk_draw_rect(vvk_target(front), st, x, y, (r & 0xff) / 255.0f, (g & 0xff) / 255.0f, (b & 0xff) / 255.0f, 1.0f, 0.0f);
}

/* read a buffer back (0 and 1: color, 2: depth), kept until it is drawn to again */
static bool vvk_read_buffer(int which) {
	if (vvk.cache_valid[which]) return true;
	const VkDeviceSize size = (VkDeviceSize)vvk.width * vvk.height * 4;
	if (vvk.readback_size < size) {
		if (vvk.readback != VK_NULL_HANDLE) {
			vvk_submit(true);
			vkDeviceWaitIdle(vvk.dev.device);
			vkUnmapMemory(vvk.dev.device, vvk.readback_memory);
			vkDestroyBuffer(vvk.dev.device, vvk.readback, NULL);
			vkFreeMemory(vvk.dev.device, vvk.readback_memory, NULL);
			vvk.readback = VK_NULL_HANDLE;
			vvk.readback_memory = VK_NULL_HANDLE;
		}
		vvk.readback_size = 0;
		if (!vvk_create_buffer(size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, vvk.readback, vvk.readback_memory, vvk.readback_ptr))
			return false;
		vvk.readback_size = size;
	}

	vvk_begin();
	vvk_end_pass();
	const VkCommandBuffer cmd = vvk.slots[vvk.slot].cmd;
	VkBufferImageCopy copy = {};
	copy.imageSubresource.layerCount = 1;
	copy.imageExtent.width = vvk.width;
	copy.imageExtent.height = vvk.height;
	copy.imageExtent.depth = 1;
	if (which < 2) {
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		vvk_color_to(vvk.color[which].image, true, true);
		vkCmdCopyImageToBuffer(cmd, vvk.color[which].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vvk.readback, 1, &copy);
		vvk_color_to(vvk.color[which].image, true, false);
	} else {
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
		vvk_depth_to(vvk.depth.image, true, true);
		vkCmdCopyImageToBuffer(cmd, vvk.depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vvk.readback, 1, &copy);
		vvk_depth_to(vvk.depth.image, true, false);
	}
	VkBufferMemoryBarrier bb = {};
	bb.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
	bb.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
	bb.dstAccessMask = VK_ACCESS_HOST_READ_BIT;
	bb.srcQueueFamilyIndex = bb.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	bb.buffer = vvk.readback;
	bb.size = VK_WHOLE_SIZE;
	vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, 0, 0, NULL, 1, &bb, 0, NULL);
	vvk_submit(true);

	vector<UINT32> &c = vvk.cache[which];
	c.resize((size_t)vvk.width * vvk.height);
	if (which < 2 || vvk.depth_format == VK_FORMAT_D32_SFLOAT || vvk.depth_format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
	    vvk.depth_format == VK_FORMAT_D24_UNORM_S8_UINT) {
		memcpy(c.data(), vvk.readback_ptr, c.size() * 4);
	} else {
		/* D16: 2 bytes each */
		const UINT16 *s = (const UINT16 *)vvk.readback_ptr;
		for (size_t i = 0; i < c.size(); i++) c[i] = s[i];
	}
	vvk.cache_valid[which] = true;
	return true;
}

/* a depth buffer value as 16 bits */
static UINT32 vvk_depth16(UINT32 raw) {
	switch (vvk.depth_format) {
		case VK_FORMAT_D24_UNORM_S8_UINT:
			return (UINT32)(((raw & 0xffffff) * 65535.0) / 16777215.0 + 0.5);
		case VK_FORMAT_D16_UNORM:
			return raw & 0xffff;
		default: {
			float f;
			memcpy(&f, &raw, 4);
			if (f < 0.0f) f = 0.0f;
			if (f > 1.0f) f = 1.0f;
			return (UINT32)(f * 65535.0f + 0.5f);
		}
	}
}

/* two pixels from (x, y) in the 3dfx video mode, row 0 at the top. With a larger render
 * resolution, the render pixel in the middle of each one is read (as OpenGL). */
UINT32 voodoo_vk_read_pixel(int x, int y) {
	UINT32 data[2];
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;
	if (!vvk_enter() || vvk.width == 0) return 0;

	int rx[2], ry, p0, p1;
	for (int i = 0; i < 2; i++) {
		vvk_lfb_span(x + i, vvk.width, v->fbi.width, p0, p1);
		rx[i] = (p0 + p1 - 1) / 2;
	}
	vvk_lfb_span(y, vvk.height, v->fbi.height, p0, p1);
	ry = (p0 + p1 - 1) / 2;

	int which;
	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0: which = vvk.front; break;		/* front buffer */
		case 1: which = vvk.front ^ 1; break;		/* back buffer */
		case 2: which = 2; break;			/* aux buffer */
		default:
			E_Exit("read from invalid buf %x", LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			return 0;
	}
	if (!vvk_read_buffer(which)) return 0;
	const UINT32 *row = &vvk.cache[which][(size_t)ry * vvk.width];
	data[0] = row[rx[0]];
	data[1] = row[rx[1]];
	if (which == 2) return vvk_depth16(data[0]) | (vvk_depth16(data[1]) << 16);

	/* RGBA bytes, like the OpenGL emulation reads them */
	return ((RGB_BLUE(data[0])>>3)<<11) | ((RGB_GREEN(data[0])>>2)<<5) | (RGB_RED(data[0])>>3) |
			((RGB_BLUE(data[1])>>3)<<27) | ((RGB_GREEN(data[1])>>2)<<21) | ((RGB_RED(data[1])>>3)<<16);
}

/* the fragment shader for a rasterizer's settings */
static VkShaderModule vvk_fragment_shader(const poly_extra_data *extra, bool tmu0, bool tmu1) {
	VvkFsKey key;
	memset(&key, 0, sizeof(key));
	key.texmode[0] = tmu0 ? (UINT32)extra->r_textureMode0 : 0;
	key.texmode[1] = tmu1 ? (UINT32)extra->r_textureMode1 : 0;
	key.fbzcp = v->reg[fbzColorPath].u;
	key.alphamode = v->reg[alphaMode].u & 0x0f;	/* alpha test and function */
	key.fogmode = FOGMODE_ENABLE_FOG(v->reg[fogMode].u) ? (v->reg[fogMode].u & 0x3f) : 0;
	key.flags = (tmu0 ? VVK_FS_TMU0 : 0) | (tmu1 ? VVK_FS_TMU1 : 0) |
		(FBZMODE_ENABLE_CHROMAKEY(v->reg[fbzMode].u) ? VVK_FS_CHROMAKEY : 0);
	map<VvkFsKey, VkShaderModule>::iterator i = vvk.fs_cache.find(key);
	if (i != vvk.fs_cache.end()) return i->second;
	VvkFragmentShader fs;
	const VkShaderModule mod = vvk_shader(fs.Build(key));
	vvk.fs_cache[key] = mod;
	return mod;
}

void voodoo_vk_draw_triangle(poly_extra_data *extra) {
	v = extra->state;
	if (!vvk_enter() || vvk.width == 0) return;

	const UINT32 ALPHAMODE = extra->r_alphaMode;
	const UINT32 FBZMODE = extra->r_fbzMode;

	ogl_vertex_data vd[3];
	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);

	/* the textures (uploaded before the draw, outside of the render pass) */
	VvkTexture *tex[2] = { NULL, NULL };
	bool use[2] = { false, false };
	for (UINT32 j = 0; j < 2; j++) {
		if (j >= extra->texcount || v->tmu[j].ram == NULL || v->tmu[j].lodmin >= (8 << 8)) continue;
		tex[j] = vvk_cache_texture(extra, j);
		use[j] = tex[j] != NULL;
	}
	VkSampler sampler[2];
	VkImageView view[2];
	for (int j = 0; j < 2; j++) {
		const UINT32 TEXMODE = (UINT32)(j == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
		sampler[j] = vvk_sampler(use[j] ? TEXMODE : 0, use[j] && v->tmu[j].lodmin != v->tmu[j].lodmax);
		view[j] = use[j] ? tex[j]->image.view : vvk.white.image.view;
	}
	const VkDescriptorSet set = vvk_descriptor_set(view[0], sampler[0], view[1], sampler[1]);
	const VkShaderModule fs = vvk_fragment_shader(extra, use[0], use[1]);
	if (fs == VK_NULL_HANDLE || set == VK_NULL_HANDLE) return;

	VvkPushConstants pc;
	vvk_push_constants(pc);

	/* vertices: position in normalized device coordinates (the Y origin at the top or bottom,
	 * as the OpenGL emulation's projection) */
	const bool y_bottom = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u) != 0;
	float vert[3][16];
	for (int i = 0; i < 3; i++) {
		float *o = vert[i];
		const float x = vd[i].x / (float)v->fbi.width;
		const float y = vd[i].y / (float)v->fbi.height;
		o[0] = 2.0f * x - 1.0f;
		o[1] = y_bottom ? 1.0f - 2.0f * y : 2.0f * y - 1.0f;
		o[2] = vd[i].d;
		/* (OpenGL clamps the colors of the fixed function vertex shader) */
		const float c[4] = { vd[i].r, vd[i].g, vd[i].b, vd[i].a };
		for (int k = 0; k < 4; k++) o[3 + k] = c[k] < 0.0f ? 0.0f : (c[k] > 1.0f ? 1.0f : c[k]);
		for (int t = 0; t < 2; t++) {
			o[7 + t * 3] = use[t] ? vd[i].m[t].sw : 0.0f;
			o[8 + t * 3] = use[t] ? vd[i].m[t].tw : 0.0f;
			o[9 + t * 3] = use[t] ? vd[i].m[t].w : 1.0f;
		}
		o[13] = vd[i].fogblend;
		o[14] = use[0] ? vd[i].m[0].lodblend : 0.0f;
		o[15] = use[1] ? vd[i].m[1].lodblend : 0.0f;
	}

	const bool front = FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u) == 0;
	const int target = vvk_target(front);
	if (front) v->fbi.vblank_flush_pending = true;

	/* depth source compare: the stencil marks where the triangle passes the depth test
	 * against zaColor, and it is drawn there */
	const bool stencil_pass = FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && vvk.has_stencil;
	if (stencil_pass) {
		VvkPipelineState st = vvk_default_state();
		st.depth_test = FBZMODE_ENABLE_DEPTHBUF(FBZMODE) && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0;
		st.depth_func = (VkCompareOp)FBZMODE_DEPTH_FUNCTION(FBZMODE);
		st.color_mask = 0;
		st.stencil = VVK_STENCIL_WRITE;
		vvk_begin_pass(target);
		vvk_flush_batch();
		VkClearAttachment ca = {};
		ca.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
		VkClearRect rect = {};
		rect.rect.extent.width = vvk.width;
		rect.rect.extent.height = vvk.height;
		rect.layerCount = 1;
		vkCmdClearAttachments(vvk.slots[vvk.slot].cmd, 1, &ca, 1, &rect);
		float flat[3][16];
		memcpy(flat, vert, sizeof(flat));
		const float depth = (float)(v->reg[zaColor].u & 0xffff) / (float)0xffff;
		for (int i = 0; i < 3; i++) {
			flat[i][2] = depth;
			flat[i][3] = flat[i][4] = flat[i][5] = flat[i][6] = 1.0f;
		}
		vvk_draw(target, vvk_pipeline(vvk.fs_flat, st), set, pc, false, flat, 3);
	}

	VvkPipelineState st = vvk_default_state();
	if (stencil_pass) {
		st.stencil = VVK_STENCIL_TEST;
		st.depth_test = FBZMODE_ENABLE_DEPTHBUF(FBZMODE) != 0;
		st.depth_func = VK_COMPARE_OP_ALWAYS;
	} else if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			st.depth_test = true;
			st.depth_func = (VkCompareOp)FBZMODE_DEPTH_FUNCTION(FBZMODE);
		} else if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
			st.depth_test = true;
			st.depth_func = VK_COMPARE_OP_ALWAYS;
		}
	} else {
		st.depth_test = true;
		st.depth_func = VK_COMPARE_OP_ALWAYS;
	}
	const bool aux = FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0;
	st.depth_write = aux && FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0;
	st.color_mask = vvk_color_mask(FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0, aux);
	vvk_blend_state(st, ALPHAMODE);

	vvk_draw(target, vvk_pipeline(fs, st), set, pc, false, vert, 3);
	vvk.cache_valid[target] = false;
	vvk.cache_valid[2] = false;
}

#else

/* no Vulkan output */

bool voodoo_vk_init(voodoo_state *vs) { (void)vs; return false; }
void voodoo_vk_leave(bool leavemode) { (void)leavemode; }
void voodoo_vk_shutdown(voodoo_state *vs) { (void)vs; }
void voodoo_vk_set_window(voodoo_state *vs) { (void)vs; }
void voodoo_vk_present(void) { }
void voodoo_vk_load_framebuffers_from_ram(void) { }
void voodoo_vk_follow_dimensions(void) { }
void voodoo_vk_swap_buffer(void) { }
void voodoo_vk_vblank_flush(void) { }
void voodoo_vk_update_dimensions(void) { }
void voodoo_vk_clear(void) { }
void voodoo_vk_fastfill(void) { }
void voodoo_vk_clip_window(voodoo_state *vs) { (void)vs; }
void voodoo_vk_texture_clear(UINT32 address, int TMU) { (void)address; (void)TMU; }
void voodoo_vk_invalidate_paltex(void) { }
void voodoo_vk_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	(void)x; (void)y; (void)has_rgb; (void)has_alpha; (void)r; (void)g; (void)b; (void)a;
}
void voodoo_vk_draw_z(int x, int y, int z) { (void)x; (void)y; (void)z; }
void voodoo_vk_draw_pixel_pipeline(int x, int y, int r, int g, int b) { (void)x; (void)y; (void)r; (void)g; (void)b; }
UINT32 voodoo_vk_read_pixel(int x, int y) { (void)x; (void)y; return 0; }
void voodoo_vk_draw_triangle(poly_extra_data *extra) { (void)extra; }

#endif
