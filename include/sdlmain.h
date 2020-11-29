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
    ,SCREEN_TTF
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

enum method {
    METHOD_NONE = 0,
    METHOD_X11,
    METHOD_XRANDR,
    METHOD_WIN98BASE,
    METHOD_COREGRAPHICS
};

// Screen DPI and size info
class ScreenSizeInfo {
public:
    struct wxh {
        double      width = -1;
        double      height = -1;

        void clear(void) {
            width = height = -1;
        }
    };
    struct xvy {
        double      x = 0;
        double      y = 0;

        void clear(void) {
            x = y = 0;
        }
    };
public:
    xvy             screen_position_pixels;     // position of the screen on the "virtual" overall desktop
    wxh             screen_dimensions_pixels;   // size of the screen in pixels
    wxh             screen_dimensions_mm;       // size of the screen in mm
    wxh             screen_dpi;                 // DPI of the screen
    enum method     method = METHOD_NONE;
public:
    void clear(void) {
        screen_dpi.clear();
        screen_dimensions_mm.clear();
        screen_dimensions_pixels.clear();
        screen_position_pixels.clear();
        method = METHOD_NONE;
    }
};

extern ScreenSizeInfo       screen_size_info;

struct SDL_Block {
    bool inited = false;
    bool active = false; // if this isn't set don't draw
    bool updating = false;
#if defined(C_SDL2)
    bool update_window = false;
    bool update_display_contents = false;
    int window_desired_width = 0, window_desired_height = 0;
#endif
    struct {
        uint32_t width = 0;
        uint32_t height = 0;
        uint32_t bpp = 0;
        Bitu flags = 0;
        double scalex = 0, scaley = 0;
        GFX_CallBack_t callback = 0;
    } draw;
    bool wait_on_error = false;
    struct {
        struct {
            uint16_t width = 0, height = 0;
            bool fixed = false;
            bool display_res = false;
            bool width_auto = false, height_auto = false;
        } full;
        struct {
            uint16_t width = 0, height = 0;
        } window;
        uint8_t bpp = 0;
#if defined(C_SDL2)
        uint32_t pixelFormat = 0;
#endif
        bool fullscreen = false;
        bool lazy_fullscreen = false;
        bool prevent_fullscreen = false;
        bool lazy_fullscreen_req = false;
        bool doublebuf = false;
        SCREEN_TYPES type = (SCREEN_TYPES)0;
        SCREEN_TYPES want_type = (SCREEN_TYPES)0;
    } desktop;
    struct {
        SDL_Surface * surface = NULL;
#if (HAVE_DDRAW_H) && defined(WIN32)
        RECT rect = {};
#endif
    } blit;
    struct {
        PRIORITY_LEVELS focus = (PRIORITY_LEVELS)0;
        PRIORITY_LEVELS nofocus = (PRIORITY_LEVELS)0;
    } priority;
    SDL_Rect clip = {};
    SDL_Surface * surface = NULL;
#if defined(C_SDL2)
    SDL_Window * window = NULL;
    SDL_Renderer * renderer = NULL;
    const char * rendererDriver = NULL;
    int displayNumber = 0;
    struct {
        SDL_Texture * texture = NULL;
        SDL_PixelFormat * pixelFormat = NULL;
    } texture;
#endif
    SDL_cond *cond = NULL;
    struct {
        bool autolock = false;
        AUTOLOCK_FEEDBACK autolock_feedback = (AUTOLOCK_FEEDBACK)0;
        bool autoenable = false;
        bool requestlock = false;
        bool locked = false;
        int xsensitivity = 0;
        int ysensitivity = 0;
        MOUSE_EMULATION emulation = (MOUSE_EMULATION)0;
    } mouse;
    SDL_Rect updateRects[1024] = {};
    Bitu overscan_color = 0;
    Bitu overscan_width = 0;
    Bitu num_joysticks = 0;
#if defined (WIN32)
    bool using_windib = false;
    // Time when sdl regains focus (alt-tab) in windowed mode
    uint32_t focus_ticks = 0;
#endif
    // state of alt-keys for certain special handlings
    uint16_t laltstate = 0, raltstate = 0;
    uint16_t lctrlstate = 0, rctrlstate = 0;
    uint16_t lshiftstate = 0, rshiftstate = 0;
    bool must_redraw_all = false;
    bool deferred_resize = false;
    bool init_ignore = false;
    unsigned int gfx_force_redraw_count = 0;
    struct {
        int x = 0;
        int y = 0;
        double xToY = 0;
        double yToX = 0;
    } srcAspect;
#if C_SURFACE_POSTRENDER_ASPECT
    std::vector<uint32_t> aspectbuf = {};
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

void GFX_DrawSDLMenu(DOSBoxMenu &menu, DOSBoxMenu::displaylist &dl);
void GFX_LogSDLState(void);
void GFX_SDL_Overscan(void);
void GFX_SetIcon(void);
void SDL_rect_cliptoscreen(SDL_Rect &r);
void UpdateWindowDimensions(void);
void UpdateWindowDimensions(Bitu width, Bitu height);

#if defined(C_SDL2)
SDL_Window* GFX_SetSDLWindowMode(uint16_t width, uint16_t height, SCREEN_TYPES screenType);
#endif

#if defined(C_SDL2) && defined(C_OPENGL)/*HACK*/
void SDL_GL_SwapBuffers(void);
#endif

#endif /*DOSBOX_SDLMAIN_H*/
