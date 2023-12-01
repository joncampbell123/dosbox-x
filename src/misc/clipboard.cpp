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
#include "dos_inc.h"
#include "sdlmain.h"
#include "keyboard.h"
#include "render.h"
#include "jfont.h"
#include "bios.h"
#include "../ints/int10.h"

#ifdef __WIN32__
#include <malloc.h>
#endif

#include <output/output_ttf.h>

uint8_t *clipAscii = NULL;
uint32_t clipSize = 0;
bool direct_mouse_clipboard = false;
extern const char *modifier;
extern std::map<int, int> lowboxdrawmap;
extern bool morelen, showdbcs, selmark, clipboard_biospaste;
extern int mouse_start_x, mouse_start_y, mouse_end_x, mouse_end_y, fx, fy, selsrow, selscol, selerow, selecol, mbutton;
bool CodePageHostToGuestUTF16(char *d/*CROSS_LEN*/,const uint16_t *s/*CROSS_LEN*/);
bool CodePageGuestToHostUTF16(uint16_t *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);

#ifdef MACOSX
void GetClipboard(std::string* result);
bool SetClipboard(std::string value);
#endif

#if defined(WIN32)
bool Unicode2Ascii(const uint16_t* unicode) {
    int memNeeded = 0;
    char temp[4096];
    bool ret=false;
    morelen=true;
    int flags = 0;
#ifdef WC_NO_BEST_FIT_CHARS
    flags |= WC_NO_BEST_FIT_CHARS;
#endif

    if (CodePageHostToGuestUTF16(temp,unicode) && (clipAscii = (uint8_t *)malloc(strlen(temp)+1))) {
        morelen=false;
        ret=true;
        strcpy((char *)clipAscii, temp);
        memNeeded = strlen(temp);
    } else {
        morelen=false;
        int memNeeded = WideCharToMultiByte(dos.loaded_codepage==808?866:(dos.loaded_codepage==859?858:(dos.loaded_codepage==872?855:(dos.loaded_codepage==951?950:dos.loaded_codepage))), flags, (LPCWSTR)unicode, -1, NULL, 0, "\x07", NULL);
        if (memNeeded <= 1)																// Includes trailing null
            return false;
        if (!(clipAscii = (uint8_t *)malloc(memNeeded)))
            return false;
        // Untranslated characters will be set to 0x07 (BEL), and later stripped
        if (WideCharToMultiByte(dos.loaded_codepage==808?866:(dos.loaded_codepage==859?858:(dos.loaded_codepage==872?855:(dos.loaded_codepage==951?950:dos.loaded_codepage))), flags, (LPCWSTR)unicode, -1, (LPSTR)clipAscii, memNeeded, "\x07", NULL) != memNeeded) {																			// Can't actually happen of course
            free(clipAscii);
            clipAscii = NULL;
            return false;
        }
        memNeeded--;																	// Don't include trailing null
    }
	for (int i = 0; i < memNeeded; i++)
		if (clipAscii[i] > 31 || clipAscii[i] == 9 || clipAscii[i] == 10 || clipAscii[i] == 13 // Space and up, or TAB, CR/LF allowed (others make no sense when pasting)
            || (dos.loaded_codepage == 932 && (TTF_using() || IS_JEGA_ARCH || showdbcs)
#if defined(USE_TTF)
            && halfwidthkana
#endif
            && ((IS_JEGA_ARCH && clipAscii[i] && clipAscii[i] < 32) || (!IS_PC98_ARCH && !IS_JEGA_ARCH && lowboxdrawmap.find(clipAscii[i])!=lowboxdrawmap.end()))))
			clipAscii[clipSize++] = clipAscii[i];
	return ret;																			// clipAscii dould be downsized, but of no real interest
}
#else
typedef char host_cnv_char_t;
host_cnv_char_t *CodePageGuestToHost(const char *s);
#endif

bool swapad=true;
extern std::string strPasteBuffer;
void PasteClipboard(bool bPressed);
bool getClipboard() {
	if (clipAscii) {
		free(clipAscii);
		clipAscii = NULL;
	}
#if defined(WIN32)
    clipSize = 0;
    if (OpenClipboard(NULL)) {
        if (HANDLE cbText = GetClipboardData(CF_UNICODETEXT)) {
            uint16_t *unicode = (uint16_t *)GlobalLock(cbText);
            Unicode2Ascii(unicode);
            GlobalUnlock(cbText);
        }
        CloseClipboard();
    }
#else
    swapad=false;
    PasteClipboard(true);
    swapad=true;
    clipSize = 0;
    unsigned int extra = 0;
    unsigned char head, last=13;
    for (size_t i=0; i<strPasteBuffer.length(); i++) if (strPasteBuffer[i]==10||strPasteBuffer[i]==13) extra++;
    clipAscii = (uint8_t*)malloc(strPasteBuffer.length() + extra);
    if (clipAscii)
        while (strPasteBuffer.length()) {
            head = strPasteBuffer[0];
            if (head == 10 && last != 13) clipAscii[clipSize++] = 13;
            if (head > 31 || head == 9 || head == 10 || head == 13)
                clipAscii[clipSize++] = head;
            if (head == 13 && (strPasteBuffer.length() < 2 || strPasteBuffer[1] != 10)) clipAscii[clipSize++] = 10;
            strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
            last = head;
        }
#endif
    return clipSize != 0;
}

#if defined(WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#ifndef DIK_PAUSE
#define DIK_PAUSE   0xC5
#endif
#ifndef DIK_OEM_102
#define DIK_OEM_102 0x56    /* < > | on UK/Germany keyboards */
#endif
#else
#define UINT unsigned int
#endif
#if defined(C_SDL2)
#define SDLKey SDL_Keycode
#define SDLMod SDL_Keymod
#endif
static SDLKey aryScanCodeToSDLKey[0x100];
static bool   bScanCodeMapInited = false;
static void PasteInitMapSCToSDLKey()
{
    /* Map the DIK scancodes to SDL keysyms */
    for (unsigned int i = 0; i<SDL_arraysize(aryScanCodeToSDLKey); ++i)
        aryScanCodeToSDLKey[i] = SDLK_UNKNOWN;

    /* Defined DIK_* constants */
#if defined(WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
    aryScanCodeToSDLKey[DIK_ESCAPE] = SDLK_ESCAPE;
    aryScanCodeToSDLKey[DIK_1] = SDLK_1;
    aryScanCodeToSDLKey[DIK_2] = SDLK_2;
    aryScanCodeToSDLKey[DIK_3] = SDLK_3;
    aryScanCodeToSDLKey[DIK_4] = SDLK_4;
    aryScanCodeToSDLKey[DIK_5] = SDLK_5;
    aryScanCodeToSDLKey[DIK_6] = SDLK_6;
    aryScanCodeToSDLKey[DIK_7] = SDLK_7;
    aryScanCodeToSDLKey[DIK_8] = SDLK_8;
    aryScanCodeToSDLKey[DIK_9] = SDLK_9;
    aryScanCodeToSDLKey[DIK_0] = SDLK_0;
    aryScanCodeToSDLKey[DIK_MINUS] = SDLK_MINUS;
    aryScanCodeToSDLKey[DIK_EQUALS] = SDLK_EQUALS;
    aryScanCodeToSDLKey[DIK_BACK] = SDLK_BACKSPACE;
    aryScanCodeToSDLKey[DIK_TAB] = SDLK_TAB;
    aryScanCodeToSDLKey[DIK_Q] = SDLK_q;
    aryScanCodeToSDLKey[DIK_W] = SDLK_w;
    aryScanCodeToSDLKey[DIK_E] = SDLK_e;
    aryScanCodeToSDLKey[DIK_R] = SDLK_r;
    aryScanCodeToSDLKey[DIK_T] = SDLK_t;
    aryScanCodeToSDLKey[DIK_Y] = SDLK_y;
    aryScanCodeToSDLKey[DIK_U] = SDLK_u;
    aryScanCodeToSDLKey[DIK_I] = SDLK_i;
    aryScanCodeToSDLKey[DIK_O] = SDLK_o;
    aryScanCodeToSDLKey[DIK_P] = SDLK_p;
    aryScanCodeToSDLKey[DIK_LBRACKET] = SDLK_LEFTBRACKET;
    aryScanCodeToSDLKey[DIK_RBRACKET] = SDLK_RIGHTBRACKET;
    aryScanCodeToSDLKey[DIK_RETURN] = SDLK_RETURN;
    aryScanCodeToSDLKey[DIK_LCONTROL] = SDLK_LCTRL;
    aryScanCodeToSDLKey[DIK_A] = SDLK_a;
    aryScanCodeToSDLKey[DIK_S] = SDLK_s;
    aryScanCodeToSDLKey[DIK_D] = SDLK_d;
    aryScanCodeToSDLKey[DIK_F] = SDLK_f;
    aryScanCodeToSDLKey[DIK_G] = SDLK_g;
    aryScanCodeToSDLKey[DIK_H] = SDLK_h;
    aryScanCodeToSDLKey[DIK_J] = SDLK_j;
    aryScanCodeToSDLKey[DIK_K] = SDLK_k;
    aryScanCodeToSDLKey[DIK_L] = SDLK_l;
    aryScanCodeToSDLKey[DIK_SEMICOLON] = SDLK_SEMICOLON;
    aryScanCodeToSDLKey[DIK_APOSTROPHE] = SDLK_QUOTE;
    aryScanCodeToSDLKey[DIK_GRAVE] = SDLK_BACKQUOTE;
    aryScanCodeToSDLKey[DIK_LSHIFT] = SDLK_LSHIFT;
    aryScanCodeToSDLKey[DIK_BACKSLASH] = SDLK_BACKSLASH;
    aryScanCodeToSDLKey[DIK_OEM_102] = SDLK_LESS;
    aryScanCodeToSDLKey[DIK_Z] = SDLK_z;
    aryScanCodeToSDLKey[DIK_X] = SDLK_x;
    aryScanCodeToSDLKey[DIK_C] = SDLK_c;
    aryScanCodeToSDLKey[DIK_V] = SDLK_v;
    aryScanCodeToSDLKey[DIK_B] = SDLK_b;
    aryScanCodeToSDLKey[DIK_N] = SDLK_n;
    aryScanCodeToSDLKey[DIK_M] = SDLK_m;
    aryScanCodeToSDLKey[DIK_COMMA] = SDLK_COMMA;
    aryScanCodeToSDLKey[DIK_PERIOD] = SDLK_PERIOD;
    aryScanCodeToSDLKey[DIK_SLASH] = SDLK_SLASH;
    aryScanCodeToSDLKey[DIK_RSHIFT] = SDLK_RSHIFT;
    aryScanCodeToSDLKey[DIK_MULTIPLY] = SDLK_KP_MULTIPLY;
    aryScanCodeToSDLKey[DIK_LMENU] = SDLK_LALT;
    aryScanCodeToSDLKey[DIK_SPACE] = SDLK_SPACE;
    aryScanCodeToSDLKey[DIK_CAPITAL] = SDLK_CAPSLOCK;
    aryScanCodeToSDLKey[DIK_F1] = SDLK_F1;
    aryScanCodeToSDLKey[DIK_F2] = SDLK_F2;
    aryScanCodeToSDLKey[DIK_F3] = SDLK_F3;
    aryScanCodeToSDLKey[DIK_F4] = SDLK_F4;
    aryScanCodeToSDLKey[DIK_F5] = SDLK_F5;
    aryScanCodeToSDLKey[DIK_F6] = SDLK_F6;
    aryScanCodeToSDLKey[DIK_F7] = SDLK_F7;
    aryScanCodeToSDLKey[DIK_F8] = SDLK_F8;
    aryScanCodeToSDLKey[DIK_F9] = SDLK_F9;
    aryScanCodeToSDLKey[DIK_F10] = SDLK_F10;
#if defined(C_SDL2)
    aryScanCodeToSDLKey[DIK_NUMLOCK] = SDLK_NUMLOCKCLEAR;
    aryScanCodeToSDLKey[DIK_SCROLL] = SDLK_SCROLLLOCK;
#else
    aryScanCodeToSDLKey[DIK_NUMLOCK] = SDLK_NUMLOCK;
    aryScanCodeToSDLKey[DIK_SCROLL] = SDLK_SCROLLOCK;
#endif
    aryScanCodeToSDLKey[DIK_SUBTRACT] = SDLK_KP_MINUS;
    aryScanCodeToSDLKey[DIK_ADD] = SDLK_KP_PLUS;
#if defined(C_SDL2)
    aryScanCodeToSDLKey[DIK_NUMPAD7] = SDLK_KP_7;
    aryScanCodeToSDLKey[DIK_NUMPAD8] = SDLK_KP_8;
    aryScanCodeToSDLKey[DIK_NUMPAD9] = SDLK_KP_9;
    aryScanCodeToSDLKey[DIK_NUMPAD4] = SDLK_KP_4;
    aryScanCodeToSDLKey[DIK_NUMPAD5] = SDLK_KP_5;
    aryScanCodeToSDLKey[DIK_NUMPAD6] = SDLK_KP_6;
    aryScanCodeToSDLKey[DIK_NUMPAD1] = SDLK_KP_1;
    aryScanCodeToSDLKey[DIK_NUMPAD2] = SDLK_KP_2;
    aryScanCodeToSDLKey[DIK_NUMPAD3] = SDLK_KP_3;
    aryScanCodeToSDLKey[DIK_NUMPAD0] = SDLK_KP_0;
#else
    aryScanCodeToSDLKey[DIK_NUMPAD7] = SDLK_KP7;
    aryScanCodeToSDLKey[DIK_NUMPAD8] = SDLK_KP8;
    aryScanCodeToSDLKey[DIK_NUMPAD9] = SDLK_KP9;
    aryScanCodeToSDLKey[DIK_NUMPAD4] = SDLK_KP4;
    aryScanCodeToSDLKey[DIK_NUMPAD5] = SDLK_KP5;
    aryScanCodeToSDLKey[DIK_NUMPAD6] = SDLK_KP6;
    aryScanCodeToSDLKey[DIK_NUMPAD1] = SDLK_KP1;
    aryScanCodeToSDLKey[DIK_NUMPAD2] = SDLK_KP2;
    aryScanCodeToSDLKey[DIK_NUMPAD3] = SDLK_KP3;
    aryScanCodeToSDLKey[DIK_NUMPAD0] = SDLK_KP0;
#endif
    aryScanCodeToSDLKey[DIK_DECIMAL] = SDLK_KP_PERIOD;
    aryScanCodeToSDLKey[DIK_F11] = SDLK_F11;
    aryScanCodeToSDLKey[DIK_F12] = SDLK_F12;

    aryScanCodeToSDLKey[DIK_F13] = SDLK_F13;
    aryScanCodeToSDLKey[DIK_F14] = SDLK_F14;
    aryScanCodeToSDLKey[DIK_F15] = SDLK_F15;

    aryScanCodeToSDLKey[DIK_NUMPADEQUALS] = SDLK_KP_EQUALS;
    aryScanCodeToSDLKey[DIK_NUMPADENTER] = SDLK_KP_ENTER;
    aryScanCodeToSDLKey[DIK_RCONTROL] = SDLK_RCTRL;
    aryScanCodeToSDLKey[DIK_DIVIDE] = SDLK_KP_DIVIDE;
#if defined(C_SDL2)
    aryScanCodeToSDLKey[DIK_SYSRQ] = SDLK_PRINTSCREEN;
#else
    aryScanCodeToSDLKey[DIK_SYSRQ] = SDLK_PRINT;
#endif
    aryScanCodeToSDLKey[DIK_RMENU] = SDLK_RALT;
    aryScanCodeToSDLKey[DIK_PAUSE] = SDLK_PAUSE;
    aryScanCodeToSDLKey[DIK_HOME] = SDLK_HOME;
    aryScanCodeToSDLKey[DIK_UP] = SDLK_UP;
    aryScanCodeToSDLKey[DIK_PRIOR] = SDLK_PAGEUP;
    aryScanCodeToSDLKey[DIK_LEFT] = SDLK_LEFT;
    aryScanCodeToSDLKey[DIK_RIGHT] = SDLK_RIGHT;
    aryScanCodeToSDLKey[DIK_END] = SDLK_END;
    aryScanCodeToSDLKey[DIK_DOWN] = SDLK_DOWN;
    aryScanCodeToSDLKey[DIK_NEXT] = SDLK_PAGEDOWN;
    aryScanCodeToSDLKey[DIK_INSERT] = SDLK_INSERT;
    aryScanCodeToSDLKey[DIK_DELETE] = SDLK_DELETE;
#if defined(C_SDL2)
    aryScanCodeToSDLKey[DIK_LWIN] = SDLK_LGUI;
    aryScanCodeToSDLKey[DIK_RWIN] = SDLK_RGUI;
#else
    aryScanCodeToSDLKey[DIK_LWIN] = SDLK_LMETA;
    aryScanCodeToSDLKey[DIK_RWIN] = SDLK_RMETA;
#endif
    aryScanCodeToSDLKey[DIK_APPS] = SDLK_MENU;
#elif defined(C_SDL2)
    aryScanCodeToSDLKey['1'] = SDLK_KP_1;
    aryScanCodeToSDLKey['2'] = SDLK_KP_2;
    aryScanCodeToSDLKey['3'] = SDLK_KP_3;
    aryScanCodeToSDLKey['4'] = SDLK_KP_4;
    aryScanCodeToSDLKey['5'] = SDLK_KP_5;
    aryScanCodeToSDLKey['6'] = SDLK_KP_6;
    aryScanCodeToSDLKey['7'] = SDLK_KP_7;
    aryScanCodeToSDLKey['8'] = SDLK_KP_8;
    aryScanCodeToSDLKey['9'] = SDLK_KP_9;
    aryScanCodeToSDLKey['0'] = SDLK_KP_0;
    aryScanCodeToSDLKey[0xFF] = SDLK_LALT;
#else
    aryScanCodeToSDLKey['1'] = SDLK_KP1;
    aryScanCodeToSDLKey['2'] = SDLK_KP2;
    aryScanCodeToSDLKey['3'] = SDLK_KP3;
    aryScanCodeToSDLKey['4'] = SDLK_KP4;
    aryScanCodeToSDLKey['5'] = SDLK_KP5;
    aryScanCodeToSDLKey['6'] = SDLK_KP6;
    aryScanCodeToSDLKey['7'] = SDLK_KP7;
    aryScanCodeToSDLKey['8'] = SDLK_KP8;
    aryScanCodeToSDLKey['9'] = SDLK_KP9;
    aryScanCodeToSDLKey['0'] = SDLK_KP0;
    aryScanCodeToSDLKey[0xFF] = SDLK_LALT;
#endif
    bScanCodeMapInited = true;

}

// Just in case, to keep us from entering an unexpected KB state
const  size_t      kPasteMinBufExtra = 4;
/// Slightly inefficient, but who cares
static void GenKBStroke(const UINT uiScanCode, const bool bDepressed, const SDLMod keymods)
{
    const SDLKey sdlkey = aryScanCodeToSDLKey[uiScanCode & 0xFF];
    if (sdlkey == SDLK_UNKNOWN)
        return;

    SDL_Event evntKeyStroke = { 0 };
    evntKeyStroke.type = bDepressed ? SDL_KEYDOWN : SDL_KEYUP;
#if defined(C_SDL2)
    evntKeyStroke.key.keysym.scancode = SDL_GetScancodeFromKey(sdlkey);
#else
    evntKeyStroke.key.keysym.scancode = uiScanCode & 0xFF;
#endif
    evntKeyStroke.key.keysym.sym = sdlkey;
    evntKeyStroke.key.keysym.mod = keymods;
#if !defined(C_SDL2)
    evntKeyStroke.key.keysym.unicode = 0;
#endif
    evntKeyStroke.key.state = bDepressed ? SDL_PRESSED : SDL_RELEASED;
    SDL_PushEvent(&evntKeyStroke);
}

bool PasteClipboardNext() {
    if (strPasteBuffer.length() == 0)
        return false;
    if(clipboard_biospaste) {
        if (strPasteBuffer[0]==13) {
            KEYBOARD_AddKey(KBD_enter, true);
            KEYBOARD_AddKey(KBD_enter, false);
        } else
            BIOS_AddKeyToBuffer(strPasteBuffer[0]<0?strPasteBuffer[0]&0xff:strPasteBuffer[0]);
		strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
		return true;
	}

    if (!bScanCodeMapInited)
        PasteInitMapSCToSDLKey();

    const char cKey = strPasteBuffer[0];
#if defined(WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
    SHORT shVirKey = VkKeyScan(cKey); // If it fails then MapVirtK will also fail, so no bail yet
    UINT uiScanCode = MapVirtualKey(LOBYTE(shVirKey), MAPVK_VK_TO_VSC);
    if (uiScanCode)
    {
        const bool   bModShift = ((shVirKey & 0x0100) != 0);
        const bool   bModCntrl = ((shVirKey & 0x0200) != 0);
        const bool   bModAlt = ((shVirKey & 0x0400) != 0);
        const SDLMod sdlmModsOn = SDL_GetModState();
        const bool   bModShiftOn = ((sdlmModsOn & (KMOD_LSHIFT | KMOD_RSHIFT)) > 0);
        const bool   bModCntrlOn = ((sdlmModsOn & (KMOD_LCTRL | KMOD_RCTRL)) > 0);
        const bool   bModAltOn = ((sdlmModsOn & (KMOD_LALT | KMOD_RALT)) > 0);
        const UINT   uiScanCodeShift = MapVirtualKey(VK_SHIFT, MAPVK_VK_TO_VSC);
        const UINT   uiScanCodeCntrl = MapVirtualKey(VK_CONTROL, MAPVK_VK_TO_VSC);
        const UINT   uiScanCodeAlt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
        const SDLMod sdlmMods = (SDLMod)((sdlmModsOn & ~(KMOD_LSHIFT | KMOD_RSHIFT |
            KMOD_LCTRL | KMOD_RCTRL |
            KMOD_LALT | KMOD_RALT)) |
            (bModShiftOn ? KMOD_LSHIFT : 0) |
            (bModCntrlOn ? KMOD_LCTRL : 0) |
            (bModAltOn ? KMOD_LALT : 0));

        /// \note Currently pasting a character is a two step affair, because if
        ///       you do it too quickly DI can miss a key press/release.
        // Could be made more efficient, but would require tracking of more state,
        // so let's forgot that for now...
        size_t sStrokesRequired = 2; // At least the key & up/down
        if (bModShift != bModShiftOn) sStrokesRequired += 2; // To press/release Shift
        if (bModCntrl != bModCntrlOn) sStrokesRequired += 2; // To press/release Control
        if (bModAlt != bModAltOn) sStrokesRequired += 2; // To press/release Alt
        /// \fixme Should check if key is already pressed or not so it can toggle press
        ///        but since we don't actually have any mappings from VK/SC to DI codes
        ///        (which SDL (can) use(s) internally as actually scancodes), we can't
        ///        actually check that ourselves, sadly...
        if (KEYBOARD_BufferSpaceAvail() < (sStrokesRequired + kPasteMinBufExtra))
            return false;

        if (bModShift != bModShiftOn) GenKBStroke(uiScanCodeShift, !bModShiftOn, sdlmMods);
        if (bModCntrl != bModCntrlOn) GenKBStroke(uiScanCodeCntrl, !bModCntrlOn, sdlmMods);
        if (bModAlt != bModAltOn) GenKBStroke(uiScanCodeAlt, !bModAltOn, sdlmMods);
        GenKBStroke(uiScanCode, true, sdlmMods);
        GenKBStroke(uiScanCode, false, sdlmMods);
        if (bModShift != bModShiftOn) GenKBStroke(uiScanCodeShift, bModShiftOn, sdlmMods);
        if (bModCntrl != bModCntrlOn) GenKBStroke(uiScanCodeCntrl, bModCntrlOn, sdlmMods);
        if (bModAlt != bModAltOn) GenKBStroke(uiScanCodeAlt, bModAltOn, sdlmMods);
        //putchar(cKey); // For debugging dropped strokes
    } else {
		const UINT   uiScanCodeAlt = MapVirtualKey(VK_MENU, MAPVK_VK_TO_VSC);
#else
    {
		//UINT uiScanCode = cKey; UNUSED
		const UINT   uiScanCodeAlt = 0xFF;
#endif
		if (KEYBOARD_BufferSpaceAvail() < (10+kPasteMinBufExtra)) // For simplicity, we just mimic Alt+<3 digits>
			return false;

#if !defined(WIN32) || (defined(__MINGW32__) && !defined(__MINGW64_VERSION_MAJOR))
        if (strPasteBuffer[0]==13) {
            KEYBOARD_AddKey(KBD_enter, true);
            KEYBOARD_AddKey(KBD_enter, false);
            strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length());
            return true;
        }
#endif
		const SDLMod sdlmModsOn = SDL_GetModState();
		const bool bModShiftOn = ((sdlmModsOn & (KMOD_LSHIFT|KMOD_RSHIFT)) > 0);
		const bool bModCntrlOn = ((sdlmModsOn & (KMOD_LCTRL|KMOD_RCTRL )) > 0);
		const bool bModAltOn = ((sdlmModsOn&(KMOD_LALT|KMOD_RALT)) > 0);
		const SDLMod sdlmMods = (SDLMod)((sdlmModsOn&~(KMOD_LSHIFT|KMOD_RSHIFT|KMOD_LCTRL|KMOD_RCTRL|KMOD_LALT|KMOD_RALT)) |
												 (bModShiftOn ? KMOD_LSHIFT : 0) |
												 (bModCntrlOn ? KMOD_LCTRL  : 0) |
												 (bModAltOn   ? KMOD_LALT   : 0));

		if (!bModAltOn)                                                // Alt down if not already down
			GenKBStroke(uiScanCodeAlt, true, sdlmMods);

		uint8_t ansiVal = cKey;
		unsigned int ct = 0;
		for (int i = 100; i; i /= 10) {
			int numKey = ansiVal/i;                                    // High digit of Alt+ASCII number combination
			if (!numKey) ct++;
			ansiVal %= i;
#if defined(WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
			UINT uiScanCode = MapVirtualKey(numKey+VK_NUMPAD0, MAPVK_VK_TO_VSC);
#else
			UINT uiScanCode = numKey+'0';
#endif
			GenKBStroke(uiScanCode, true, sdlmMods);
			GenKBStroke(uiScanCode, false, sdlmMods);
		}
		GenKBStroke(uiScanCodeAlt, false, sdlmMods);                // Alt up
		if (ct%2) {
			GenKBStroke(uiScanCodeAlt, true, sdlmMods);
#if defined(WIN32) && (!defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR))
			UINT uiScanCode = MapVirtualKey(0+VK_NUMPAD0, MAPVK_VK_TO_VSC);
#else
			UINT uiScanCode = 0+'0';
#endif
			GenKBStroke(uiScanCode, true, sdlmMods);
			GenKBStroke(uiScanCode, false, sdlmMods);
			GenKBStroke(uiScanCodeAlt, false, sdlmMods);
		}
		if (bModAltOn)                                                // Alt down if is was already down
			GenKBStroke(uiScanCodeAlt, true, sdlmMods);
    }
    // Pop head. Could be made more efficient, but this is neater.
    strPasteBuffer = strPasteBuffer.substr(1, strPasteBuffer.length()); // technically -1, but it clamps by itself anyways...
    return true;
}

// added emendelson from dbDos; improved by Wengier
#if defined(WIN32) && !defined(C_SDL2) && !defined(__MINGW32__)
void PasteClipboard(bool bPressed)
{
    if (!bPressed) return;
    SDL_SysWMinfo wmiInfo;
    SDL_VERSION(&wmiInfo.version);

    if (SDL_GetWMInfo(&wmiInfo) != 1||!OpenClipboard(wmiInfo.window)) return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {CloseClipboard();return;}

    HANDLE hContents = GetClipboardData(CF_UNICODETEXT);
    if (!hContents) {CloseClipboard();return;}

    uint16_t *szClipboard = (uint16_t *)GlobalLock(hContents);
    if (szClipboard)
    {
		clipSize=0;
		bool ret=Unicode2Ascii(szClipboard);
        unsigned long j=0;
        for (size_t i = 0; i < clipSize; ++i) {
            if (clipAscii[i] == 9) j++;
            else if (ret&&clipAscii[i]==0x0A&&(i==0||clipAscii[i-1]!=0x0D)) clipAscii[i]=0x0D;
        }
        if (ret&&clipAscii[clipSize-1]==0xD) clipAscii[--clipSize]=0;
        // Create a copy of the string, and filter out Linefeed characters (ASCII '10')
        char* szFilteredText = reinterpret_cast<char*>(alloca(clipSize + j*3 + 1));
        char* szFilterNextChar = szFilteredText;
        for (size_t i = 0; i < clipSize; ++i)
            if (clipAscii[i] == 9) // Tab to spaces
                for (int k=0; k<4; k++) {
                    *szFilterNextChar = ' ';
                    ++szFilterNextChar;
                }
            else if (clipAscii[i] != 0x0A) // Skip linefeeds
            {
                *szFilterNextChar = clipAscii[i];
                ++szFilterNextChar;
            }
        *szFilterNextChar = '\0'; // Cap it.

        strPasteBuffer.append(szFilteredText);
        GlobalUnlock(hContents);
		clipSize=0;
    }
    CloseClipboard();
}
/// TODO: add menu items here 
#else // end emendelson from dbDOS; improved by Wengier
#if defined(WIN32) // SDL2, MinGW / Added by Wengier
void PasteClipboard(bool bPressed) {
	if (!bPressed||!OpenClipboard(NULL)) return;
    if (!IsClipboardFormatAvailable(CF_UNICODETEXT)) {CloseClipboard();return;}
    HANDLE hContents = GetClipboardData(CF_UNICODETEXT);
    if (!hContents) {CloseClipboard();return;}
    uint16_t *szClipboard = (uint16_t *)GlobalLock(hContents);
    if (szClipboard)
    {
		clipSize=0;
		bool ret=Unicode2Ascii(szClipboard);
        unsigned long j=0;
        for (size_t i = 0; i < clipSize; ++i) {
            if (clipAscii[i] == 9) j++;
            else if (ret&&clipAscii[i]==0xA&&(i==0||clipAscii[i-1]!=0xD)) clipAscii[i]=0xD;
        }
        if (ret&&clipAscii[clipSize-1]==0xD) clipAscii[--clipSize]=0;
        char* szFilteredText = reinterpret_cast<char*>(alloca(clipSize + j*3 + 1));
        char* szFilterNextChar = szFilteredText;
        for (size_t i = 0; i < clipSize; ++i)
            if (clipAscii[i] == 9) // Tab to spaces
                for (int k=0; k<4; k++) {
                    *szFilterNextChar = ' ';
                    ++szFilterNextChar;
                }
            else if (clipAscii[i] != 0x0A) // Skip linefeeds
            {
                *szFilterNextChar = clipAscii[i];
                ++szFilterNextChar;
            }
        *szFilterNextChar = '\0'; // Cap it.

        strPasteBuffer.append(szFilteredText);
        GlobalUnlock(hContents);
		clipSize=0;
    }
	CloseClipboard();
}
#elif defined(C_SDL2) || defined(MACOSX)
typedef char host_cnv_char_t;
char *CodePageHostToGuest(const host_cnv_char_t *s);
void PasteClipboard(bool bPressed) {
	if (!bPressed) return;
    char *text;
#if defined(C_SDL2)
    text = SDL_GetClipboardText();
#else
    std::string clip="";
    GetClipboard(&clip);
    text = new char[clip.size()+1];
    strcpy(text, clip.c_str());
#endif
    if (text==NULL) return;
    std::string result="", pre="";
    morelen=true;
    for (unsigned int i=0; i<strlen(text); i++) {
        if (swapad&&text[i]==0x0A&&(i==0||text[i-1]!=0x0D)) text[i]=0x0D;
        if (text[i]==9) result+="    ";
        else if (text[i]<0) {
            char c=text[i];
            int n=1;
            if ((c & 0xe0) == 0xc0) n=2;
            else if ((c & 0xf0) == 0xe0) n=3;
            else if ((c & 0xf8) == 0xf0) n=4;
            pre="";
            bool exit=false;
            for (int k=0; k<n; k++) {
                if (n>2&&i>=strlen(text)) {exit=true;break;}
                else if (i>=strlen(text)) {i++;break;}
                if (text[i]>=0) {exit=true;break;}
                pre+=std::string(1, text[i]);
                i++;
            }
            if (exit) continue;
            const char* asc = CodePageHostToGuest(pre.c_str());
            result+=asc!=NULL?std::string(asc):(n>2?"":pre);
            i--;
        } else {
            const char* asc = CodePageHostToGuest(std::string(1, text[i]).c_str());
            result+=asc != NULL?std::string(asc):std::string(1, text[i]);
        }
    }
    morelen=false;
    delete text;
    strPasteBuffer.append(result.c_str());
}
#elif defined(LINUX) && C_X11
#include <X11/Xlib.h>
#include <X11/Xatom.h>
typedef char host_cnv_char_t;
char *CodePageHostToGuest(const host_cnv_char_t *s);
void paste_utf8_prop(Display *dpy, Window w, Atom p)
{
    Atom da, incr, type;
    int di;
    unsigned long size, dul;
    unsigned char *prop_ret = NULL;

    XGetWindowProperty(dpy, w, p, 0, 0, False, AnyPropertyType, &type, &di, &dul, &size, &prop_ret);
    XFree(prop_ret);

    incr = XInternAtom(dpy, "INCR", False);
    if (type == incr) return;

    XGetWindowProperty(dpy, w, p, 0, size, False, AnyPropertyType, &da, &di, &dul, &dul, &prop_ret);
    char *text=(char *)prop_ret;
    std::string result="", pre="";
    for (unsigned int i=0; i<strlen(text); i++) {
        if (swapad&&text[i]==0x0A&&(i==0||text[i-1]!=0x0D)) text[i]=0x0D;
        if (text[i]==9) result+="    ";
        else if (swapad&&text[i]==0x0A) continue;
        else if (text[i]<0) {
            char c=text[i];
            int n=1;
            if ((c & 0xe0) == 0xc0) n=2;
            else if ((c & 0xf0) == 0xe0) n=3;
            else if ((c & 0xf8) == 0xf0) n=4;
            pre="";
            bool exit=false;
            for (int k=0; k<n; k++) {
                if (n>2&&i>=strlen(text)) {exit=true;break;}
                else if (i>=strlen(text)) {i++;break;}
                if (text[i]>=0) {exit=true;break;}
                pre+=std::string(1, text[i]);
                i++;
            }
            if (exit) continue;
            const char* asc = CodePageHostToGuest(pre.c_str());
            result+=asc!=NULL?std::string(asc):(n>2?"":pre);
            i--;
        } else {
            const char* asc = CodePageHostToGuest(std::string(1, text[i]).c_str());
            result+=asc != NULL?std::string(asc):std::string(1, text[i]);
        }
    }
    strPasteBuffer.append(result.c_str());
    fflush(stdout);
    XFree(prop_ret);
    XDeleteProperty(dpy, w, p);
}

void PasteClipboard(bool bPressed) {
	if (!bPressed) return;
    Display *dpy;
    Window owner, target_window, root;
    int screen;
    Atom sel, target_property, utf8;
    XEvent ev;
    XSelectionEvent *sev;

    dpy = XOpenDisplay(NULL);
    if (!dpy) return;

    screen = DefaultScreen(dpy);
    root = RootWindow(dpy, screen);

    sel = XInternAtom(dpy, "CLIPBOARD", False);
    utf8 = XInternAtom(dpy, "UTF8_STRING", False);

    owner = XGetSelectionOwner(dpy, sel);
    if (owner == None) return;

    target_window = XCreateSimpleWindow(dpy, root, -10, -10, 1, 1, 0, 0, 0);
    target_property = XInternAtom(dpy, "PENGUIN", False);
    XConvertSelection(dpy, sel, utf8, target_property, target_window,
                      CurrentTime);
    for (;;) {
        XNextEvent(dpy, &ev);
        switch (ev.type) {
            case SelectionNotify:
                sev = (XSelectionEvent*)&ev.xselection;
                if (sev->property == None) return;
                else {
                    paste_utf8_prop(dpy, target_window, target_property);
                    return;
                }
                break;
        }
    }
}
#else
void PasteClipboard(bool bPressed) {
	if (!bPressed) return;
    // stub
}
#endif
#endif

extern uint16_t baselen;
extern std::list<uint16_t> bdlist;
#ifdef WIN32
void CopyClipboard(int all) {
	uint16_t len=0;
	const char* text = (char *)(all==2?Mouse_GetSelected(0,0,(int)(currentWindowWidth-1-sdl.clip.x),(int)(currentWindowHeight-1-sdl.clip.y),(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len):(all==1?Mouse_GetSelected(selscol, selsrow, selecol, selerow, -1, -1, &len):Mouse_GetSelected(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,mouse_end_x-sdl.clip.x,mouse_end_y-sdl.clip.y,sdl.clip.w,sdl.clip.h, &len)));
	if (OpenClipboard(NULL)&&EmptyClipboard()) {
        std::wstring result=L"";
        std::istringstream iss(text);
        uint16_t* wch, temp[4096];
        morelen=true;
        baselen=0;
        for (std::string token; std::getline(iss, token); ) {
            if (CodePageGuestToHostUTF16(temp,token.c_str())) {
                result+=(wchar_t *)temp;
            } else {
                int reqsize = MultiByteToWideChar(dos.loaded_codepage==808?866:(dos.loaded_codepage==859?858:(dos.loaded_codepage==872?855:(dos.loaded_codepage==951?950:dos.loaded_codepage))), 0, token.c_str(), token.size()+1, NULL, 0);
                if (reqsize>0) {
                    wch = new uint16_t[reqsize+1];
                    if (MultiByteToWideChar(dos.loaded_codepage==808?866:(dos.loaded_codepage==859?858:(dos.loaded_codepage==872?855:(dos.loaded_codepage==951?950:dos.loaded_codepage))), 0, token.c_str(), token.size()+1, (LPWSTR)wch, reqsize)==reqsize) {
                        result+=(wchar_t *)wch;
                        delete[] wch;
                        continue;
                    } else
                        delete[] wch;
                }
                wch = new uint16_t[token.size()+1];
                mbstowcs((wchar_t *)wch, token.c_str(), token.size()+1);
                result+=(wchar_t *)wch;
                delete[] wch;
            }
            result+=std::wstring(1, '\r')+std::wstring(1, '\n');
            baselen+=token.size()+1;
        }
        if (baselen>0) {
            result.pop_back();
            result.pop_back();
            baselen--;
        }
        morelen=false;
        baselen=0;
        bdlist={};
        HGLOBAL clipbuffer = GlobalAlloc(GMEM_DDESHARE, (result.size()+1)*sizeof(wchar_t));
        LPWSTR buffer = static_cast<LPWSTR>(GlobalLock(clipbuffer));
        if (buffer!=NULL) {
            for (unsigned int i=0; i<result.size(); i++) buffer[i]=(wchar_t)result[i];
            GlobalUnlock(clipbuffer);
            SetClipboardData(CF_UNICODETEXT,clipbuffer);
        }
    }
    CloseClipboard();
}
#elif defined(C_SDL2) || defined(MACOSX)
bool CodePageGuestToHostUTF8(char *d/*CROSS_LEN*/,const char *s/*CROSS_LEN*/);
void CopyClipboard(int all) {
	uint16_t len=0;
	char* text = (char *)(all==2?Mouse_GetSelected(0,0,currentWindowWidth-1-sdl.clip.x,currentWindowHeight-1-sdl.clip.y,(int)(currentWindowWidth-sdl.clip.x),(int)(currentWindowHeight-sdl.clip.y), &len):(all==1?Mouse_GetSelected(selscol, selsrow, selecol, selerow, -1, -1, &len):Mouse_GetSelected(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,mouse_end_x-sdl.clip.x,mouse_end_y-sdl.clip.y,sdl.clip.w,sdl.clip.h, &len)));
    std::string result="";
    std::istringstream iss(text);
    char temp[4096];
    morelen=true;
    baselen=0;
    for (std::string token; std::getline(iss, token); ) {
        if (token.size() && token.back() == 13) token.pop_back();
        if (CodePageGuestToHostUTF8(temp,token.c_str()))
            result+=temp;
        else
            result+=token;
        result+=std::string(1, 10);
        baselen+=token.size()+1;
    }
    morelen=false;
    baselen=0;
    bdlist={};
    if (result.size()&&result.back()==10) result.pop_back();
#if defined(C_SDL2)
    SDL_SetClipboardText(result.c_str());
#else
    SetClipboard(result);
#endif
}
#endif

void PasteClipStop(bool bPressed) {
    if (!bPressed) return;
    strPasteBuffer = "";
}

#if defined(WIN32) || defined(MACOSX) || defined(C_SDL2)
bool isModifierApplied() {
    return direct_mouse_clipboard || !strcmp(modifier,"none") ||
    ((!strcmp(modifier,"ctrl") || !strcmp(modifier,"lctrl")) && sdl.lctrlstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrl") || !strcmp(modifier,"rctrl")) && sdl.rctrlstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"alt") || !strcmp(modifier,"lalt")) && sdl.laltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"alt") || !strcmp(modifier,"ralt")) && sdl.raltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"shift") || !strcmp(modifier,"lshift")) && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"shift") || !strcmp(modifier,"rshift")) && sdl.rshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlalt") || !strcmp(modifier,"lctrlalt")) && sdl.lctrlstate==SDL_KEYDOWN && sdl.laltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlalt") || !strcmp(modifier,"rctrlalt")) && sdl.rctrlstate==SDL_KEYDOWN && sdl.raltstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlshift") || !strcmp(modifier,"lctrlshift")) && sdl.lctrlstate==SDL_KEYDOWN && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"ctrlshift") || !strcmp(modifier,"rctrlshift")) && sdl.rctrlstate==SDL_KEYDOWN && sdl.rshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"altshift") || !strcmp(modifier,"laltshift")) && sdl.laltstate==SDL_KEYDOWN && sdl.lshiftstate==SDL_KEYDOWN) ||
    ((!strcmp(modifier,"altshift") || !strcmp(modifier,"raltshift")) && sdl.raltstate==SDL_KEYDOWN && sdl.rshiftstate==SDL_KEYDOWN);
}

void ClipKeySelect(int sym) {
    if (sym==SDLK_ESCAPE) {
        if (mbutton==4 && selsrow>-1 && selscol>-1) {
            Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
            selmark = false;
            selsrow = selscol = selerow = selecol = -1;
        } else if (mouse_start_x >= 0 && mouse_start_y >= 0 && fx >= 0 && fy >= 0) {
            Mouse_Select(mouse_start_x-sdl.clip.x,mouse_start_y-sdl.clip.y,fx-sdl.clip.x,fy-sdl.clip.y,sdl.clip.w,sdl.clip.h, false);
            mouse_start_x = mouse_start_y = -1;
            mouse_end_x = mouse_end_y = -1;
            fx = fy = -1;
        }
        return;
    }
    if (mbutton!=4 || (CurMode->type!=M_TEXT && !IS_DOSV && !IS_PC98_ARCH)) return;
    if (sym==SDLK_HOME) {
        if (selsrow==-1 || selscol==-1) {
            int p=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
            selscol = CURSOR_POS_COL(p);
            selsrow = CURSOR_POS_ROW(p);
        } else {
            if (selsrow>-1 && selscol>-1) Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
            if (selmark) {
                selscol = selecol;
                selsrow = selerow;
            }
        }
        selmark = true;
        selecol = selscol;
        selerow = selsrow;
        Mouse_Select(selscol, selsrow, selecol, selerow, -1, -1, true);
    } else if (sym==SDLK_END) {
        if (selmark) {
            if (selsrow>-1 && selscol>-1 && selerow > -1 && selecol > -1) {
                Mouse_Select(selscol, selsrow, selecol, selerow, -1, -1, false);
                CopyClipboard(1);
            }
            selmark = false;
            selsrow = selscol = selerow = selecol = -1;
        }
    } else if (sym==SDLK_LEFT || sym==SDLK_RIGHT || sym==SDLK_UP || sym==SDLK_DOWN) {
        if (selsrow==-1 || selscol==-1) {
            int p=real_readb(BIOSMEM_SEG,BIOSMEM_CURRENT_PAGE);
            selscol = CURSOR_POS_COL(p);
            selsrow = CURSOR_POS_ROW(p);
            selerow = selecol = -1;
            selmark = false;
        } else
            Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, false);
        if (sym==SDLK_LEFT && (selmark?selecol:selscol)>0) (selmark?selecol:selscol)--;
        else if (sym==SDLK_RIGHT && (selmark?selecol:selscol)<(IS_PC98_ARCH?80:real_readw(BIOSMEM_SEG,BIOSMEM_NB_COLS))-1) (selmark?selecol:selscol)++;
        else if (sym==SDLK_UP && (selmark?selerow:selsrow)>0) (selmark?selerow:selsrow)--;
        else if (sym==SDLK_DOWN && (selmark?selerow:selsrow)<(IS_PC98_ARCH?(real_readb(0x60,0x113)&0x01?25:20)-1:(IS_EGAVGA_ARCH?real_readb(BIOSMEM_SEG,BIOSMEM_NB_ROWS):24))) (selmark?selerow:selsrow)++;
        Mouse_Select(selscol, selsrow, selmark?selecol:selscol, selmark?selerow:selsrow, -1, -1, true);
    }
}
#endif
