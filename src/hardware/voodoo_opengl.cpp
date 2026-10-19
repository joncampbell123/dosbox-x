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

// Tell Mac OS X to shut up about deprecated OpenGL calls
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#include <stdlib.h>
#include <math.h>
#include <map>
#include <vector>

#include "dosbox.h"
#include "control.h"
#include "logging.h"
#include "video.h"

#include "paging.h"
#include "voodoo.h"
#include "voodoo_emu.h"
#include "voodoo_opengl.h"
#include "sdlmain.h"
#include "render.h"
#include <output/output_opengl.h>

bool Voodoo_OGL_Active() {
	return (v != NULL && v->clock_enabled && v->output_on);
}

#if C_OPENGL

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

#include "voodoo_def.h"

INT32 cached_line_front_y=-1;
INT32 cached_line_front_width = -1;
INT32 cached_line_front_length=-1;
INT32 cached_line_front_pixels=-1;
UINT32* cached_line_front_data=NULL;

INT32 cached_line_back_y=-1;
INT32 cached_line_back_width = -1;
INT32 cached_line_back_length=-1;
INT32 cached_line_back_pixels=-1;
UINT32* cached_line_back_data=NULL;

static UINT32 last_clear_color=0;

static UINT32 last_width=0;
static UINT32 last_height=0;
static INT32 last_orientation=-1;
UINT32 new_width = 0;
UINT32 new_height = 0;

static UINT32 ogl_texture_index = 1;
extern int transparency;

/* texture cache buffer */
UINT32 texrgb[256*256];

/* texture address map */
std::map <const UINT32, ogl_texmap> textures[2];

void SetWindowTransparency(int trans);

/* DOSBox-X: The 3dfx emulation renders with an OpenGL context of its own, into framebuffer
 * objects the size of the 3dfx video mode: front and back buffer textures and a depth/stencil
 * buffer. The coordinates are the same as when it rendered to a window (bottom row first).
 * At every vertical retrace the front buffer is copied on the GPU, flipped to top row first,
 * to a texture that the OpenGL output (which shares textures with this context) draws as the
 * screen. The output keeps its menus, window resizing and fullscreen while the 3dfx card is in
 * use, and nothing is read back to the CPU. */

#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER			0x8D40
#define GL_RENDERBUFFER			0x8D41
#define GL_FRAMEBUFFER_COMPLETE		0x8CD5
#define GL_COLOR_ATTACHMENT0		0x8CE0
#define GL_DEPTH_ATTACHMENT		0x8D00
#define GL_STENCIL_ATTACHMENT		0x8D20
#endif
#ifndef GL_READ_FRAMEBUFFER
#define GL_READ_FRAMEBUFFER		0x8CA8
#define GL_DRAW_FRAMEBUFFER		0x8CA9
#endif
#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8		0x88F0
#endif
#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24		0x81A6
#endif
#ifndef GL_MAX_RENDERBUFFER_SIZE
#define GL_MAX_RENDERBUFFER_SIZE	0x84E8
#endif
#ifndef GL_SYNC_GPU_COMMANDS_COMPLETE
#define GL_SYNC_GPU_COMMANDS_COMPLETE	0x9117
#endif
#ifndef APIENTRY
#define APIENTRY
#endif

/* GLsync is a pointer, handled as void* here (older headers don't have it) */
typedef void (APIENTRY *VOGL_PFNGENFRAMEBUFFERS)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *VOGL_PFNDELETEFRAMEBUFFERS)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *VOGL_PFNBINDFRAMEBUFFER)(GLenum target, GLuint id);
typedef void (APIENTRY *VOGL_PFNFRAMEBUFFERTEXTURE2D)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef void (APIENTRY *VOGL_PFNGENRENDERBUFFERS)(GLsizei n, GLuint *ids);
typedef void (APIENTRY *VOGL_PFNDELETERENDERBUFFERS)(GLsizei n, const GLuint *ids);
typedef void (APIENTRY *VOGL_PFNBINDRENDERBUFFER)(GLenum target, GLuint id);
typedef void (APIENTRY *VOGL_PFNRENDERBUFFERSTORAGE)(GLenum target, GLenum format, GLsizei w, GLsizei h);
typedef void (APIENTRY *VOGL_PFNFRAMEBUFFERRENDERBUFFER)(GLenum target, GLenum attachment, GLenum rbtarget, GLuint rb);
typedef GLenum (APIENTRY *VOGL_PFNCHECKFRAMEBUFFERSTATUS)(GLenum target);
typedef void (APIENTRY *VOGL_PFNBLITFRAMEBUFFER)(GLint sx0, GLint sy0, GLint sx1, GLint sy1, GLint dx0, GLint dy0, GLint dx1, GLint dy1, GLbitfield mask, GLenum filter);
typedef void* (APIENTRY *VOGL_PFNFENCESYNC)(GLenum condition, GLbitfield flags);
typedef void (APIENTRY *VOGL_PFNWAITSYNC)(void *sync, GLbitfield flags, GLuint64 timeout);
typedef void (APIENTRY *VOGL_PFNDELETESYNC)(void *sync);

static struct {
	GFX_GLContext context;

	VOGL_PFNGENFRAMEBUFFERS GenFramebuffers;
	VOGL_PFNDELETEFRAMEBUFFERS DeleteFramebuffers;
	VOGL_PFNBINDFRAMEBUFFER BindFramebuffer;
	VOGL_PFNFRAMEBUFFERTEXTURE2D FramebufferTexture2D;
	VOGL_PFNGENRENDERBUFFERS GenRenderbuffers;
	VOGL_PFNDELETERENDERBUFFERS DeleteRenderbuffers;
	VOGL_PFNBINDRENDERBUFFER BindRenderbuffer;
	VOGL_PFNRENDERBUFFERSTORAGE RenderbufferStorage;
	VOGL_PFNFRAMEBUFFERRENDERBUFFER FramebufferRenderbuffer;
	VOGL_PFNCHECKFRAMEBUFFERSTATUS CheckFramebufferStatus;
	VOGL_PFNBLITFRAMEBUFFER BlitFramebuffer;
	VOGL_PFNFENCESYNC FenceSync;
	VOGL_PFNWAITSYNC WaitSync;
	VOGL_PFNDELETESYNC DeleteSync;

	GLuint fbo;			/* the 3dfx card's buffers */
	GLuint color[2];		/* front/back buffer, which one is front is "front" */
	GLuint depth_stencil;
	bool has_stencil;
	int front;
	GLuint present_fbo;		/* the front buffer, top row first, for the output */
	GLuint present_texture;
	UINT32 width, height;		/* render resolution, the 3dfx video mode's or larger */
	UINT32 max_size;		/* largest buffer the driver can make */

	void *fence;			/* the main context waits for the 3dfx emulation's commands */
	bool warned;
} vctx = {};

/* render resolution (voodoo_resolution setting): the 3dfx video mode's, twice that,
 * or the size of the area the output shows the 3dfx frame in (follows window resizes) */
static int ogl_resolution_mode = VOODOO_OGL_RES_NATIVE;
static UINT32 ogl_display_width = 0, ogl_display_height = 0;

static void *voodoo_ogl_getproc(const char *name, const char *ext_name) {
	void *p = (void *)SDL_GL_GetProcAddress(name);
	if (p == NULL && ext_name != NULL) p = (void *)SDL_GL_GetProcAddress(ext_name);
	return p;
}

/* framebuffer objects (OpenGL 3.0, or the EXT extensions on older OpenGL) */
static bool voodoo_ogl_load_functions(void) {
	vctx.GenFramebuffers = (VOGL_PFNGENFRAMEBUFFERS)voodoo_ogl_getproc("glGenFramebuffers", "glGenFramebuffersEXT");
	vctx.DeleteFramebuffers = (VOGL_PFNDELETEFRAMEBUFFERS)voodoo_ogl_getproc("glDeleteFramebuffers", "glDeleteFramebuffersEXT");
	vctx.BindFramebuffer = (VOGL_PFNBINDFRAMEBUFFER)voodoo_ogl_getproc("glBindFramebuffer", "glBindFramebufferEXT");
	vctx.FramebufferTexture2D = (VOGL_PFNFRAMEBUFFERTEXTURE2D)voodoo_ogl_getproc("glFramebufferTexture2D", "glFramebufferTexture2DEXT");
	vctx.GenRenderbuffers = (VOGL_PFNGENRENDERBUFFERS)voodoo_ogl_getproc("glGenRenderbuffers", "glGenRenderbuffersEXT");
	vctx.DeleteRenderbuffers = (VOGL_PFNDELETERENDERBUFFERS)voodoo_ogl_getproc("glDeleteRenderbuffers", "glDeleteRenderbuffersEXT");
	vctx.BindRenderbuffer = (VOGL_PFNBINDRENDERBUFFER)voodoo_ogl_getproc("glBindRenderbuffer", "glBindRenderbufferEXT");
	vctx.RenderbufferStorage = (VOGL_PFNRENDERBUFFERSTORAGE)voodoo_ogl_getproc("glRenderbufferStorage", "glRenderbufferStorageEXT");
	vctx.FramebufferRenderbuffer = (VOGL_PFNFRAMEBUFFERRENDERBUFFER)voodoo_ogl_getproc("glFramebufferRenderbuffer", "glFramebufferRenderbufferEXT");
	vctx.CheckFramebufferStatus = (VOGL_PFNCHECKFRAMEBUFFERSTATUS)voodoo_ogl_getproc("glCheckFramebufferStatus", "glCheckFramebufferStatusEXT");
	vctx.BlitFramebuffer = (VOGL_PFNBLITFRAMEBUFFER)voodoo_ogl_getproc("glBlitFramebuffer", "glBlitFramebufferEXT");
	/* optional (OpenGL 3.2 or GL_ARB_sync), glFinish() is used instead */
	vctx.FenceSync = (VOGL_PFNFENCESYNC)voodoo_ogl_getproc("glFenceSync", NULL);
	vctx.WaitSync = (VOGL_PFNWAITSYNC)voodoo_ogl_getproc("glWaitSync", NULL);
	vctx.DeleteSync = (VOGL_PFNDELETESYNC)voodoo_ogl_getproc("glDeleteSync", NULL);
	if (!vctx.FenceSync || !vctx.WaitSync || !vctx.DeleteSync) {
		vctx.FenceSync = NULL;
		vctx.WaitSync = NULL;
		vctx.DeleteSync = NULL;
	}

	if (!vctx.GenFramebuffers || !vctx.DeleteFramebuffers || !vctx.BindFramebuffer ||
	    !vctx.FramebufferTexture2D || !vctx.GenRenderbuffers || !vctx.DeleteRenderbuffers ||
	    !vctx.BindRenderbuffer || !vctx.RenderbufferStorage || !vctx.FramebufferRenderbuffer ||
	    !vctx.CheckFramebufferStatus || !vctx.BlitFramebuffer) {
		LOG_MSG("VOODOO: OpenGL: framebuffer objects (OpenGL 3.0 or GL_EXT_framebuffer_object/blit) are not supported");
		return false;
	}

	GLint max_texture = 0, max_renderbuffer = 0;
	glGetIntegerv(GL_MAX_TEXTURE_SIZE, &max_texture);
	glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &max_renderbuffer);
	if (max_texture < 1024) max_texture = 1024;
	if (max_renderbuffer < 1024) max_renderbuffer = 1024;
	vctx.max_size = (UINT32)(max_texture < max_renderbuffer ? max_texture : max_renderbuffer);
	return true;
}

static GLuint voodoo_ogl_create_color_texture(UINT32 w, UINT32 h) {
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glBindTexture(GL_TEXTURE_2D, 0);
	return tex;
}

static void voodoo_ogl_destroy_framebuffers(void) {
	if (vctx.BindFramebuffer) vctx.BindFramebuffer(GL_FRAMEBUFFER, 0);
	if (vctx.fbo) vctx.DeleteFramebuffers(1, &vctx.fbo);
	if (vctx.present_fbo) vctx.DeleteFramebuffers(1, &vctx.present_fbo);
	if (vctx.depth_stencil) vctx.DeleteRenderbuffers(1, &vctx.depth_stencil);
	if (vctx.color[0]) glDeleteTextures(2, vctx.color);
	if (vctx.present_texture) glDeleteTextures(1, &vctx.present_texture);
	vctx.fbo = vctx.present_fbo = vctx.depth_stencil = vctx.present_texture = 0;
	vctx.color[0] = vctx.color[1] = 0;
	vctx.width = vctx.height = 0;
}

/* the 3dfx emulation's own context is current from here on */
static void voodoo_ogl_apply_buffers(void) {
	VOGL_SetBuffers(GL_COLOR_ATTACHMENT0 + vctx.front, GL_COLOR_ATTACHMENT0 + (vctx.front ^ 1));
}

/* make the buffers (none may exist) */
static bool voodoo_ogl_create_framebuffers(UINT32 w, UINT32 h) {
	VOGL_ClearBeginMode();

	vctx.color[0] = voodoo_ogl_create_color_texture(w, h);
	vctx.color[1] = voodoo_ogl_create_color_texture(w, h);
	vctx.present_texture = voodoo_ogl_create_color_texture(w, h);

	vctx.GenFramebuffers(1, &vctx.present_fbo);
	vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.present_fbo);
	vctx.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vctx.present_texture, 0);
	if (vctx.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		LOG_MSG("VOODOO: OpenGL: incomplete framebuffer for the output");
		voodoo_ogl_destroy_framebuffers();
		return false;
	}
	/* clears are clipped by the scissor test */
	const bool scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
	if (scissor) glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
	if (scissor) glEnable(GL_SCISSOR_TEST);

	vctx.GenFramebuffers(1, &vctx.fbo);
	vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.fbo);
	vctx.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, vctx.color[0], 0);
	vctx.FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0 + 1, GL_TEXTURE_2D, vctx.color[1], 0);

	/* depth and stencil together, or depth only if the driver can't */
	vctx.GenRenderbuffers(1, &vctx.depth_stencil);
	vctx.BindRenderbuffer(GL_RENDERBUFFER, vctx.depth_stencil);
	vctx.has_stencil = true;
	vctx.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, (GLsizei)w, (GLsizei)h);
	vctx.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, vctx.depth_stencil);
	vctx.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, vctx.depth_stencil);
	if (vctx.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
		vctx.has_stencil = false;
		vctx.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
		vctx.RenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)w, (GLsizei)h);
		vctx.FramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, vctx.depth_stencil);
		if (vctx.CheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
			LOG_MSG("VOODOO: OpenGL: incomplete framebuffer");
			voodoo_ogl_destroy_framebuffers();
			return false;
		}
		LOG_MSG("VOODOO: OpenGL: no stencil buffer");
	}
	vctx.BindRenderbuffer(GL_RENDERBUFFER, 0);

	vctx.width = w;
	vctx.height = h;
	vctx.front = 0;
	voodoo_ogl_apply_buffers();

	/* both buffers start black */
	if (scissor) glDisable(GL_SCISSOR_TEST);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glClearStencil(0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	glDrawBuffer(GL_COLOR_ATTACHMENT0 + 1);
	glClear(GL_COLOR_BUFFER_BIT);
	/* the scissor box starts out at the size of the window the context was first used with */
	glScissor(0, 0, (GLsizei)w, (GLsizei)h);
	if (scissor) glEnable(GL_SCISSOR_TEST);
	voodoo_ogl_apply_buffers();
	last_clear_color = 0;
	return true;
}

/* make the 3dfx emulation's context current, false if it can't draw now
 * (no OpenGL output, the OpenGL functions below do nothing then) */
static bool voodoo_ogl_enter(void) {
	if (vctx.context == NULL) return false;
	if (GFX_GL_GetCurrentContext() == vctx.context) return true;

	if (!OUTPUT_OPENGL_Active() || !GFX_GL_MakeContextCurrent(vctx.context)) {
		if (!vctx.warned) {
			LOG_MSG("VOODOO: OpenGL: not drawing, the 3dfx emulation needs the OpenGL output");
			vctx.warned = true;
		}
		return false;
	}
	vctx.warned = false;
	if (vctx.fence) {
		vctx.DeleteSync(vctx.fence); /* nothing to wait for in this context */
		vctx.fence = NULL;
	}
	return true;
}

GFX_GLContext Voodoo_OGL_GetContext(void) {
	return vctx.context;
}

/* the 3dfx emulation's context is current, another one is about to be */
void Voodoo_OGL_ReleaseContext(void) {
	VOGL_ClearBeginMode();
	if (vctx.FenceSync) {
		if (vctx.fence) vctx.DeleteSync(vctx.fence);
		vctx.fence = vctx.FenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
		glFlush();
	} else {
		glFinish();
	}
}

/* in the context that is current now: wait (on the GPU) for the 3dfx emulation's commands */
void Voodoo_OGL_WaitRelease(void) {
	if (vctx.fence) {
		vctx.WaitSync(vctx.fence, 0, (GLuint64)-1 /* GL_TIMEOUT_IGNORED */);
		vctx.DeleteSync(vctx.fence);
		vctx.fence = NULL;
	}
}

/* the render resolution for the current setting and 3dfx video mode */
static void voodoo_ogl_render_size(UINT32 &w, UINT32 &h) {
	w = v->fbi.width;
	h = v->fbi.height;
	if (ogl_resolution_mode == VOODOO_OGL_RES_DOUBLE) {
		w *= 2;
		h *= 2;
	} else if (ogl_resolution_mode == VOODOO_OGL_RES_DISPLAY) {
		/* never below the 3dfx video mode in either direction, which would lose pixels
		 * (that LFB reads would miss) */
		if (w < ogl_display_width) w = ogl_display_width;
		if (h < ogl_display_height) h = ogl_display_height;
	}
	if (vctx.max_size > 0) {
		if (w > vctx.max_size) w = vctx.max_size;
		if (h > vctx.max_size) h = vctx.max_size;
	}
	if (w < 1) w = 1;
	if (h < 1) h = 1;
}

/* the projection, viewport etc. for a new render resolution */
static void voodoo_ogl_setup_window(void) {
	glMatrixMode( GL_PROJECTION );
	voodoo_ogl_set_window(v);

	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glMatrixMode( GL_MODELVIEW );
	glLoadIdentity( );
	glMatrixMode( GL_PROJECTION );
}

/* change the render resolution, keeping what the buffers show (scaled) */
static bool voodoo_ogl_resize_framebuffers(UINT32 w, UINT32 h) {
	VOGL_ClearBeginMode();

	const GLuint old_fbo = vctx.fbo, old_present_fbo = vctx.present_fbo;
	const GLuint old_color[2] = { vctx.color[0], vctx.color[1] };
	const GLuint old_depth_stencil = vctx.depth_stencil, old_present_texture = vctx.present_texture;
	const UINT32 old_width = vctx.width, old_height = vctx.height;
	const bool old_has_stencil = vctx.has_stencil;
	const int old_front = vctx.front;

	vctx.fbo = vctx.present_fbo = vctx.depth_stencil = vctx.present_texture = 0;
	vctx.color[0] = vctx.color[1] = 0;
	if (!voodoo_ogl_create_framebuffers(w, h)) {
		vctx.fbo = old_fbo;
		vctx.present_fbo = old_present_fbo;
		vctx.color[0] = old_color[0];
		vctx.color[1] = old_color[1];
		vctx.depth_stencil = old_depth_stencil;
		vctx.present_texture = old_present_texture;
		vctx.width = old_width;
		vctx.height = old_height;
		vctx.has_stencil = old_has_stencil;
		vctx.front = old_front;
		vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.fbo);
		voodoo_ogl_apply_buffers();
		return false;
	}
	vctx.front = old_front;

	/* blits are clipped by the scissor test */
	const bool scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
	if (scissor) glDisable(GL_SCISSOR_TEST);

	vctx.BindFramebuffer(GL_READ_FRAMEBUFFER, old_fbo);
	vctx.BindFramebuffer(GL_DRAW_FRAMEBUFFER, vctx.fbo);
	for (int i = 0; i < 2; i++) {
		glReadBuffer(GL_COLOR_ATTACHMENT0 + i);
		glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
		/* without filtering, the pixels keep their colors (for LFB reads) */
		vctx.BlitFramebuffer(0, 0, (GLint)old_width, (GLint)old_height, 0, 0, (GLint)w, (GLint)h,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}
	/* depth (and stencil) can only be copied without filtering */
	vctx.BlitFramebuffer(0, 0, (GLint)old_width, (GLint)old_height, 0, 0, (GLint)w, (GLint)h,
		GL_DEPTH_BUFFER_BIT | ((old_has_stencil && vctx.has_stencil) ? GL_STENCIL_BUFFER_BIT : 0), GL_NEAREST);

	vctx.BindFramebuffer(GL_READ_FRAMEBUFFER, old_present_fbo);
	vctx.BindFramebuffer(GL_DRAW_FRAMEBUFFER, vctx.present_fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	vctx.BlitFramebuffer(0, 0, (GLint)old_width, (GLint)old_height, 0, 0, (GLint)w, (GLint)h,
		GL_COLOR_BUFFER_BIT, GL_LINEAR);

	if (scissor) glEnable(GL_SCISSOR_TEST);

	vctx.BindFramebuffer(GL_FRAMEBUFFER, 0);
	vctx.DeleteFramebuffers(1, &old_fbo);
	vctx.DeleteFramebuffers(1, &old_present_fbo);
	vctx.DeleteRenderbuffers(1, &old_depth_stencil);
	glDeleteTextures(2, old_color);
	glDeleteTextures(1, &old_present_texture);

	vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.fbo);
	voodoo_ogl_apply_buffers();

	new_width = w;
	new_height = h;
	cached_line_front_y = -1;
	cached_line_back_y = -1;
	voodoo_ogl_setup_window();

	OUTPUT_OPENGL_SetExternalTexture(vctx.present_texture, w, h);
	LOG_MSG("VOODOO: OpenGL: render resolution %u:%u", (unsigned int)w, (unsigned int)h);
	return true;
}

void Voodoo_UpdateResolutionMenu(void);

/* follow the resolution setting (changed from the menu, configuration tool, CONFIG -set...)
 * and the display area, between frames */
static void voodoo_ogl_check_render_size(void) {
	Section_prop *section = static_cast<Section_prop *>(control->GetSection("voodoo"));
	const int mode = Voodoo_OGL_ParseResolutionMode(section != NULL ? section->Get_string("voodoo_resolution") : NULL);
	if (mode != ogl_resolution_mode) {
		ogl_resolution_mode = mode;
		Voodoo_UpdateResolutionMenu();
	}

	UINT32 w, h;
	voodoo_ogl_render_size(w, h);
	if (vctx.fbo != 0 && (w != vctx.width || h != vctx.height))
		voodoo_ogl_resize_framebuffers(w, h);
}

/* the 3dfx card was drawn to in software until the emulation became active (with the
 * buffers in its memory), so start from what it has there instead of black */
void voodoo_ogl_load_framebuffers_from_ram(void) {
	if (!voodoo_ogl_enter() || vctx.fbo == 0) return;
	VOGL_ClearBeginMode();

	const UINT32 fw = v->fbi.width, fh = v->fbi.height;
	if (fw == 0 || fh == 0) return;

	std::vector<UINT8> pixels((size_t)fw * fh * 4u);
	GLuint tex = voodoo_ogl_create_color_texture(fw, fh);
	GLuint fbo = 0;
	vctx.GenFramebuffers(1, &fbo);
	vctx.BindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
	vctx.FramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
	vctx.BindFramebuffer(GL_DRAW_FRAMEBUFFER, vctx.fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0);

	const bool scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
	if (scissor) glDisable(GL_SCISSOR_TEST);

	for (int b = 0; b < 2; b++) {
		const UINT32 offs = v->fbi.rgboffs[b == 0 ? v->fbi.frontbuf : v->fbi.backbuf];
		if (offs == (UINT32)(~0) || offs > v->fbi.mask) continue;
		const UINT16 *src = (const UINT16 *)(v->fbi.ram + offs);
		const UINT32 srcmax = (v->fbi.mask + 1 - offs) / 2;

		/* RGB 565, top row first, to RGBA bottom row first */
		for (UINT32 y = 0; y < fh; y++) {
			UINT8 *d = &pixels[(size_t)(fh - 1 - y) * fw * 4u];
			for (UINT32 x = 0; x < fw; x++, d += 4) {
				const UINT32 i = y * v->fbi.rowpixels + x;
				const UINT32 p = (i < srcmax) ? src[i] : 0;
				const UINT32 r = (p >> 11) & 0x1f, g = (p >> 5) & 0x3f, bl = p & 0x1f;
				d[0] = (UINT8)((r << 3) | (r >> 2));
				d[1] = (UINT8)((g << 2) | (g >> 4));
				d[2] = (UINT8)((bl << 3) | (bl >> 2));
				d[3] = 0;
			}
		}
		glBindTexture(GL_TEXTURE_2D, tex);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, (GLsizei)fw, (GLsizei)fh, GL_RGBA, GL_UNSIGNED_BYTE, &pixels[0]);
		glBindTexture(GL_TEXTURE_2D, 0);

		glDrawBuffer(GL_COLOR_ATTACHMENT0 + (b == 0 ? vctx.front : (vctx.front ^ 1)));
		/* without filtering, so LFB reads get the pixels written in software */
		vctx.BlitFramebuffer(0, 0, (GLint)fw, (GLint)fh, 0, 0, (GLint)vctx.width, (GLint)vctx.height,
			GL_COLOR_BUFFER_BIT, GL_NEAREST);
	}

	if (scissor) glEnable(GL_SCISSOR_TEST);

	vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.fbo);
	vctx.DeleteFramebuffers(1, &fbo);
	glDeleteTextures(1, &tex);
	voodoo_ogl_apply_buffers();
	cached_line_front_y = -1;
	cached_line_back_y = -1;
}

void Voodoo_OGL_SetResolutionMode(int mode) {
	ogl_resolution_mode = mode;
}

int Voodoo_OGL_GetResolutionMode(void) {
	return ogl_resolution_mode;
}

int Voodoo_OGL_ParseResolutionMode(const char *s) {
	if (s != NULL && !strcasecmp(s, "double")) return VOODOO_OGL_RES_DOUBLE;
	if (s != NULL && !strcasecmp(s, "display")) return VOODOO_OGL_RES_DISPLAY;
	return VOODOO_OGL_RES_NATIVE;
}

/* the output shows the 3dfx frame in an area this size (in pixels) */
void Voodoo_OGL_SetDisplaySize(unsigned int w, unsigned int h) {
	ogl_display_width = w;
	ogl_display_height = h;
}

/* show the front buffer: copy it top row first to the output's texture, and have the output draw it */
void voodoo_ogl_present(void) {
	if (!v->active || !voodoo_ogl_enter() || vctx.present_fbo == 0) return;

	VOGL_ClearBeginMode();
	voodoo_ogl_check_render_size();
	/* blits are clipped by the scissor test */
	const bool scissor = glIsEnabled(GL_SCISSOR_TEST) == GL_TRUE;
	if (scissor) glDisable(GL_SCISSOR_TEST);

	vctx.BindFramebuffer(GL_READ_FRAMEBUFFER, vctx.fbo);
	glReadBuffer(GL_COLOR_ATTACHMENT0 + vctx.front);
	vctx.BindFramebuffer(GL_DRAW_FRAMEBUFFER, vctx.present_fbo);
	vctx.BlitFramebuffer(0, 0, (GLint)vctx.width, (GLint)vctx.height,
		0, (GLint)vctx.height, (GLint)vctx.width, 0, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	vctx.BindFramebuffer(GL_FRAMEBUFFER, vctx.fbo);
	voodoo_ogl_apply_buffers();

	if (scissor) glEnable(GL_SCISSOR_TEST);

	OUTPUT_OPENGL_PresentExternal();
}

static void ogl_get_depth(voodoo_state* VV, INT32 ITERZ, INT64 ITERW, INT32 *depthval, INT32 *out_wfloat)
{
	INT32 wfloat;
	UINT32 FBZMODE = VV->reg[fbzMode].u;
	UINT32 FBZCOLORPATH = VV->reg[fbzColorPath].u;

	/* compute "floating point" W value (used for depth and fog) */
	if ((ITERW) & LONGTYPE(0xffff00000000))
		wfloat = 0x0000;
	else
	{
		UINT32 temp = (UINT32)(ITERW);
		if ((temp & 0xffff0000) == 0)
			wfloat = 0xffff;
		else
		{
			int exp = count_leading_zeros(temp);
			wfloat = ((exp << 12) | (INT32)(((~temp) >> (unsigned int)(19 - exp)) & 0xfffu));
			if (wfloat < 0xffff) wfloat++;
		}
	}

	/* compute depth value (W or Z) for this pixel */
	if (FBZMODE_WBUFFER_SELECT(FBZMODE) == 0)
		CLAMPED_Z(ITERZ, FBZCOLORPATH, *depthval);
	else if (FBZMODE_DEPTH_FLOAT_SELECT(FBZMODE) == 0)
		*depthval = wfloat;
	else
	{
		if ((unsigned int)(ITERZ) & 0xf0000000l)
			*depthval = 0x0000;
		else
		{
			UINT32 temp = (UINT32)((ITERZ) << 4);
			if ((temp & 0xffff0000ul) == 0)
				*depthval = 0xffff;
			else
			{
				int exp = count_leading_zeros(temp);
				*depthval = ((exp << 12) | (INT32)(((~temp) >> (unsigned int)(19 - exp)) & 0xfffu));
				if (*depthval < 0xffff) (*depthval)++;
			}
		}
	}

	/* add the bias */
	if (FBZMODE_ENABLE_DEPTH_BIAS(FBZMODE))
	{
		*depthval += (INT16)(VV)->reg[zaColor].u;
		CLAMP(*depthval, 0, 0xffff);
	}

	*out_wfloat = wfloat;
}

void ogl_get_fog_blend(voodoo_state* v, INT32 wfloat, INT32 ITERZ, INT64 ITERW, INT32 *fogblend)
{
	UINT32 FOGMODE = v->reg[fogMode].u;
	UINT32 FBZCP = v->reg[fbzColorPath].u;

	*fogblend = 0;
	if (FOGMODE_ENABLE_FOG(FOGMODE) && !FOGMODE_FOG_CONSTANT(FOGMODE))
	{
		/* fog blending mode */
		switch (FOGMODE_FOG_ZALPHA(FOGMODE))
		{
		case 0:		/* fog table */
			{
				INT32 delta = v->fbi.fogdelta[wfloat >> 10];
				INT32 deltaval;

				/* perform the multiply against lower 8 bits of wfloat */
				deltaval = (delta & v->fbi.fogdelta_mask) *
					((wfloat >> 2) & 0xff);

				/* fog zones allow for negating this value */
				if (FOGMODE_FOG_ZONES(FOGMODE) && (delta & 2))
					deltaval = -deltaval;
				deltaval >>= 6;

				/* apply dither */
//				if (FOGMODE_FOG_DITHER(FOGMODE))
//					deltaval += DITHER4[(XX) & 3];
				deltaval >>= 4;

				/* add to the blending factor */
				*fogblend = v->fbi.fogblend[wfloat >> 10] + deltaval;
				break;
			}

		case 1:		/* iterated A */
//			fogblend = ITERAXXX.rgb.a;
			break;

		case 2:		/* iterated Z */
			CLAMPED_Z((ITERZ), FBZCP, *fogblend);
			*fogblend >>= 8;
			break;

		case 3:		/* iterated W - Voodoo 2 only */
			CLAMPED_W((ITERW), FBZCP, *fogblend);
			break;
		}

	}
}

void ogl_get_vertex_data(INT32 x, INT32 y, const void *extradata, ogl_vertex_data *vd) {
	const poly_extra_data *extra = (const poly_extra_data *)extradata;
	v=(voodoo_state*)extra->state;
	INT32 iterr, iterg, iterb, itera;
	INT32 iterz;
	INT64 iterw;
	INT32 dx, dy;
	INT32 d;

	dx = x - extra->ax;
	dy = y - extra->ay;
	iterr = extra->startr + ((dy * extra->drdy)>>4) + ((dx * extra->drdx)>>4);
	iterg = extra->startg + ((dy * extra->dgdy)>>4) + ((dx * extra->dgdx)>>4);
	iterb = extra->startb + ((dy * extra->dbdy)>>4) + ((dx * extra->dbdx)>>4);
	itera = extra->starta + ((dy * extra->dady)>>4) + ((dx * extra->dadx)>>4);
	iterz = extra->startz + ((dy * extra->dzdy)>>4) + ((dx * extra->dzdx)>>4);
	iterw = extra->startw + ((dy * extra->dwdy)>>4) + ((dx * extra->dwdx)>>4);

	for (Bitu i=0; i<extra->texcount; i++) {
		INT64 iters,itert,iterw;
		UINT32 smax,tmax;
		INT64 s, t;
		INT32 lod=0;

		UINT32 texmode = (UINT32)(i == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
		UINT32 TEXMODE = texmode;
		INT32 LODBASE = (i==0) ? extra->lodbase0 : extra->lodbase1;

		UINT32 ilod = (UINT32)(v->tmu[i].lodmin >> 8);
		if (!((v->tmu[i].lodmask >> ilod) & 1))
			ilod++;

		smax = (v->tmu[i].wmask >> ilod) + 1;
		tmax = (v->tmu[i].hmask >> ilod) + 1;

		iterw = v->tmu[i].startw + ((dy * v->tmu[i].dwdy)>>4) + ((dx * v->tmu[i].dwdx)>>4);
		iters = v->tmu[i].starts + ((dy * v->tmu[i].dsdy)>>4) + ((dx * v->tmu[i].dsdx)>>4);
		itert = v->tmu[i].startt + ((dy * v->tmu[i].dtdy)>>4) + ((dx * v->tmu[i].dtdx)>>4);

		/* determine the S/T/LOD values for this texture */
		if (TEXMODE_ENABLE_PERSPECTIVE(texmode))
		{
			INT64 oow = fast_reciplog((iterw), &lod);
			s = (oow * (iters)) >> 29;
			t = (oow * (itert)) >> 29;
			lod += LODBASE;
		}
		else
		{
			s = (iters) >> 14;
			t = (itert) >> 14;
			lod = LODBASE;
		}

		/* clamp the LOD */
		lod += v->tmu[i].lodbias;
//		if (TEXMODE_ENABLE_LOD_DITHER(TEXMODE))
//			lod += DITHER4[(XX) & 3] << 4;
		if (lod < v->tmu[i].lodmin)
			lod = v->tmu[i].lodmin;
		if (lod > v->tmu[i].lodmax)
			lod = v->tmu[i].lodmax;

		/* clamp W */
		if (TEXMODE_CLAMP_NEG_W(texmode) && (iterw) < 0)
			s = t = 0;

		if (s != 0) vd->m[i].s = (float)((float)s/(float)(smax*(1u<<(18u+ilod))));
		else vd->m[i].s = 0.0f;
		if (t != 0) vd->m[i].t = (float)((float)t/(float)(tmax*(1u<<(18u+ilod))));
		else vd->m[i].t = 0.0f;
		if (iterw != 0) vd->m[i].w = (float)((float)iterw/(float)(0xffffff));
		else vd->m[i].w = 0.0f;

		vd->m[i].sw = vd->m[i].s * vd->m[i].w;
		vd->m[i].tw = vd->m[i].t * vd->m[i].w;
		vd->m[i].z  = 0.0f;

		INT32 lodblend=0;

		if ((TEXMODE_TC_MSELECT(TEXMODE)==4) || (TEXMODE_TCA_MSELECT(TEXMODE)==4))
		{
			if (v->tmu[i].detailbias <= lod)
				lodblend = 0;
			else
			{
				lodblend = (((v->tmu[i].detailbias - lod) << v->tmu[i].detailscale) >> 8);
				if (lodblend > v->tmu[i].detailmax)
					lodblend = v->tmu[i].detailmax;
			}
		} else if ((TEXMODE_TC_MSELECT(TEXMODE)==5) || (TEXMODE_TCA_MSELECT(TEXMODE)==5))
			lodblend = lod & 0xff;

		vd->m[i].lodblend = (float)lodblend/255.0f;
	}

	vd->r = (float)((float)iterr/(float)(1<<20));
	vd->g = (float)((float)iterg/(float)(1<<20));
	vd->b = (float)((float)iterb/(float)(1<<20));
	vd->a = (float)((float)itera/(float)(1<<20));
	vd->z = (float)((float)iterz/(float)(1<<20));
	vd->w = (float)((float)iterw/(float)(0xffffff));

	INT32 wfloat;
	ogl_get_depth(v,iterz,iterw,&d,&wfloat);
	vd->d = (float)((float)d/(float)(0xffff));

	INT32 fogblend;
	ogl_get_fog_blend(v,wfloat,iterz,iterw,&fogblend);
	vd->fogblend = (float)fogblend/255.0f;

//	vd->x = (float)x / (16.0f);
//	vd->y = (float)y / (16.0f);

	// OpenGL-correction for Blood/ShadowWarrior
	vd->x = (((float)x) - (1.0f/16.0f)) / 16.0f;
	vd->y = (((float)y) - (1.0f/16.0f)) / 16.0f;
}

static UINT32 crc_32_tab[] = { /* CRC polynomial 0xedb88320 */
0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

static bool ogl_palette_format(UINT32 format) {
	return format == 1 || format == 9 || format == 5 || format == 14;
}

UINT32 calculate_palsum(UINT32 tmunum, UINT32 texmode) {
	const UINT32 format = TEXMODE_FORMAT(texmode);
	const rgb_t *colors = (format & 7) == 1
		? v->tmu[tmunum].ncc[TEXMODE_NCC_TABLE_SELECT(texmode)].texel
		: v->tmu[tmunum].palette;
	UINT32 csum = 0;
	for (Bitu pct=0; pct<256; pct++) {
		UINT32 pval = colors[pct];
		csum = crc_32_tab[(csum ^ pval) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>8)) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>16)) & 0xff] ^ (csum>>8);
		csum = crc_32_tab[(csum ^ (pval>>24)) & 0xff] ^ (csum>>8);
	}
	return csum;
}

static void ogl_destroy_cached_texture(ogl_texmap &texture) {
	if (texture.ids != NULL) {
		for (std::map<const UINT32, GLuint>::iterator i = texture.ids->begin();
		     i != texture.ids->end(); ++i) {
			glDeleteTextures(1, &i->second);
		}
		delete texture.ids;
		texture.ids = NULL;
	} else {
		glDeleteTextures(1, &texture.current_id);
	}
	texture.current_id = 0;
}

// Writes only accumulate a conservative byte interval. Visit cached textures
// once before the next draw, after pending GL primitives have been completed.
struct ogl_dirty_range {
	bool pending = false;
	UINT32 first = 0;
	UINT32 last = 0;
};
static ogl_dirty_range texture_dirty[2];

static void ogl_flush_texture_writes(int TMU) {
	ogl_dirty_range &dirty = texture_dirty[TMU];
	if (!dirty.pending) return;
	const UINT32 mask = v->tmu[TMU].mask;
	for (auto t = textures[TMU].begin(); t != textures[TMU].end();) {
		const UINT32 size = t->second.width * t->second.height *
			(t->second.format < 8 ? 1 : 2);
		const UINT32 last = t->first + size - 1;
		const bool overlaps = last <= mask
			? t->first <= dirty.last && last >= dirty.first
			: t->first <= dirty.last || (last & mask) >= dirty.first;
		if (overlaps) {
			VOGL_ClearBeginMode();
			ogl_destroy_cached_texture(t->second);
			textures[TMU].erase(t++);
		} else {
			++t;
		}
	}
	dirty.pending = false;
}

void ogl_cache_texture(const poly_extra_data *extra, ogl_texture_data *td) {
	v=(voodoo_state*)extra->state;
	UINT32 texbase;

	INT32 smax, tmax;
	UINT32 *texrgbp;
	GLuint texID;

	UINT32 num_tmus = 1;
	if (v->tmu[1].ram != NULL) num_tmus++;

	for (UINT32 j=0; j<num_tmus; j++) {
		ogl_flush_texture_writes(j);
		UINT32 TEXMODE = (UINT32)(j==0 ? extra->r_textureMode0 : extra->r_textureMode1);
		const UINT32 format = TEXMODE_FORMAT(TEXMODE);
		const UINT32 ncc_table = (format & 7) == 1 ? TEXMODE_NCC_TABLE_SELECT(TEXMODE) : 0;

		td[j].enable = false;
		if (j < extra->texcount && v->tmu[j].lodmin < (8 << 8)) {
			UINT32 ilod = (UINT32)(v->tmu[j].lodmin >> 8);
			if (!((v->tmu[j].lodmask >> ilod) & 1))
				ilod++;

			texbase = v->tmu[j].lodoffset[ilod];

			const UINT32 width = (v->tmu[j].wmask >> ilod) + 1;
			const UINT32 height = (v->tmu[j].hmask >> ilod) + 1;
			bool valid_texid = true;
			std::map<const UINT32, ogl_texmap>::iterator t;
			t = textures[j].find(texbase);
			// A RAM address can be reused with a different texture layout.
			if (t != textures[j].end() &&
			    (t->second.format != format || t->second.width != width ||
			     t->second.height != height || t->second.ilod != ilod ||
			     t->second.lodmask != v->tmu[j].lodmask || t->second.ncc_table != ncc_table)) {
				VOGL_ClearBeginMode();
				ogl_destroy_cached_texture(t->second);
				textures[j].erase(t);
				t = textures[j].end();
			}
			if (t != textures[j].end())  {
				if (t->second.valid_pal) {
					texID = t->second.current_id;
				} else {
					UINT32 psum = calculate_palsum(j, TEXMODE);
					std::map<const UINT32, GLuint>::iterator u = t->second.ids->find(psum);
					if (u != t->second.ids->end()) {
						t->second.valid_pal = true;
						t->second.current_id = u->second;
						texID = u->second;
					} else {
						valid_texid = false;
//						LOG_MSG("texture removed... size %d",t->second.ids->size());
						if (t->second.ids->size() > 8) {
							for (u=t->second.ids->begin(); u!=t->second.ids->end(); ++u) {
								glDeleteTextures(1,&u->second);
							}
							t->second.ids->clear();
						}
						t->second.valid_pal = true;
					}
				}
			} else {
				valid_texid = false;
			}
			if (!valid_texid) {
				smax = (INT32)width;
				tmax = (INT32)height;

				UINT32 texboffset = texbase;
				texrgbp = (UINT32 *)&texrgb[0];
				memset(texrgbp,0,256*256*4);

				texID = ogl_texture_index++;
				glGenTextures(1, &texID);

				if (format < 8) {
					if (format != 5) {
						for (int i=0; i<(smax*tmax); i++) {
							UINT8 *texptr8 = (UINT8 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
							UINT32 data = v->tmu[j].lookup[*texptr8];
							*texrgbp = data;
							texboffset++;
							texrgbp++;
						}
					} else {
						for (int i=0; i<(smax*tmax); i++) {
							UINT8 *texptr8 = (UINT8 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
							UINT8 texel = *texptr8;
							UINT32 data = v->tmu[j].lookup[texel];
							*texrgbp = data;
							texboffset++;
							texrgbp++;
						}
					}
				} else if (format >= 10 && format <= 12) {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT32 data = v->tmu[j].lookup[*texptr16];
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				} else if (format != 14) {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT32 data = (v->tmu[j].lookup[*texptr16 & 0xFF] & 0xFFFFFF) | ((*texptr16 & 0xff00u) << 16u);
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				} else {
					for (int i=0; i<(smax*tmax); i++) {
						UINT16 *texptr16 = (UINT16 *)&v->tmu[j].ram[(texboffset) & v->tmu[j].mask];
						UINT16 texel1 = *texptr16 & 0xFF;
						UINT32 data = (v->tmu[j].lookup[texel1] & 0xFFFFFF) | ((*texptr16 & 0xff00u) << 16u);
						*texrgbp = data;
						texboffset+=2;
						texrgbp++;
					}
				}

				texrgbp = (UINT32 *)&texrgb[0];
//				LOG_MSG("texid %d format %d -- %d x %d",
//					texID,format,smax,tmax);

				glBindTexture(GL_TEXTURE_2D, texID);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,TEXMODE_CLAMP_S(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
				glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,TEXMODE_CLAMP_T(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, smax, tmax, 0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, texrgbp);
				extern PFNGLGENERATEMIPMAPEXTPROC glGenerateMipmapEXT;
				glGenerateMipmapEXT(GL_TEXTURE_2D);
				if (ogl_palette_format(format)) {
					UINT32 palsum = calculate_palsum(j, TEXMODE);
					if (t == textures[j].end()) {
						std::map<const UINT32, GLuint>* ids = new std::map<const UINT32, GLuint>();
						(*ids)[palsum] = texID;
						ogl_texmap tex = { true, format, texID, ids,
							width, height, ilod, v->tmu[j].lodmask, ncc_table };
						textures[j][texbase] = tex;
					} else {
						(*textures[j][texbase].ids)[palsum] = texID;
						textures[j][texbase].current_id = texID;
					}
				} else {
					ogl_texmap tex = { true, format, texID, NULL,
						width, height, ilod, v->tmu[j].lodmask, ncc_table };
					textures[j][texbase] = tex;
				}
			}

			td[j].texID = texID;
			td[j].enable = true;
		}
	}
}

void voodoo_ogl_invalidate_paltex(void) {
	std::map<const UINT32, ogl_texmap>::iterator t;
	for (int j=0; j<2; j++) {
		for (t=textures[j].begin(); t!=textures[j].end(); ++t) {
			if (ogl_palette_format(t->second.format)) {
				t->second.valid_pal = false;
			}
		}
	}
}

GLhandleARB m_hProgramObject = 0;

void ogl_printInfoLog(GLhandleARB obj)
{
    int infologLength = 0;
    int charsWritten  = 0;

    glGetObjectParameterivARB(obj, GL_OBJECT_INFO_LOG_LENGTH_ARB, &infologLength);

    if (infologLength > 0)
    {
		char *infoLog = (char *)malloc((size_t)infologLength);
		glGetInfoLogARB(obj, infologLength, &charsWritten, infoLog);
		LOG_MSG("%s\n",infoLog);
		free(infoLog);
    }
}

void ogl_sh_tex_combine(std::string *strFShader, const int TMU, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 TEXMODE = (UINT32)(TMU == 0 ? extra->r_textureMode0 : extra->r_textureMode1);

	if (!TEXMODE_TC_ZERO_OTHER(TEXMODE))
		*strFShader += "  tt.rgb = cother.rgb;\n";
	else
		*strFShader += "  tt.rgb = vec3(0.0);\n";

	if (!TEXMODE_TCA_ZERO_OTHER(TEXMODE))
		*strFShader += "  tt.a = cother.a;\n";
	else
		*strFShader += "  tt.a = 0.0;\n";

	if (TEXMODE_TC_SUB_CLOCAL(TEXMODE))
		*strFShader += "  tt.rgb -= clocal.rgb;\n";
	if (TEXMODE_TCA_SUB_CLOCAL(TEXMODE))
		*strFShader += "  tt.a -= clocal.a;\n";

	switch (TEXMODE_TC_MSELECT(TEXMODE))
	{
	default:
	case 0:
		*strFShader += "  blend.rgb = vec3(0.0);\n";
		break;
	case 1:
		*strFShader += "  blend.rgb = clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  blend.rgb = vec3(cother.a);\n";
		break;
	case 3:
		*strFShader += "  blend.rgb = vec3(clocal.a);\n";
		break;
	case 4:
		// detail //
	case 5:
		// LOD //
		*strFShader += "  blend.rgb = vec3(f_lodblend";
		*strFShader += TMU==0?"0":"1";
		*strFShader += ");\n";
		break;
	}

	switch (TEXMODE_TCA_MSELECT(TEXMODE))
	{
	default:
	case 0:
		*strFShader += "  blend.a = 0.0;\n";
		break;
	case 1:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 2:
		*strFShader += "  blend.a = cother.a;\n";
		break;
	case 3:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 4:
		// detail //
	case 5:
		// LOD //
		*strFShader += "  blend.a = f_lodblend";
		*strFShader += TMU==0?"0":"1";
		*strFShader += ";\n";
		break;
	}

	if (!TEXMODE_TC_REVERSE_BLEND(TEXMODE))
		*strFShader += "  blend.rgb = vec3(1.0) - blend.rgb;\n";

	if (!TEXMODE_TCA_REVERSE_BLEND(TEXMODE))
		*strFShader += "  blend.a = 1.0 - blend.a;\n";

	*strFShader += "  tt *= blend;\n";

	switch (TEXMODE_TC_ADD_ACLOCAL(TEXMODE))
	{
	case 3:
	case 0:
		break;
	case 1:
		*strFShader += "  tt.rgb += clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  tt.rgb += vec3(clocal.a);\n";
		break;
	}

	if (TEXMODE_TCA_ADD_ACLOCAL(TEXMODE))
		*strFShader += "  tt.a += clocal.a;\n";

	*strFShader += "  clocal = tt;\n";

	if (TEXMODE_TC_INVERT_OUTPUT(TEXMODE))
		*strFShader += "  clocal.rgb = vec3(1.0)-clocal.rgb;\n";

	if (TEXMODE_TCA_INVERT_OUTPUT(TEXMODE))
		*strFShader += "  clocal.a = 1.0 - clocal.a;\n";

}

void ogl_sh_color_path(std::string *strFShader, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 FBZCOLORPATH = v->reg[fbzColorPath].u;
	UINT32 FBZMODE = v->reg[fbzMode].u;
	UINT32 ALPHAMODE = v->reg[alphaMode].u;

	switch (FBZCP_CC_RGBSELECT(FBZCOLORPATH))
	{
	case 0:
		*strFShader += "  cother = gl_Color;\n";
		break;
	case 1:
		*strFShader += "  cother = texel;\n";
		break;
	case 2:
		*strFShader += "  cother = color1;\n";
		break;
	default:
		*strFShader += "  cother = vec4(0.0);\n";
	}

	// TODO fix chroma key //
	if (FBZMODE_ENABLE_CHROMAKEY(FBZMODE)) {
//		if (!CHROMARANGE_ENABLE(v->reg[chromaRange].u))
			*strFShader +=	"  if (distance (cother.rgb , chromaKey.rgb) < 0.0001) discard;\n";
//		else {
//			*strFShader +=	"  if ((cother.rgb >= (chromaKey.rgb-0.01)) && (cother.rgb <= (chromaRange.rgb+0.01))) discard;";
//		}
	}


	switch (FBZCP_CC_ASELECT(FBZCOLORPATH))
	{
	case 0:
		*strFShader += "  cother.a = gl_Color.a;\n";
		break;
	case 1:
		*strFShader += "  cother.a = texel.a;\n";
		break;
	case 2:
		*strFShader += "  cother.a = color1.a;\n";
		break;
	default:
		*strFShader += "  cother.a = 0.0;\n";
		break;
	}

	// TODO check alpha mask //
//	if (FBZMODE_ENABLE_ALPHA_MASK(FBZMODE))
//		*strFShader += "  if (cother.a > 0.0) discard;";

	if (ALPHAMODE_ALPHATEST(ALPHAMODE))
		switch (ALPHAMODE_ALPHAFUNCTION(ALPHAMODE)) {
			case 0:
				*strFShader += "  discard;\n";
				break;
			case 1:
				*strFShader += "  if (cother.a >= alphaRef) discard;\n";
				break;
			case 2:
//				*strFShader += "  if (cother.a != alphaRef) discard;\n";
				*strFShader += "  if (distance(cother.a , alphaRef) > 0.0001) discard;\n";
				break;
			case 3:
				*strFShader += "  if (cother.a >  alphaRef) discard;\n";
				break;
			case 4:
				*strFShader += "  if (cother.a <= alphaRef) discard;\n";
				break;
			case 5:
//				*strFShader += "  if (cother.a == alphaRef) discard;\n";
				*strFShader += "  if (distance(cother.a , alphaRef) < 0.0001) discard;\n";
				break;
			case 6:
				*strFShader += "  if (cother.a <  alphaRef) discard;\n";
				break;
			case 7:
				break;
	}

	if (FBZCP_CC_LOCALSELECT_OVERRIDE(FBZCOLORPATH) == 0)
	{
		if (FBZCP_CC_LOCALSELECT(FBZCOLORPATH) == 0)
			*strFShader += "  clocal = gl_Color;\n";
		else
			*strFShader += "  clocal = color0;\n";
	}
	else
	{
		*strFShader +=	"  if (texel.a < 0.5) {\n"
							"    clocal = gl_Color;\n"
						"  } else {\n"
							"    clocal = color0;\n"
						"  }\n";
	}

	switch (FBZCP_CCA_LOCALSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  clocal.a = gl_Color.a;\n";
		break;
	case 1:
		*strFShader += "  clocal.a = color0.a;\n";
		break;
	case 2:
		*strFShader += "  clocal.a = gl_Color.a;\n"; // TODO CLAMPED_Z
		break;
	case 3:
		// voodoo2 only
		break;
	}

	if (FBZCP_CC_ZERO_OTHER(FBZCOLORPATH) == 0)
		*strFShader += "  tt.rgb = cother.rgb;\n";
	else
		*strFShader += "  tt.rgb = vec3(0.0);\n";

	if (FBZCP_CCA_ZERO_OTHER(FBZCOLORPATH) == 0)
		*strFShader += "  tt.a = cother.a;\n";
	else
		*strFShader += "  tt.a = 0.0;\n";

	if (FBZCP_CC_SUB_CLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.rgb -= clocal.rgb;\n";

	if (FBZCP_CCA_SUB_CLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.a -= clocal.a;\n";

	switch (FBZCP_CC_MSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  blend.rgb = vec3(0.0);\n";
		break;
	case 1:
		*strFShader += "  blend.rgb = clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  blend.rgb = vec3(cother.a);\n";
		break;
	case 3:
		*strFShader += "  blend.rgb = vec3(clocal.a);\n";
		break;
	case 4:
		*strFShader += "  blend.rgb = vec3(texel.a);\n";
		break;
	case 5:
		// voodoo2 only
		*strFShader += "  blend.rgb = texel.rgb;\n";
		break;
	}

	switch (FBZCP_CCA_MSELECT(FBZCOLORPATH))
	{
	default:
	case 0:
		*strFShader += "  blend.a = 0.0;\n";
		break;
	case 1:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 2:
		*strFShader += "  blend.a = cother.a;\n";
		break;
	case 3:
		*strFShader += "  blend.a = clocal.a;\n";
		break;
	case 4:
		*strFShader += "  blend.a = texel.a;\n";
		break;
	}

	if (!FBZCP_CC_REVERSE_BLEND(FBZCOLORPATH))
		*strFShader += "  blend.rgb = vec3(1.0) - blend.rgb;\n";

	if (!FBZCP_CCA_REVERSE_BLEND(FBZCOLORPATH))
		*strFShader += "  blend.a = 1.0 - blend.a;\n";

	*strFShader += "  tt *= blend;\n";

	switch (FBZCP_CC_ADD_ACLOCAL(FBZCOLORPATH))
	{
	case 3:
	case 0:
		break;
	case 1:
		*strFShader += "  tt.rgb += clocal.rgb;\n";
		break;
	case 2:
		*strFShader += "  tt.rgb += vec3(clocal.a);\n";
		break;
	}

	if (FBZCP_CCA_ADD_ACLOCAL(FBZCOLORPATH))
		*strFShader += "  tt.a += clocal.a;\n";

	// clamp ?? //

	*strFShader += "  pixel = tt;\n";

	if (FBZCP_CC_INVERT_OUTPUT(FBZCOLORPATH))
		*strFShader += "  pixel.rgb = vec3(1.0) - tt.rgb;\n";

	if (FBZCP_CCA_INVERT_OUTPUT(FBZCOLORPATH))
		*strFShader += "  pixel.a = 1.0 - tt.a;\n";
}

void ogl_sh_fog(std::string *strFShader, const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	UINT32 FOGMODE = v->reg[fogMode].u;

	*strFShader += "  vec4 ff;\n";

	/* constant fog bypasses everything else */
	if (FOGMODE_FOG_CONSTANT(FOGMODE))
		*strFShader += "  ff = fogColor;\n";

	/* non-constant fog comes from several sources */
	else {
		/* if fog_add is zero, we start with the fog color */
		if (FOGMODE_FOG_ADD(FOGMODE) == 0)
			*strFShader += "  ff = fogColor;\n";
		else
			*strFShader += "  ff = vec4(0.0);\n";

		/* if fog_mult is zero, we subtract the incoming color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)
			*strFShader += "  ff -= pixel;\n";

		*strFShader += "  float fogblend;\n";
		/* fog blending mode */
		switch (FOGMODE_FOG_ZALPHA(FOGMODE))
		{
			case 0:		/* fog table */
				// blend factor calculated in ogl_get_fog_blend //
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
			case 1:		/* iterated A */
				*strFShader += "  fogblend = gl_Color.a;\n";
				break;
			case 2:		/* iterated Z */
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
			case 3:		/* iterated W - Voodoo 2 only */
				*strFShader += "  fogblend = f_fogblend;\n";
				break;
		}

		/* perform the blend */
		*strFShader += "  ff *= fogblend;\n";

		/* if fog_mult is 0, we add this to the original color */
		if (FOGMODE_FOG_MULT(FOGMODE) == 0)
			*strFShader += "  pixel.rgb += ff.rgb;\n";
		/* otherwise this just becomes the new color */
		else
			*strFShader += "  pixel.rgb = ff.rgb;\n";

	}
}


void ogl_shaders(const poly_extra_data *extra) {
	v=(voodoo_state*)extra->state;

	GLint res;
	std::string strVShader, strFShader;

	/* shaders extensions not loaded */
	if (!glCreateShaderObjectARB) return;

//	UINT32 FBZMODE      = extra->r_fbzMode;
	UINT32 FOGMODE      = extra->r_fogMode;
	UINT32 texcount     = extra->texcount;

	/* build a new shader program */
	if (!extra->info->shader_ready) {

		/* create vertex shader */
		int fcount=0;
		while (glGetError()!=0) {
			fcount++;
			if (fcount>1000) E_Exit("opengl error");
		}

		GLhandleARB m_hVertexShader = glCreateShaderObjectARB(GL_VERTEX_SHADER_ARB);

		strVShader =
			"attribute float v_fogblend;\n"
			"varying   float f_fogblend;\n"
			"attribute float v_lodblend0;\n"
			"varying   float f_lodblend0;\n"
			"attribute float v_lodblend1;\n"
			"varying   float f_lodblend1;\n"
			"\n"
			"void main()"
			"{\n"
				"  gl_TexCoord[0] = gl_MultiTexCoord0;\n"
				"  gl_TexCoord[1] = gl_MultiTexCoord1;\n"
				"  gl_FrontColor = gl_Color;\n"
				"  f_fogblend = v_fogblend;\n"
				"  f_lodblend0 = v_lodblend0;\n"
				"  f_lodblend1 = v_lodblend1;\n"
				"  gl_Position = ftransform();\n"
			"}\n";

		const char *szVShader = strVShader.c_str();
		glShaderSourceARB(m_hVertexShader, 1, &szVShader, NULL);
		glCompileShaderARB(m_hVertexShader);
		glGetObjectParameterivARB(m_hVertexShader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
		if(res == 0) {
			char infobuffer[1000];
			int infobufferlen = 0;
			glGetInfoLogARB(m_hVertexShader, 999, &infobufferlen, infobuffer);
			infobuffer[infobufferlen] = 0;
			ogl_printInfoLog(m_hVertexShader);
			E_Exit("ERROR: Error compiling vertex shader");
			return;
		}

		/* create fragment shader */
		GLhandleARB m_hFragmentShader = glCreateShaderObjectARB(GL_FRAGMENT_SHADER_ARB);
		strFShader =
			"varying float f_fogblend;\n"
			"varying float f_lodblend0;\n"
			"varying float f_lodblend1;\n"
			"uniform sampler2D tex0;\n"
			"uniform sampler2D tex1;\n"
			"uniform vec4 color0;\n"
			"uniform vec4 color1;\n"
			"uniform vec4 chromaKey;\n"
			"uniform vec4 chromaRange;\n"
			"uniform float alphaRef;\n"
			"uniform float zaColor;\n"
			"uniform vec4 fogColor;\n"
			"\n"
			"void main()"
			"{\n"
				"  vec4 pixel  = vec4(0.0);\n"
				"  vec4 texel  = vec4(1.0);\n"
				"  vec4 clocal = vec4(1.0);\n"
				"  vec4 cother = vec4(0.0);\n"
				"  vec4 tt     = vec4(0.0);\n"
				"  vec4 blend  = vec4(0.0);\n";

				// TODO glsl depth test? //

				if (texcount >= 2 && v->tmu[1].lodmin < (8 << 8))
				{
					strFShader += "  clocal = texture2DProj(tex1,gl_TexCoord[1]);\n";
					ogl_sh_tex_combine(&strFShader, 1, extra);
					strFShader += "  cother = clocal;\n";
					strFShader += "  texel = clocal;\n";
				}

				if (texcount >= 1 && v->tmu[0].lodmin < (8 << 8))
				{
					strFShader += "  clocal = texture2DProj(tex0,gl_TexCoord[0]);\n";
					ogl_sh_tex_combine(&strFShader, 0, extra);
					strFShader += "  texel = clocal;\n";
				}

				// TODO Clamped ARGB //

				ogl_sh_color_path(&strFShader, extra);

				// fogging //
				if (FOGMODE_ENABLE_FOG(FOGMODE))
					ogl_sh_fog(&strFShader, extra);

				strFShader += "  gl_FragColor = pixel;\n";

				strFShader +=
			"}";

		const char *szFShader = strFShader.c_str();
		glShaderSourceARB(m_hFragmentShader, 1, &szFShader, NULL);

		glCompileShaderARB(m_hFragmentShader);
		glGetObjectParameterivARB(m_hFragmentShader, GL_OBJECT_COMPILE_STATUS_ARB, &res);
		if(res == 0) {
			ogl_printInfoLog(m_hFragmentShader);
			E_Exit("ERROR: Error compiling fragment shader");
			return;
		}


		/* create program object */
		m_hProgramObject = glCreateProgramObjectARB();

		glAttachObjectARB(m_hProgramObject, m_hVertexShader);
		glAttachObjectARB(m_hProgramObject, m_hFragmentShader);

		glLinkProgramARB(m_hProgramObject);

		glGetObjectParameterivARB(m_hProgramObject, GL_OBJECT_LINK_STATUS_ARB, &res);
		if(res == 0) {
			ogl_printInfoLog(m_hProgramObject);
			E_Exit("ERROR: Error linking program");
			return;
		}

		/* use this shader */
		glUseProgramObjectARB(m_hProgramObject);
		extra->info->so_shader_program=(uintptr_t)m_hProgramObject;
		extra->info->so_vertex_shader=(uintptr_t)m_hVertexShader;
		extra->info->so_fragment_shader=(uintptr_t)m_hFragmentShader;

		extra->info->shader_ready=true;

		GLenum glerr=glGetError();
		if (glerr!=0) {
			E_Exit("create shader start glError->%x",glerr);
		}

		int* locations=new int[12];
		locations[0]=glGetUniformLocationARB(m_hProgramObject, "chromaKey");
		locations[1]=glGetUniformLocationARB(m_hProgramObject, "chromaRange");
		locations[2]=glGetUniformLocationARB(m_hProgramObject, "color0");
		locations[3]=glGetUniformLocationARB(m_hProgramObject, "color1");
		locations[4]=glGetUniformLocationARB(m_hProgramObject, "alphaRef");
		locations[5]=glGetUniformLocationARB(m_hProgramObject, "zaColor");
		locations[6]=glGetUniformLocationARB(m_hProgramObject, "tex0");
		locations[7]=glGetUniformLocationARB(m_hProgramObject, "tex1");
		locations[8]=glGetUniformLocationARB(m_hProgramObject, "fogColor");

		locations[9] = glGetAttribLocationARB(m_hProgramObject, "v_fogblend");
		locations[10] = glGetAttribLocationARB(m_hProgramObject, "v_lodblend0");
		locations[11] = glGetAttribLocationARB(m_hProgramObject, "v_lodblend1");
		extra->info->shader_ulocations=locations;
	} else {
		/* use existing shader program */
		if (m_hProgramObject != (GLhandleARB)extra->info->so_shader_program) {
			glUseProgramObjectARB((GLhandleARB)extra->info->so_shader_program);
			m_hProgramObject = (GLhandleARB)extra->info->so_shader_program;
		}
	}

	if (extra->info->shader_ulocations[0]>=0) glUniform4fARB(extra->info->shader_ulocations[0], v->reg[chromaKey].rgb.r/255.0f, v->reg[chromaKey].rgb.g/255.0f, v->reg[chromaKey].rgb.b/255.0f,0);
	if (extra->info->shader_ulocations[1]>=0) glUniform4fARB(extra->info->shader_ulocations[1], v->reg[chromaRange].rgb.r/255.0f, v->reg[chromaRange].rgb.g/255.0f, v->reg[chromaRange].rgb.b/255.0f,0);
	if (extra->info->shader_ulocations[2]>=0) glUniform4fARB(extra->info->shader_ulocations[2], v->reg[color0].rgb.r/255.0f, v->reg[color0].rgb.g/255.0f, v->reg[color0].rgb.b/255.0f, v->reg[color0].rgb.a/255.0f);
	if (extra->info->shader_ulocations[3]>=0) glUniform4fARB(extra->info->shader_ulocations[3], v->reg[color1].rgb.r/255.0f, v->reg[color1].rgb.g/255.0f, v->reg[color1].rgb.b/255.0f, v->reg[color1].rgb.a/255.0f);
	if (extra->info->shader_ulocations[4]>=0) glUniform1fARB(extra->info->shader_ulocations[4], v->reg[alphaMode].rgb.a/255.0f);
	if (extra->info->shader_ulocations[5]>=0) glUniform1fARB(extra->info->shader_ulocations[5], (float)((UINT16)v->reg[zaColor].u)/65535.0f);
	if (extra->info->shader_ulocations[8]>=0) glUniform4fARB(extra->info->shader_ulocations[8], v->reg[fogColor].rgb.r/255.0f, v->reg[fogColor].rgb.g/255.0f, v->reg[fogColor].rgb.b/255.0f,1.0f);

}


void voodoo_ogl_draw_triangle(poly_extra_data *extra) {
	v=extra->state;
	ogl_texture_data td[2] = {};
	ogl_vertex_data vd[3];

	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();

	td[0].enable = false;
	td[1].enable = false;

	UINT32 ALPHAMODE = extra->r_alphaMode;
	UINT32 FBZMODE   = extra->r_fbzMode;


	ogl_get_vertex_data(v->fbi.ax, v->fbi.ay, (void*)extra, &vd[0]);
	ogl_get_vertex_data(v->fbi.bx, v->fbi.by, (void*)extra, &vd[1]);
	ogl_get_vertex_data(v->fbi.cx, v->fbi.cy, (void*)extra, &vd[2]);

	for (int i = 0; i < 3; i++)
	{
		vd[i].x = (vd[i].x * new_width) / v->fbi.width;
		vd[i].y = (vd[i].y * new_height) / v->fbi.height;
	}

	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		if (m_hProgramObject != 0) {
			glUseProgramObjectARB(0);
			m_hProgramObject = 0;
		}

		if ((FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) && (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0)) {
			VOGL_SetDepthMode(1,FBZMODE_DEPTH_FUNCTION(FBZMODE));
		} else {
			VOGL_SetDepthMode(0,0);
		}

		VOGL_SetDepthMaskMode(false);
		VOGL_SetColorMaskMode(false, false);

		VOGL_SetAlphaMode(0, 0,0,0,0);

		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
			VOGL_SetDrawMode(true);
		} else {
			VOGL_SetDrawMode(false);
		}

		glEnable(GL_STENCIL_TEST);
		glClear(GL_STENCIL_BUFFER_BIT);
		glStencilFunc(GL_ALWAYS, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);

		glBegin(GL_TRIANGLES);

		glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

		float depth = (float)(v->reg[zaColor].u&0xffff)/(float)0xffff;
		for (int i=0;i<3;i++)
			glVertex3f(vd[i].x, vd[i].y, depth);

		glEnd();
	}

	ogl_cache_texture(extra,td);
	ogl_shaders(extra);

	if (extra->texcount > 0) {
		for (unsigned int t=0; t<2; t++)
		if ( td[t].enable ) {
			UINT32 TEXMODE = (UINT32)(t == 0 ? extra->r_textureMode0 : extra->r_textureMode1);
			glActiveTexture(GL_TEXTURE0_ARB+t);
			glBindTexture (GL_TEXTURE_2D, td[t].texID);
			if (!extra->info->shader_ready) {
				glEnable (GL_TEXTURE_2D);
				// TODO proper fixed-pipeline combiners
				glTexEnvi (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
			} else {
				if (extra->info->shader_ulocations[6+t] >= 0)
					glUniform1iARB(extra->info->shader_ulocations[6+t],(GLint)t);
			}

			GLint minFilter;
			minFilter = (int)GL_NEAREST + (int)TEXMODE_MINIFICATION_FILTER(TEXMODE);
			if (v->tmu[t].lodmin != v->tmu[t].lodmax)
				minFilter += 0x0100 + (int)TEXMODE_TRILINEAR(TEXMODE)*2;
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minFilter);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,(int)GL_NEAREST+(int)TEXMODE_MAGNIFICATION_FILTER(TEXMODE));
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,TEXMODE_CLAMP_S(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
			glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,TEXMODE_CLAMP_T(TEXMODE)?GL_CLAMP_TO_EDGE:GL_REPEAT);
		}
	}

	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		glStencilFunc(GL_EQUAL, 1, 1);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
			VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
		} else {
			VOGL_SetDepthMode(0,0);
		}
	} else {
		if (FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0) {
			if (FBZMODE_ENABLE_DEPTHBUF(FBZMODE)) {
				VOGL_SetDepthMode(1,FBZMODE_DEPTH_FUNCTION(FBZMODE));
			} else {
				if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
					VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
				} else {
					VOGL_SetDepthMode(0,0);
				}
			}
		} else {
			VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);
		}
	}

	bool color_mask = (FBZMODE_RGB_BUFFER_MASK(FBZMODE) > 0);

	if (FBZMODE_AUX_BUFFER_MASK(FBZMODE) > 0) {
		VOGL_SetDepthMaskMode(FBZMODE_ENABLE_ALPHA_PLANES(FBZMODE) == 0);
		VOGL_SetColorMaskMode(color_mask, true);
	} else {
		VOGL_SetDepthMaskMode(false);
		VOGL_SetColorMaskMode(color_mask, false);
	}

	if (ALPHAMODE_ALPHABLEND(ALPHAMODE)) {
		VOGL_SetAlphaMode(1, ogl_sfactor[ALPHAMODE_SRCRGBBLEND(ALPHAMODE)], ogl_dfactor[ALPHAMODE_DSTRGBBLEND(ALPHAMODE)],
			(ALPHAMODE_SRCALPHABLEND(ALPHAMODE)==4)?GL_ONE:GL_ZERO,
			(ALPHAMODE_DSTALPHABLEND(ALPHAMODE)==4)?GL_ONE:GL_ZERO);
	} else {
		VOGL_SetAlphaMode(0, 0,0,0,0);
	}

	if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
		cached_line_front_y=-1;
	} else {
		VOGL_SetDrawMode(false);
		cached_line_back_y=-1;
	}


	glBegin(GL_TRIANGLES);

	for (unsigned int i=0;i<3;i++) {
		glColor4fv(&vd[i].r);

		for (unsigned int t=0;t<2;t++)
			if (td[t].enable) {
				glMultiTexCoord4fv(GL_TEXTURE0_ARB+t,&vd[i].m[t].sw);
				if (extra->info->shader_ulocations[10u+t] >= 0)
					glVertexAttrib1fARB((GLuint)extra->info->shader_ulocations[10u+t],vd[i].m[t].lodblend);
			}

		if (extra->info->shader_ulocations[9] >= 0)
			glVertexAttrib1fARB((GLuint)extra->info->shader_ulocations[9],vd[i].fogblend);

		glVertex3fv(&vd[i].x);
	}

	glEnd();

	if (FBZMODE_DEPTH_SOURCE_COMPARE(FBZMODE) && VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) {
		glDisable(GL_STENCIL_TEST);
	}

	if (!extra->info->shader_ready) {
		glDisable (GL_TEXTURE_2D);
	}
}


void voodoo_ogl_swap_buffer() {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();

	/* the back buffer becomes the front buffer (it is shown at the next vertical retrace) */
	vctx.front ^= 1;
	voodoo_ogl_apply_buffers();

	cached_line_front_y=-1;
	cached_line_back_y=-1;
}


void voodoo_ogl_texture_clear(UINT32 address, int TMU) {
	if (textures[TMU].empty()) return;
	const UINT32 mask = v->tmu[TMU].mask;
	UINT32 first = address & mask;
	UINT32 last = (first + 3) & mask;
	// A wrapped dword conservatively dirties the whole circular address space.
	if (last < first) {
		first = 0;
		last = mask;
	}
	ogl_dirty_range &dirty = texture_dirty[TMU];
	if (!dirty.pending) {
		dirty.first = first;
		dirty.last = last;
		dirty.pending = true;
	} else {
		if (first < dirty.first) dirty.first = first;
		if (last > dirty.last) dirty.last = last;
	}
}


/* the render resolution pixels [p0, p1) that 3dfx video mode pixel i covers: the ones with
 * their center in it (at least one), the same ones a GL_NEAREST blit from the 3dfx video
 * mode's size fills from it. LFB writes and reads both use this. */
static void voodoo_ogl_lfb_span(int i, UINT32 render_size, UINT32 mode_size, int &p0, int &p1) {
	const double d = (double)render_size / (double)mode_size;
	p0 = (int)ceil(i * d - 0.5);
	p1 = (int)ceil((i + 1) * d - 0.5);
	if (p1 <= p0) p1 = p0 + 1;
	if (p0 >= (int)render_size) p0 = (int)render_size - 1;
	if (p1 > (int)render_size) p1 = (int)render_size;
}

/* an LFB pixel (x, y) in the 3dfx video mode, row 0 at the top: a quad over the render
 * resolution pixels it covers (between glBegin(GL_QUADS) and glEnd()). The projection is
 * upside down when the Y origin is at the bottom (fbzMode). */
static void voodoo_ogl_lfb_quad(int x, int y, float z) {
	int x0, x1, y0, y1;
	voodoo_ogl_lfb_span(x, new_width, v->fbi.width, x0, x1);
	voodoo_ogl_lfb_span(y, new_height, v->fbi.height, y0, y1);
	if (last_orientation == 1) {
		const int t = (int)new_height - y0;
		y0 = (int)new_height - y1;
		y1 = t;
	}
	glVertex3f((float)x0, (float)y0, z);
	glVertex3f((float)x1, (float)y0, z);
	glVertex3f((float)x1, (float)y1, z);
	glVertex3f((float)x0, (float)y1, z);
}

void voodoo_ogl_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
	if (!voodoo_ogl_enter()) return;
	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(0,0);

	VOGL_SetDepthMaskMode(false);
	VOGL_SetColorMaskMode(has_rgb, has_alpha);

	VOGL_SetAlphaMode(0, 0,0,0,0);

	VOGL_BeginMode(GL_QUADS);
	glColor4ub((GLubyte)(r&0xff), (GLubyte)(g&0xff), (GLubyte)(b&0xff), (GLubyte)(a&0xff));
	voodoo_ogl_lfb_quad(x, y, 0.0f);
}

void voodoo_ogl_draw_z(int x, int y, int z) {
	if (!voodoo_ogl_enter()) return;
//	VOGL_ClearBeginMode();

	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(1,GL_ALWAYS-GL_NEVER);

	VOGL_SetDepthMaskMode(true);
	VOGL_SetColorMaskMode(false, false);

	VOGL_SetAlphaMode(0, 0,0,0,0);

	VOGL_BeginMode(GL_QUADS);
//	glBegin(GL_POINTS);
	voodoo_ogl_lfb_quad(x, y, (float)z);	// z adjustment??
//	glEnd();
}

void voodoo_ogl_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
	if (!voodoo_ogl_enter()) return;
//	VOGL_ClearBeginMode();

	// TODO redo everything //
	if (m_hProgramObject != 0) {
		glUseProgramObjectARB(0);
		m_hProgramObject = 0;
	}

	if (LFBMODE_WRITE_BUFFER_SELECT(v->reg[lfbMode].u)==0) {
		VOGL_SetDrawMode(true);
		v->fbi.vblank_flush_pending=true;
	} else {
		VOGL_SetDrawMode(false);
	}

	VOGL_SetDepthMode(0,0);

	VOGL_SetDepthMaskMode(false);
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) > 0) {
		VOGL_SetColorMaskMode(true, true);
	} else {
		VOGL_SetColorMaskMode(true, false);
	}

	if (ALPHAMODE_ALPHABLEND(v->reg[alphaMode].u)) {
		VOGL_SetAlphaMode(1, ogl_sfactor[ALPHAMODE_SRCRGBBLEND(v->reg[alphaMode].u)], ogl_dfactor[ALPHAMODE_DSTRGBBLEND(v->reg[alphaMode].u)],
			(ALPHAMODE_SRCALPHABLEND(v->reg[alphaMode].u)==4)?GL_ONE:GL_ZERO,
			(ALPHAMODE_DSTALPHABLEND(v->reg[alphaMode].u)==4)?GL_ONE:GL_ZERO);
	} else {
		VOGL_SetAlphaMode(0, 0,0,0,0);
	}

	VOGL_BeginMode(GL_QUADS);
//	glBegin(GL_POINTS);
//	glColor3f((float)r/255.0f, (float)g/255.0f, (float)b/255.0f);
	glColor3ub((GLubyte)(r&0xff), (GLubyte)(g&0xff), (GLubyte)(b&0xff));
	voodoo_ogl_lfb_quad(x, y, 0.0f);
//	glEnd();
}


void voodoo_ogl_clip_window(voodoo_state *v) {
    (void)v;//UNUSED
/*	VOGL_ClearBeginMode();

	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
		sy = (v->fbi.yorigin+1 - sy) & 0x3ff;
		ey = (v->fbi.yorigin+1 - ey) & 0x3ff;
	}

	if ((sx>0) || (sy>0) || (ex<v->fbi.width) || (ey<v->fbi.height))
	{
		glEnable(GL_SCISSOR_TEST);
		glScissor(sx,sy,ex-sx,ey-sy);
	} else {
		glScissor(0,0,v->fbi.width,v->fbi.height);
		glDisable(GL_SCISSOR_TEST);
	} */
}


void voodoo_ogl_fastfill(void) {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();

	VOGL_SetDepthMaskMode(true);

	int sx = (v->reg[clipLeftRight].u >> 16) & 0x3ff;
	int ex = (v->reg[clipLeftRight].u >> 0) & 0x3ff;
	int sy = (v->reg[clipLowYHighY].u >> 16) & 0x3ff;
	int ey = (v->reg[clipLowYHighY].u >> 0) & 0x3ff;

	/* the clip rows are screen rows (0 at the top), counted from the bottom when the Y origin
	 * is (yorigin - y), like the software fastfill. In OpenGL, row 0 is at the bottom. */
	const int fw = (int)v->fbi.width, fh = (int)v->fbi.height;
	if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u)) {
		const int adj = fh - 1 - (int)v->fbi.yorigin;
		sy += adj;
		ey += adj;
	} else {
		const int tmp = fh - ey;
		ey = fh - sy;
		sy = tmp;
	}
	if (sx < 0) sx = 0;
	if (ex > fw) ex = fw;
	if (sy < 0) sy = 0;
	if (ey > fh) ey = fh;
	if (ex <= sx || ey <= sy) return;

	const bool scissors_needed = !(sx == 0 && sy == 0 && ex == fw && ey == fh);

	sx = (sx* new_width) / v->fbi.width;
	ex = (ex* new_width) / v->fbi.width;
	sy = (sy* new_height) / v->fbi.height;
	ey = (ey* new_height) / v->fbi.height;

	if (scissors_needed) {
		glEnable(GL_SCISSOR_TEST);
		glScissor(sx,sy,ex-sx,ey-sy);
	}


	uint32_t clear_mask=0;
	if (FBZMODE_RGB_BUFFER_MASK(v->reg[fbzMode].u)) {
		clear_mask|=GL_COLOR_BUFFER_BIT;

//		if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) ...
		VOGL_SetColorMaskMode(true, true);

		if (last_clear_color!=v->reg[color1].u) {
			glClearColor((float)v->reg[color1].rgb.r/255.0f,
				(float)v->reg[color1].rgb.g/255.0f,
				(float)v->reg[color1].rgb.b/255.0f,
				(float)v->reg[color1].rgb.a/255.0f);
			last_clear_color=v->reg[color1].u;
		}
		if (FBZMODE_DRAW_BUFFER(v->reg[fbzMode].u)==0) {
			VOGL_SetDrawMode(true);
			v->fbi.vblank_flush_pending=true;
			cached_line_front_y=-1;
		} else {
			VOGL_SetDrawMode(false);
			cached_line_back_y=-1;
		}
	}
	if (FBZMODE_AUX_BUFFER_MASK(v->reg[fbzMode].u) && v->fbi.auxoffs != (UINT32)(~0)) {
//	if (FBZMODE_ENABLE_DEPTHBUF(v->reg[fbzMode].u)) {
		clear_mask|=GL_DEPTH_BUFFER_BIT;
		glClearDepth((float)((UINT16)v->reg[zaColor].u)/65535.0f);
	}

	if (clear_mask) glClear(clear_mask);

	if (scissors_needed) {
		glScissor(0,0,(int)new_width,(int)new_height);
		glDisable(GL_SCISSOR_TEST);
	}
}

void voodoo_ogl_clear(void) {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();

	VOGL_SetDrawMode(false);

	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	last_clear_color = 0;
	glClearDepth(1.0f);
	glClearStencil(0);

	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	voodoo_ogl_swap_buffer();
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}


/* read row y (0 at the top) from the render resolution's buffer: cached, it is read a line at a time */
static const UINT32 *voodoo_ogl_read_line(bool front, int row) {
	INT32 &cached_y = front ? cached_line_front_y : cached_line_back_y;
	INT32 &cached_width = front ? cached_line_front_width : cached_line_back_width;
	INT32 &cached_length = front ? cached_line_front_length : cached_line_back_length;
	UINT32 *&cached_data = front ? cached_line_front_data : cached_line_back_data;

	if (cached_y != row || cached_width != (INT32)new_width) {
		if (cached_length < (INT32)new_width) {
			if (cached_data != NULL) free(cached_data);
			size_t span_length = ((new_width+64u)&(~15u));
			cached_data = (UINT32*)malloc(sizeof(UINT32)*span_length);
			cached_length = (INT32)span_length;
		}
		VOGL_SetReadMode(front);
		glReadPixels(0, (int)new_height - 1 - row, (int)new_width, 1, GL_RGBA, GL_UNSIGNED_BYTE, cached_data);
		cached_y = row;
		cached_width = (INT32)new_width;
	}
	return cached_data;
}

/* two pixels from (x, y) in the 3dfx video mode, row 0 at the top. With a larger render
 * resolution, the render pixel in the middle of each one is read. */
UINT32 voodoo_ogl_read_pixel(int x, int y) {
	UINT32 data[2];
	if ((x < 0) || (y < 0) || (x >= (INT32)v->fbi.width) || (y >= (INT32)v->fbi.height)) return 0xffffffff;
	if (!voodoo_ogl_enter()) return 0;

	int rx[2], ry, p0, p1;
	for (int i = 0; i < 2; i++) {
		voodoo_ogl_lfb_span(x + i, new_width, v->fbi.width, p0, p1);
		rx[i] = (p0 + p1 - 1) / 2;
	}
	voodoo_ogl_lfb_span(y, new_height, v->fbi.height, p0, p1);
	ry = (p0 + p1 - 1) / 2;

	switch (LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u)) {
		case 0:			/* front buffer */
		case 1: {		/* back buffer */
			const UINT32 *line = voodoo_ogl_read_line(LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u) == 0, ry);
			data[0] = line[rx[0]];
			data[1] = line[rx[1]];
			break;
		}
		case 2:			/* aux buffer */
			VOGL_SetReadMode(false);
			for (int i = 0; i < 2; i++)
				glReadPixels(rx[i], (int)new_height - 1 - ry, 1, 1, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, &data[i]);
			return ((data[0]>>16)&0xffff) | (data[1] & 0xffff0000);
		default:
			E_Exit("read from invalid buf %x",LFBMODE_READ_BUFFER_SELECT(v->reg[lfbMode].u));
			break;
	}

	return ((RGB_BLUE(data[0])>>3)<<11) | ((RGB_GREEN(data[0])>>2)<<5) | (RGB_RED(data[0])>>3) |
			((RGB_BLUE(data[1])>>3)<<27) | ((RGB_GREEN(data[1])>>2)<<21) | ((RGB_RED(data[1])>>3)<<16);
}


void voodoo_ogl_vblank_flush(void) {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();
	glFlush();
}


void voodoo_ogl_set_window(voodoo_state *v) {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();

	// 	matrix mode GL_PROJECTION assumed
	bool size_changed=false;
	if ((new_width!=last_width) || (new_height!=last_height)) size_changed=true;
	if (size_changed || (last_orientation != (INT32)FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))) {
		glLoadIdentity( );
		if (FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
			glOrtho( 0, new_width, 0, new_height, 0.0f, -1.0f );
		else
			glOrtho( 0, new_width, new_height, 0, 0.0f, -1.0f );
		if (last_orientation != (INT32)FBZMODE_Y_ORIGIN(v->reg[fbzMode].u))
			last_orientation = FBZMODE_Y_ORIGIN(v->reg[fbzMode].u);
	}
	if (size_changed) {
		glViewport( 0, 0, (int)new_width, (int)new_height );
		last_width = new_width;
		last_height = new_height;
	}
}

/* (re)make the buffers for the 3dfx video mode, and show it in the output at that size */
static bool voodoo_ogl_reset_videomode(void) {
	last_width=0;
	last_height=0;
	last_orientation=-1;

	OUTPUT_OPENGL_SetExternalTexture(0, 0, 0);
	voodoo_ogl_destroy_framebuffers();

	/* the output shows the 3dfx frame like any video mode, so it can be resized, made
	 * fullscreen, have the menu etc. like any other. The window is sized for the 3dfx
	 * video mode, whatever the render resolution is. This comes first, for the size
	 * of the display area (voodoo_resolution=display). */
	RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
	if (!voodoo_ogl_enter()) return false; /* the output's context was current for its resize */

	voodoo_ogl_render_size(new_width, new_height);
	if (!voodoo_ogl_create_framebuffers(new_width, new_height)) return false;

	if (vctx.has_stencil) VOGL_FlagFeature(VOGL_HAS_STENCIL_BUFFER);
	VOGL_FlagFeature(VOGL_HAS_ALPHA_PLANE);

	glViewport( 0, 0, (GLsizei)new_width, (GLsizei)new_height );
	last_width = new_width;
	last_height = new_height;

	/* NTS: Some demoscene 3Dfx stuff looks terrible without this.
	 *      This is said to be the initial state of an OpenGL context.
	 *      This is in direct contradiction to the OpenGL output setup
	 *      in src/gui/sdlmain.cpp that sets up GL_FLAT shading */
	glShadeModel (GL_SMOOTH);

	cached_line_front_y=-1;
	cached_line_back_y=-1;

	OUTPUT_OPENGL_SetExternalTexture(vctx.present_texture, new_width, new_height);

	LOG_MSG("VOODOO: OpenGL: mode set, resolution %d:%d, rendering at %d:%d",
		v->fbi.width, v->fbi.height, new_width, new_height);
	return true;
}

/* the 3dfx video mode changed: the buffers are resized right away (voodoo_ogl_follow_dimensions),
 * this tells the output, which changes the window etc. */
void voodoo_ogl_update_dimensions(void) {
	if (!voodoo_ogl_enter()) return;

	if (vctx.fbo == 0) {
		voodoo_ogl_reset_videomode();
	} else {
		RENDER_SetSize(v->fbi.width, v->fbi.height, 32, 60.0f, 4.0/3.0);
		if (!voodoo_ogl_enter()) return; /* the output's context was current for its resize */
		voodoo_ogl_check_render_size();
		OUTPUT_OPENGL_SetExternalTexture(vctx.present_texture, vctx.width, vctx.height);
		LOG_MSG("VOODOO: OpenGL: mode set, resolution %d:%d, rendering at %d:%d",
			v->fbi.width, v->fbi.height, vctx.width, vctx.height);
	}

	if (!voodoo_ogl_enter()) return; /* the output's context was current for its resize */
	voodoo_ogl_setup_window();
}

/* the 3dfx video mode changed while active: anything drawn from now on is drawn at the new size */
void voodoo_ogl_follow_dimensions(void) {
	if (!voodoo_ogl_enter()) return;
	VOGL_ClearBeginMode();
	voodoo_ogl_check_render_size();
}

static void voodoo_ogl_destroy_context(void) {
	if (vctx.context == NULL) return;

	if (voodoo_ogl_enter()) {
		VOGL_ClearBeginMode();
		voodoo_ogl_destroy_framebuffers();
	}
	if (vctx.fence) {
		vctx.DeleteSync(vctx.fence);
		vctx.fence = NULL;
	}
	OUTPUT_OPENGL_SetExternalTexture(0, 0, 0);
	GFX_OpenGL_MakeCurrent();
	GFX_GL_DeleteContext(vctx.context);
	vctx.context = NULL;
	VOGL_Reset();
}

bool voodoo_ogl_init(voodoo_state *v) {
	(void)v; /* the same as the global one, which the functions below use */
	extern void CPU_Core_Dyn_X86_SetFPUMode(bool dh_fpu);
//	CPU_Core_Dyn_X86_SetFPUMode(false);

	extern void CPU_Core_Dyn_X86_Cache_Reset(void);
//	CPU_Core_Dyn_X86_Cache_Reset();

	/* it renders with a context that shares textures with the OpenGL output's */
	if (!OUTPUT_OPENGL_Active()) {
		LOG_MSG("VOODOO: OpenGL: the OpenGL 3dfx emulation needs output=opengl");
		return false;
	}
	if (vctx.context == NULL) {
		vctx.context = GFX_GL_CreateSharedContext();
		if (vctx.context == NULL) return false;
	}
	vctx.warned = false;

	if (!voodoo_ogl_enter()) {
		voodoo_ogl_destroy_context();
		return false;
	}

	ogl_resolution_mode = Voodoo_OGL_ParseResolutionMode(
		static_cast<Section_prop *>(control->GetSection("voodoo"))->Get_string("voodoo_resolution"));

	VOGL_Reset();
	last_clear_color=0;
	if (!VOGL_Initialize() || !voodoo_ogl_load_functions() || !voodoo_ogl_reset_videomode()) {
		voodoo_ogl_destroy_context();
		return false;
	}

	transparency = 0;
	SetWindowTransparency(static_cast<Section_prop *>(control->GetSection("sdl"))->Get_int("transparency"));

/*	std::string features = "";
	if (VOGL_CheckFeature(VOGL_HAS_SHADERS)) features += " shader";
	if (VOGL_CheckFeature(VOGL_HAS_ALPHA_PLANE)) features += " alpha-plane";
	if (VOGL_CheckFeature(VOGL_HAS_STENCIL_BUFFER)) features += " stencil-buffer";

	if (features == "") features = " none";

	LOG_MSG("VOODOO: OpenGL: features enabled:%s",features.c_str()); */

	if (!voodoo_ogl_enter()) return false;
	voodoo_ogl_setup_window();

	return true;
}

void voodoo_ogl_leave(bool leavemode) {
	if (voodoo_ogl_enter()) {
		VOGL_ClearBeginMode();

		std::map<const UINT32, ogl_texmap>::iterator t;
		for (int j=0; j<2; j++) {
			for (t=textures[j].begin(); t!=textures[j].end(); ++t) {
				ogl_destroy_cached_texture(t->second);
			}
			textures[j].clear();
			texture_dirty[j].pending = false;
		}


		if (m_hProgramObject != 0) {
			glUseProgramObjectARB(0);
			m_hProgramObject = 0;
		}

		for (int hct=0; hct<RASTER_HASH_SIZE; hct++) {
			raster_info *info = v->raster_hash[hct];
			for (; info; info = info->next) {
				if (info->shader_ready) {
					delete[] info->shader_ulocations;
					info->shader_ulocations=NULL;

					if (info->so_shader_program > 0) {
						if (info->so_vertex_shader > 0) glDetachObjectARB((GLhandleARB)info->so_shader_program, (GLhandleARB)info->so_vertex_shader);
						if (info->so_fragment_shader > 0) glDetachObjectARB((GLhandleARB)info->so_shader_program, (GLhandleARB)info->so_fragment_shader);
						if (info->so_vertex_shader > 0) glDeleteObjectARB((GLhandleARB)info->so_vertex_shader);
						if (info->so_fragment_shader > 0) glDeleteObjectARB((GLhandleARB)info->so_fragment_shader);
						glDeleteObjectARB((GLhandleARB)info->so_shader_program);
					}

					info->shader_ready=false;
				}
			}
		}
	}


	cached_line_front_y=-1;
	cached_line_back_y=-1;

	if (leavemode) {
		LOG_MSG("VOODOO: OpenGL: quit");

		/* the output goes back to the emulated video card's frames */
		voodoo_ogl_destroy_context();

		transparency = 0;
		SetWindowTransparency(static_cast<Section_prop *>(control->GetSection("sdl"))->Get_int("transparency"));
	}
}

void voodoo_ogl_shutdown(voodoo_state *v) {
	new_width = new_height = 0;
	voodoo_ogl_leave(false);
	voodoo_ogl_destroy_context();

	v->active = false;
}

#else


bool voodoo_ogl_init(voodoo_state *v) {
    (void)v;
	return false;
}

void voodoo_ogl_leave(void) {
}

void voodoo_ogl_shutdown(voodoo_state *v) {
    (void)v;
}

void voodoo_ogl_present(void) {
}

void voodoo_ogl_load_framebuffers_from_ram(void) {
}

void voodoo_ogl_follow_dimensions(void) {
}

void Voodoo_OGL_SetResolutionMode(int mode) {
	(void)mode;
}

int Voodoo_OGL_GetResolutionMode(void) {
	return VOODOO_OGL_RES_NATIVE;
}

int Voodoo_OGL_ParseResolutionMode(const char *s) {
	(void)s;
	return VOODOO_OGL_RES_NATIVE;
}

void Voodoo_OGL_SetDisplaySize(unsigned int w, unsigned int h) {
	(void)w;
	(void)h;
}

void voodoo_ogl_set_window(voodoo_state *v) {
    (void)v;
	E_Exit("invalid call to voodoo_ogl_set_window");
}

void voodoo_ogl_swap_buffer(void) {
	E_Exit("invalid call to voodoo_ogl_swap_buffer");
}

void voodoo_ogl_vblank_flush(void) {
	E_Exit("invalid call to voodoo_ogl_vblank_flush");
}

void voodoo_ogl_clear(void) {
	E_Exit("invalid call to voodoo_ogl_clear");
}

void voodoo_ogl_fastfill(void) {
	E_Exit("invalid call to voodoo_ogl_fastfill");
}

void voodoo_ogl_clip_window(voodoo_state *v) {
    (void)v;
	E_Exit("invalid call to voodoo_ogl_clip_window");
}

void voodoo_ogl_texture_clear(UINT32 texbase, int TMU) {
    (void)texbase;
    (void)TMU;
	E_Exit("invalid call to voodoo_ogl_texture_clear");
}

void voodoo_ogl_invalidate_paltex(void) {
	E_Exit("invalid call to voodoo_ogl_invalidate_paltex");
}

void voodoo_ogl_draw_pixel(int x, int y, bool has_rgb, bool has_alpha, int r, int g, int b, int a) {
    (void)has_alpha;
    (void)has_rgb;
    (void)x;
    (void)y;
    (void)r;
    (void)g;
    (void)b;
    (void)a;
	E_Exit("invalid call to voodoo_ogl_draw_pixel");
}

void voodoo_ogl_draw_z(int x, int y, int z1, int z2) {
    (void)z1;
    (void)z2;
    (void)x;
    (void)y;
	E_Exit("invalid call to voodoo_ogl_draw_z");
}

void voodoo_ogl_draw_pixel_pipeline(int x, int y, int r, int g, int b) {
    (void)x;
    (void)y;
    (void)r;
    (void)g;
    (void)b;
	E_Exit("invalid call to voodoo_ogl_draw_pixel_pipeline");
}

UINT32 voodoo_ogl_read_pixel(int x, int y) {
    (void)x;
    (void)y;
	E_Exit("invalid call to voodoo_ogl_read_pixel");
	return 0;
}

void voodoo_ogl_draw_triangle(poly_extra_data *extra) {
    (void)extra;
	E_Exit("invalid call to voodoo_ogl_draw_triangle");
}

#endif
