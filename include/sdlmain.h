#include "dosbox.h"

#include "control.h"
#include "menu.h"
#include "mouse.h"
#include "render.h"
#include "video.h"

#include "cross.h"
#include "SDL.h"
#include "SDL_video.h"

#ifdef __WIN32__
#include "SDL_syswm.h"
#endif

#ifndef DOSBOX_SDLMAIN_H
#define DOSBOX_SDLMAIN_H

enum SCREEN_TYPES {
    SCREEN_SURFACE
    ,SCREEN_OPENGL // [FIXME] cannot make this conditional because somehow SDL2 code uses it while C_OPENGL is definitely disabled by C_SDL2 so SCREEN_OPENGL is unavailable
#if C_DIRECT3D
    ,SCREEN_DIRECT3D
#endif
};

enum AUTOLOCK_FEEDBACK
{
    AUTOLOCK_FEEDBACK_NONE,
    AUTOLOCK_FEEDBACK_BEEP,
    AUTOLOCK_FEEDBACK_FLASH
};

enum PRIORITY_LEVELS {
    PRIORITY_LEVEL_PAUSE,
    PRIORITY_LEVEL_LOWEST,
    PRIORITY_LEVEL_LOWER,
    PRIORITY_LEVEL_NORMAL,
    PRIORITY_LEVEL_HIGHER,
    PRIORITY_LEVEL_HIGHEST
};

// do not specify any defaults inside, it is zeroed at start of main()
struct SDL_Block {
    bool inited;
    bool active; // if this isn't set don't draw
    bool updating;
#if defined(C_SDL2)
    bool update_window;
    bool update_display_contents;
    int window_desired_width, window_desired_height;
#endif
    struct {
        Bit32u width;
        Bit32u height;
        Bit32u bpp;
        Bitu flags;
        double scalex, scaley;
        GFX_CallBack_t callback;
    } draw;
    bool wait_on_error;
    struct {
        struct {
            Bit16u width, height;
            bool fixed;
            bool display_res;
        } full;
        struct {
            Bit16u width, height;
        } window;
        Bit8u bpp;
#if defined(C_SDL2)
        Bit32u pixelFormat;
#endif
        bool fullscreen;
        bool lazy_fullscreen;
        bool prevent_fullscreen;
        bool lazy_fullscreen_req;
        bool doublebuf;
        SCREEN_TYPES type;
        SCREEN_TYPES want_type;
    } desktop;
    struct {
        SDL_Surface * surface;
#if (HAVE_DDRAW_H) && defined(WIN32)
        RECT rect;
#endif
    } blit;
    struct {
        PRIORITY_LEVELS focus;
        PRIORITY_LEVELS nofocus;
    } priority;
    SDL_Rect clip;
    SDL_Surface * surface;
#if defined(C_SDL2)
    SDL_Window * window;
    SDL_Renderer * renderer;
    const char * rendererDriver;
    int displayNumber;
    struct {
        SDL_Texture * texture;
        SDL_PixelFormat * pixelFormat;
    } texture;
#endif
    SDL_cond *cond;
    struct {
        bool autolock;
        AUTOLOCK_FEEDBACK autolock_feedback;
        bool autoenable;
        bool requestlock;
        bool locked;
        Bitu sensitivity;
        MOUSE_EMULATION emulation;
    } mouse;
    SDL_Rect updateRects[1024];
    Bitu overscan_color;
    Bitu overscan_width;
    Bitu num_joysticks;
#if defined (WIN32)
    bool using_windib;
#endif
    // state of alt-keys for certain special handlings
    Bit16u laltstate;
    Bit16u raltstate;
    bool must_redraw_all;
    bool deferred_resize;
    bool init_ignore;
    unsigned int gfx_force_redraw_count;
    struct {
        int x;
        int y;
        double xToY;
        double yToX;
    } srcAspect;
#if C_SURFACE_POSTRENDER_ASPECT
    std::vector<uint32_t> aspectbuf;
#endif
};

#if defined(WIN32) && !defined(C_SDL2)
extern "C" unsigned int SDL1_hax_inhibit_WM_PAINT;
#endif

extern Bitu frames;
extern SDL_Block sdl;

#include <output/output_surface.h>
#include <output/output_direct3d.h>
#include <output/output_opengl.h>
#include <output/output_tools.h>
#include <output/output_tools_xbrz.h>

#include "zipfile.h"

extern Bitu userResizeWindowWidth;
extern Bitu userResizeWindowHeight;
extern Bitu currentWindowWidth;
extern Bitu currentWindowHeight;

extern ZIPFile savestate_zip;

void GFX_DrawSDLMenu(DOSBoxMenu &menu, DOSBoxMenu::displaylist &dl);
void GFX_LogSDLState(void);
void GFX_SDL_Overscan(void);
void GFX_SetIcon(void);
void SDL_rect_cliptoscreen(SDL_Rect &r);
void UpdateWindowDimensions(void);
void UpdateWindowDimensions(Bitu width, Bitu height);

#if defined(C_SDL2)
SDL_Window* GFX_SetSDLWindowMode(Bit16u width, Bit16u height, SCREEN_TYPES screenType);
#endif

#endif /*DOSBOX_SDLMAIN_H*/
