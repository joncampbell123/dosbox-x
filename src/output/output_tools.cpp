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

#include "dosbox.h"
#include "video.h"
#include "setup.h"
#include "jfont.h"
#include "cpu.h"
#include "sdlmain.h"
#include "control.h"
#include "render.h"
#include "logging.h"
#include "../ints/int10.h"

#if defined(WIN32)
#include "resource.h"
#endif

#if C_DIRECT3D
void d3d_init(void);
#endif

#if defined(USE_TTF)
void resetFontSize();
#endif

void res_init(void), RENDER_Reset(void), UpdateOverscanMenu(void), GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);

extern int initgl, posx, posy;
extern bool rtl, gbk, chinasea, window_was_maximized, dpi_aware_enable, isVirtualBox;

void refreshExtChar() {
    mainMenu.get_item("ttf_extcharset").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables);
    if (dos.loaded_codepage == 936) mainMenu.get_item("ttf_extcharset").check(gbk).refresh_item(mainMenu);
    else if (dos.loaded_codepage == 950 || dos.loaded_codepage == 951) mainMenu.get_item("ttf_extcharset").check(chinasea).refresh_item(mainMenu);
    else mainMenu.get_item("ttf_extcharset").check(gbk&&chinasea).refresh_item(mainMenu);
}

std::string GetDefaultOutput() {
    static std::string output = "surface";
#if defined(USE_TTF)
# if 0 /* TODO: If someone wants to compile DOSBox-X to default to TTF, change this to "# if 1" or "# if defined(...)" here */
    std::string mtype(static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_string("machine"));
    if (mtype.substr(0, 3) == "vga" || mtype.substr(0, 4) == "svga" || mtype.substr(0, 4) == "vesa" || mtype.substr(0, 4) == "pc98")
        return "ttf";
# endif
#endif
#if defined(WIN32)
# if defined(HX_DOS)
    output = "surface"; /* HX-DOS should stick to surface */
# elif defined(__MINGW32__) && !(C_DIRECT3D) && !defined(C_SDL2)
    /* NTS: OpenGL output never seems to work in VirtualBox under Windows XP */
    output = isVirtualBox ? "surface" : "opengl"; /* MinGW builds do not yet have Direct3D */
# elif C_DIRECT3D
    output = "direct3d";
# elif C_OPENGL && !defined(C_SDL2)
    output = isVirtualBox ? "surface" : "opengl";
# elif C_OPENGL
    output = "opengl";
# else
    output = "surface";
# endif
#elif defined(C_OPENGL) && !(defined(LINUX) && !defined(C_SDL2)) && !(defined(MACOSX) && defined(C_SDL2))
    /* NTS: Lately, especially on Macbooks with Retina displays, OpenGL gives better performance
            than the CG bitmap-based "surface" output.

            On my dev system, "top" shows a 20% CPU load doing 1:1 CG Bitmap output to the screen
            on a High DPI setup, while ignoring High DPI and rendering through Cocoa at 2x size
            pegs the CPU core DOSBox-X is running on at 100% and emulation stutters.

            So for the best experience, default to OpenGL.

            Note that "surface" yields good performance with SDL2 and macOS because SDL2 doesn't
            use the CGBitmap system, it uses OpenGL or Metal underneath automatically. */
    output = "opengl";
#else
    output = "surface";
#endif
    return output;
}

void change_output(int output) {
    GFX_Stop();
    Section * sec = control->GetSection("sdl");
    Section_prop * section=static_cast<Section_prop *>(sec);
    sdl.overscan_width=(unsigned int)section->Get_int("overscan");
    UpdateOverscanMenu();
    sdl.desktop.isperfect = false; /* Reset before selection */

    switch (output) {
    case 0:
        OUTPUT_SURFACE_Select();
        break;
    case 1:
        OUTPUT_SURFACE_Select();
        break;
    case 2: /* do nothing */
        break;
    case 3:
#if C_OPENGL
        change_output(2);
        OUTPUT_OPENGL_Select(GLBilinear);
#endif
        break;
    case 4:
#if C_OPENGL
        change_output(2);
        OUTPUT_OPENGL_Select(GLNearest);
#endif
        break;
    case 5:
#if C_OPENGL
        change_output(2);
        OUTPUT_OPENGL_Select(GLPerfect);
#endif
        break;
#if C_DIRECT3D
    case 6:
        OUTPUT_DIRECT3D_Select();
        d3d_init();
        break;
#endif
    case 7:
        break;
    case 8:
        // do not set want_type
        break;
    case 9:
#if C_DIRECT3D
        if (sdl.desktop.want_type == SCREEN_DIRECT3D) {
            OUTPUT_DIRECT3D_Select();
            d3d_init();
        }
#endif
        break;
#if defined(USE_TTF)
    case 10:
        OUTPUT_TTF_Select();
    case 11:
        sdl.desktop.want_type = SCREEN_TTF;
        ttf.inUse = true;
        break;
#endif

#if C_GAMELINK
    case 12:
        OUTPUT_GAMELINK_Select();
        break;
#endif

    default:
        LOG_MSG("SDL: Unsupported output device %d, switching back to surface",output);
        OUTPUT_SURFACE_Select();
        break;
    }

#if C_GAMELINK
    if (!OUTPUT_GAMELINK_InitTrackingMode() && sdl.desktop.want_type == SCREEN_GAMELINK) OUTPUT_SURFACE_Select();
#endif


#if C_OPENGL
    if (sdl.desktop.want_type != SCREEN_OPENGL) mainMenu.get_item("load_glsl_shader").enable(false).refresh_item(mainMenu);
#endif
#if C_DIRECT3D
    if (sdl.desktop.want_type != SCREEN_DIRECT3D) mainMenu.get_item("load_d3d_shader").enable(false).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    if (sdl.desktop.want_type != SCREEN_TTF) {
        mainMenu.get_item("load_ttf_font").enable(false).refresh_item(mainMenu);
        resetreq = false;
        ttf.inUse = false;
    }
#endif

    const char* windowresolution=section->Get_string("windowresolution");
    if (windowresolution && *windowresolution) 
    {
        char res[100];
        safe_strncpy( res,windowresolution, sizeof( res ));
        windowresolution = lowcase (res);//so x and X are allowed
        if (strcmp(windowresolution,"original")) 
        {
            if (output == 0) 
            {
                std::string tmp("windowresolution=original");
                sec->HandleInputline(tmp);
            }
        }
    }
    res_init();

    if (sdl.draw.callback)
        (sdl.draw.callback)( GFX_CallBackReset );
#ifdef C_OPENGL
        mainMenu.get_item("load_glsl_shader").enable(sdl.desktop.want_type==SCREEN_OPENGL&&initgl==2).refresh_item(mainMenu);
#endif
#ifdef C_DIRECT3D
        mainMenu.get_item("load_d3d_shader").enable(sdl.desktop.want_type==SCREEN_DIRECT3D).refresh_item(mainMenu);
#endif
#ifdef USE_TTF
        mainMenu.get_item("load_ttf_font").enable(sdl.desktop.want_type==SCREEN_TTF).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    if ((output==10||output==11)&&ttf.inUse) {
        resetFontSize();
        resetreq = true;
#if defined(HX_DOS)
        PIC_AddEvent(ttfreset, 100);
        mainMenu.setScale(1);
#endif
    }
#if defined(WIN32) && !defined(HX_DOS)
    HMENU sysmenu = GetSystemMenu(GetHWND(), TRUE);
    if (sysmenu != NULL) {
        EnableMenuItem(sysmenu, ID_WIN_SYSMENU_TTFINCSIZE, MF_BYCOMMAND|(TTF_using()?MF_ENABLED:MF_DISABLED));
        EnableMenuItem(sysmenu, ID_WIN_SYSMENU_TTFDECSIZE, MF_BYCOMMAND|(TTF_using()?MF_ENABLED:MF_DISABLED));
    }
#endif
    mainMenu.get_item("mapper_aspratio").enable(!TTF_using()).check(render.aspect).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_1_1").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_3_2").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_4_3").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_16_9").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_16_10").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_18_10").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_original").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("video_ratio_set").enable(!TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("mapper_incsize").enable(TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("mapper_decsize").enable(TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("mapper_resetcolor").enable(TTF_using()).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showbold").enable(TTF_using()).check(showbold).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showital").enable(TTF_using()).check(showital).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showline").enable(TTF_using()).check(showline).refresh_item(mainMenu);
    mainMenu.get_item("ttf_showsout").enable(TTF_using()).check(showsout).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpno").enable(TTF_using()).check(!wpType).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpwp").enable(TTF_using()).check(wpType==1).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpws").enable(TTF_using()).check(wpType==2).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpxy").enable(TTF_using()).check(wpType==3).refresh_item(mainMenu);
    mainMenu.get_item("ttf_wpfe").enable(TTF_using()).check(wpType==4).refresh_item(mainMenu);
    mainMenu.get_item("ttf_blinkc").enable(TTF_using()).check(blinkCursor>-1).refresh_item(mainMenu);
    mainMenu.get_item("ttf_right_left").enable(TTF_using()).check(rtl).refresh_item(mainMenu);
#if C_PRINTER
    mainMenu.get_item("ttf_printfont").enable(TTF_using()).check(printfont).refresh_item(mainMenu);
#endif
    mainMenu.get_item("mapper_dbcssbcs").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(dbcs_sbcs||IS_PC98_ARCH||IS_JEGA_ARCH).refresh_item(mainMenu);
    mainMenu.get_item("mapper_autoboxdraw").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(autoboxdraw||IS_PC98_ARCH||IS_JEGA_ARCH).refresh_item(mainMenu);
    mainMenu.get_item("ttf_halfwidthkana").enable(TTF_using()&&!IS_PC98_ARCH&&!IS_JEGA_ARCH&&enable_dbcs_tables).check(halfwidthkana||IS_PC98_ARCH||IS_JEGA_ARCH).refresh_item(mainMenu);
    refreshExtChar();
#endif

    if (output != 7) GFX_SetTitle((int32_t)(CPU_CycleAutoAdjust?CPU_CyclePercUsed:CPU_CycleMax),-1,-1,false);
    GFX_LogSDLState();

    UpdateWindowDimensions();
}

void OutputSettingMenuUpdate(void) {
    mainMenu.get_item("output_surface").check(sdl.desktop.want_type == SCREEN_SURFACE).refresh_item(mainMenu);
#if C_DIRECT3D
    mainMenu.get_item("output_direct3d").check(sdl.desktop.want_type == SCREEN_DIRECT3D).refresh_item(mainMenu);
#endif
#if C_OPENGL
    mainMenu.get_item("output_opengl").check(sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLBilinear).refresh_item(mainMenu);
    mainMenu.get_item("output_openglnb").check(sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLNearest).refresh_item(mainMenu);
    mainMenu.get_item("output_openglpp").check(sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLPerfect).refresh_item(mainMenu);
#endif
#if defined(USE_TTF)
    mainMenu.get_item("output_ttf").check(sdl.desktop.want_type == SCREEN_TTF).refresh_item(mainMenu);
#endif
#if C_GAMELINK
    mainMenu.get_item("output_gamelink").check(sdl.desktop.want_type == SCREEN_GAMELINK).refresh_item(mainMenu);
#endif
}

void SwitchFS(Bitu val) {
    GFX_SwitchFullScreen();
}

bool toOutput(const char *what) {
    bool reset=false;
#if defined(USE_TTF)
    if (TTF_using()) reset=true;
#endif

    if (!strcmp(what,"surface")) {
        if (sdl.desktop.want_type == SCREEN_SURFACE) return false;
        if (window_was_maximized&&!GFX_IsFullscreen()) {
            change_output(0);
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        } else
            change_output(0);
        RENDER_Reset();
    }
    else if (!strcmp(what,"opengl")) {
#if C_OPENGL
        if (sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLBilinear) return false;
        if (window_was_maximized&&!GFX_IsFullscreen()) {
            change_output(3);
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        } else
            change_output(3);
#endif
    }
    else if (!strcmp(what,"openglnb")) {
#if C_OPENGL
        if (sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLNearest) return false;
        if (window_was_maximized&&!GFX_IsFullscreen()) {
            change_output(4);
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        } else
            change_output(4);
#endif
    }
    else if (!strcmp(what,"openglpp")) {
#if C_OPENGL
        if (sdl.desktop.want_type == SCREEN_OPENGL && sdl_opengl.kind == GLPerfect) return false;
        if (window_was_maximized&&!GFX_IsFullscreen()) {
            change_output(5);
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        } else
            change_output(5);
#endif
    }
    else if (!strcmp(what,"direct3d")) {
#if C_DIRECT3D
        if (sdl.desktop.want_type == SCREEN_DIRECT3D) return false;
#if C_OPENGL && defined(C_SDL2)
        if (sdl.desktop.want_type == SCREEN_OPENGL)
            GFX_SetSDLWindowMode(currentWindowWidth, currentWindowHeight, SCREEN_SURFACE);
#endif
        if (window_was_maximized&&!GFX_IsFullscreen()) {
            change_output(6);
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_MAXIMIZE);
#endif
        } else
            change_output(6);
#endif
    }
    else if (!strcmp(what,"ttf")) {
#if defined(USE_TTF)
        if (sdl.desktop.want_type == SCREEN_TTF || (CurMode->type!=M_TEXT && !IS_PC98_ARCH)) return false;
#if C_OPENGL && defined(MACOSX) && !defined(C_SDL2)
        if (sdl.desktop.want_type == SCREEN_SURFACE) {
            sdl_opengl.framebuf = calloc(sdl.draw.width*sdl.draw.height, 4);
            sdl.desktop.type = SCREEN_OPENGL;
        }
#endif
        bool switchfull = false;
        if (GFX_IsFullscreen()) {
            switchfull = true;
            GFX_SwitchFullScreen();
        } else if (window_was_maximized) {
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_RESTORE);
#elif defined(C_SDL2)
            SDL_RestoreWindow(sdl.window);
#endif
        }
#if !defined(C_SDL2)
        if (posx != -2 || posy != -2) putenv((char*)"SDL_VIDEO_CENTERED=center");
#endif
        firstset=false;
        change_output(10);
        if (!GFX_IsFullscreen() && switchfull) {
            switchfull = false;
            ttf.fullScrn = false;
#if !defined(C_SDL2)
            if (!dpi_aware_enable)
                PIC_AddEvent(SwitchFS, 100);
            else
#endif
            GFX_SwitchFullScreen();
        } else if (!GFX_IsFullscreen() && ttf.fullScrn) {
            ttf.fullScrn = false;
            reset = true;
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            if (!control->opt_nomenu && static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("showmenu")) DOSBox_SetMenu();
#endif
        }
#endif
    }
    else if (!strcmp(what,"gamelink")) {
#if C_GAMELINK
        if (sdl.desktop.want_type == SCREEN_GAMELINK) return false;
        change_output(12);
        reset = true;
#endif
    }
    if (reset) RENDER_Reset();
    OutputSettingMenuUpdate();
    return true;
}
