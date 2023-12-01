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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <assert.h>
#include <math.h>

#include "dosbox.h"
#include "logging.h"
#include "sdlmain.h"
#include "render.h"
#include "jfont.h"
#include "inout.h"
#include "bios.h"
#include "regs.h"
#include "control.h"
#include "menudef.h"
#include "callback.h"
#include "../ints/int10.h"

#include <output/output_ttf.h>

using namespace std;

std::map<int, int> lowboxdrawmap {
    {1, 201}, {2, 187}, {3, 200}, {4, 188}, {5, 186}, {6, 205}, {0xe, 178},
    {0x10, 206}, {0x14, 177}, {0x15, 202}, {0x16, 203}, {0x17, 185}, {0x19, 204}, {0x1a, 176},
};
std::map<int, int> pc98boxdrawmap {
    {0xB4, 0x67}, {0xB9, 0x6e}, {0xBA, 0x46}, {0xBB, 0x56}, {0xBC, 0x5E}, {0xB3, 0x45}, {0xBF, 0x53},
    {0xC0, 0x57}, {0xC1, 0x77}, {0xC2, 0x6f}, {0xC3, 0x5f}, {0xC4, 0x43}, {0xC5, 0x7f}, {0xC8, 0x5A},
    {0xC9, 0x52}, {0xCA, 0x7e}, {0xCB, 0x76}, {0xCC, 0x66}, {0xCD, 0x44}, {0xD9, 0x5B}, {0xDA, 0x4f},
};
uint16_t cpMap_AX[32] = {
	0x0020, 0x00b6, 0x2195, 0x2194, 0x2191, 0x2193, 0x2192, 0x2190, 0x2500, 0x2502, 0x250c, 0x2510, 0x2518, 0x2514, 0x251c, 0x252c,
	0x2524, 0x2534, 0x253c, 0x2550, 0x2551, 0x2554, 0x2557, 0x255d, 0x255a, 0x2560, 0x2566, 0x2563, 0x2569, 0x256c, 0x00ab, 0x00bb
};

#if defined(USE_TTF)
#include "cp437_uni.h"
#include "DOSBoxTTF.h"
#include "../gui/sdl_ttf.c"

#define MIN_PTSIZE 9

#ifdef _MSC_VER
# define MIN(a,b) (std::min)(a,b)
# define MAX(a,b) (std::max)(a,b)
#else
# define MIN(a,b) std::min(a,b)
# define MAX(a,b) std::max(a,b)
#endif

Render_ttf ttf;
bool char512 = true;
bool showbold = true;
bool showital = true;
bool showline = true;
bool showsout = false;
bool dbcs_sbcs = true;
bool printfont = true;
bool autoboxdraw = true;
bool halfwidthkana = true;
bool ttf_dosv = false;
bool lesssize = false;
bool lastmenu = true;
bool initttf = false;
bool copied = false;
bool firstset = true;
bool forceswk = false;
bool notrysgf = false;
bool wpExtChar = false;
int wpType = 0;
int wpVersion = 0;
int wpBG = -1;
int wpFG = 7;
int lastset = 0;
int lastfontsize = 0;
int switchoutput = -1;
int checkcol = 0;

static unsigned long ttfSize = sizeof(DOSBoxTTFbi), ttfSizeb = 0, ttfSizei = 0, ttfSizebi = 0;
static void * ttfFont = DOSBoxTTFbi, * ttfFontb = NULL, * ttfFonti = NULL, * ttfFontbi = NULL;
extern int posx, posy, eurAscii, transparency, NonUserResizeCounter;
extern bool rtl, gbk, chinasea, switchttf, force_conversion, blinking, showdbcs, loadlang, window_was_maximized;
extern const char* RunningProgram;
extern uint8_t ccount;
extern uint16_t cpMap[512], cpMap_PC98[256];
uint16_t cpMap_copy[256];
static SDL_Color ttf_fgColor = {0, 0, 0, 0};
static SDL_Color ttf_bgColor = {0, 0, 0, 0};
static SDL_Rect ttf_textRect = {0, 0, 0, 0};
static SDL_Rect ttf_textClip = {0, 0, 0, 0};

ttf_cell curAttrChar[txtMaxLins*txtMaxCols];					// currently displayed textpage
ttf_cell newAttrChar[txtMaxLins*txtMaxCols];					// to be replaced by

extern alt_rgb altBGR0[16];
alt_rgb altBGR1[16];
int blinkCursor = -1;
static int prev_sline = -1;
static int charSet = 0;
static alt_rgb *rgbColors = (alt_rgb*)render.pal.rgb;
static bool blinkstate = false;
bool colorChanged = false, justChanged = false, staycolors = false, firstsize = true, ttfswitch = false, switch_output_from_ttf = false;
bool init_once = false, init_twice = false;

int menuwidth_atleast(int width), FileDirExistCP(const char *name), FileDirExistUTF8(std::string &localname, const char *name);
void AdjustIMEFontSize(void),refreshExtChar(void), initcodepagefont(void), change_output(int output), drawmenu(Bitu val), KEYBOARD_Clear(void), RENDER_Reset(void), DOSBox_SetSysMenu(void), GetMaxWidthHeight(unsigned int *pmaxWidth, unsigned int *pmaxHeight), SetWindowTransparency(int trans), resetFontSize(void), RENDER_CallBack( GFX_CallBackFunctions_t function );
bool isDBCSCP(void), InitCodePage(void), CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/), systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);
std::string GetDOSBoxXPath(bool withexe=false);

#if defined(C_SDL2)
void GFX_SetResizeable(bool enable);
SDL_Window * GFX_SetSDLSurfaceWindow(uint16_t width, uint16_t height);
#endif

Bitu OUTPUT_TTF_SetSize() {
    bool text=CurMode&&(CurMode->type==0||CurMode->type==2||CurMode->type==M_TEXT||IS_PC98_ARCH);
    if (text) {
        sdl.clip.x = sdl.clip.y = 0;
        sdl.draw.width = sdl.clip.w = ttf.cols*ttf.width;
        sdl.draw.height = sdl.clip.h = ttf.lins*ttf.height;
        ttf.inUse = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
        if (mainMenu.isVisible() && !ttf.fullScrn) {
            sdl.clip.y += mainMenu.menuBox.h;
            // hack
            ttf.offX = sdl.clip.x;
            ttf.offY = sdl.clip.y;
        }
#endif
    } else
        ttf.inUse = false;
#if defined(C_SDL2)
    int bx = 0, by = 0;
    if (sdl.displayNumber>0) {
        int displays = SDL_GetNumVideoDisplays();
        SDL_Rect bound;
        for( int i = 1; i <= displays; i++ ) {
            bound = SDL_Rect();
            SDL_GetDisplayBounds(i-1, &bound);
            if (i == sdl.displayNumber) {
                bx = bound.x;
                by = bound.y;
                break;
            }
        }
        SDL_DisplayMode dm;
        if (SDL_GetDesktopDisplayMode(sdl.displayNumber?sdl.displayNumber-1:0,&dm) == 0) {
            bx += (dm.w - sdl.draw.width - sdl.clip.x)/2;
            by += (dm.h - sdl.draw.height - sdl.clip.y)/2;
        }
    }
#endif
    if (ttf.inUse && ttf.fullScrn) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(C_SDL2)
        GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(maxWidth, maxHeight);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        if (posx != -2 || posy != -2) {
            if (sdl.displayNumber==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else SDL_SetWindowPosition(sdl.window, bx, by);
        }
#else
        sdl.surface = SDL_SetVideoMode(maxWidth, maxHeight, 32, SDL_SWSURFACE|
#if defined(WIN32)
        SDL_NOFRAME
#else
        SDL_FULLSCREEN
#endif
        );
#if defined(WIN32) && !defined(C_SDL2) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
        if (sdl.displayNumber>0) {
            xyp xy={0};
            xy.x=-1;
            xy.y=-1;
            curscreen=0;
            EnumDisplayMonitors(0, 0, EnumDispProc, reinterpret_cast<LPARAM>(&xy));
            HMONITOR monitor = MonitorFromRect(&monrect, MONITOR_DEFAULTTONEAREST);
            MONITORINFO info;
            info.cbSize = sizeof(MONITORINFO);
            GetMonitorInfo(monitor, &info);
            MoveWindow(GetHWND(), info.rcMonitor.left, info.rcMonitor.top, info.rcMonitor.right-info.rcMonitor.left, info.rcMonitor.bottom-info.rcMonitor.top, true);
        }
#endif
#endif
    } else {
#if defined(C_SDL2)
        GFX_SetResizeable(false);
        sdl.window = GFX_SetSDLSurfaceWindow(sdl.draw.width + sdl.clip.x, sdl.draw.height + sdl.clip.y);
        sdl.surface = sdl.window?SDL_GetWindowSurface(sdl.window):NULL;
        if (firstsize && (posx < 0 || posy < 0) && !(posx == -2 && posy == -2) && text) {
            firstsize=false;
            if (sdl.displayNumber==0) SDL_SetWindowPosition(sdl.window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
            else SDL_SetWindowPosition(sdl.window, bx, by);
        }
#else
        sdl.surface = SDL_SetVideoMode(sdl.draw.width + sdl.clip.x, sdl.draw.height + sdl.clip.y, 32, SDL_SWSURFACE);
#endif
    }
	if (!sdl.surface)
		E_Exit("SDL: Failed to create surface");
	SDL_ShowCursor(!ttf.fullScrn);
	sdl.active = true;

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    mainMenu.screenWidth = (size_t)sdl.surface->w;
    mainMenu.screenHeight = (size_t)sdl.surface->h;
    mainMenu.updateRect();
    mainMenu.setRedraw();
#endif

	AdjustIMEFontSize();

    return GFX_CAN_32 | GFX_SCALING;
}

void setVGADAC() {
    if (ttf.inUse&&CurMode&&IS_VGA_ARCH) {
        std::map<uint8_t,int> imap;
        for (uint8_t i = 0; i < 0x10; i++) {
            IO_ReadB(mem_readw(BIOS_VIDEO_PORT)+6);
            IO_WriteB(VGAREG_ACTL_ADDRESS, i+32);
            imap[i]=IO_ReadB(VGAREG_ACTL_READ_DATA);
            IO_WriteB(VGAREG_DAC_WRITE_ADDRESS, imap[i]);
            IO_WriteB(VGAREG_DAC_DATA, altBGR0[i].red>>2);
            IO_WriteB(VGAREG_DAC_DATA, altBGR0[i].green>>2);
            IO_WriteB(VGAREG_DAC_DATA, altBGR0[i].blue>>2);
        }
    }
}

/* NTS: EGA/VGA etc have at least 16 DOS colors. Check also CGA etc. */
bool setColors(const char *colorArray, int n) {
    if (IS_PC98_ARCH) return false;
    staycolors = strlen(colorArray) && *colorArray == '+';
    const char* nextRGB = colorArray + (staycolors?1:0);
	uint8_t * altPtr = (uint8_t *)altBGR1;
	int32_t rgbVal[4] = {-1,-1,-1,-1};
    for(int colNo = n > -1 ? n : 0; colNo < (n > -1 ? n + 1 : 16); colNo++) {
        if(sscanf(nextRGB, " ( %d , %d , %d)", &rgbVal[0], &rgbVal[1], &rgbVal[2]) == 3) {	// Decimal: (red,green,blue)
            for(int i = 0; i < 3; i++) {
                if(rgbVal[i] < 0 || rgbVal[i] > 255)
                    return false;
            }
            while(*nextRGB != ')')
                nextRGB++;
            nextRGB++;
        }
        else if(sscanf(nextRGB, " #%6x", ((uint32_t*)(&rgbVal[3]))) == 1) {	// Hexadecimal
            if(rgbVal[3] < 0 || rgbVal[3] > 0xFFFFFF)
                return false;
            for(int i = 2; i >= 0; i--) {
                rgbVal[i] = rgbVal[3] & 255;
                rgbVal[3] >>= 8;
            }
            nextRGB = strchr(nextRGB, '#') + 7;
        }
        else
            return false;

        altBGR0[colNo].blue = rgbVal[2];
        altBGR0[colNo].green = rgbVal[1];
        altBGR0[colNo].red = rgbVal[0];
        rgbColors[colNo].blue = (uint8_t)rgbVal[2];
        rgbColors[colNo].green = (uint8_t)rgbVal[1];
        rgbColors[colNo].red = (uint8_t)rgbVal[0];
        altBGR1[colNo].blue = rgbVal[2];
        altBGR1[colNo].green = rgbVal[1];
        altBGR1[colNo].red = rgbVal[0];
    }
    setVGADAC();
    colorChanged=justChanged=true;
	return true;
}

bool readTTFStyle(unsigned long& size, void*& font, FILE * fh) {
    long pos = ftell(fh);
    if (pos != -1L) {
        size = pos;
        font = malloc((size_t)size);
        if (font && !fseek(fh, 0, SEEK_SET) && fread(font, 1, (size_t)size, fh) == (size_t)size) {
            fclose(fh);
            return true;
        }
    }
    return false;
}

std::string failName="";
bool readTTF(const char *fName, bool bold, bool ital) {
	FILE * ttf_fh = NULL;
	std::string exepath = "";
	char ttfPath[1024];

	strcpy(ttfPath, fName);													// Try to load it from working directory
	strcat(ttfPath, ".ttf");
	ttf_fh = fopen(ttfPath, "rb");
	if (!ttf_fh) {
		strcpy(ttfPath, fName);
		ttf_fh = fopen(ttfPath, "rb");
	}
    if (!ttf_fh) {
        exepath=GetDOSBoxXPath();
        if (exepath.size()) {
            strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);	// Try to load it from where DOSBox-X was started
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(strrchr(strcpy(ttfPath, exepath.c_str()), CROSS_FILESPLIT)+1, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        std::string config_path;
        Cross::GetPlatformConfigDir(config_path);
        struct stat info;
        if (!stat(config_path.c_str(), &info) && (info.st_mode & S_IFDIR)) {
            strcpy(ttfPath, config_path.c_str());
            strcat(ttfPath, fName);
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(ttfPath, config_path.c_str());
                strcat(ttfPath, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        std::string res_path;
        Cross::GetPlatformResDir(res_path);
        struct stat info;
        if (!stat(res_path.c_str(), &info) && (info.st_mode & S_IFDIR)) {
            strcpy(ttfPath, res_path.c_str());
            strcat(ttfPath, fName);
            strcat(ttfPath, ".ttf");
            ttf_fh = fopen(ttfPath, "rb");
            if (!ttf_fh) {
                strcpy(ttfPath, res_path.c_str());
                strcat(ttfPath, fName);
                ttf_fh = fopen(ttfPath, "rb");
            }
        }
    }
    if (!ttf_fh) {
        std::string basedir = static_cast<Section_prop *>(control->GetSection("printer"))->Get_string("fontpath");
        if (basedir.back()!='\\' && basedir.back()!='/') basedir += CROSS_FILESPLIT;
        strcpy(ttfPath, basedir.c_str());
        strcat(ttfPath, fName);
        strcat(ttfPath, ".ttf");
        ttf_fh = fopen(ttfPath, "rb");
        if (!ttf_fh) {
            strcpy(ttfPath, basedir.c_str());
            strcat(ttfPath, fName);
            ttf_fh = fopen(ttfPath, "rb");
        }
    }
    if (!ttf_fh) {
        char fontdir[300];
#if defined(WIN32)
        strcpy(fontdir, "C:\\WINDOWS\\fonts\\");
        struct stat wstat;
        if(stat(fontdir,&wstat) || !(wstat.st_mode & S_IFDIR)) {
            char dir[MAX_PATH];
            if (GetWindowsDirectory(dir, MAX_PATH)) {
                strcpy(fontdir, dir);
                strcat(fontdir, "\\fonts\\");
            }
        }
#elif defined(LINUX)
        strcpy(fontdir, "/usr/share/fonts/");
#elif defined(MACOSX)
        strcpy(fontdir, "/Library/Fonts/");
#else
        strcpy(fontdir, "/fonts/");
#endif
        strcpy(ttfPath, fontdir);
        strcat(ttfPath, fName);
        strcat(ttfPath, ".ttf");
        ttf_fh = fopen(ttfPath, "rb");
        if (!ttf_fh) {
            strcpy(ttfPath, fontdir);
            strcat(ttfPath, fName);
            ttf_fh = fopen(ttfPath, "rb");
#if defined(LINUX) || defined(MACOSX)
            if (!ttf_fh) {
#if defined(LINUX)
                strcpy(fontdir, "/usr/share/fonts/truetype/");
#else
                strcpy(fontdir, "/System/Library/Fonts/");
#endif
                strcpy(ttfPath, fontdir);
                strcat(ttfPath, fName);
                strcat(ttfPath, ".ttf");
                ttf_fh = fopen(ttfPath, "rb");
                if (!ttf_fh) {
                    strcpy(ttfPath, fontdir);
                    strcat(ttfPath, fName);
                    ttf_fh = fopen(ttfPath, "rb");
                    if (!ttf_fh) {
                        std::string in;
#if defined(LINUX)
                        in = "~/.fonts/";
#else
                        in = "~/Library/Fonts/";
#endif
                        Cross::ResolveHomedir(in);
                        strcpy(ttfPath, in.c_str());
                        strcat(ttfPath, fName);
                        strcat(ttfPath, ".ttf");
                        ttf_fh = fopen(ttfPath, "rb");
                        if (!ttf_fh) {
                            strcpy(ttfPath, in.c_str());
                            strcat(ttfPath, fName);
                            ttf_fh = fopen(ttfPath, "rb");
                        }
                    }
                }
            }
#endif
        }
    }
    if (ttf_fh) {
        if (!fseek(ttf_fh, 0, SEEK_END)) {
            if (bold && ital && readTTFStyle(ttfSizebi, ttfFontbi, ttf_fh))
                return true;
            else if (bold && !ital && readTTFStyle(ttfSizeb, ttfFontb, ttf_fh))
                return true;
            else if (!bold && ital && readTTFStyle(ttfSizei, ttfFonti, ttf_fh))
                return true;
            else if (readTTFStyle(ttfSize, ttfFont, ttf_fh))
                return true;
        }
        fclose(ttf_fh);
    }
    if (!failName.size()||failName.compare(fName)) {
        failName=std::string(fName);
        bool alllower=true;
        for (size_t i=0; i<strlen(fName); i++) {if ((unsigned char)fName[i]>0x7f) alllower=false;break;}
        std::string message=alllower?("Could not load font file: "+std::string(fName)+(strlen(fName)<5||strcasecmp(fName+strlen(fName)-4, ".ttf")?".ttf":"")):"Could not load the specified font file.";
        systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
    }
	return false;
}

void SetBlinkRate(Section_prop* section) {
    const char * blinkCstr = section->Get_string("blinkc");
    unsigned int num=-1;
    if (!strcasecmp(blinkCstr, "false")||!strcmp(blinkCstr, "-1")) blinkCursor = -1;
    else if (1==sscanf(blinkCstr,"%u",&num)&&int(num)>=0&&int(num)<=7) blinkCursor = num;
    else blinkCursor = IS_PC98_ARCH?6:4; // default cursor blinking is slower on PC-98 systems
}

void CheckTTFLimit() {
    ttf.lins = MAX(24, MIN(IS_VGA_ARCH?txtMaxLins:60, (int)ttf.lins));
    ttf.cols = MAX(40, MIN(IS_VGA_ARCH?txtMaxCols:160, (int)ttf.cols));
    if (ttf.cols*ttf.lins>16384) {
        if (lastset==1) {
            ttf.lins=16384/ttf.cols;
            SetVal("ttf", "lins", std::to_string(ttf.lins));
        } else if (lastset==2) {
            ttf.cols=16384/ttf.lins;
            SetVal("ttf", "cols", std::to_string(ttf.cols));
        } else {
            ttf.lins = 25;
            ttf.cols = 80;
        }
    }
}

int setTTFMap(bool changecp) {
    char text[2];
    uint16_t uname[4], wcTest[256];
    int cp = dos.loaded_codepage;
    for (int i = 0; i < 256; i++) {
        text[0]=i;
        text[1]=0;
        uname[0]=0;
        uname[1]=0;
        if (cp == 932 && (halfwidthkana || IS_JEGA_ARCH)) forceswk=true;
        if (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951) dos.loaded_codepage = 437;
        if (CodePageGuestToHostUTF16(uname,text)) {
            wcTest[i] = uname[1]==0?uname[0]:i;
            if (cp == 932 && lowboxdrawmap.find(i)!=lowboxdrawmap.end() && TTF_GlyphIsProvided(ttf.SDL_font, wcTest[i]))
                cpMap[i] = wcTest[i];
        }
        forceswk=false;
        if (cp == 932 || cp == 936 || cp == 949 || cp == 950 || cp == 951) dos.loaded_codepage = cp;
    }
    uint16_t unimap;
    int notMapped = 0;
    for (int y = ((cpMap[1]!=1&&cpMap[1]!=0x263A)||cp==867||(customcp&&(dos.loaded_codepage==customcp||changecp))||(altcp&&(dos.loaded_codepage==altcp||changecp))?0:8); y < 16; y++)
        for (int x = 0; x < 16; x++) {
            if (y<8 && (wcTest[y*16+x] == y*16+x || wcTest[y*16+x] == cp437_to_unicode[y*16+x])) unimap = cpMap_copy[y*16+x];
            else unimap = wcTest[y*16+x];
            if (!TTF_GlyphIsProvided(ttf.SDL_font, unimap)) {
                cpMap[y*16+x] = 0;
                notMapped++;
                LOG_MSG("Unmapped character: %3d - %4x", y*16+x, unimap);
            } else
                cpMap[y*16+x] = unimap;
        }
    if (eurAscii != -1 && TTF_GlyphIsProvided(ttf.SDL_font, 0x20ac))
        cpMap[eurAscii] = 0x20ac;
    return notMapped;
}

int setTTFCodePage() {
    if (!copied) {
        memcpy(cpMap_copy,cpMap,sizeof(cpMap[0])*256);
        copied=true;
    }
    int cp = dos.loaded_codepage;
    if (IS_PC98_ARCH) {
        static_assert(sizeof(cpMap[0])*256 >= sizeof(cpMap_PC98), "sizeof err 1");
        static_assert(sizeof(cpMap[0]) == sizeof(cpMap_PC98[0]), "sizeof err 2");
        memcpy(cpMap,cpMap_PC98,sizeof(cpMap[0])*256);
        return 0;
    }

    if (cp) {
        LOG_MSG("Loaded system codepage: %d\n", cp);
        int notMapped = setTTFMap(true);
        if (strcmp(RunningProgram, "LOADLIN") && !dos_kernel_disabled)
            initcodepagefont();
#if defined(WIN32) && !defined(HX_DOS)
        DOSBox_SetSysMenu();
#endif
        if(IS_JEGA_ARCH) memcpy(cpMap,cpMap_AX,sizeof(cpMap[0])*32);
        if (cp == 932 && halfwidthkana) resetFontSize();
        refreshExtChar();
        return notMapped;
    } else
        return -1;
}

void GFX_SelectFontByPoints(int ptsize) {
	bool initCP = true;
	if (ttf.SDL_font != 0) {
		TTF_CloseFont(ttf.SDL_font);
		initCP = false;
	}
	if (ttf.SDL_fontb != 0) TTF_CloseFont(ttf.SDL_fontb);
	if (ttf.SDL_fonti != 0) TTF_CloseFont(ttf.SDL_fonti);
	if (ttf.SDL_fontbi != 0) TTF_CloseFont(ttf.SDL_fontbi);
	SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFont, (int)ttfSize);
	ttf.SDL_font = TTF_OpenFontRW(rwfont, 1, ptsize);
    if (ttfSizeb>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontb, (int)ttfSizeb);
        ttf.SDL_fontb = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontb = NULL;
    if (ttfSizei>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFonti, (int)ttfSizei);
        ttf.SDL_fonti = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fonti = NULL;
    if (ttfSizebi>0) {
        SDL_RWops *rwfont = SDL_RWFromConstMem(ttfFontbi, (int)ttfSizebi);
        ttf.SDL_fontbi = TTF_OpenFontRW(rwfont, 1, ptsize);
    } else
        ttf.SDL_fontbi = NULL;
    ttf.pointsize = ptsize;
	TTF_GlyphMetrics(ttf.SDL_font, 65, NULL, NULL, NULL, NULL, &ttf.width);
	ttf.height = TTF_FontAscent(ttf.SDL_font)-TTF_FontDescent(ttf.SDL_font);
	if (ttf.fullScrn) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
		ttf.offX = (maxWidth-ttf.width*ttf.cols)/2;
		ttf.offY = (maxHeight-ttf.height*ttf.lins)/2;
	}
	else
		ttf.offX = ttf.offY = 0;
	if (initCP) setTTFCodePage();
}

void SetOutputSwitch(const char *outputstr) {
#if C_DIRECT3D
        if (!strcasecmp(outputstr, "direct3d") || !strcasecmp(outputstr, "auto"))
            switchoutput = 6;
        else
#endif
#if C_OPENGL
        if (!strcasecmp(outputstr, "openglpp"))
            switchoutput = 5;
        else if (!strcasecmp(outputstr, "openglnb"))
            switchoutput = 4;
        else if (!strcasecmp(outputstr, "opengl")||!strcasecmp(outputstr, "openglnq") || !strcasecmp(outputstr, "auto"))
            switchoutput = 3;
        else
#endif
        if (!strcasecmp(outputstr, "surface"))
            switchoutput = 0;
        else
            switchoutput = -1;
}

void OUTPUT_TTF_Select(int fsize) {
    if (!initttf&&TTF_Init()) {											// Init SDL-TTF
        std::string message = "Could not init SDL-TTF: " + std::string(SDL_GetError());
        systemmessagebox("Error", message.c_str(), "ok","error", 1);
        change_output(switchoutput == -1 ? 0 : switchoutput);
        return;
    }
    int fontSize = 0;
    int winPerc = 0;
    if (fsize==3)
        winPerc = 100;
    else if (fsize>=MIN_PTSIZE)
        fontSize = fsize;
    else {
        Section_prop * ttf_section=static_cast<Section_prop *>(control->GetSection("ttf"));
        const char * fName = ttf_section->Get_string("font");
        const char * fbName = ttf_section->Get_string("fontbold");
        const char * fiName = ttf_section->Get_string("fontital");
        const char * fbiName = ttf_section->Get_string("fontboit");
        LOG_MSG("SDL: TTF activated %s", fName);
        force_conversion = true;
        int cp = dos.loaded_codepage;
        bool trysgf = false;
        if (!*fName) {
            std::string mtype(static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_string("machine"));
            if (IS_PC98_ARCH||mtype.substr(0, 4)=="pc98"||(!notrysgf&&InitCodePage()&&isDBCSCP())) trysgf = true;
        }
        force_conversion = false;
        dos.loaded_codepage = cp;
        if (trysgf) failName = "SarasaGothicFixed";
        if ((!*fName&&!trysgf)||!readTTF(*fName?fName:failName.c_str(), false, false)) {
            ttfFont = DOSBoxTTFbi;
            ttfFontb = NULL;
            ttfFonti = NULL;
            ttfFontbi = NULL;
            ttfSize = sizeof(DOSBoxTTFbi);
            ttfSizeb = ttfSizei = ttfSizebi = 0;
            ttf.DOSBox = true;
            std::string message="";
            if (*fbName)
                message="A valid ttf.font setting is required for the ttf.fontbold setting: "+std::string(fbName);
            else if (*fiName)
                message="A valid ttf.font setting is required for the ttf.fontital setting: "+std::string(fiName);
            else if (*fbiName)
                message="A valid ttf.font setting is required for the ttf.fontboit setting: "+std::string(fbiName);
            if (fsize==0&&message.size())
                systemmessagebox("Warning", message.c_str(), "ok","warning", 1);
        } else {
            if (!*fbName||!readTTF(fbName, true, false)) {
                ttfFontb = NULL;
                ttfSizeb = 0;
            }
            if (!*fiName||!readTTF(fiName, false, true)) {
                ttfFonti = NULL;
                ttfSizei = 0;
            }
            if (!*fbiName||!readTTF(fbiName, true, true)) {
                ttfFontbi = NULL;
                ttfSizebi = 0;
            }
        }
        const char * colors = ttf_section->Get_string("colors");
        staycolors = strlen(colors) && *colors == '+'; // Always switch to preset value when '+' is specified
        if ((*colors && (!init_once || !init_twice))|| staycolors) {
            if (!setColors(colors,-1)) {
                LOG_MSG("Incorrect color scheme: %s", colors);
                //setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);
            }
            if(ttf.inUse) init_once = true;
            if(!init_once) init_once = true;
            else if(!init_twice) init_twice = true;
        }
        else if (IS_EGAVGA_ARCH) {
            alt_rgb *rgbcolors = (alt_rgb*)render.pal.rgb;
            std::string str = "";
            char value[30];
            for (int i = 0; i < 16; i++) {
                sprintf(value,"#%02x%02x%02x",rgbcolors[i].red,rgbcolors[i].green,rgbcolors[i].blue);
                str+=std::string(value)+" ";
            }
            if (str.size()) {
                setColors(str.c_str(),-1);
                colorChanged=justChanged=false;
            }
        }
        SetBlinkRate(ttf_section);
        const char *wpstr=ttf_section->Get_string("wp");
        wpType=0;
        wpVersion=0;
        if (strlen(wpstr)>1) {
            if (!strncasecmp(wpstr, "WP", 2)) wpType=1;
            else if (!strncasecmp(wpstr, "WS", 2)) wpType=2;
            else if (!strncasecmp(wpstr, "XY", 2)) wpType=3;
            else if (!strncasecmp(wpstr, "FE", 2)) wpType=4;
            if (strlen(wpstr)>2&&wpstr[2]>='1'&&wpstr[2]<='9') wpVersion=wpstr[2]-'0';
        }
        wpBG = ttf_section->Get_int("wpbg");
        wpFG = ttf_section->Get_int("wpfg");
        if (wpFG<0) wpFG = 7;
        winPerc = ttf_section->Get_int("winperc");
        if (winPerc>100||(fsize==2&&GFX_IsFullscreen())||(fsize!=1&&fsize!=2&&(control->opt_fullscreen||static_cast<Section_prop *>(control->GetSection("sdl"))->Get_bool("fullscreen")))) winPerc=100;
        else if (winPerc<25) winPerc=25;
#if defined(HX_DOS)
        winPerc=100;
#else
        if ((fsize==1||switchttf)&&winPerc==100) {
            winPerc=60;
            if (switchttf&&GFX_IsFullscreen()) GFX_SwitchFullScreen();
        }
#endif
        fontSize = ttf_section->Get_int("ptsize");
        char512 = ttf_section->Get_bool("char512");
        showbold = ttf_section->Get_bool("bold");
        showital = ttf_section->Get_bool("italic");
        showline = ttf_section->Get_bool("underline");
        showsout = ttf_section->Get_bool("strikeout");
        printfont = ttf_section->Get_bool("printfont");
        dbcs_sbcs = ttf_section->Get_bool("autodbcs");
        autoboxdraw = ttf_section->Get_bool("autoboxdraw");
        halfwidthkana = ttf_section->Get_bool("halfwidthkana");
        ttf_dosv = ttf_section->Get_bool("dosvfunc");
        SetOutputSwitch(ttf_section->Get_string("outputswitch"));
        rtl = ttf_section->Get_bool("righttoleft");
        ttf.lins = ttf_section->Get_int("lins");
        ttf.cols = ttf_section->Get_int("cols");
        if (fsize&&!IS_PC98_ARCH&&!IS_EGAVGA_ARCH) ttf.lins = 25;
        if ((!CurMode||CurMode->type!=M_TEXT)&&!IS_PC98_ARCH) {
            if (ttf.cols<1) ttf.cols=80;
            if (ttf.lins<1) ttf.lins=25;
            CheckTTFLimit();
        } else if (firstset) {
            bool alter_vmode=false;
            uint16_t c=0, r=0;
            if (IS_PC98_ARCH) {
                c=80;
                r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
            } else {
                c=strcmp(RunningProgram, "LOADLIN")?real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS):80;
                r=(uint16_t)(IS_EGAVGA_ARCH&&strcmp(RunningProgram, "LOADLIN")?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
            }
            if (ttf.lins<1||ttf.cols<1)	{
                if (ttf.cols<1)
                    ttf.cols = c;
                else {
                    ttf.cols = MAX(40, MIN(txtMaxCols, (int)ttf.cols));
                    if (ttf.cols != c) alter_vmode = true;
                }
                if (ttf.lins<1)
                    ttf.lins = r;
                else {
                    ttf.lins = MAX(24, MIN(txtMaxLins, (int)ttf.lins));
                    if (ttf.lins != r) alter_vmode = true;
                }
            } else {
                CheckTTFLimit();
                if (ttf.cols != c || ttf.lins != r) alter_vmode = true;
            }
            if (alter_vmode) {
                for (Bitu i = 0; ModeList_VGA[i].mode <= 7; i++) {								// Set the cols and lins in video mode 2,3,7
                    ModeList_VGA[i].twidth = ttf.cols;
                    ModeList_VGA[i].theight = ttf.lins;
                }
                if (!IS_PC98_ARCH) {
                    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_COLS,ttf.cols);
                    if (IS_EGAVGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,ttf.lins-1);
                }
            }
            firstset=false;
        } else {
            uint16_t c=0, r=0;
            if (IS_PC98_ARCH) {
                c=80;
                r=real_readb(0x60,0x113) & 0x01 ? 25 : 20;
            } else {
                c=strcmp(RunningProgram, "LOADLIN")?real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS):80;
                r=(uint16_t)(IS_EGAVGA_ARCH&&strcmp(RunningProgram, "LOADLIN")?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24)+1;
            }
            ttf.cols=c;
            ttf.lins=r;
        }
    }

    if (winPerc == 100) {
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
        lastmenu = menu.toggle;
        menu.toggle=false;
        NonUserResizeCounter=1;
#if !defined(C_SDL2)
        SDL1_hax_SetMenu(NULL);
#endif
        RENDER_CallBack( GFX_CallBackReset );
#endif
#if defined (WIN32)
        DOSBox_SetSysMenu();
#endif
        ttf.fullScrn = true;
    } else
        ttf.fullScrn = false;

    unsigned int maxWidth, maxHeight;
    GetMaxWidthHeight(&maxWidth, &maxHeight);

#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW /* SDL drawn menus */
    maxHeight -= mainMenu.menuBarHeightBase * 2;
#endif

#if defined(WIN32)
    if (!ttf.fullScrn) {																			// 3D borders
        maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
        maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
    }
#endif
    int	curSize = fontSize>=MIN_PTSIZE?fontSize:30;													// no clear idea what would be a good starting value
    int lastGood = -1;
    int trapLoop = 0;

    if (fontSize<MIN_PTSIZE) {
        while (curSize != lastGood) {
            GFX_SelectFontByPoints(curSize);
            if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {				// if it fits on screen
                lastGood = curSize;
                float coveredPerc = float(100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight);
                if (trapLoop++ > 4 && coveredPerc <= winPerc)										// we can get into a +/-/+/-... loop!
                    break;
                curSize = (int)(curSize*sqrt((float)winPerc/coveredPerc));							// rounding down is ok
                if (curSize < MIN_PTSIZE)															// minimum size = 9
                    curSize = MIN_PTSIZE;
            } else if (--curSize < MIN_PTSIZE)														// silly, but OK, one never can tell..
                E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
        }
        if (ttf.DOSBox && curSize > MIN_PTSIZE)														// make it even for DOSBox-X internal font (a bit nicer)
            curSize &= ~1;
    }
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
resize1:
#endif
    GFX_SelectFontByPoints(curSize);
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
    if (!ttf.fullScrn && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0) {
        if (ttf.cols*ttf.width > maxWidth || ttf.lins*ttf.height > maxHeight) E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
        curSize++;
        goto resize1;
    }
#endif
	GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(WIN32)
	if (!ttf.fullScrn) {
		maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
		maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
	}
#endif
resize2:
    if ((fontSize>=MIN_PTSIZE && 100*ttf.cols*ttf.width/maxWidth*ttf.lins*ttf.height/maxHeight > 100) || (lesssize && (ttf.cols*ttf.width>maxWidth || ttf.lins*ttf.height>maxHeight))) {
        if (lesssize && curSize > MIN_PTSIZE) {
            curSize--;
            GFX_SelectFontByPoints(curSize);
            goto resize2;
        }
        E_Exit("Cannot accommodate a window for %dx%d", ttf.lins, ttf.cols);
    }
    if (ttf.SDL_font && ttf.width) {
        int widthb, widthm, widthx, width0, width1, width9;
        widthb = widthm = widthx = width0 = width1 = width9 = 0;
        TTF_GlyphMetrics(ttf.SDL_font, 'B', NULL, NULL, NULL, NULL, &widthb);
        TTF_GlyphMetrics(ttf.SDL_font, 'M', NULL, NULL, NULL, NULL, &widthm);
        TTF_GlyphMetrics(ttf.SDL_font, 'X', NULL, NULL, NULL, NULL, &widthx);
        TTF_GlyphMetrics(ttf.SDL_font, '0', NULL, NULL, NULL, NULL, &width0);
        if (abs(ttf.width-widthb)>1 || abs(ttf.width-widthm)>1 || abs(ttf.width-widthx)>1 || abs(ttf.width-width0)>1) LOG_MSG("TTF: The loaded font is not monospaced.");
        int cp=dos.loaded_codepage;
        if (!cp) InitCodePage();
        if ((IS_PC98_ARCH || isDBCSCP()) && dbcs_sbcs) {
            TTF_GlyphMetrics(ttf.SDL_font, 0x4E00, NULL, NULL, NULL, NULL, &width1);
            TTF_GlyphMetrics(ttf.SDL_font, 0x4E5D, NULL, NULL, NULL, NULL, &width9);
            if (width1 <= ttf.width || width9 <= ttf.width) LOG_MSG("TTF: The loaded font may not support DBCS characters.");
            else if ((ttf.width*2 != width1 || ttf.width*2 != width9) && ttf.width == widthb && ttf.width == widthm && ttf.width == widthx && ttf.width == width0) LOG_MSG("TTF: The loaded font is not monospaced.");
        }
        dos.loaded_codepage = cp;
    }
    resetreq=false;
    sdl.desktop.want_type = SCREEN_TTF;
}

void ResetTTFSize(Bitu /*val*/) {
    resetFontSize();
}

void processWP(uint8_t *pcolorBG, uint8_t *pcolorFG) {
    charSet = 0;
    if (!wpType) return;
    uint8_t colorBG = *pcolorBG, colorFG = *pcolorFG;
    int style = 0;
    if (CurMode->mode == 7) {														// Mono (Hercules)
        style = showline && (colorFG&7) == 1 ? TTF_STYLE_UNDERLINE : TTF_STYLE_NORMAL;
        if ((colorFG&0xa) == colorFG && (colorBG&15) == 0)
            colorFG = 8;
        else if (colorFG&7)
            colorFG |= 7;
    }
    else if (wpType == 1) {															// WordPerfect
        if (showital && colorFG == 0xe && (colorBG&15) == (wpBG > -1 ? wpBG : 1)) {
            style = TTF_STYLE_ITALIC;
            colorFG = wpFG;
        }
        else if (showline && (colorFG == 1 || colorFG == wpFG+8) && (colorBG&15) == 7) {
            style = TTF_STYLE_UNDERLINE;
            colorBG = wpBG > -1 ? wpBG : 1;
            colorFG = colorFG == 1 ? wpFG : (wpFG+8);
        }
        else if (showsout && colorFG == 0 && (colorBG&15) == 3) {
            style = TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : 1;
            colorFG = wpFG;
        }
    } else if (wpType == 2) {														// WordStar
        if (colorBG&8) {
            if (showline && colorBG&1)
                style |= TTF_STYLE_UNDERLINE;
            if (showital && colorBG&2)
                style |= TTF_STYLE_ITALIC;
            if (showsout && colorBG&4)
                style |= TTF_STYLE_STRIKETHROUGH;
            if (style)
                colorBG = wpBG > -1 ? wpBG : 0;
        }
    } else if (wpType == 3) {														// XyWrite
        if (showital && (colorFG == 10 || colorFG == 14) && colorBG != 12 && !(!showline && colorBG == 3)) {
            style = TTF_STYLE_ITALIC;
            if (colorBG == 3) {
                style |= TTF_STYLE_UNDERLINE;
                colorBG = wpBG > -1 ? wpBG : 1;
            }
            colorFG = colorFG == 10 ? wpFG : (wpFG+8);
        }
        else if (showline && (colorFG == 3 || colorFG == 0xb)) {
            style = TTF_STYLE_UNDERLINE;
            colorFG = colorFG == 3 ? wpFG : (wpFG+8);
        }
        else if (!showsout || colorBG != 4)
            style = TTF_STYLE_NORMAL;
        if (showsout && colorBG == 4 && colorFG != 12 && colorFG != 13) {
            style |= TTF_STYLE_STRIKETHROUGH;
            colorBG = wpBG > -1 ? wpBG : (wpVersion < 4 ? 0 : 1);
            if (colorFG != wpFG+8) colorFG = wpFG;
        }
    } else if (wpType == 4 && colorFG == wpFG) {									// FastEdit
        if (colorBG == 1 || colorBG == 5 || colorBG == 6 || colorBG == 7 || colorBG == 11 || colorBG == 12 || colorBG == 13 || colorBG == 15)
            colorFG += 8;
        if (colorBG == 2 || colorBG == 5) {
            if (showital) style |= TTF_STYLE_ITALIC;
        } else if (colorBG == 3 || colorBG == 6) {
            if (showline) style |= TTF_STYLE_UNDERLINE;
        } else if (colorBG == 4 || colorBG == 7) {
            if (showsout) style |= TTF_STYLE_STRIKETHROUGH;
        } else if (colorBG == 8 || colorBG == 11) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showline?TTF_STYLE_UNDERLINE:0);
        } else if (colorBG == 9 || colorBG == 12) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        } else if (colorBG == 10 || colorBG == 13) {
            style |= (showline?TTF_STYLE_UNDERLINE:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        } else if (colorBG == 14 || colorBG == 15) {
            style |= (showital?TTF_STYLE_ITALIC:0) | (showline?TTF_STYLE_UNDERLINE:0) | (showsout?TTF_STYLE_STRIKETHROUGH:0);
        }
        colorBG = wpBG;
    }
    if (char512 && wpType == 1) {
        if (wpExtChar && (colorFG&8)) {	// WordPerfect high bit = second character set (if 512char active)
            charSet = 1;
            colorFG &= 7;
        }
    }
    if (showbold && (colorFG == wpFG+8 || (wpType == 1 && (wpVersion < 1 || wpVersion > 5 ) && colorFG == 3 && (colorBG&15) == (wpBG > -1 ? wpBG : 1)))) {
        if (ttf.SDL_fontbi != 0 || !(style&TTF_STYLE_ITALIC) || wpType == 4) style |= TTF_STYLE_BOLD;
        if ((ttf.SDL_fontbi != 0 && (style&TTF_STYLE_ITALIC)) || (ttf.SDL_fontb != 0 && !(style&TTF_STYLE_ITALIC)) || wpType == 4) colorFG = wpFG;
    }
    if (style)
        TTF_SetFontStyle(ttf.SDL_font, style);
    else
        TTF_SetFontStyle(ttf.SDL_font, TTF_STYLE_NORMAL);
    *pcolorBG = colorBG;
    *pcolorFG = colorFG;
}

bool hasfocus = true, lastfocus = true;
void GFX_EndTextLines(bool force) {
    if (!force&&!IS_PC98_ARCH&&(!CurMode||CurMode->type!=M_TEXT)) return;
#if DOSBOXMENU_TYPE == DOSBOXMENU_SDLDRAW
    if (!ttf.fullScrn) {
        mainMenu.setRedraw();
        GFX_DrawSDLMenu(mainMenu,mainMenu.display_list);
    }
#endif
    static uint8_t bcount = 0;
	Uint16 unimap[txtMaxCols+1];							// max+1 characters in a line
	int xmin = ttf.cols;									// keep track of changed area
	int ymin = ttf.lins;
	int xmax = -1;
	int ymax = -1;
	ttf_cell *curAC = curAttrChar;							// pointer to old/current buffer
	ttf_cell *newAC = newAttrChar;							// pointer to new/changed buffer

	if (ttf.fullScrn && (ttf.offX || ttf.offY)) {
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = 0; rect->y = 0; rect->w = maxWidth; rect->h = ttf.offY;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->w = ttf.offX; rect->h = maxHeight;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->x = 0; rect->y = sdl.draw.height + ttf.offY; rect->w = maxWidth; rect->h = maxHeight - sdl.draw.height - ttf.offY;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
        rect->x = sdl.draw.width + ttf.offX; rect->y = 0; rect->w = maxWidth - sdl.draw.width - ttf.offX; rect->h = maxHeight;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
    }

	if (ttf.cursor < ttf.cols*ttf.lins)	// hide/restore (previous) cursor-character if we had one

//		if (cursor_enabled && (vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15))
//		if (ttf.cursor != vga.draw.cursor.address>>1 || (vga.draw.cursor.enabled !=  cursor_enabled) || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15)
		if ((ttf.cursor != vga.draw.cursor.address>>1) || vga.draw.cursor.sline > vga.draw.cursor.eline || vga.draw.cursor.sline > 15) {
			curAC[ttf.cursor] = newAC[ttf.cursor];
            curAC[ttf.cursor].chr ^= 0xf0f0;	// force redraw (differs)
        }

	lastfocus = hasfocus;
	hasfocus =
#if defined(C_SDL2)
	SDL_GetWindowFlags(sdl.window) & SDL_WINDOW_INPUT_FOCUS
#else
	SDL_GetAppState () & SDL_APPINPUTFOCUS
#endif
	? true : false;
	bool focuschanged = lastfocus != hasfocus, noframe = !menu.toggle || ttf.fullScrn;
	ttf_textClip.h = ttf.height;
	ttf_textClip.y = 0;
	for (unsigned int y = 0; y < ttf.lins; y++) {
		bool draw = false;
		ttf_textRect.y = ttf.offY+y*ttf.height;
		for (unsigned int x = 0; x < ttf.cols; x++) {
			if (((newAC[x] != curAC[x] || newAC[x].selected != curAC[x].selected || (colorChanged && (justChanged || draw)) || force) && (!newAC[x].skipped || force)) || (!y && focuschanged && noframe)) {
				draw = true;
				xmin = min((int)x, xmin);
				ymin = min((int)y, ymin);
				ymax = y;

				bool dw = false;
				const int x1 = x;
				uint8_t colorBG = newAC[x].bg;
				uint8_t colorFG = newAC[x].fg;
				processWP(&colorBG, &colorFG);
                if (newAC[x].selected) {
                    uint8_t color = colorBG;
                    colorBG = colorFG;
                    colorFG = color;
                }
                bool colornul = staycolors || (IS_VGA_ARCH && (altBGR1[colorBG&15].red > 4 || altBGR1[colorBG&15].green > 4 || altBGR1[colorBG&15].blue > 4 || altBGR1[colorFG&15].red > 4 || altBGR1[colorFG&15].green > 4 || altBGR1[colorFG&15].blue > 4) && rgbColors[colorBG].red < 5 && rgbColors[colorBG].green < 5 && rgbColors[colorBG].blue < 5 && rgbColors[colorFG].red < 5 && rgbColors[colorFG].green <5 && rgbColors[colorFG].blue < 5);
				ttf_textRect.x = ttf.offX+(rtl?(ttf.cols-x-1):x)*ttf.width;
				ttf_bgColor.r = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].red:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].red:rgbColors[colorBG].red);
				ttf_bgColor.g = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].green:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].green:rgbColors[colorBG].green);
				ttf_bgColor.b = !y&&!hasfocus&&noframe?altBGR0[colorBG&15].blue:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue);
				ttf_fgColor.r = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].red:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].red:rgbColors[colorFG].red);
				ttf_fgColor.g = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].green:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].green:rgbColors[colorFG].green);
				ttf_fgColor.b = !y&&!hasfocus&&noframe?altBGR0[colorFG&15].blue:(colornul||(colorChanged&&!IS_VGA_ARCH)?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue);

                if (newAC[x].unicode) {
                    dw = newAC[x].doublewide;
                    unimap[x-x1] = newAC[x].chr;
                    curAC[x] = newAC[x];
                    x++;

                    if (dw) {
                        curAC[x] = newAC[x];
                        x++;
                        if (rtl) ttf_textRect.x -= ttf.width;
                    }
                }
                else {
                    uint8_t ascii = newAC[x].chr&255;
                    if(ttf_dosv && ascii == 0x5c)
                        ascii = 0x9d;
                    curAC[x] = newAC[x];
                    if (ascii > 175 && ascii < 179 && !IS_PC98_ARCH && !IS_JEGA_ARCH && dos.loaded_codepage != 864 && dos.loaded_codepage != 868 && dos.loaded_codepage != 874 && dos.loaded_codepage != 3021 && !(dos.loaded_codepage == 932 && halfwidthkana) && (dos.loaded_codepage < 1250 || dos.loaded_codepage > 1258) && !(altcp && dos.loaded_codepage == altcp) && !(customcp && dos.loaded_codepage == customcp)) {	// special: shade characters 176-178 unless PC-98
                        ttf_bgColor.b = (ttf_bgColor.b*(179-ascii) + ttf_fgColor.b*(ascii-175))>>2;
                        ttf_bgColor.g = (ttf_bgColor.g*(179-ascii) + ttf_fgColor.g*(ascii-175))>>2;
                        ttf_bgColor.r = (ttf_bgColor.r*(179-ascii) + ttf_fgColor.r*(ascii-175))>>2;
                        unimap[x-x1] = ' ';							// shaded space
                    } else {
                        unimap[x-x1] = cpMap[ascii+charSet*256];
                    }

                    x++;
                }

                {
                    unimap[x-x1] = 0;
                    xmax = max((int)(x-1), xmax);

                    SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
                    ttf_textClip.w = (x-x1)*ttf.width;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
                    x--;
                }
			}
		}
		curAC += ttf.cols;
		newAC += ttf.cols;
	}
    if (!force) justChanged = false;
    // NTS: Additional fix is needed for the cursor in PC-98 mode; also expect further cleanup
	bcount++;
	if (vga.draw.cursor.enabled && vga.draw.cursor.sline <= vga.draw.cursor.eline && vga.draw.cursor.sline <= 16 && blinkCursor) {	// Draw cursor?
		unsigned int newPos = (unsigned int)(vga.draw.cursor.address>>1);
		if (newPos < ttf.cols*ttf.lins) {								// If on screen
			int y = newPos/ttf.cols;
			int x = newPos%ttf.cols;
			if (IS_JEGA_ARCH) {
                ccount++;
                if (ccount>=0x20) {
                    ccount=0;
                    vga.draw.cursor.count++;
                }
			} else
				vga.draw.cursor.count++;

			if (blinkCursor>-1)
				vga.draw.cursor.blinkon = (vga.draw.cursor.count & 1<<blinkCursor) ? true : false;

			if (ttf.cursor != newPos || vga.draw.cursor.sline != prev_sline || ((blinkstate != vga.draw.cursor.blinkon) && blinkCursor>-1)) {				// If new position or shape changed, force draw
				if (blinkCursor>-1 && blinkstate == vga.draw.cursor.blinkon) {
					vga.draw.cursor.count = 4;
					vga.draw.cursor.blinkon = true;
				}
				prev_sline = vga.draw.cursor.sline;
				xmin = min(x, xmin);
				xmax = max(x, xmax);
				ymin = min(y, ymin);
				ymax = max(y, ymax);
			}
			blinkstate = vga.draw.cursor.blinkon;
			ttf.cursor = newPos;
//			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax  && (GetTickCount()&0x400))	// If overdrawn previously (or new shape)
			if (x >= xmin && x <= xmax && y >= ymin && y <= ymax && !newAttrChar[ttf.cursor].skipped) {							// If overdrawn previously (or new shape)
				uint8_t colorBG = newAttrChar[ttf.cursor].bg;
				uint8_t colorFG = newAttrChar[ttf.cursor].fg;
				processWP(&colorBG, &colorFG);

				/* Don't do this in PC-98 mode, the bright pink cursor in EDIT.COM looks awful and not at all how the cursor is supposed to look --J.C. */
				if (!IS_PC98_ARCH && blinking && colorBG&8) {
					colorBG-=8;
					if ((bcount/8)%2)
						colorFG=colorBG;
				}
				bool dw = newAttrChar[ttf.cursor].unicode && newAttrChar[ttf.cursor].doublewide;
				ttf_bgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].red:rgbColors[colorBG].red;
				ttf_bgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].green:rgbColors[colorBG].green;
				ttf_bgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorBG&15].blue:rgbColors[colorBG].blue;
				ttf_fgColor.r = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].red:rgbColors[colorFG].red;
				ttf_fgColor.g = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].green:rgbColors[colorFG].green;
				ttf_fgColor.b = colorChanged&&!IS_VGA_ARCH?altBGR1[colorFG&15].blue:rgbColors[colorFG].blue;
				unimap[0] = newAttrChar[ttf.cursor].unicode?newAttrChar[ttf.cursor].chr:cpMap[newAttrChar[ttf.cursor].chr&255];
                if (dw) {
                    unimap[1] = newAttrChar[ttf.cursor].chr;
                    unimap[2] = 0;
                    xmax = max(x+1, xmax);
                } else
                    unimap[1] = 0;
				// first redraw character
				SDL_Surface* textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_fgColor, ttf_bgColor, ttf.width*(dw?2:1));
				ttf_textClip.w = ttf.width*(dw?2:1);
				ttf_textRect.x = ttf.offX+(rtl?(ttf.cols-x-(dw?2:1)):x)*ttf.width;
				ttf_textRect.y = ttf.offY+y*ttf.height;
				SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
				SDL_FreeSurface(textSurface);
				if (vga.draw.cursor.blinkon || blinkCursor<0) {
                    // second reverse lower lines
                    textSurface = TTF_RenderUNICODE_Shaded(ttf.SDL_font, unimap, ttf_bgColor, ttf_fgColor, ttf.width*(dw?2:1));
                    ttf_textClip.y = (ttf.height*(vga.draw.cursor.sline>15?15:vga.draw.cursor.sline))>>4;
                    ttf_textClip.h = ttf.height - ttf_textClip.y;								// for now, cursor to bottom
                    ttf_textRect.y = ttf.offY+y*ttf.height + ttf_textClip.y;
                    SDL_BlitSurface(textSurface, &ttf_textClip, sdl.surface, &ttf_textRect);
                    SDL_FreeSurface(textSurface);
				}
			}
		}
	}
	if (xmin <= xmax) {												// if any changes
        SDL_Rect *rect = &sdl.updateRects[0];
        rect->x = ttf.offX+(rtl?(ttf.cols-xmax-1):xmin)*ttf.width; rect->y = ttf.offY+ymin*ttf.height; rect->w = (xmax-xmin+1)*ttf.width; rect->h = (ymax-ymin+1)*ttf.height;
#if defined(C_SDL2)
        SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
        SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
    }
}

FT_Face GetTTFFace() {
    if (ttf.inUse && ttfFont && ttfSize) {
        TTF_Font *font = TTF_OpenFontRW(SDL_RWFromConstMem(ttfFont, (int)ttfSize), 1, ttf.pointsize);
        return font?font->face:NULL;
    } else
        return NULL;
}

void resetFontSize() {
	if (ttf.inUse) {
		GFX_SelectFontByPoints(ttf.pointsize);
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
			SDL_FillRect(sdl.surface, NULL, 0);
            SDL_Rect *rect = &sdl.updateRects[0];
            rect->x = 0; rect->y = 0; rect->w = 0; rect->h = 0;
#if defined(C_SDL2)
            SDL_UpdateWindowSurfaceRects(sdl.window, sdl.updateRects, 4);
#else
            SDL_UpdateRects(sdl.surface, 4, sdl.updateRects);
#endif
		}
		GFX_EndTextLines(true);
#if defined(HX_DOS)
		PIC_AddEvent(drawmenu, 200);
#endif
	}
}

void decreaseFontSize() {
	int dec=ttf.DOSBox ? 2 : 1;
	if (ttf.inUse && ttf.pointsize >= MIN_PTSIZE + dec) {
		GFX_SelectFontByPoints(ttf.pointsize - dec);
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
		if (!ttf.fullScrn && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0) GFX_SelectFontByPoints(ttf.pointsize + dec);
#endif
		GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
		wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);
		if (ttf.fullScrn) {																// smaller content area leaves old one behind
            ttf.fullScrn = false;
            resetFontSize();
        } else
            GFX_EndTextLines(true);
	}
}

void increaseFontSize() {
	int inc=ttf.DOSBox ? 2 : 1;
	if (ttf.inUse) {																	// increase fontsize
        unsigned int maxWidth, maxHeight;
        GetMaxWidthHeight(&maxWidth, &maxHeight);
#if defined(WIN32)
		if (!ttf.fullScrn) {															// 3D borders
			maxWidth -= GetSystemMetrics(SM_CXBORDER)*2;
			maxHeight -= GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CYBORDER)*2;
		}
#endif
		GFX_SelectFontByPoints(ttf.pointsize + inc);
		if (ttf.cols*ttf.width <= maxWidth && ttf.lins*ttf.height <= maxHeight) {		// if it fits on screen
			GFX_SetSize(720+sdl.clip.x, 400+sdl.clip.y, sdl.draw.flags,sdl.draw.scalex,sdl.draw.scaley,sdl.draw.callback);
			wmemset((wchar_t*)curAttrChar, -1, ttf.cols*ttf.lins);						// force redraw of complete window
			GFX_EndTextLines(true);
		} else
			GFX_SelectFontByPoints(ttf.pointsize - inc);
	}
}

void TTF_IncreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    increaseFontSize();
    return;
}

void TTF_DecreaseSize(bool pressed) {
    if (!pressed||ttf.fullScrn) return;
    decreaseFontSize();
    return;
}

void DBCSSBCS_mapper_shortcut(bool pressed) {
    if (!pressed) return;
    if (!isDBCSCP()) {
        systemmessagebox("Warning", "This function is only available for the Chinese/Japanese/Korean code pages.", "ok","warning", 1);
        return;
    }
    dbcs_sbcs=!dbcs_sbcs;
    SetVal("ttf", "autodbcs", dbcs_sbcs?"true":"false");
    mainMenu.get_item("mapper_dbcssbcs").check(dbcs_sbcs).refresh_item(mainMenu);
    if (ttf.inUse) resetFontSize();
}

void AutoBoxDraw_mapper_shortcut(bool pressed) {
    if (!pressed) return;
    if (!isDBCSCP()) {
        systemmessagebox("Warning", "This function is only available for the Chinese/Japanese/Korean code pages.", "ok","warning", 1);
        return;
    }
    autoboxdraw=!autoboxdraw;
    SetVal("ttf", "autoboxdraw", autoboxdraw?"true":"false");
    mainMenu.get_item("mapper_autoboxdraw").check(autoboxdraw).refresh_item(mainMenu);
    if (ttf.inUse) resetFontSize();
}

bool setVGAColor(const char *colorArray, int i);
void ttf_reset_colors() {
    if (ttf.inUse) {
        SetVal("ttf", "colors", "");
        setColors("#000000 #0000aa #00aa00 #00aaaa #aa0000 #aa00aa #aa5500 #aaaaaa #555555 #5555ff #55ff55 #55ffff #ff5555 #ff55ff #ffff55 #ffffff",-1);
    } else {
        char value[128];
        for (int i=0; i<16; i++) {
            strcpy(value,i==0?"#000000":i==1?"#0000aa":i==2?"#00aa00":i==3?"#00aaaa":i==4?"#aa0000":i==5?"#aa00aa":i==6?"#aa5500":i==7?"#aaaaaa":i==8?"#555555":i==9?"#5555ff":i==10?"#55ff55":i==11?"#55ffff":i==12?"#ff5555":i==13?"#ff55ff":i==14?"#ffff55":"#ffffff");
            setVGAColor(value, i);
        }
    }
}

void ResetColors_mapper_shortcut(bool pressed) {
    if (!pressed||(!ttf.inUse&&!IS_VGA_ARCH)) return;
    ttf_reset_colors();
}

void ttf_reset() {
    OUTPUT_TTF_Select(2);
    resetFontSize();
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
    int last = 0;
    while (TTF_using() && !GFX_IsFullscreen() && menu_gui && menu.toggle && menuwidth_atleast(ttf.cols*ttf.width+ttf.offX*2+GetSystemMetrics(SM_CXBORDER)*2)>0 && ttf.pointsize>last) {
        last = ttf.pointsize;
        increaseFontSize();
    }
#endif
}

void ttfreset(Bitu /*val*/) {
    ttf_reset();
}

void ttf_setlines(int cols, int lins) {
    if (cols>0) SetVal("ttf", "cols", std::to_string(cols));
    if (lins>0) SetVal("ttf", "lins", std::to_string(lins));
    firstset=true;
    ttf_reset();
    real_writeb(BIOSMEM_SEG,BIOSMEM_NB_COLS,ttf.cols);
    if (IS_EGAVGA_ARCH) real_writeb(BIOSMEM_SEG,BIOSMEM_NB_ROWS,ttf.lins-1);
    vga.draw.address_add = ttf.cols * 2;
}

void ttf_switch_on(bool ss=true) {
    if ((ss&&ttfswitch)||(!ss&&switch_output_from_ttf)) {
        checkcol = 0;
        if (strcmp(RunningProgram, "LOADLIN")) {
            uint16_t oldax=reg_ax;
            reg_ax=0x1600;
            CALLBACK_RunRealInt(0x2F);
            if (reg_al!=0&&reg_al!=0x80) {reg_ax=oldax;return;}
            reg_ax=oldax;
        }
        if (window_was_maximized&&!GFX_IsFullscreen()) {
#if defined(WIN32)
            ShowWindow(GetHWND(), SW_RESTORE);
#elif defined(C_SDL2)
            SDL_RestoreWindow(sdl.window);
#endif
        }
        bool OpenGL_using(void), gl = OpenGL_using();
	(void)gl; // unused var warning
#if defined(WIN32) && !defined(C_SDL2)
        change_output(0); // call OUTPUT_SURFACE_Select() to initialize output before enabling TTF output on Windows builds
#endif
        change_output(10); // call OUTPUT_TTF_Select()
        SetVal("sdl", "output", "ttf");
        std::string showdbcsstr = static_cast<Section_prop *>(control->GetSection("dosv"))->Get_string("showdbcsnodosv");
        showdbcs = showdbcsstr=="true"||showdbcsstr=="1"||(showdbcsstr=="auto" && (loadlang || dbcs_sbcs));
        if (!IS_EGAVGA_ARCH) showdbcs = false;
        void OutputSettingMenuUpdate(void);
        OutputSettingMenuUpdate();
        if (ss) ttfswitch = false;
        else switch_output_from_ttf = false;
        mainMenu.get_item("output_ttf").enable(true).refresh_item(mainMenu);
        if (ttf.fullScrn) {
            if (!GFX_IsFullscreen()) GFX_SwitchFullscreenNoReset();
            OUTPUT_TTF_Select(3);
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
            if (gl && GFX_IsFullscreen()) { // Hack for full-screen switch from OpenGL outputs
                GFX_SwitchFullScreen();
                GFX_SwitchFullScreen();
            }
#endif
            resetreq = true;
        }
        resetFontSize();
#ifdef C_SDL2
        transparency = 0;
        SetWindowTransparency(static_cast<Section_prop *>(control->GetSection("sdl"))->Get_int("transparency"));
#endif
        if (!IS_PC98_ARCH && vga.draw.address_add != ttf.cols * 2) checkcol = ss?2:1;
    }
}

void ttf_switch_off(bool ss=true) {
    checkcol = 0;
    if (!ss&&ttfswitch)
        ttf_switch_on();
    if (ttf.inUse) {
        std::string output="surface";
        int out=switchoutput;
        if (switchoutput==0)
            output = "surface";
#if C_OPENGL
        else if (switchoutput==3)
            output = "opengl";
        else if (switchoutput==4)
            output = "openglnb";
        else if (switchoutput==5)
            output = "openglpp";
#endif
#if C_DIRECT3D
        else if (switchoutput==6)
            output = "direct3d";
#endif
        else {
#if C_DIRECT3D
            out = 6;
            output = "direct3d";
#elif C_OPENGL
            out = 3;
            output = "opengl";
#else
            out = 0;
            output = "surface";
#endif
        }
        KEYBOARD_Clear();
        showdbcs = IS_EGAVGA_ARCH;
        change_output(out);
        SetVal("sdl", "output", output);
        void OutputSettingMenuUpdate(void);
        OutputSettingMenuUpdate();
        if (ss) ttfswitch = true;
        else switch_output_from_ttf = true;
        //if (GFX_IsFullscreen()) GFX_SwitchFullscreenNoReset();
        mainMenu.get_item("output_ttf").enable(false).refresh_item(mainMenu);
        RENDER_Reset();
#ifdef C_SDL2
        transparency = 0;
        SetWindowTransparency(static_cast<Section_prop *>(control->GetSection("sdl"))->Get_int("transparency"));
#endif
        if (Mouse_IsLocked()) GFX_CaptureMouse(true);
    }
}
#endif
