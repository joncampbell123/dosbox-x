#include "dosbox.h"

#ifndef DOSBOX_OUTPUT_OPENGL_H
#define DOSBOX_OUTPUT_OPENGL_H

#if C_OPENGL
#include "SDL_opengl.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

#ifndef GL_ARB_pixel_buffer_object
#define GL_ARB_pixel_buffer_object 1
#define GL_PIXEL_PACK_BUFFER_ARB           0x88EB
#define GL_PIXEL_UNPACK_BUFFER_ARB         0x88EC
#define GL_PIXEL_PACK_BUFFER_BINDING_ARB   0x88ED
#define GL_PIXEL_UNPACK_BUFFER_BINDING_ARB 0x88EF
#endif

#ifndef GL_ARB_vertex_buffer_object
#define GL_ARB_vertex_buffer_object 1
typedef void (APIENTRYP PFNGLGENBUFFERSARBPROC) (GLsizei n, GLuint *buffers);
typedef void (APIENTRYP PFNGLBINDBUFFERARBPROC) (GLenum target, GLuint buffer);
typedef void (APIENTRYP PFNGLDELETEBUFFERSARBPROC) (GLsizei n, const GLuint *buffers);
typedef void (APIENTRYP PFNGLBUFFERDATAARBPROC) (GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage);
typedef GLvoid* (APIENTRYP PFNGLMAPBUFFERARBPROC) (GLenum target, GLenum access);
typedef GLboolean(APIENTRYP PFNGLUNMAPBUFFERARBPROC) (GLenum target);
#endif

extern PFNGLGENBUFFERSARBPROC glGenBuffersARB;
extern PFNGLBINDBUFFERARBPROC glBindBufferARB;
extern PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB;
extern PFNGLBUFFERDATAARBPROC glBufferDataARB;
extern PFNGLMAPBUFFERARBPROC glMapBufferARB;
extern PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB;

#if defined(C_SDL2)
# include <SDL_video.h>
#endif

struct SDL_OpenGL {
    bool inited;
    Bitu pitch;
    void * framebuf;
    GLuint buffer;
    GLuint texture;
    GLuint displaylist;
    GLint max_texsize;
    bool bilinear;
    bool packed_pixel;
    bool paletted_texture;
    bool pixel_buffer_object;
    int menudraw_countdown;
    int clear_countdown;
#if defined(C_SDL2)
    SDL_GLContext context;
#endif
};

extern SDL_OpenGL sdl_opengl;

// output API
void OUTPUT_OPENGL_Initialize();
void OUTPUT_OPENGL_Select();
Bitu OUTPUT_OPENGL_GetBestMode(Bitu flags);
Bitu OUTPUT_OPENGL_SetSize();
bool OUTPUT_OPENGL_StartUpdate(Bit8u* &pixels, Bitu &pitch);
void OUTPUT_OPENGL_EndUpdate(const Bit16u *changedLines);
void OUTPUT_OPENGL_Shutdown();

#endif //C_OPENGL

#endif /*DOSBOX_OUTPUT_OPENGL_H*/
