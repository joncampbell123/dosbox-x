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


/* Don't guard these with GL_VERSION_2_0 - Apple defines it but not these typedefs.
 * If they're already defined they should match these definitions, so no conflicts.
 */
typedef void (APIENTRYP PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP PFNGLDELETEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLDELETESHADERPROC) (GLuint shader);
typedef void (APIENTRYP PFNGLENABLEVERTEXATTRIBARRAYPROC) (GLuint index);
typedef GLint (APIENTRYP PFNGLGETATTRIBLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLGETPROGRAMIVPROC) (GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETPROGRAMINFOLOGPROC) (GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRYP PFNGLGETSHADERIVPROC) (GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRYP PFNGLGETSHADERINFOLOGPROC) (GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef GLint (APIENTRYP PFNGLGETUNIFORMLOCATIONPROC) (GLuint program, const GLchar *name);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC) (GLuint program);
//Change to NP, as Khronos changes include guard :(
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC_NP) (GLuint shader, GLsizei count, const GLchar **string, const GLint *length);
typedef void (APIENTRYP PFNGLUNIFORM2FPROC) (GLint location, GLfloat v0, GLfloat v1);
typedef void (APIENTRYP PFNGLUNIFORM1IPROC) (GLint location, GLint v0);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLVERTEXATTRIBPOINTERPROC) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const GLvoid *pointer);


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
    bool use_shader;
    GLuint program_object;
    bool shader_def=false;
    const char *shader_src;
    struct {
        GLint texture_size;
        GLint input_size;
        GLint output_size;
        GLint frame_count;
    } ruby;
    GLuint actual_frame_count;
    GLfloat vertex_data[2*3];
#if defined(C_SDL2)
    SDL_GLContext context;
#endif
};

static char const shader_src_default[] =
	"varying vec2 v_texCoord;\n"
	"#if defined(VERTEX)\n"
	"uniform vec2 rubyTextureSize;\n"
	"uniform vec2 rubyInputSize;\n"
	"attribute vec4 a_position;\n"
	"void main() {\n"
	"  gl_Position = a_position;\n"
	"  v_texCoord = vec2(a_position.x+1.0,1.0-a_position.y)/2.0*rubyInputSize/rubyTextureSize;\n"
	"}\n"
	"#elif defined(FRAGMENT)\n"
	"uniform sampler2D rubyTexture;\n\n"
	"void main() {\n"
	"  gl_FragColor = texture2D(rubyTexture, v_texCoord);\n"
	"}\n"
	"#endif\n";

extern SDL_OpenGL sdl_opengl;

// output API
void OUTPUT_OPENGL_Initialize();
void OUTPUT_OPENGL_Select();
Bitu OUTPUT_OPENGL_GetBestMode(Bitu flags);
Bitu OUTPUT_OPENGL_SetSize();
bool OUTPUT_OPENGL_StartUpdate(uint8_t* &pixels, Bitu &pitch);
void OUTPUT_OPENGL_EndUpdate(const uint16_t *changedLines);
void OUTPUT_OPENGL_Shutdown();

#endif //C_OPENGL

#endif /*DOSBOX_OUTPUT_OPENGL_H*/
