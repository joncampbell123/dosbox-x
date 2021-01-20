
// Tell Mac OS X to shut up about deprecated OpenGL calls
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#include <sys/types.h>
#include <assert.h>
#include <math.h>
extern "C" {
#include "ppscale.h"
#include "ppscale.c"
}
#include "dosbox.h"
#include <output/output_opengl.h>

#include <algorithm>

#include "sdlmain.h"
#include "render.h"

using namespace std;

bool setSizeButNotResize();

#if C_OPENGL
PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;

/* Apple defines these functions in their GL header (as core functions)
 * so we can't use their names as function pointers. We can't link
 * directly as some platforms may not have them. So they get their own
 * namespace here to keep the official names but avoid collisions.
 */
namespace gl2 {
PFNGLATTACHSHADERPROC glAttachShader = NULL;
PFNGLCOMPILESHADERPROC glCompileShader = NULL;
PFNGLCREATEPROGRAMPROC glCreateProgram = NULL;
PFNGLCREATESHADERPROC glCreateShader = NULL;
PFNGLDELETEPROGRAMPROC glDeleteProgram = NULL;
PFNGLDELETESHADERPROC glDeleteShader = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = NULL;
PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation = NULL;
PFNGLGETPROGRAMIVPROC glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = NULL;
PFNGLGETSHADERIVPROC glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = NULL;
PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = NULL;
PFNGLLINKPROGRAMPROC glLinkProgram = NULL;
PFNGLSHADERSOURCEPROC_NP glShaderSource = NULL;
PFNGLUNIFORM2FPROC glUniform2f = NULL;
PFNGLUNIFORM1IPROC glUniform1i = NULL;
PFNGLUSEPROGRAMPROC glUseProgram = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = NULL;
}

/* "using" is meant to hide identical names declared in outer scope
 * but is unreliable, so just redefine instead.
 */
#define glAttachShader            gl2::glAttachShader
#define glCompileShader           gl2::glCompileShader
#define glCreateProgram           gl2::glCreateProgram
#define glCreateShader            gl2::glCreateShader
#define glDeleteProgram           gl2::glDeleteProgram
#define glDeleteShader            gl2::glDeleteShader
#define glEnableVertexAttribArray gl2::glEnableVertexAttribArray
#define glGetAttribLocation       gl2::glGetAttribLocation
#define glGetProgramiv            gl2::glGetProgramiv
#define glGetProgramInfoLog       gl2::glGetProgramInfoLog
#define glGetShaderiv             gl2::glGetShaderiv
#define glGetShaderInfoLog        gl2::glGetShaderInfoLog
#define glGetUniformLocation      gl2::glGetUniformLocation
#define glLinkProgram             gl2::glLinkProgram
#define glShaderSource            gl2::glShaderSource
#define glUniform2f               gl2::glUniform2f
#define glUniform1i               gl2::glUniform1i
#define glUseProgram              gl2::glUseProgram
#define glVertexAttribPointer     gl2::glVertexAttribPointer

#if C_OPENGL && DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
extern unsigned int SDLDrawGenFontTextureUnitPerRow;
extern unsigned int SDLDrawGenFontTextureRows;
extern unsigned int SDLDrawGenFontTextureWidth;
extern unsigned int SDLDrawGenFontTextureHeight;
extern GLuint SDLDrawGenFontTexture;
extern bool SDLDrawGenFontTextureInit;
#endif
extern int initgl;

SDL_OpenGL sdl_opengl;

int Voodoo_OGL_GetWidth();
int Voodoo_OGL_GetHeight();
bool Voodoo_OGL_Active();

static void PPScale (
    uint16_t  fixed_w , uint16_t  fixed_h,
    uint16_t* window_w, uint16_t* window_h )
{
    int    sx, sy, orig_w, orig_h, min_w, min_h;
    double par, par_sq;

    orig_w = min_w = render.src.width;
    orig_h = min_h = render.src.height;

    par = ( double) orig_w / orig_h * 3 / 4;
    /* HACK: because REDNER_SetSize() does not set dblw and dblh correctly: */
    /* E.g. in 360x360 mode DOXBox will wrongly allocate a 720x360 area. I  */
    /* therefore calculate square-pixel proportions par_sq myself:          */
         if( par < 0.707 ) { par_sq = 0.5; min_w *= 2; }
    else if( par > 1.414 ) { par_sq = 2.0; min_h *= 2; }
    else                     par_sq = 1.0;

    if( !render.aspect ) par = par_sq;

    *window_w = fixed_w; *window_h = fixed_h;
    /* Handle non-fixed resolutions and ensure a sufficient window size: */
    if( fixed_w < min_w ) fixed_w = *window_w = min_w;
    if( fixed_h < min_h ) fixed_h = *window_h = min_h;

    pp_getscale(
        orig_w , orig_h , par ,
        fixed_w, fixed_h, 1.14,
        &sx    , &sy         );

    sdl.clip.w = orig_w * sx;
    sdl.clip.h = orig_h * sy;
    sdl.clip.x = (*window_w - sdl.clip.w) / 2;
    sdl.clip.y = (*window_h - sdl.clip.h) / 2;

    LOG_MSG( "OpenGL PP: [%ix%i]: %ix%i (%3.2f) -> [%ix%i] -> %ix%i (%3.2f)",
        fixed_w,    fixed_h,
        orig_w,     orig_h, par,
        sx,         sy,
        sdl.clip.w, sdl.clip.h, (double)sy/sx );
}

static SDL_Surface* SetupSurfaceScaledOpenGL(uint32_t sdl_flags, uint32_t bpp) 
{
    uint16_t fixedWidth;
    uint16_t fixedHeight;
    uint16_t windowWidth;
    uint16_t windowHeight;

retry:
#if defined(C_SDL2)
    if (sdl.desktop.prevent_fullscreen) /* 3Dfx openGL do not allow resize */
        sdl_flags &= ~((unsigned int)SDL_WINDOW_RESIZABLE);
    if (sdl.desktop.want_type == SCREEN_OPENGL)
        sdl_flags |= (unsigned int)SDL_WINDOW_OPENGL;
#else
    if (sdl.desktop.prevent_fullscreen) /* 3Dfx openGL do not allow resize */
        sdl_flags &= ~((unsigned int)SDL_RESIZABLE);
    if (sdl.desktop.want_type == SCREEN_OPENGL)
        sdl_flags |= (unsigned int)SDL_OPENGL;
#endif

    if (sdl.desktop.fullscreen) 
    {
        fixedWidth = sdl.desktop.full.fixed ? sdl.desktop.full.width : 0;
        fixedHeight = sdl.desktop.full.fixed ? sdl.desktop.full.height : 0;
#if defined(C_SDL2)
        sdl_flags |= (unsigned int)(SDL_WINDOW_FULLSCREEN);
#else
        sdl_flags |= (unsigned int)(SDL_FULLSCREEN | SDL_HWSURFACE);
#endif
    }
    else 
    {
        fixedWidth = sdl.desktop.window.width;
        fixedHeight = sdl.desktop.window.height;
#if !defined(C_SDL2)
        sdl_flags |= (unsigned int)SDL_HWSURFACE;
#endif
    }

    if (fixedWidth == 0 || fixedHeight == 0) 
    {
        Bitu consider_height = menu.maxwindow ? currentWindowHeight : 0;
        Bitu consider_width = menu.maxwindow ? currentWindowWidth : 0;
        int final_height = (int)max(consider_height, userResizeWindowHeight);
        int final_width = (int)max(consider_width, userResizeWindowWidth);

        fixedWidth = final_width;
        fixedHeight = final_height;
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    /* scale the menu bar if the window is large enough */
    {
        int cw = fixedWidth, ch = fixedHeight;
        int scale = 1;

        if (cw == 0)
            cw = (uint16_t)(sdl.draw.width*sdl.draw.scalex);
        if (ch == 0)
            ch = (uint16_t)(sdl.draw.height*sdl.draw.scaley);

        while ((cw / scale) >= (640 * 2) && (ch / scale) >= (400 * 2))
            scale++;

        LOG_MSG("menuScale=%d", scale);
        mainMenu.setScale((unsigned int)scale);

        if (mainMenu.isVisible() && !sdl.desktop.fullscreen && fixedHeight)
            fixedHeight -= mainMenu.menuBox.h;
    }
#endif

    sdl.clip.x = 0; sdl.clip.y = 0;
    if (Voodoo_OGL_GetWidth() != 0 && Voodoo_OGL_GetHeight() != 0 && Voodoo_OGL_Active() && sdl.desktop.prevent_fullscreen)
    { 
        /* 3Dfx openGL do not allow resize */
        sdl.clip.w = windowWidth = (uint16_t)Voodoo_OGL_GetWidth();
        sdl.clip.h = windowHeight = (uint16_t)Voodoo_OGL_GetHeight();
    } else if (sdl_opengl.kind == GLPerfect ) {
        PPScale( fixedWidth, fixedHeight, &windowWidth, &windowHeight );
    } else
        if (fixedWidth && fixedHeight)
        {
            windowWidth  = fixedWidth;
            windowHeight = fixedHeight;
            sdl.clip.w = windowWidth;
            sdl.clip.h = windowHeight;
            if (render.aspect) aspectCorrectFitClip(sdl.clip.w, sdl.clip.h, sdl.clip.x, sdl.clip.y, fixedWidth, fixedHeight);
        }
        else
        {
            windowWidth = (uint16_t)(sdl.draw.width * sdl.draw.scalex);
            windowHeight = (uint16_t)(sdl.draw.height * sdl.draw.scaley);
            if (render.aspect) aspectCorrectExtend(windowWidth, windowHeight);
            sdl.clip.w = windowWidth; sdl.clip.h = windowHeight;
        }

    LOG(LOG_MISC, LOG_DEBUG)("GFX_SetSize OpenGL window=%ux%u clip=x,y,w,h=%d,%d,%d,%d",
        (unsigned int)windowWidth,
        (unsigned int)windowHeight,
        (unsigned int)sdl.clip.x,
        (unsigned int)sdl.clip.y,
        (unsigned int)sdl.clip.w,
        (unsigned int)sdl.clip.h);

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    if (mainMenu.isVisible() && !sdl.desktop.fullscreen) 
    {
        windowHeight += mainMenu.menuBox.h;
        sdl.clip.y += mainMenu.menuBox.h;
    }
#endif

#if defined(C_SDL2)
    (void)bpp; // unused param
    sdl.surface = NULL;
    sdl.window = GFX_SetSDLWindowMode(windowWidth, windowHeight, (sdl_flags & SDL_WINDOW_OPENGL) ? SCREEN_OPENGL : SCREEN_SURFACE);
    if (sdl.window != NULL) sdl.surface = SDL_GetWindowSurface(sdl.window);
#elif defined(SDL_DOSBOX_X_SPECIAL)
    sdl.surface = SDL_SetVideoMode(windowWidth, windowHeight, (int)bpp, (unsigned int)sdl_flags | (unsigned int)(setSizeButNotResize() ? SDL_HAX_NORESIZEWINDOW : 0));
#else
    sdl.surface = SDL_SetVideoMode(windowWidth, windowHeight, (int)bpp, (unsigned int)sdl_flags);
#endif
    if (sdl.surface == NULL && sdl.desktop.fullscreen) {
        LOG_MSG("Fullscreen not supported: %s", SDL_GetError());
        sdl.desktop.fullscreen = false;
#if defined(C_SDL2)
        sdl_flags &= ~SDL_WINDOW_FULLSCREEN;
#else
        sdl_flags &= ~SDL_FULLSCREEN;
#endif
        GFX_CaptureMouse();
        goto retry;
    }

    sdl.deferred_resize = false;
    sdl.must_redraw_all = true;

    /* There seems to be a problem with MesaGL in Linux/X11 where
    * the first swap buffer we do is misplaced according to the
    * previous window size.
    *
    * NTS: This seems to have been fixed, which is why this is
    *      commented out. I guess not calling GFX_SetSize()
    *      with a 0x0 widthxheight helps! */
    //    sdl.gfx_force_redraw_count = 2;

    UpdateWindowDimensions();
    GFX_LogSDLState();

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)(sdl.surface->w);
    mainMenu.screenHeight = (size_t)(sdl.surface->h);
    mainMenu.updateRect();
    mainMenu.setRedraw();
#endif

    return sdl.surface;
}

// output API below

void OUTPUT_OPENGL_Initialize()
{
    memset(&sdl_opengl, 0, sizeof(sdl_opengl));
}

void OUTPUT_OPENGL_Select( GLKind kind )
{
    sdl.desktop.want_type = SCREEN_OPENGL;
    render.aspectOffload = true;

#if defined(WIN32) && !defined(C_SDL2)
    SDL1_hax_inhibit_WM_PAINT = 0;
#endif

    sdl_opengl.use_shader = false;
    initgl=0;
#if defined(C_SDL2)
    void GFX_SetResizeable(bool enable);
    GFX_SetResizeable(true);
    sdl.window = GFX_SetSDLWindowMode(640,400, SCREEN_OPENGL);
    if (sdl.window) {
        sdl_opengl.context = SDL_GL_CreateContext(sdl.window);
        sdl.surface = SDL_GetWindowSurface(sdl.window);
    }
    if (!sdl.window || !sdl_opengl.context || sdl.surface == NULL) {
#else
    sdl.surface = SDL_SetVideoMode(640,400,0,SDL_OPENGL);
    if (sdl.surface == NULL) {
#endif
        LOG_MSG("Could not initialize OpenGL, switching back to surface");
        sdl.desktop.want_type = SCREEN_SURFACE;
    } else if (initgl!=2) {
        initgl = 1;
        sdl_opengl.kind = kind;
        sdl.desktop.isperfect = true;
        sdl_opengl.program_object = 0;
        glAttachShader = (PFNGLATTACHSHADERPROC)SDL_GL_GetProcAddress("glAttachShader");
        glCompileShader = (PFNGLCOMPILESHADERPROC)SDL_GL_GetProcAddress("glCompileShader");
        glCreateProgram = (PFNGLCREATEPROGRAMPROC)SDL_GL_GetProcAddress("glCreateProgram");
        glCreateShader = (PFNGLCREATESHADERPROC)SDL_GL_GetProcAddress("glCreateShader");
        glDeleteProgram = (PFNGLDELETEPROGRAMPROC)SDL_GL_GetProcAddress("glDeleteProgram");
        glDeleteShader = (PFNGLDELETESHADERPROC)SDL_GL_GetProcAddress("glDeleteShader");
        glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)SDL_GL_GetProcAddress("glEnableVertexAttribArray");
        glGetAttribLocation = (PFNGLGETATTRIBLOCATIONPROC)SDL_GL_GetProcAddress("glGetAttribLocation");
        glGetProgramiv = (PFNGLGETPROGRAMIVPROC)SDL_GL_GetProcAddress("glGetProgramiv");
        glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)SDL_GL_GetProcAddress("glGetProgramInfoLog");
        glGetShaderiv = (PFNGLGETSHADERIVPROC)SDL_GL_GetProcAddress("glGetShaderiv");
        glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)SDL_GL_GetProcAddress("glGetShaderInfoLog");
        glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)SDL_GL_GetProcAddress("glGetUniformLocation");
        glLinkProgram = (PFNGLLINKPROGRAMPROC)SDL_GL_GetProcAddress("glLinkProgram");
        glShaderSource = (PFNGLSHADERSOURCEPROC_NP)SDL_GL_GetProcAddress("glShaderSource");
        glUniform2f = (PFNGLUNIFORM2FPROC)SDL_GL_GetProcAddress("glUniform2f");
        glUniform1i = (PFNGLUNIFORM1IPROC)SDL_GL_GetProcAddress("glUniform1i");
        glUseProgram = (PFNGLUSEPROGRAMPROC)SDL_GL_GetProcAddress("glUseProgram");
        glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)SDL_GL_GetProcAddress("glVertexAttribPointer");
        sdl_opengl.use_shader = (glAttachShader && glCompileShader && glCreateProgram && glDeleteProgram && glDeleteShader && \
            glEnableVertexAttribArray && glGetAttribLocation && glGetProgramiv && glGetProgramInfoLog && \
            glGetShaderiv && glGetShaderInfoLog && glGetUniformLocation && glLinkProgram && glShaderSource && \
            glUniform2f && glUniform1i && glUseProgram && glVertexAttribPointer);
        if (sdl_opengl.use_shader) initgl = 2;
        sdl_opengl.buffer=0;
        sdl_opengl.framebuf=0;
        sdl_opengl.texture=0;
        sdl_opengl.displaylist=0;
        glGetIntegerv (GL_MAX_TEXTURE_SIZE, &sdl_opengl.max_texsize);
        glGenBuffersARB = (PFNGLGENBUFFERSARBPROC)SDL_GL_GetProcAddress("glGenBuffersARB");
        glBindBufferARB = (PFNGLBINDBUFFERARBPROC)SDL_GL_GetProcAddress("glBindBufferARB");
        glDeleteBuffersARB = (PFNGLDELETEBUFFERSARBPROC)SDL_GL_GetProcAddress("glDeleteBuffersARB");
        glBufferDataARB = (PFNGLBUFFERDATAARBPROC)SDL_GL_GetProcAddress("glBufferDataARB");
        glMapBufferARB = (PFNGLMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glMapBufferARB");
        glUnmapBufferARB = (PFNGLUNMAPBUFFERARBPROC)SDL_GL_GetProcAddress("glUnmapBufferARB");
        const char * gl_ext = (const char *)glGetString (GL_EXTENSIONS);
        if(gl_ext && *gl_ext){
            sdl_opengl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") != NULL);
            sdl_opengl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") != NULL);
            //sdl_opengl.pixel_buffer_object=(strstr(gl_ext,"GL_ARB_pixel_buffer_object") != NULL ) && glGenBuffersARB && glBindBufferARB && glDeleteBuffersARB && glBufferDataARB && glMapBufferARB && glUnmapBufferARB;
        } else {
            sdl_opengl.packed_pixel = false;
            sdl_opengl.paletted_texture = false;
            //sdl_opengl.pixel_buffer_object = false;
        }
#ifdef DB_DISABLE_DBO
        sdl_opengl.pixel_buffer_object = false;
#endif
        //LOG_MSG("OpenGL extension: pixel_buffer_object %d",sdl_opengl.pixel_buffer_object);
	} /* OPENGL is requested end */
}

Bitu OUTPUT_OPENGL_GetBestMode(Bitu flags)
{
    if (!(flags & GFX_CAN_32)) return 0; // OpenGL requires 32-bit output mode
    flags |=  GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    return flags;
}

/* Create a GLSL shader object, load the shader source, and compile the shader. */
static GLuint BuildShader ( GLenum type, const char *shaderSrc ) {
	GLuint shader;
	GLint compiled;
	const char* src_strings[2];
	std::string top;

	// look for "#version" because it has to occur first
	const char *ver = strstr(shaderSrc, "#version ");
	if (ver) {
		const char *endline = strchr(ver+9, '\n');
		if (endline) {
			top.assign(shaderSrc, endline-shaderSrc+1);
			shaderSrc = endline+1;
		}
	}

	top += (type==GL_VERTEX_SHADER) ? "#define VERTEX 1\n":"#define FRAGMENT 1\n";
	if (sdl_opengl.kind == GLNearest || sdl_opengl.kind == GLPerfect)
		top += "#define OPENGLNB 1\n";

	src_strings[0] = top.c_str();
	src_strings[1] = shaderSrc;

	// Create the shader object
	shader = glCreateShader(type);
	if (shader == 0) return 0;

	// Load the shader source
	glShaderSource(shader, 2, src_strings, NULL);

	// Compile the shader
	glCompileShader(shader);

	// Check the compile status
	glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

	if (!compiled) {
		char* infoLog = NULL;
		GLint infoLen = 0;
		glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &infoLen);

		if (infoLen>1) infoLog = (char*)malloc(infoLen);
		if (infoLog) {
			glGetShaderInfoLog(shader, infoLen, NULL, infoLog);
			LOG_MSG("Error compiling shader: %s", infoLog);
			free(infoLog);
		} else LOG_MSG("Error getting shader compilation log");

		glDeleteShader(shader);
		return 0;
	}

	return shader;
}

static bool LoadGLShaders(const char *src, GLuint *vertex, GLuint *fragment) {
	GLuint s = BuildShader(GL_VERTEX_SHADER, src);
	if (s) {
		*vertex = s;
		s = BuildShader(GL_FRAGMENT_SHADER, src);
		if (s) {
			*fragment = s;
			return true;
		}
		glDeleteShader(*vertex);
	}
	return false;
}

Bitu OUTPUT_OPENGL_SetSize()
{
    Bitu retFlags = 0;

    /* NTS: Apparently calling glFinish/glFlush before setup causes a segfault within
    *      the OpenGL library on Mac OS X. */
    if (sdl_opengl.inited)
    {
        glFinish();
        glFlush();
    }

    if (sdl_opengl.pixel_buffer_object)
    {
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
        if (sdl_opengl.buffer) glDeleteBuffersARB(1, &sdl_opengl.buffer);
    }
    else if (sdl_opengl.framebuf)
    {
        free(sdl_opengl.framebuf);
    }
    sdl_opengl.framebuf = 0;

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
#if !defined(C_SDL2)
# if SDL_VERSION_ATLEAST(1, 2, 11)
    {
        Section_prop* sec = static_cast<Section_prop*>(control->GetSection("vsync"));

        if (sec)
            SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, (!strcmp(sec->Get_string("vsyncmode"), "host")) ? 1 : 0);
    }
# endif
#endif

    // try 32 bits first then 16
#if defined(C_SDL2)
    if (SetupSurfaceScaledOpenGL(SDL_WINDOW_RESIZABLE,32)==NULL) SetupSurfaceScaledOpenGL(SDL_WINDOW_RESIZABLE,16);
#else
    if (SetupSurfaceScaledOpenGL(SDL_RESIZABLE,32)==NULL) SetupSurfaceScaledOpenGL(SDL_RESIZABLE,16);
#endif
    if (!sdl.surface || sdl.surface->format->BitsPerPixel < 15)
    {
        LOG_MSG("SDL:OPENGL:Can't open drawing surface, are you running in 16bpp(or higher) mode?");
        return 0;
    }

    glFinish();
    glFlush();

    glGetIntegerv(GL_MAX_TEXTURE_SIZE, &sdl_opengl.max_texsize);

    Bitu adjTexWidth = sdl.draw.width;
    Bitu adjTexHeight = sdl.draw.height;
#if C_XBRZ
    // we do the same as with Direct3D: precreate pixel buffer adjusted for xBRZ
    if (sdl_xbrz.enable && xBRZ_SetScaleParameters((int)adjTexWidth, (int)adjTexHeight, (int)sdl.clip.w, (int)sdl.clip.h))
    {
        adjTexWidth = adjTexWidth * (unsigned int)sdl_xbrz.scale_factor;
        adjTexHeight = adjTexHeight * (unsigned int)sdl_xbrz.scale_factor;
    }
#endif

    int texsize = 2 << int_log2((int)(adjTexWidth > adjTexHeight ? adjTexWidth : adjTexHeight));
    if (texsize > sdl_opengl.max_texsize) 
    {
        LOG_MSG("SDL:OPENGL:No support for texturesize of %d (max size is %d), falling back to surface", texsize, sdl_opengl.max_texsize);
        return 0;
    }

    if (sdl_opengl.use_shader && sdl_opengl.shader_src == NULL && !sdl_opengl.shader_def) sdl_opengl.use_shader = false;
    if (sdl_opengl.use_shader) {
        GLuint prog=0;
        // reset error
        glGetError();
        glGetIntegerv(GL_CURRENT_PROGRAM, (GLint*)&prog);
        // if there was an error this context doesn't support shaders
        if (glGetError()==GL_NO_ERROR && (sdl_opengl.program_object==0 || prog!=sdl_opengl.program_object)) {
            // check if existing program is valid
            if (sdl_opengl.program_object) {
                glUseProgram(sdl_opengl.program_object);
                if (glGetError() != GL_NO_ERROR) {
                    // program is not usable (probably new context), purge it
                    glDeleteProgram(sdl_opengl.program_object);
                    sdl_opengl.program_object = 0;
                }
            }

            // does program need to be rebuilt?
            if (sdl_opengl.program_object == 0) {
                GLuint vertexShader, fragmentShader;
                const char *src = sdl_opengl.shader_src;
                if (src && !LoadGLShaders(src, &vertexShader, &fragmentShader)) {
                    LOG_MSG("SDL:OPENGL:Failed to compile shader, falling back to default");
                    src = NULL;
                }
                if (src == NULL && !LoadGLShaders(shader_src_default, &vertexShader, &fragmentShader)) {
                    LOG_MSG("SDL:OPENGL:Failed to compile default shader!");
                    return 0;
                }

                sdl_opengl.program_object = glCreateProgram();
                if (!sdl_opengl.program_object) {
                    glDeleteShader(vertexShader);
                    glDeleteShader(fragmentShader);
                    LOG_MSG("SDL:OPENGL:Can't create program object, falling back to surface");
                    return 0;
                }
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
                // Todo: Make SDL-drawn menu work with custom GLSL shaders
                if (!sdl.desktop.prevent_fullscreen) {
                    menu.toggle=false;
                    mainMenu.showMenu(false);
                    mainMenu.get_item("mapper_togmenu").check(!menu.toggle).refresh_item(mainMenu);
                }
#endif
                glAttachShader(sdl_opengl.program_object, vertexShader);
                glAttachShader(sdl_opengl.program_object, fragmentShader);
                // Link the program
                glLinkProgram(sdl_opengl.program_object);
                // Even if we *are* successful, we may delete the shader objects
                glDeleteShader(vertexShader);
                glDeleteShader(fragmentShader);

                // Check the link status
                GLint isProgramLinked;
                glGetProgramiv(sdl_opengl.program_object, GL_LINK_STATUS, &isProgramLinked);
                if (!isProgramLinked) {
                    char * infoLog = NULL;
                    GLint infoLen = 0;

                    glGetProgramiv(sdl_opengl.program_object, GL_INFO_LOG_LENGTH, &infoLen);
                    if (infoLen>1) infoLog = (char*)malloc(infoLen);
                    if (infoLog) {
                        glGetProgramInfoLog(sdl_opengl.program_object, infoLen, NULL, infoLog);
                        LOG_MSG("SDL:OPENGL:Error linking program:\n %s", infoLog);
                        free(infoLog);
                    } else LOG_MSG("SDL:OPENGL:Failed to retrieve program link log");

                    glDeleteProgram(sdl_opengl.program_object);
                    sdl_opengl.program_object = 0;
                    return 0;
                }

                glUseProgram(sdl_opengl.program_object);

                GLint u = glGetAttribLocation(sdl_opengl.program_object, "a_position");
                // NTS: This is now a triangle strip (GL_TRIANGLE_STRIP)
                // upper left
                sdl_opengl.vertex_data[0] = -1.0f;
                sdl_opengl.vertex_data[1] =  1.0f;
                // lower left
                sdl_opengl.vertex_data[2] = -1.0f;
                sdl_opengl.vertex_data[3] = -1.0f;
                // upper right
                sdl_opengl.vertex_data[4] =  1.0f;
                sdl_opengl.vertex_data[5] =  1.0f;
                // lower right
                sdl_opengl.vertex_data[6] =  1.0f;
                sdl_opengl.vertex_data[7] = -1.0f;
                // Load the vertex positions
                glVertexAttribPointer(u, 2, GL_FLOAT, GL_FALSE, 0, sdl_opengl.vertex_data);
                glEnableVertexAttribArray(u);

                u = glGetUniformLocation(sdl_opengl.program_object, "rubyTexture");
                glUniform1i(u, 0);

                sdl_opengl.ruby.texture_size = glGetUniformLocation(sdl_opengl.program_object, "rubyTextureSize");
                sdl_opengl.ruby.input_size = glGetUniformLocation(sdl_opengl.program_object, "rubyInputSize");
                sdl_opengl.ruby.output_size = glGetUniformLocation(sdl_opengl.program_object, "rubyOutputSize");
                sdl_opengl.ruby.frame_count = glGetUniformLocation(sdl_opengl.program_object, "rubyFrameCount");
                // Don't force updating unless a shader depends on frame_count
                RENDER_SetForceUpdate(sdl_opengl.ruby.frame_count != (GLint)-1);
            }
        }
    }

    /* Create the texture and display list */
    if (sdl_opengl.pixel_buffer_object) 
    {
        glGenBuffersARB(1, &sdl_opengl.buffer);
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl_opengl.buffer);
        glBufferDataARB(GL_PIXEL_UNPACK_BUFFER_EXT, (int)(adjTexWidth*adjTexHeight * 4), NULL, GL_STREAM_DRAW_ARB);
        glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
    }
    else
    {
        sdl_opengl.framebuf = calloc(adjTexWidth*adjTexHeight, 4); //32 bit color
    }
    sdl_opengl.pitch = adjTexWidth * 4;

    glBindTexture(GL_TEXTURE_2D, 0);

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    if (SDLDrawGenFontTextureInit)
    {
        glDeleteTextures(1, &SDLDrawGenFontTexture);
        SDLDrawGenFontTexture = (GLuint)(~0UL);
        SDLDrawGenFontTextureInit = 0;
    }
#endif

    if (sdl.desktop.fullscreen&&sdl_opengl.use_shader)
        glViewport((sdl.surface->w-sdl.clip.w)/2,(sdl.surface->h-sdl.clip.h)/2,sdl.clip.w,sdl.clip.h);
    else
        glViewport(0, 0, sdl.surface->w, sdl.surface->h);
    if (sdl_opengl.texture > 0) glDeleteTextures(1, &sdl_opengl.texture);
    glGenTextures(1, &sdl_opengl.texture);
    glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, 0);

    // No borders
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    GLint interp;
    if( sdl_opengl.kind == GLNearest || sdl_opengl.kind == GLPerfect )
        interp = GL_NEAREST; else
        interp = GL_LINEAR ;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, interp );
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, interp );

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

    // NTS: I'm told that nVidia hardware seems to triple buffer despite our
    //      request to double buffer (according to @pixelmusement), therefore
    //      the next 3 frames, instead of 2, need to be cleared.
    sdl_opengl.menudraw_countdown = 3; // two GL buffers with possible triple buffering behind our back
    sdl_opengl.clear_countdown = 3; // two GL buffers with possible triple buffering behind our back

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    // SDL_GL_SwapBuffers();
    // glClear(GL_COLOR_BUFFER_BIT);
    //glShadeModel(GL_FLAT);
    glBlendFunc(GL_ONE, GL_ZERO);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_LIGHTING);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    glDisable(GL_FOG);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);
    glEnable(GL_TEXTURE_2D);

    if (sdl_opengl.program_object) {
        // Set shader variables
        glUniform2f(sdl_opengl.ruby.texture_size, (float)texsize, (float)texsize);
        glUniform2f(sdl_opengl.ruby.input_size, (float)adjTexWidth, (float)adjTexHeight);
        glUniform2f(sdl_opengl.ruby.output_size, sdl.clip.w, sdl.clip.h);
        // The following uniform is *not* set right now
        sdl_opengl.actual_frame_count = 0;
    } else {
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, sdl.surface->w, sdl.surface->h, 0, -1, 1);

        glMatrixMode(GL_TEXTURE);
        glLoadIdentity();
        glScaled(1.0 / texsize, 1.0 / texsize, 1.0);

        // if (glIsList(sdl_opengl.displaylist))
        //   glDeleteLists(sdl_opengl.displaylist, 1);
        // sdl_opengl.displaylist = glGenLists(1);
        sdl_opengl.displaylist = 1;

        glNewList(sdl_opengl.displaylist, GL_COMPILE);
        glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);

        glBegin(GL_QUADS);

        glTexCoord2i(0, 0); glVertex2i((GLint)sdl.clip.x, (GLint)sdl.clip.y); // lower left
        glTexCoord2i((GLint)adjTexWidth, 0); glVertex2i((GLint)sdl.clip.x + (GLint)sdl.clip.w, (GLint)sdl.clip.y); // lower right
        glTexCoord2i((GLint)adjTexWidth, (GLint)adjTexHeight); glVertex2i((GLint)sdl.clip.x + (GLint)sdl.clip.w, (GLint)sdl.clip.y + (GLint)sdl.clip.h); // upper right
        glTexCoord2i(0, (GLint)adjTexHeight); glVertex2i((GLint)sdl.clip.x, (GLint)sdl.clip.y + (GLint)sdl.clip.h); // upper left

        glEnd();
        glEndList();

        glBindTexture(GL_TEXTURE_2D, 0);
    }

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    void GFX_DrawSDLMenu(DOSBoxMenu &menu, DOSBoxMenu::displaylist &dl);
    mainMenu.setRedraw();
    GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);

    // FIXME: Why do we have to reinitialize the font texture?
    // if (!SDLDrawGenFontTextureInit) {
    GLuint err = 0;

    glGetError(); /* read and discard last error */

    SDLDrawGenFontTexture = (GLuint)(~0UL);
    glGenTextures(1, &SDLDrawGenFontTexture);
    if (SDLDrawGenFontTexture == (GLuint)(~0UL) || (err = glGetError()) != 0)
    {
        LOG_MSG("WARNING: Unable to make font texture. id=%llu err=%lu",
            (unsigned long long)SDLDrawGenFontTexture, (unsigned long)err);
    }
    else
    {
        LOG_MSG("font texture id=%lu will make %u x %u",
            (unsigned long)SDLDrawGenFontTexture,
            (unsigned int)SDLDrawGenFontTextureWidth,
            (unsigned int)SDLDrawGenFontTextureHeight);

        SDLDrawGenFontTextureInit = 1;

        glBindTexture(GL_TEXTURE_2D, SDLDrawGenFontTexture);

        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, 0);

        // No borders
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (int)SDLDrawGenFontTextureWidth, (int)SDLDrawGenFontTextureHeight, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

        /* load the font */
        {
            extern uint8_t int10_font_16[256 * 16];

            uint32_t tmp[8 * 16];
            unsigned int x, y, c;

            for (c = 0; c < 256; c++) 
            {
                unsigned char *bmp = int10_font_16 + (c * 16);
                for (y = 0; y < 16; y++) 
                {
                    for (x = 0; x < 8; x++) 
                    {
                        tmp[(y * 8) + x] = (bmp[y] & (0x80 >> x)) ? 0xFFFFFFFFUL : 0x00000000UL;
                    }
                }

                glTexSubImage2D(GL_TEXTURE_2D, /*level*/0, /*x*/(int)((c % 16) * 8), /*y*/(int)((c / 16) * 16),
                    8, 16, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, (void*)tmp);
            }
        }

        glBindTexture(GL_TEXTURE_2D, 0);
    }
    // }
#endif

    glFinish();
    glFlush();

    sdl_opengl.inited = true;
    retFlags = GFX_CAN_32 | GFX_SCALING;

    if (sdl_opengl.pixel_buffer_object)
        retFlags |= GFX_HARDWARE;

    return retFlags;
}

bool OUTPUT_OPENGL_StartUpdate(uint8_t* &pixels, Bitu &pitch)
{
#if C_XBRZ    
    if (sdl_xbrz.enable && sdl_xbrz.scale_on) 
    {
        sdl_xbrz.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl_xbrz.renderbuf.empty() ? nullptr : reinterpret_cast<uint8_t*>(&sdl_xbrz.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
    {
        if (sdl_opengl.pixel_buffer_object)
        {
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl_opengl.buffer);
            pixels = (uint8_t *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
        }
        else
        {
            pixels = (uint8_t *)sdl_opengl.framebuf;
        }
        pitch = sdl_opengl.pitch;
    }

    sdl.updating = true;
    return true;
}

void OUTPUT_OPENGL_EndUpdate(const uint16_t *changedLines)
{
    if (!(sdl.must_redraw_all && changedLines == NULL)) 
    {
        if (sdl_opengl.clear_countdown > 0)
        {
            sdl_opengl.clear_countdown--;
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        if (sdl_opengl.menudraw_countdown > 0) 
        {
            sdl_opengl.menudraw_countdown--;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
            mainMenu.setRedraw();
            GFX_DrawSDLMenu(mainMenu, mainMenu.display_list);
#endif
        }

#if C_XBRZ
        if (sdl_xbrz.enable && sdl_xbrz.scale_on)
        {
            // OpenGL pixel buffer is precreated for direct xBRZ output, while xBRZ render buffer is used for rendering
            const uint32_t srcWidth = sdl.draw.width;
            const uint32_t srcHeight = sdl.draw.height;

            if (sdl_xbrz.renderbuf.size() == (unsigned int)srcWidth * (unsigned int)srcHeight && srcWidth > 0 && srcHeight > 0)
            {
                // we assume render buffer is *not* scaled!
                const uint32_t* renderBuf = &sdl_xbrz.renderbuf[0]; // help VS compiler a little + support capture by value
                uint32_t* trgTex;
                if (sdl_opengl.pixel_buffer_object) 
                {
                    glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl_opengl.buffer);
                    trgTex = (uint32_t *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
                }
                else
                {
                    trgTex = reinterpret_cast<uint32_t*>(static_cast<void*>(sdl_opengl.framebuf));
                }

                if (trgTex)
                    xBRZ_Render(renderBuf, trgTex, changedLines, (int)srcWidth, (int)srcHeight, sdl_xbrz.scale_factor);
            }

            // and here we go repeating some stuff with xBRZ related modifications
            if (sdl_opengl.pixel_buffer_object)
            {
                glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
                glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    (int)(sdl.draw.width * (unsigned int)sdl_xbrz.scale_factor), (int)(sdl.draw.height * (unsigned int)sdl_xbrz.scale_factor), GL_BGRA_EXT,
#if defined (MACOSX) && !defined(C_SDL2)
                    // needed for proper looking graphics on macOS 10.12, 10.13
                    GL_UNSIGNED_INT_8_8_8_8,
#else
                    GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
                    0);
                glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
            }
            else
            {
                glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    (int)(sdl.draw.width * (unsigned int)sdl_xbrz.scale_factor), (int)(sdl.draw.height * (unsigned int)sdl_xbrz.scale_factor), GL_BGRA_EXT,
#if defined (MACOSX) && !defined(C_SDL2)
                    // needed for proper looking graphics on macOS 10.12, 10.13
                    GL_UNSIGNED_INT_8_8_8_8,
#else
                    // works on Linux
                    GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
                    (uint8_t *)sdl_opengl.framebuf);
            }
            glCallList(sdl_opengl.displaylist);
            SDL_GL_SwapBuffers();
        }
        else
#endif /*C_XBRZ*/
        if (sdl_opengl.pixel_buffer_object) 
        {
            if (changedLines && (changedLines[0] == sdl.draw.height))
                return;

            glUnmapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT);
            glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                (int)sdl.draw.width, (int)sdl.draw.height, GL_BGRA_EXT,
#if defined (MACOSX)
                // needed for proper looking graphics on macOS 10.12, 10.13
                GL_UNSIGNED_INT_8_8_8_8,
#else
                // works on Linux
                GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
                (void*)0);
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, 0);
            //glCallList(sdl_opengl.displaylist);
            //SDL_GL_SwapBuffers();
        }
        else if (changedLines) 
        {
            if (changedLines[0] == sdl.draw.height)
                return;

            Bitu y = 0, index = 0;
            glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
            while (y < sdl.draw.height) 
            {
                if (!(index & 1)) 
                {
                    y += changedLines[index];
                }
                else 
                {
                    uint8_t *pixels = (uint8_t *)sdl_opengl.framebuf + y * sdl_opengl.pitch;
                    Bitu height = changedLines[index];
                    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, (int)y,
                        (int)sdl.draw.width, (int)height, GL_BGRA_EXT,
#if defined (MACOSX) && !defined(C_SDL2)
                        // needed for proper looking graphics on macOS 10.12, 10.13
                        GL_UNSIGNED_INT_8_8_8_8,
#else
                        // works on Linux
                        GL_UNSIGNED_INT_8_8_8_8_REV,
#endif
                        (void*)pixels);
                    y += height;
                }
                index++;
            }
        } else
            return;
        if (sdl_opengl.program_object) {
            glUniform1i(sdl_opengl.ruby.frame_count, sdl_opengl.actual_frame_count++);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        } else
            glCallList(sdl_opengl.displaylist);

#if 0 /* DEBUG Prove to me that you're drawing the damn texture */
            glBindTexture(GL_TEXTURE_2D, SDLDrawGenFontTexture);

            glPushMatrix();

            glMatrixMode(GL_TEXTURE);
            glLoadIdentity();
            glScaled(1.0 / SDLDrawGenFontTextureWidth, 1.0 / SDLDrawGenFontTextureHeight, 1.0);

            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_ALPHA_TEST);
            glEnable(GL_BLEND);

            glBegin(GL_QUADS);

            glTexCoord2i(0, 0); glVertex2i(0, 0); // lower left
            glTexCoord2i(SDLDrawGenFontTextureWidth, 0); glVertex2i(SDLDrawGenFontTextureWidth, 0); // lower right
            glTexCoord2i(SDLDrawGenFontTextureWidth, SDLDrawGenFontTextureHeight); glVertex2i(SDLDrawGenFontTextureWidth, SDLDrawGenFontTextureHeight); // upper right
            glTexCoord2i(0, SDLDrawGenFontTextureHeight); glVertex2i(0, SDLDrawGenFontTextureHeight); // upper left

            glEnd();

            glBlendFunc(GL_ONE, GL_ZERO);
            glDisable(GL_ALPHA_TEST);
            glEnable(GL_TEXTURE_2D);

            glPopMatrix();

            glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
#endif

            SDL_GL_SwapBuffers();

        if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
    }
}

void OUTPUT_OPENGL_Shutdown()
{
    // nothing to shutdown (yet?)
}

#endif