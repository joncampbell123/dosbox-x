
// Tell Mac OS X to shut up about deprecated OpenGL calls
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif

#include <sys/types.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include <output/output_opengl.h>

#include <algorithm>

#include "sdlmain.h"

using namespace std;

bool setSizeButNotResize();

#if C_OPENGL
PFNGLGENBUFFERSARBPROC glGenBuffersARB = NULL;
PFNGLBINDBUFFERARBPROC glBindBufferARB = NULL;
PFNGLDELETEBUFFERSARBPROC glDeleteBuffersARB = NULL;
PFNGLBUFFERDATAARBPROC glBufferDataARB = NULL;
PFNGLMAPBUFFERARBPROC glMapBufferARB = NULL;
PFNGLUNMAPBUFFERARBPROC glUnmapBufferARB = NULL;

#if C_OPENGL && DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
extern unsigned int SDLDrawGenFontTextureUnitPerRow;
extern unsigned int SDLDrawGenFontTextureRows;
extern unsigned int SDLDrawGenFontTextureWidth;
extern unsigned int SDLDrawGenFontTextureHeight;
extern GLuint SDLDrawGenFontTexture;
extern bool SDLDrawGenFontTextureInit;
#endif

SDL_OpenGL sdl_opengl;

int Voodoo_OGL_GetWidth();
int Voodoo_OGL_GetHeight();
bool Voodoo_OGL_Active();

static SDL_Surface* SetupSurfaceScaledOpenGL(Bit32u sdl_flags, Bit32u bpp) 
{
    Bit16u fixedWidth;
    Bit16u fixedHeight;
    Bit16u windowWidth;
    Bit16u windowHeight;

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
            cw = (Bit16u)(sdl.draw.width*sdl.draw.scalex);
        if (ch == 0)
            ch = (Bit16u)(sdl.draw.height*sdl.draw.scaley);

        while ((cw / scale) >= (640 * 2) && (ch / scale) >= (400 * 2))
            scale++;

        LOG_MSG("menuScale=%d", scale);
        mainMenu.setScale((unsigned int)scale);

        if (mainMenu.isVisible() && !sdl.desktop.fullscreen)
            fixedHeight -= mainMenu.menuBox.h;
    }
#endif

    sdl.clip.x = 0; sdl.clip.y = 0;
    if (Voodoo_OGL_GetWidth() != 0 && Voodoo_OGL_GetHeight() != 0 && Voodoo_OGL_Active() && sdl.desktop.prevent_fullscreen)
    { 
        /* 3Dfx openGL do not allow resize */
        sdl.clip.w = windowWidth = (Bit16u)Voodoo_OGL_GetWidth();
        sdl.clip.h = windowHeight = (Bit16u)Voodoo_OGL_GetHeight();
    }
    else if (fixedWidth && fixedHeight) 
    {
        sdl.clip.w = windowWidth = fixedWidth;
        sdl.clip.h = windowHeight = fixedHeight;
        if (render.aspect) aspectCorrectFitClip(sdl.clip.w, sdl.clip.h, sdl.clip.x, sdl.clip.y, fixedWidth, fixedHeight);
    }
    else 
    {
        windowWidth = (Bit16u)(sdl.draw.width * sdl.draw.scalex);
        windowHeight = (Bit16u)(sdl.draw.height * sdl.draw.scaley);
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
#else
    sdl.surface = SDL_SetVideoMode(windowWidth, windowHeight, (int)bpp, (unsigned int)sdl_flags | (unsigned int)(setSizeButNotResize() ? SDL_HAX_NORESIZEWINDOW : 0));
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

void OUTPUT_OPENGL_Select()
{
    sdl.desktop.want_type = SCREEN_OPENGL;
    render.aspectOffload = true;

#if defined(WIN32) && !defined(C_SDL2)
    SDL1_hax_inhibit_WM_PAINT = 0;
#endif
}

Bitu OUTPUT_OPENGL_GetBestMode(Bitu flags)
{
    if (!(flags & GFX_CAN_32)) return 0; // OpenGL requires 32-bit output mode
    flags |= GFX_SCALING;
    flags &= ~(GFX_CAN_8 | GFX_CAN_15 | GFX_CAN_16);
    return flags;
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

#if defined(C_SDL2)
    SetupSurfaceScaledOpenGL(SDL_WINDOW_RESIZABLE, 0);
#else
    SetupSurfaceScaledOpenGL(SDL_RESIZABLE, 0);
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

    glViewport(0, 0, sdl.surface->w, sdl.surface->h);
    glDeleteTextures(1, &sdl_opengl.texture);
    glGenTextures(1, &sdl_opengl.texture);
    glBindTexture(GL_TEXTURE_2D, sdl_opengl.texture);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, 0);

    // No borders
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, sdl_opengl.bilinear ? GL_LINEAR : GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, sdl_opengl.bilinear ? GL_LINEAR : GL_NEAREST);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);

    // NTS: I'm told that nVidia hardware seems to triple buffer despite our
    //      request to double buffer (according to @pixelmusement), therefore
    //      the next 3 frames, instead of 2, need to be cleared.
    sdl_opengl.menudraw_countdown = 3; // two GL buffers with possible triple buffering behind our back
    sdl_opengl.clear_countdown = 3; // two GL buffers with possible triple buffering behind our back

    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    // SDL_GL_SwapBuffers();
    // glClear(GL_COLOR_BUFFER_BIT);
    glShadeModel(GL_FLAT);
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
            extern Bit8u int10_font_16[256 * 16];

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

bool OUTPUT_OPENGL_StartUpdate(Bit8u* &pixels, Bitu &pitch)
{
#if C_XBRZ    
    if (sdl_xbrz.enable && sdl_xbrz.scale_on) 
    {
        sdl_xbrz.renderbuf.resize(sdl.draw.width * sdl.draw.height);
        pixels = sdl_xbrz.renderbuf.empty() ? nullptr : reinterpret_cast<Bit8u*>(&sdl_xbrz.renderbuf[0]);
        pitch = sdl.draw.width * sizeof(uint32_t);
    }
    else
#endif
    {
        if (sdl_opengl.pixel_buffer_object)
        {
            glBindBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, sdl_opengl.buffer);
            pixels = (Bit8u *)glMapBufferARB(GL_PIXEL_UNPACK_BUFFER_EXT, GL_WRITE_ONLY);
        }
        else
        {
            pixels = (Bit8u *)sdl_opengl.framebuf;
        }
        pitch = sdl_opengl.pitch;
    }

    sdl.updating = true;
    return true;
}

void OUTPUT_OPENGL_EndUpdate(const Bit16u *changedLines)
{
    if (!(sdl.must_redraw_all && changedLines == NULL)) 
    {
        if (sdl_opengl.clear_countdown > 0)
        {
            sdl_opengl.clear_countdown--;
            glClearColor(0.0, 0.0, 0.0, 1.0);
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
            const Bit32u srcWidth = sdl.draw.width;
            const Bit32u srcHeight = sdl.draw.height;

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
                    (Bit8u *)sdl_opengl.framebuf);
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
            glCallList(sdl_opengl.displaylist);
            SDL_GL_SwapBuffers();
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
                    Bit8u *pixels = (Bit8u *)sdl_opengl.framebuf + y * sdl_opengl.pitch;
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
        }

        if (!menu.hidecycles && !sdl.desktop.fullscreen) frames++;
    }
}

void OUTPUT_OPENGL_Shutdown()
{
    // nothing to shutdown (yet?)
}

#endif
