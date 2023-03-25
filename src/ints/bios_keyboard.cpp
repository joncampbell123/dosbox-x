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

#include <queue>

#include "dosbox.h"
#include "callback.h"
#include "logging.h"
#include "mem.h"
#include "bios.h"
#include "keyboard.h"
#include "regs.h"
#include "inout.h"
#include "dos_inc.h"
#include "SDL.h"
#include "int10.h"
#include "jfont.h"
#include "render.h"

#if defined(_MSC_VER)
# pragma warning(disable:4244) /* const fmath::local::uint64_t to double possible loss of data */
#endif

/* SDL by default treats numlock and scrolllock different from all other keys.
 * In recent versions this can disabled by a environment variable which we set in sdlmain.cpp
 * Define the following if this is the case */
#if SDL_VERSION_ATLEAST(1, 2, 14)
#define CAN_USE_LOCK 1
/* For lower versions of SDL we also use a slight hack to get the startup states of numclock and capslock right.
 * The proper way is in the mapper, but the repeating key is an unwanted side effect for lower versions of SDL */
#endif

static Bitu call_int16 = 0,call_irq1 = 0,irq1_ret_ctrlbreak_callback = 0,call_irq6 = 0,call_irq_pcjr_nmi = 0;
static uint8_t fep_line = 0x01;

/* Nice table from BOCHS i should feel bad for ripping this */
#define none 0
typedef struct {
  uint16_t normal;
  uint16_t shift;
  uint16_t control;
  uint16_t alt;
  uint16_t kana;
  uint16_t kana_shift;
} scancode_tbl;

static scancode_tbl scan_to_scanascii[MAX_SCAN_CODE + 1] = {
//      normal, shift , ctrl  , alt    ,kana  , kana_shift
      {   none,   none,   none,   none,   none,   none  },
      { 0x011b, 0x011b, 0x011b, 0x01f0 ,0x011b, 0x011b }, /*  1 escape */
      { 0x0231, 0x0221,   none, 0x7800 ,0x02c7, 0x0200 }, /*  2 1! */
      { 0x0332, 0x0340, 0x0300, 0x7900 ,0x03cc, 0x0300 }, /*  3 2@ */
      { 0x0433, 0x0423,   none, 0x7a00 ,0x04b1, 0x04a7 }, /*  4 3# */
      { 0x0534, 0x0524,   none, 0x7b00 ,0x05b3, 0x05a9 }, /*  5 4$ */
      { 0x0635, 0x0625,   none, 0x7c00 ,0x06b4, 0x06aa }, /*  6 5% */
      { 0x0736, 0x075e, 0x071e, 0x7d00 ,0x07b5, 0x07ab }, /*  7 6^ */
      { 0x0837, 0x0826,   none, 0x7e00 ,0x08d4, 0x08ac }, /*  8 7& */
      { 0x0938, 0x092a,   none, 0x7f00 ,0x09d5, 0x09ad }, /*  9 8* */
      { 0x0a39, 0x0a28,   none, 0x8000 ,0x0ad6, 0x0aae }, /* 0a 9( */
      { 0x0b30, 0x0b29,   none, 0x8100 ,0x0bdc, 0x0ba6 }, /* 0b 0) */
      { 0x0c2d, 0x0c5f, 0x0c1f, 0x8200 ,0x0cce, 0x0c00 }, /* 0c -_ */
      { 0x0d3d, 0x0d2b,   none, 0x8300 ,0x0dcd, 0x0d00 }, /* 0d =+ */
      { 0x0e08, 0x0e08, 0x0e7f, 0x0ef0 ,0x0e08, 0x0e08 }, /* 0e backspace */
      { 0x0f09, 0x0f00, 0x9400,   none ,0x0f09, 0x0f09 }, /* 0f tab */
      { 0x1071, 0x1051, 0x1011, 0x1000 ,0x10c0, 0x1000 }, /* 10 Q */
      { 0x1177, 0x1157, 0x1117, 0x1100 ,0x11c3, 0x1100 }, /* 11 W */
      { 0x1265, 0x1245, 0x1205, 0x1200 ,0x12b2, 0x12a8 }, /* 12 E */
      { 0x1372, 0x1352, 0x1312, 0x1300 ,0x13bd, 0x1300 }, /* 13 R */
      { 0x1474, 0x1454, 0x1414, 0x1400 ,0x14b6, 0x1400 }, /* 14 T */
      { 0x1579, 0x1559, 0x1519, 0x1500 ,0x15dd, 0x1500 }, /* 15 Y */
      { 0x1675, 0x1655, 0x1615, 0x1600 ,0x16c5, 0x1600 }, /* 16 U */
      { 0x1769, 0x1749, 0x1709, 0x1700 ,0x17c6, 0x1700 }, /* 17 I */
      { 0x186f, 0x184f, 0x180f, 0x1800 ,0x18d7, 0x1800 }, /* 18 O */
      { 0x1970, 0x1950, 0x1910, 0x1900 ,0x19be, 0x1900 }, /* 19 P */
      { 0x1a5b, 0x1a7b, 0x1a1b, 0x1af0 ,0x1ade, 0x1a00 }, /* 1a [{ */
      { 0x1b5d, 0x1b7d, 0x1b1d, 0x1bf0 ,0x1bdf, 0x1ba2 }, /* 1b ]} */
      { 0x1c0d, 0x1c0d, 0x1c0a,   none ,0x1c0d, 0x1c0d }, /* 1c Enter */
      {   none,   none,   none,   none ,   none,   none }, /*  1d L Ctrl */
      { 0x1e61, 0x1e41, 0x1e01, 0x1e00 ,0x1ec1, 0x1e00 }, /* 1e A */
      { 0x1f73, 0x1f53, 0x1f13, 0x1f00 ,0x1fc4, 0x1f00 }, /* 1f S */
      { 0x2064, 0x2044, 0x2004, 0x2000 ,0x20bc, 0x2000 }, /* 20 D */
      { 0x2166, 0x2146, 0x2106, 0x2100 ,0x21ca, 0x2100 }, /* 21 F */
      { 0x2267, 0x2247, 0x2207, 0x2200 ,0x22b7, 0x2200 }, /* 22 G */
      { 0x2368, 0x2348, 0x2308, 0x2300 ,0x23b8, 0x2300 }, /* 23 H */
      { 0x246a, 0x244a, 0x240a, 0x2400 ,0x24cf, 0x2400 }, /* 24 J */
      { 0x256b, 0x254b, 0x250b, 0x2500 ,0x25c9, 0x2500 }, /* 25 K */
      { 0x266c, 0x264c, 0x260c, 0x2600 ,0x26d8, 0x2600 }, /* 26 L */
      { 0x273b, 0x273a,   none, 0x27f0 ,0x27da, 0x2700 }, /* 27 ;: */
      { 0x2827, 0x2822,   none, 0x28f0 ,0x28b9, 0x2800 }, /* 28 '" */
      { 0x2960, 0x297e,   none, 0x29f0 ,0x29d1, 0x29a3 }, /* 29 `~ */ //29h `~ﾑ｣
      {   none,   none,   none,   none,   none,   none }, /*  2a L shift */
      { 0x2b5c, 0x2b7c, 0x2b1c, 0x2bf0 ,0x2bb0, 0x2bf0 }, /* 2b */ //2bh \|ｰ
      { 0x2c7a, 0x2c5a, 0x2c1a, 0x2c00 ,0x2cc2, 0x2caf }, /* 2c Z */
      { 0x2d78, 0x2d58, 0x2d18, 0x2d00 ,0x2dbb, 0x2d00 }, /* 2d X */
      { 0x2e63, 0x2e43, 0x2e03, 0x2e00 ,0x2ebf, 0x2e00 }, /* 2e C */
      { 0x2f76, 0x2f56, 0x2f16, 0x2f00 ,0x2fcb, 0x2f00 }, /* 2f V */
      { 0x3062, 0x3042, 0x3002, 0x3000 ,0x30ba, 0x3000 }, /* 30 B */
      { 0x316e, 0x314e, 0x310e, 0x3100 ,0x31d0, 0x3100 }, /* 31 N */
      { 0x326d, 0x324d, 0x320d, 0x3200 ,0x32d3, 0x3200 }, /* 32 M */
      { 0x332c, 0x333c,   none, 0x33f0 ,0x33c8, 0x33a4 }, /* 33 ,< */
      { 0x342e, 0x343e,   none, 0x34f0 ,0x34d9, 0x34a1 }, /* 34 .> */
      { 0x352f, 0x353f,   none, 0x35f0 ,0x35d2, 0x35a5 }, /* 35 /? */
      {   none,   none,   none,   none ,   none,   none }, /*  36 R Shift */
      { 0x372a, 0x372a, 0x9600, 0x37f0 , 0x372a, 0x3700 }, /* 37 * */
      {   none,   none,   none,   none ,   none,   none }, /*  38 L Alt */
      { 0x3920, 0x3920, 0x3920, 0x3920 , 0x3920, 0x3920 }, /* 39 space */
      {   none,   none,   none,   none ,   none,   none }, /*  3a caps lock */
//    { 0x3a00, 0x3a00,   none,   none , 0x3a00, 0x3a00 }, /* 3a Kanji */
      { 0x3b00, 0x5400, 0x5e00, 0x6800 ,0x3b00, 0x5400 }, /* 3b F1 */
      { 0x3c00, 0x5500, 0x5f00, 0x6900 ,0x3c00, 0x5500 }, /* 3c F2 */
      { 0x3d00, 0x5600, 0x6000, 0x6a00 ,0x3d00, 0x5600 }, /* 3d F3 */
      { 0x3e00, 0x5700, 0x6100, 0x6b00 ,0x3e00, 0x5700 }, /* 3e F4 */
      { 0x3f00, 0x5800, 0x6200, 0x6c00 ,0x3f00, 0x5800 }, /* 3f F5 */
      { 0x4000, 0x5900, 0x6300, 0x6d00 ,0x4000, 0x5900 }, /* 40 F6 */
      { 0x4100, 0x5a00, 0x6400, 0x6e00 ,0x4100, 0x5a00 }, /* 41 F7 */
      { 0x4200, 0x5b00, 0x6500, 0x6f00 ,0x4200, 0x5b00 }, /* 42 F8 */
      { 0x4300, 0x5c00, 0x6600, 0x7000 ,0x4300, 0x5c00 }, /* 43 F9 */
      { 0x4400, 0x5d00, 0x6700, 0x7100 ,0x4400, 0x5d00 }, /* 44 F10 */
      {   none,   none,   none,   none ,   none,   none }, /*  45 Num Lock */
      {   none,   none,   none,   none ,   none,   none }, /*  46 Scroll Lock */
      { 0x4700, 0x4737, 0x7700, 0x0007 ,0x4700, 0x4737 }, /* 47 7 Home */
      { 0x4800, 0x4838, 0x8d00, 0x0008 ,0x4800, 0x4838 }, /* 48 8 UP */
      { 0x4900, 0x4939, 0x8400, 0x0009 ,0x4900, 0x4939 }, /* 49 9 PgUp */
      { 0x4a2d, 0x4a2d, 0x8e00, 0x4af0 ,0x4a00, 0x4a2d }, /* 4a - */
      { 0x4b00, 0x4b34, 0x7300, 0x0004 ,0x4b00, 0x4b34 }, /* 4b 4 Left */
      { 0x4cf0, 0x4c35, 0x8f00, 0x0005 ,0x4c00, 0x4c35 }, /* 4c 5 */
      { 0x4d00, 0x4d36, 0x7400, 0x0006 ,0x4d00, 0x4d36 }, /* 4d 6 Right */
      { 0x4e2b, 0x4e2b, 0x9000, 0x4ef0 ,0x4e00, 0x4e2b }, /* 4e + */
      { 0x4f00, 0x4f31, 0x7500, 0x0001 ,0x4f00, 0x4f31 }, /* 4f 1 End */
      { 0x5000, 0x5032, 0x9100, 0x0002 ,0x5000, 0x5032 }, /* 50 2 Down */
      { 0x5100, 0x5133, 0x7600, 0x0003 ,0x5100, 0x5133 }, /* 51 3 PgDn */
      { 0x5200, 0x5230, 0x9200, 0x0000 ,0x5200, 0x5230 }, /* 52 0 Ins */
      { 0x5300, 0x532e, 0x9300,   none ,0x5300, 0x532e }, /* 53 Del */
      {   none,   none,   none,   none ,   none,   none },
      {   none,   none,   none,   none ,   none,   none },
//	  { 0x565c, 0x567c,   none,   none ,0x565c, 0x567c }, /* 56 (102-key) */
//      normal, shift , ctrl  , alt    ,kana  , kana_shift
      { 0x565c, 0x567c,   none,   none   ,0x56db, 0x56f0 }, /* 56 (102-key), \_ロ| for AX */ //56h \|ﾛ underscore
      { 0x8500, 0x8700, 0x8900, 0x8b00 ,0x8500, 0x8700 }, /* 57 F11 */
      { 0x8600, 0x8800, 0x8a00, 0x8c00 ,0x8600, 0x8800 },  /* 58 F12 */
	  { 0xab00, 0xac00, 0xad00, 0xae00, 0xab00, 0xac00 },  /* 5a 無変換 (non-conversion) for AX */
	  { 0xa700, 0xa800, 0xa900, 0xaa00, 0xa700, 0xa800 },  /* 5b 変換 (conversion) for AX */
	  { 0xd200, 0xd300, 0xd400, 0xd500, 0xd200, 0xd300 }   /* 5c AX (additional function key) for AX */
      };

static scancode_tbl scan_to_scanascii_pc98[0x80] = {
    //normal,  shift,   CTRL, GRPH, Kana, Kana+shift
    { 0x001b, 0x001b, 0x0016, 0x001b, 0x001b, 0x001b }, /* 00 escape */
    { 0x0131, 0x0121,   none,   none, 0x01c7, 0x01c7 }, /* 01 1! */
    { 0x0232, 0x0222,   none,   none, 0x02cc, 0x02cc }, /* 02 2" */
    { 0x0333, 0x0323,   none,   none, 0x03b1, 0x03a7 }, /* 03 3# */
    { 0x0434, 0x0424,   none,   none, 0x04b3, 0x04a9 }, /* 04 4$ */
    { 0x0535, 0x0525,   none, 0x05f2, 0x05b4, 0x05aa }, /* 05 5% */
    { 0x0636, 0x0626,   none, 0x06f3, 0x06b5, 0x06ab }, /* 06 6& */
    { 0x0737, 0x0727,   none, 0x07f4, 0x07d4, 0x07ac }, /* 07 7' */
    { 0x0838, 0x0828,   none, 0x08f5, 0x08d5, 0x08ad }, /* 08 8( */
    { 0x0939, 0x0929,   none, 0x09f6, 0x09d6, 0x09ae }, /* 09 9) */
    { 0x0a30, 0x0a30,   none, 0x0af7, 0x0adc, 0x0aa6 }, /* 0a 0  */
    { 0x0b2d, 0x0b3d,   none, 0x0b8c, 0x0bce, 0x0bce }, /* 0b -= */
    { 0x0c5e, 0x0c60, 0x0c1e, 0x0c8b, 0x0ccd, 0x0ccd }, /* 0c ^~ */
    { 0x0d5c, 0x0d7c, 0x0d1c, 0x0df1, 0x0db0, 0x0db0 }, /* 0d \| */
    { 0x0e08, 0x0e08, 0x0e08, 0x0e08, 0x0e08, 0x0e08 }, /* 0e backspace */
    { 0x0f09, 0x0f09, 0x0f09, 0x0f09, 0x0f09, 0x0f09 }, /* 0f tab */
    //normal, shift,CTRL, GRPH, Kana, Kana+shift
    { 0x1071, 0x1051, 0x1011, 0x109c, 0x10c0, 0x10c0 }, /* 10 Q */
    { 0x1177, 0x1157, 0x1117, 0x119d, 0x11c3, 0x11c3 }, /* 11 W */
    { 0x1265, 0x1245, 0x1205, 0x12e4, 0x12b2, 0x12a8 }, /* 12 E */
    { 0x1372, 0x1352, 0x1312, 0x13e5, 0x13bd, 0x13bd }, /* 13 R */
    { 0x1474, 0x1454, 0x1414, 0x14ee, 0x14b6, 0x14b6 }, /* 14 T */
    { 0x1579, 0x1559, 0x1519, 0x15ef, 0x15dd, 0x15dd }, /* 15 Y */
    { 0x1675, 0x1655, 0x1615, 0x16f0, 0x16c5, 0x16c5 }, /* 16 U */
    { 0x1769, 0x1749, 0x1709, 0x17e8, 0x17c6, 0x17c6 }, /* 17 I */
    { 0x186f, 0x184f, 0x180f, 0x18e9, 0x18d7, 0x18c7 }, /* 18 O */
    { 0x1970, 0x1950, 0x1910, 0x198d, 0x19be, 0x19be }, /* 19 P */
    { 0x1a40, 0x1a7e, 0x1a00, 0x1a8a, 0x1ade, 0x1ade }, /* 1a [{ */
    { 0x1b5b, 0x1b7b, 0x1b1b,   none, 0x1bdf, 0x1ba2 }, /* 1b ]} */
    { 0x1c0d, 0x1c0d, 0x1c0d, 0x1c0d, 0x1c0d, 0x1c0d }, /* 1c Enter */
    { 0x1d61, 0x1d41, 0x1d01, 0x1d9e, 0x1dc1, 0x1dc1 }, /* 1d A */
    { 0x1e73, 0x1e53, 0x1e13, 0x1e9f, 0x1ec4, 0x1ec4 }, /* 1e S */
    { 0x1f64, 0x1f44, 0x1f04, 0x1fe6, 0x1fbc, 0x1fbc }, /* 1f D */
    //normal,  shift,   CTRL,   GRPH, Kana, Kana+shift
    { 0x2066, 0x2046, 0x2006, 0x20e7, 0x20ca, 0x20ca }, /* 20 F */
    { 0x2167, 0x2147, 0x2107, 0x21ec, 0x21b7, 0x21b7 }, /* 21 G */
    { 0x2268, 0x2248, 0x2208, 0x22ed, 0x22b8, 0x22b8 }, /* 22 H */
    { 0x236a, 0x234a, 0x230a, 0x23ea, 0x23cf, 0x23cf }, /* 23 J */
    { 0x246b, 0x244b, 0x240b, 0x24eb, 0x24c9, 0x24c9 }, /* 24 K */
    { 0x256c, 0x254c, 0x250c, 0x258e, 0x25d8, 0x25d8 }, /* 25 L */
    { 0x263b, 0x262b,   none, 0x2689, 0x26da, 0x26da }, /* 26 ;: */
    { 0x273a, 0x272a,   none, 0x279f, 0x27b9, 0x27b9 }, /* 27 '" */
    { 0x285d, 0x287d, 0x281d,   none, 0x28d1, 0x28a3 }, /* 28 `~ */ //29h `~ﾑ｣
    { 0x297a, 0x295a, 0x291a, 0x2980, 0x29c2, 0x29af }, /* 29 Z */
    { 0x2a78, 0x2a58, 0x2a18, 0x2a81, 0x2abb, 0x2abb }, /* 2a X */
    { 0x2b63, 0x2b43, 0x2b03, 0x2b82, 0x2bbf, 0x2bbf }, /* 2b C */
    { 0x2c76, 0x2c56, 0x2c16, 0x2c83, 0x2ccb, 0x2ccb }, /* 2c V */
    { 0x2d62, 0x2d42, 0x2d02, 0x2d84, 0x2dba, 0x2dba }, /* 2d B */
    { 0x2e6e, 0x2e4e, 0x2e0e, 0x2e85, 0x2ed0, 0x2ed0 }, /* 2e N */
    { 0x2f6d, 0x2f4d, 0x2f0d, 0x2f86, 0x2fd3, 0x2fd3 }, /* 2f M */
    //normal,  shift,   CTRL,   GRPH, Kana, Kana+shift
    { 0x302c, 0x303c,   none, 0x30f0, 0x30c8, 0x30a4 }, /* 30 ,< */
    { 0x312e, 0x313e,   none, 0x31f0, 0x31d9, 0x31a1 }, /* 31 .> */
    { 0x322f, 0x323f,   none, 0x32f0, 0x32d2, 0x32a5 }, /* 32 /? */
    {   none, 0x335f, 0x331f,   none, 0x33db, 0x33db }, /* 33  _ */
    { 0x3420, 0x3420, 0x3420, 0x3420, 0x3420, 0x3420 }, /* 34 space */
    { 0x3500, 0xa500, 0xb500, 0x3500, 0x3500, 0xa500 }, /* 35 XFER */
    { 0x3600, 0x3600, 0x3600, 0x3600, 0x3600, 0x3600 }, /* 36 ROLL UP */
    { 0x3700, 0x3700, 0x3700, 0x3700, 0x3700, 0x3700 }, /* 37 ROLL DOWN */
    { 0x3800, 0x3800, 0x3800, 0x3800, 0x3800, 0x3800 }, /* 38 INS */
    { 0x3900, 0x3900, 0x3900, 0x3900, 0x3900, 0x3900 }, /* 39 DEL */
    { 0x3a00, 0x3a00, 0x3a00, 0x3a00, 0x3a00, 0x3a00 }, /* 3a up arrow */
    { 0x3b00, 0x3b00, 0x3b00, 0x3b00, 0x3b00, 0x3b00 }, /* 3b left arrow */
    { 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00 }, /* 3c right arrow */
    { 0x3d00, 0x3d00, 0x3d00, 0x3d00, 0x3d00, 0x3d00 }, /* 3d down arrow */
    { 0x3e00, 0xae00,   none,   none, 0x3e00, 0xae00 }, /* 3e HOME CLR */
    { 0x3f00, 0x3f00, 0x3f00, 0x3f00, 0x3f00, 0x3f00 }, /* 3f HELP */
    //normal,  shift,   CTRL,   GRPH, Kana, Kana+shift
    { 0x402d, 0x402d, 0x402d, 0x402d, 0x402d, 0x402d }, /* 40 - */
    { 0x412f, 0x412f, 0x412f, 0x412f, 0x412f, 0x412f }, /* 41 / */
    { 0x4237, 0x4237, 0x4237, 0x4237, 0x4237, 0x4237 }, /* 42 7 */
    { 0x4338, 0x4338, 0x4338, 0x4338, 0x4338, 0x4338 }, /* 43 8 */
    { 0x4439, 0x4439, 0x4439, 0x4439, 0x4439, 0x4439 }, /* 44 9 */
    { 0x452a, 0x452a, 0x452a, 0x452a, 0x452a, 0x452a }, /* 45 * */
    { 0x4634, 0x4634, 0x4634, 0x4634, 0x4634, 0x4634 }, /* 46 4 */
    { 0x4735, 0x4735, 0x4735, 0x4735, 0x4735, 0x4735 }, /* 47 5 */
    { 0x4836, 0x4836, 0x4836, 0x4836, 0x4836, 0x4836 }, /* 48 6 */
    { 0x492b, 0x492b, 0x492b, 0x492b, 0x492b, 0x492b }, /* 49 + */
    { 0x4a31, 0x4a31, 0x4a31, 0x4a31, 0x4a31, 0x4a31 }, /* 4a 1 */
    { 0x4b32, 0x4b32, 0x4b32, 0x4b32, 0x4b32, 0x4b32 }, /* 4b 2 */
    { 0x4c33, 0x4c33, 0x4c33, 0x4c33, 0x4c33, 0x4c33 }, /* 4c 3 */
    { 0x4d3d, 0x4d3d, 0x4d3d, 0x4d3d, 0x4d3d, 0x4d3d }, /* 4d = */
    { 0x4e30, 0x4e30, 0x4e30, 0x4e30, 0x4e30, 0x4e30 }, /* 4e 0 */
    { 0x4f2c, 0x4f2c, 0x4f2c, 0x4f2c, 0x4f2c, 0x4f2c }, /* 4f , */
    //normal,  shift,   CTRL,   GRPH, Kana, Kana+shift
    { 0x502e, 0x502e, 0x502e, 0x509e, 0x502e, 0x502e }, /* 50 .    */
    { 0x5100, 0xa100, 0xb100, 0x5100, 0x5100, 0xa100 }, /* 51 NFER */
    { 0x5200, 0xc200, 0x5200, 0xd200, 0x5200, 0xc200 }, /* 52 vf1  */
    { 0x5300, 0xc300, 0x5300, 0xd300, 0x5300, 0xc300 }, /* 53 vf2  */
    { 0x5400, 0xc400, 0x5400, 0xd400, 0x5400, 0xc400 }, /* 54 vf3  */
    { 0x5500, 0xc500, 0x5500, 0xd500, 0x5500, 0xc500 }, /* 55 vf4  */
    { 0x5600, 0xc600, 0x5600, 0xd600, 0x5600, 0xc600 }, /* 56 vf5  */
    {   none,   none,   none,   none,   none,   none }, /* 57      */
    {   none,   none,   none,   none,   none,   none }, /* 58      */
    {   none,   none,   none,   none,   none,   none }, /* 59      */
    {   none,   none,   none,   none,   none,   none }, /* 5a      */
    {   none,   none,   none,   none,   none,   none }, /* 5b      */
    {   none,   none,   none,   none,   none,   none }, /* 5c      */
    {   none,   none,   none,   none,   none,   none }, /* 5d      */
    {   none,   none,   none,   none,   none,   none }, /* 5e      */
    {   none,   none,   none,   none,   none,   none }, /* 5f      */
    //normal,  shift,   CTRL,   GRPH, Kana, Kana+shift
    {   none,   none,   none,   none,   none,   none }, /* 60 STOP */
    {   none,   none,   none,   none,   none,   none }, /* 61 COPY */
    { 0x6200, 0x8200, 0x9200,   none, 0x6200, 0x8200 }, /* 62 f1   */
    { 0x6300, 0x8300, 0x9300,   none, 0x6300, 0x8300 }, /* 63 f2   */
    { 0x6400, 0x8400, 0x9400,   none, 0x6400, 0x8400 }, /* 64 f3   */
    { 0x6500, 0x8500, 0x9500,   none, 0x6500, 0x8500 }, /* 65 f4   */
    { 0x6600, 0x8600, 0x9600,   none, 0x6600, 0x8600 }, /* 66 f5   */
    { 0x6700, 0x8700, 0x9700,   none, 0x6700, 0x8700 }, /* 67 f6   */
    { 0x6800, 0x8800, 0x9800,   none, 0x6800, 0x8800 }, /* 68 f7   */
    { 0x6900, 0x8900, 0x9900,   none, 0x6900, 0x8900 }, /* 69 f8   */
    { 0x6a00, 0x8a00, 0x9a00,   none, 0x6a00, 0x8a00 }, /* 6a f9   */
    { 0x6b00, 0x8b00, 0x9b00,   none, 0x6b00, 0x8b00 }, /* 6b f10  */
    {   none,   none,   none,   none,   none,   none }, /* 6c      */
    {   none,   none,   none,   none,   none,   none }, /* 6d      */
    {   none,   none,   none,   none,   none,   none }, /* 6e      */
    {   none,   none,   none,   none,   none,   none }, /* 6f      */
    {   none,   none,   none,   none,   none,   none }, /* 70 SHIFT*/
    {   none,   none,   none,   none,   none,   none }, /* 71 CAPS */
    {   none,   none,   none,   none,   none,   none }, /* 72 KANA */
    {   none,   none,   none,   none,   none,   none }, /* 73 GRAPH*/
    {   none,   none,   none,   none,   none,   none }, /* 74 CTRL */
    {   none,   none,   none,   none,   none,   none }, /* 75      */
    {   none,   none,   none,   none,   none,   none }, /* 76      */
    {   none,   none,   none,   none,   none,   none }, /* 77      */
    {   none,   none,   none,   none,   none,   none }, /* 78      */
    {   none,   none,   none,   none,   none,   none }, /* 79      */
    {   none,   none,   none,   none,   none,   none }, /* 7a      */
    {   none,   none,   none,   none,   none,   none }, /* 7b      */
    {   none,   none,   none,   none,   none,   none }, /* 7c      */
    {   none,   none,   none,   none,   none,   none }, /* 7d      */
    {   none,   none,   none,   none,   none,   none }, /* 7e      */
    {   none,   none,   none,   none,   none,   none }  /* 7f      */
};

std::queue <uint16_t>over_key_buffer;

extern bool inshell;
#if defined(USE_TTF)
extern bool ttf_dosv;
#endif

bool BIOS_AddKeyToBuffer(uint16_t code) {
    if (!IS_PC98_ARCH) {
        if (mem_readb(BIOS_KEYBOARD_FLAGS2)&8) return true;
    }

    uint16_t start,end,head,tail,ttail;
    if (IS_PC98_ARCH) {
        start=0x502;
        end=0x522;
    }
    else if (machine==MCH_PCJR || machine==MCH_CGA) {
        /* should be done for others as well, to be tested */
        start=0x1e;
        end=0x3e;
    } else {
        start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
        end  =mem_readw(BIOS_KEYBOARD_BUFFER_END);
    }

    if (IS_PC98_ARCH) {
        head =mem_readw(0x524/*head*/);
        tail =mem_readw(0x526/*tail*/);
    }
    else {
        head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
        tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
    }

    ttail=tail+2;
    if (ttail>=end) {
        ttail=start;
    }
    if(J3_IsJapanese()) {
        if((code & 0xff) == 0xe0) {
            if((code & 0xff00) >= 0x3b00 && (code & 0xff00) <= 0x5300) {
                code &= 0xff00;
            } else if((code & 0xff00) == 0x7300) {
                code = 0x4b00;
            } else if((code & 0xff00) == 0x7400) {
                code = 0x4d00;
            } else if((code & 0xff00) == 0x7500) {
                code = 0x4f00;
            } else if((code & 0xff00) == 0x7600) {
                code = 0x4900;
            } else if((code & 0xff00) == 0x7700) {
                code = 0x4700;
            } else if((code & 0xff00) == 0x8400) {
                code = 0x5100;
            } else if((code & 0xff00) == 0x8d00) {
                code = 0x4800;
            } else if((code & 0xff00) == 0x9100) {
                code = 0x5000;
            } else if((code & 0xff00) == 0x9200) {
                code = 0x5200;
            } else if((code & 0xff00) == 0x9300) {
                code = 0x5300;
            }
        }
    }
    /* Check for buffer Full */
    //TODO Maybe beeeeeeep or something although that should happened when internal buffer is full
    if (ttail==head) {
#if defined(USE_TTF)
        if(IS_DOSV || ttf_dosv) {
#else
        if(IS_DOSV) {
#endif
            over_key_buffer.push(code);
        }
        return false;
    }

    if (IS_PC98_ARCH) {
        real_writew(0x0,tail,code);
        mem_writew(0x526/*tail*/,ttail);

        /* PC-98 BIOSes also manage a key counter, which is required for
         * some games and even the PC-98 version of MS-DOS to detect keyboard input */
        unsigned char b = real_readw(0,0x528);
        real_writew(0,0x528,b+1);
    }
    else {
        real_writew(0x40,tail,code);
        mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,ttail);
    }

    return true;
}

static void add_key(uint16_t code) {
    if (code!=0 || IS_PC98_ARCH) BIOS_AddKeyToBuffer(code);
}

static bool get_key(uint16_t &code) {
    uint16_t start,end,head,tail,thead;
    if (IS_PC98_ARCH) {
        start=0x502;
        end=0x522;
    }
    else if (machine==MCH_PCJR || machine==MCH_CGA) {
        /* should be done for others as well, to be tested */
        start=0x1e;
        end=0x3e;
    } else {
        start=mem_readw(BIOS_KEYBOARD_BUFFER_START);
        end  =mem_readw(BIOS_KEYBOARD_BUFFER_END);
    }

    if (IS_PC98_ARCH) {
        head =mem_readw(0x524/*head*/);
        tail =mem_readw(0x526/*tail*/);

        /* PC-98 BIOSes also manage a key counter, which is required for
         * some games and even the PC-98 version of MS-DOS to detect keyboard input */
        unsigned char b = real_readw(0,0x528);
        if (b != 0) real_writew(0,0x528,b-1);
    }
    else {
        head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
        tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
    }

    if (head==tail) return false;
    thead=head+2;
    if (thead>=end) thead=start;

    if (IS_PC98_ARCH) {
        mem_writew(0x524/*head*/,thead);
        code = real_readw(0x0,head);
    }
    else {
        mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,thead);
        code = real_readw(0x40,head);
    }
#if defined(USE_TTF)
    if(IS_DOSV || ttf_dosv) {
#else
    if(IS_DOSV) {
#endif
        if (!over_key_buffer.empty()) {
            add_key(over_key_buffer.front());
            over_key_buffer.pop();
        }
    }

    return true;
}

bool INT16_get_key(uint16_t &code) {
    return get_key(code);
}

static bool check_key(uint16_t &code) {
    uint16_t head,tail;

    if (IS_PC98_ARCH) {
        head =mem_readw(0x524/*head*/);
        tail =mem_readw(0x526/*tail*/);
    }
    else {
        head =mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
        tail =mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
    }

    if (head==tail) return false;

    if (IS_PC98_ARCH)
        code = real_readw(0x0,head);
    else
        code = real_readw(0x40,head);

    return true;
}

void trimKana(void) {
    uint8_t kana_status = mem_readb(BIOS_KEYBOARD_AX_KBDSTATUS);
    if (kana_status & 0x02) { //Kana locked
        LOG(LOG_KEYBOARD, LOG_NORMAL)("Kana shift OFF");
        mem_writeb(BIOS_KEYBOARD_AX_KBDSTATUS, 0xfc & kana_status);//Kana status LED=on and Shift=on
    }
    else {
        LOG(LOG_KEYBOARD, LOG_NORMAL)("Kana shift ON");
        mem_writeb(BIOS_KEYBOARD_AX_KBDSTATUS, 0x03 | kana_status);//Kana status LED=on and Shift=on
    }
}

bool INT16_peek_key(uint16_t &code) {
    return check_key(code);
}

static void empty_keyboard_buffer() {
    mem_writew(BIOS_KEYBOARD_BUFFER_TAIL, mem_readw(BIOS_KEYBOARD_BUFFER_HEAD));
}

    /*  Flag Byte 1 
        bit 7 =1 INSert active
        bit 6 =1 Caps Lock active
        bit 5 =1 Num Lock active
        bit 4 =1 Scroll Lock active
        bit 3 =1 either Alt pressed
        bit 2 =1 either Ctrl pressed
        bit 1 =1 Left Shift pressed
        bit 0 =1 Right Shift pressed
    */
    /*  Flag Byte 2
        bit 7 =1 INSert pressed
        bit 6 =1 Caps Lock pressed
        bit 5 =1 Num Lock pressed
        bit 4 =1 Scroll Lock pressed
        bit 3 =1 Pause state active
        bit 2 =1 Sys Req pressed
        bit 1 =1 Left Alt pressed
        bit 0 =1 Left Ctrl pressed
    */
    /* 
        Keyboard status byte 3
        bit 7 =1 read-ID in progress
        bit 6 =1 last code read was first of two ID codes
        bit 5 =1 force Num Lock if read-ID and enhanced keyboard
        bit 4 =1 enhanced keyboard installed
        bit 3 =1 Right Alt pressed
        bit 2 =1 Right Ctrl pressed
        bit 1 =1 last code read was E0h
        bit 0 =1 last code read was E1h
    */


void KEYBOARD_SetLEDs(uint8_t bits);
bool ctrlbrk=false;

/* the scancode is in reg_al */
static Bitu IRQ1_Handler(void) {
/* handling of the locks key is difficult as sdl only gives
 * states for numlock capslock. 
 */
    Bitu scancode=reg_al;
    uint8_t flags1,flags2,flags3,kanafl,leds,leds_orig;
    flags1=mem_readb(BIOS_KEYBOARD_FLAGS1);
    flags2=mem_readb(BIOS_KEYBOARD_FLAGS2);
    flags3=mem_readb(BIOS_KEYBOARD_FLAGS3);
    kanafl= mem_readb(BIOS_KEYBOARD_AX_KBDSTATUS);
    leds  =mem_readb(BIOS_KEYBOARD_LEDS); 
    leds_orig = leds;
#ifdef CAN_USE_LOCK
    /* No hack anymore! */
#else
    flags2&=~(0x40+0x20);//remove numlock/capslock pressed (hack for sdl only reporting states)
#endif
    if (DOS_LayoutKey(scancode,flags1,flags2,flags3)) return CBRET_NONE;
//LOG_MSG("key input %d %d %d %d",scancode,flags1,flags2,flags3);
    switch (scancode) {
    /* First the hard ones  */
    case 0xfa:  /* Acknowledge */
        leds |= 0x10;
        break;
    case 0xe1:  /* Extended key special. Only pause uses this */
        flags3 |=0x01;
        break;
    case 0xe0:                      /* Extended key */
        flags3 |=0x02;
        break;
    case 0x1d:                      /* Ctrl Pressed */
        if (INT16_AX_GetKBDBIOSMode() == 0x51 && (flags3 & 0x02))
            trimKana();//英数カナ keycode not assignged in AX
        else if (!(flags3 &0x01)) {
            flags1 |=0x04;
            if (flags3 &0x02) flags3 |=0x04;
            else flags2 |=0x01;
        }   /* else it's part of the pause scancodes */
        break;
    case 0x9d:                      /* Ctrl Released */
        if (INT16_AX_GetKBDBIOSMode() == 0x51 && (flags3 & 0x02)) break;// for AX: Skip if AX KBDBIOS is in JP mode (for AX)
        if (!(flags3 &0x01)) {
            if (flags3 &0x02) flags3 &=~0x04;
            else flags2 &=~0x01;
            if( !( (flags3 &0x04) || (flags2 &0x01) ) ) flags1 &=~0x04;
        }
        break;
    case 0x2a:                      /* Left Shift Pressed */
        flags1 |=0x02;
        break;
    case 0xaa:                      /* Left Shift Released */
        flags1 &=~0x02;
        break;
    case 0x36:                      /* Right Shift Pressed */
        flags1 |=0x01;
        break;
    case 0xb6:                      /* Right Shift Released */
        flags1 &=~0x01;
        break;
    case 0x37:						/* Keypad * or PrtSc Pressed */
        if (!(flags3 & 0x02)) goto normal_key;
        reg_ip += 7; // call int 5
        break;
    case 0xb7:						/* Keypad * or PrtSc Released */
        if (!(flags3 & 0x02)) goto normal_key;
        break;
    case 0x38:                      /* Alt Pressed */
        /* Key      Scan   Ch Sh Ct Al US mode
           漢字   - E0 38  3A 3A -  -  RAlt	*/
        if (INT16_AX_GetKBDBIOSMode() == 0x51 && (flags3 & 0x02)) {// for AX: Is AX KBDBIOS in JP mode AND R-Alt pressed?
            if (!(flags1 & 0x0c))//neither Shift nor Ctrl pressed?
                add_key(0x3a00);//add 漢字 (Kanji) to key buffer
        } else {
            flags1 |=0x08;
            if (flags3 &0x02) flags3 |=0x08;
            else flags2 |=0x02;
        }
        break;
    case 0xb8:                      /* Alt Released */
        if (INT16_AX_GetKBDBIOSMode() == 0x51 && (flags3 & 0x02)) break;// for AX: Skip if AX KBDBIOS is in JP mode (for AX)
        if (flags3 &0x02) flags3 &= ~0x08;
        else flags2 &= ~0x02;
        if( !( (flags3 &0x08) || (flags2 &0x02) ) ) { /* Both alt released */
            flags1 &= ~0x08;
            uint16_t token =mem_readb(BIOS_KEYBOARD_TOKEN);
            if(token != 0){
                add_key(token);
                mem_writeb(BIOS_KEYBOARD_TOKEN,0);
            }
        }
        break;

#ifdef CAN_USE_LOCK
    case 0x3a:flags2 |=0x40;break;//CAPSLOCK
    case 0xba:flags1 ^=0x40;flags2 &=~0x40;leds ^=0x04;break;
#else
    case 0x3a:flags2 |=0x40;flags1 |=0x40;leds |=0x04;break; //SDL gives only the state instead of the toggle                   /* Caps Lock */
    case 0xba:flags1 &=~0x40;leds &=~0x04;break;
#endif
    case 0x45:
        /* if it has E1 prefix or is Ctrl-NumLock on non-enhanced keyboard => Pause */
        if ((flags3 &0x01) || (!(flags3&0x10) && (flags1&0x04))) {
            /* last scancode of pause received; first remove 0xe1-prefix */
            flags3 &=~0x01;
            mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
            if ((flags2&8)==0) {
                /* normal pause key, enter loop */
                mem_writeb(BIOS_KEYBOARD_FLAGS2,flags2|8);
                IO_Write(0x20,0x20);
                while (mem_readb(BIOS_KEYBOARD_FLAGS2)&8) CALLBACK_Idle();  // pause loop
                reg_ip+=5;  // skip out 20,20
                return CBRET_NONE;
            }
        } else {
            /* Num Lock */
#ifdef CAN_USE_LOCK
            flags2 |=0x20;
#else
            flags2 |=0x20;
            flags1 |=0x20;
            leds |=0x02;
#endif
        }
        break;
    case 0xc5:
        if ((flags3 &0x01) || (!(flags3&0x10) && (flags1&0x04))) {
            /* pause released */
            flags3 &=~0x01;
        } else {
#ifdef CAN_USE_LOCK
            flags1^=0x20;
            leds^=0x02;
            flags2&=~0x20;
#else
            /* Num Lock released */
            flags1 &=~0x20;
            leds &=~0x02;
#endif
        }
        break;
    case 0x46:                      /* Scroll Lock or Ctrl-Break */
        /* if it has E0 prefix, or is Ctrl-NumLock on non-enhanced keyboard => Break */
        if((flags3&0x02) || (!(flags3&0x10) && (flags1&0x04))) {                /* Ctrl-Break? */
            ctrlbrk=true;
            /* remove 0xe0-prefix */
            flags3 &=~0x02;
            mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
            mem_writeb(BIOS_CTRL_BREAK_FLAG,0x80);
            empty_keyboard_buffer();
            /* NTS: Real hardware shows that, at least through INT 16h, CTRL+BREAK injects
             *      scan code word 0x0000 into the keyboard buffer. Upon CTRL+BREAK, 0x0000
             *      appears in the keyboard buffer, which INT 16h AH=1h/AH=11h will signal as
             *      an available scan code, and INT 16h AH=0h/10h will return with ZF=0. */
            BIOS_AddKeyToBuffer(0);
            SegSet16(cs, RealSeg(CALLBACK_RealPointer(irq1_ret_ctrlbreak_callback)));
            reg_ip = RealOff(CALLBACK_RealPointer(irq1_ret_ctrlbreak_callback));
            return CBRET_NONE;
        } else {                                        /* Scroll Lock. */
            flags2 |=0x10;              /* Scroll Lock SDL Seems to do this one fine (so break and make codes) */
        }
        break;
    case 0xc6:
        if((flags3&0x02) || (!(flags3&0x10) && (flags1&0x04))) {                /* Ctrl-Break released? */
            /* nothing to do */
        } else {
            flags1 ^=0x10;flags2 &=~0x10;leds ^=0x01;break;     /* Scroll Lock released */
        }
        break;
    case 0xd2: /* NUMPAD insert, ironically, regular one is handled by 0x52 */
		if (flags3 & BIOS_KEYBOARD_FLAGS3_HIDDEN_E0 || !(flags1 & BIOS_KEYBOARD_FLAGS1_NUMLOCK_ACTIVE))
		{
			flags1 ^= BIOS_KEYBOARD_FLAGS1_INSERT_ACTIVE;
			flags2 &= BIOS_KEYBOARD_FLAGS2_INSERT_PRESSED;
		}
	    break; 
    case 0x47:      /* Numpad */
    case 0x48:
    case 0x49:
    case 0x4b:
    case 0x4c:
    case 0x4d:
    case 0x4f:
    case 0x50:
    case 0x51:
    case 0x52:
    case 0x53: /* del . Not entirely correct, but works fine */
        if (scancode == 0x53 && !(flags3 & 0x01) && !(flags1 & 0x03) && (flags1 & 0x0c) == 0x0c && ((!(flags3 & 0x10) && (flags3 & 0x0c) == 0x0c) || ((flags3 & 0x10) && (flags2 & 0x03) == 0x03))) { /* Ctrl-Alt-Del? */
			throw int(3);
		}
        if(flags3 &0x02) {  /*extend key. e.g key above arrows or arrows*/
            if(scancode == 0x52) flags2 |=0x80; /* press insert */         
            if(flags1 &0x08) {
                add_key(scan_to_scanascii[scancode].normal+0x5000);
            } else if (flags1 &0x04) {
                add_key((scan_to_scanascii[scancode].control&0xff00)|0xe0);
            } else if( ((flags1 &0x3) != 0) || ((flags1 &0x20) != 0) ) { //Due to |0xe0 results are identical. 
                add_key((scan_to_scanascii[scancode].shift&0xff00)|0xe0);
            } else add_key((scan_to_scanascii[scancode].normal&0xff00)|0xe0);
            break;
        }
        if(flags1 &0x08) {
            uint8_t token = mem_readb(BIOS_KEYBOARD_TOKEN);
            token = token*10 + (uint8_t)(scan_to_scanascii[scancode].alt&0xff);
            mem_writeb(BIOS_KEYBOARD_TOKEN,token);
        } else if (flags1 &0x04) {
            add_key(scan_to_scanascii[scancode].control);
        } else if( ((flags1 &0x3) != 0) ^ ((flags1 &0x20) != 0) ) { //Xor shift and numlock (both means off)
            add_key(scan_to_scanascii[scancode].shift);
        } else add_key(scan_to_scanascii[scancode].normal);
        break;

    default: /* Normal Key */
        if (scancode==0x2e && !(flags3 & 0x01) && (flags1&0x04))
            ctrlbrk=true;
    normal_key:
        uint16_t asciiscan;
        /* Now Handle the releasing of keys and see if they match up for a code */
        /* Handle the actual scancode */
        if (scancode & 0x80) goto irq1_end;
        if (scancode > MAX_SCAN_CODE) goto irq1_end;
        if (flags1 & 0x08) {                    /* Alt is being pressed */
            asciiscan=scan_to_scanascii[scancode].alt;
#if 0 /* old unicode support disabled*/
        } else if (ascii) {
            asciiscan=(scancode << 8) | ascii;
#endif
        } else if (flags1 & 0x04) {                 /* Ctrl is being pressed */
            asciiscan=scan_to_scanascii[scancode].control;
        } else if ((flags1 & 0x03) && (kanafl & 0x02)) { //for AX: Kana is active + Shift is being pressed
            asciiscan = scan_to_scanascii[scancode].kana_shift; //for AX
        } else if (flags1 & 0x03) {                 /* Either shift is being pressed */
            asciiscan=scan_to_scanascii[scancode].shift;
        } else if (kanafl & 0x02) {
            asciiscan = scan_to_scanascii[scancode].kana; //for AX: Kana is active
        } else {
            asciiscan=scan_to_scanascii[scancode].normal;
        }
        /* cancel shift is letter and capslock active */
        if(flags1&64) {
            if(flags1&3) {
                /*cancel shift */  
                if(((asciiscan&0x00ff) >0x40) && ((asciiscan&0x00ff) <0x5b)) 
                    asciiscan=scan_to_scanascii[scancode].normal; 
            } else {
                /* add shift */
                if(((asciiscan&0x00ff) >0x60) && ((asciiscan&0x00ff) <0x7b)) 
                    asciiscan=scan_to_scanascii[scancode].shift; 
            }
        }
        if (flags3 &0x02) {
            /* extended key (numblock), return and slash need special handling */
            if (scancode==0x1c) {   /* return */
                if (flags1 &0x08) asciiscan=0xa600;
                else asciiscan=(asciiscan&0xff)|0xe000;
            } else if (scancode==0x35) {    /* slash */
                if (flags1 &0x08) asciiscan=0xa400;
                else if (flags1 &0x04) asciiscan=0x9500;
                else asciiscan=0xe02f;
            }
        }
        add_key(asciiscan);
        break;
    }
irq1_end:
    if(scancode !=0xe0) flags3 &=~0x02;                                 //Reset 0xE0 Flag
    mem_writeb(BIOS_KEYBOARD_FLAGS1,flags1);
    if ((scancode&0x80)==0) flags2&=0xf7;
    mem_writeb(BIOS_KEYBOARD_FLAGS2,flags2);
    mem_writeb(BIOS_KEYBOARD_FLAGS3,flags3);
    mem_writeb(BIOS_KEYBOARD_LEDS,leds);

    /* update LEDs on keyboard */
    if (leds_orig != leds) KEYBOARD_SetLEDs(leds);

    /* update insert cursor */
    /* FIXME: Wait a second... I doubt the BIOS IRQ1 handler does this! The program (or DOS prompt) decides whether INS changes cursor shape! */
    extern bool dos_program_running;
    if (!dos_program_running)
    {
        /* As long as this hack exists... maintain the cursor's invisible state if invisible so it
         * does not reappear unexpectedly when the user presses Insert. Though long term, this should
         * be handled by the shell, not the keyboard ISR */
        const auto invisible = static_cast<bool>(real_readw(BIOSMEM_SEG,BIOSMEM_CURSOR_TYPE) & 0x2000); /* NTS: Written (last|(first<<8)) and (first & 0x20) means invisible */
        const auto flg = mem_readb(BIOS_KEYBOARD_FLAGS1);
        const auto ins = static_cast<bool>(flg & BIOS_KEYBOARD_FLAGS1_INSERT_ACTIVE);
        const auto ssl = static_cast<uint8_t>(ins ? CURSOR_SCAN_LINE_INSERT : CURSOR_SCAN_LINE_NORMAL);
        if (CurMode->type == M_TEXT && inshell) {
            if (machine==MCH_MDA||machine==MCH_HERC)
                INT10_SetCursorShape((ssl + 6) | (invisible?0x20:0x00), CURSOR_SCAN_LINE_END + 6); /* 14 - 8 = 6 */
            else
                INT10_SetCursorShape(ssl | (invisible?0x20:0x00), CURSOR_SCAN_LINE_END);
        }
    }
					
/*  IO_Write(0x20,0x20); moved out of handler to be virtualizable */
#if 0
/* Signal the keyboard for next code */
/* In dosbox port 60 reads do this as well */
    uint8_t old61=IO_Read(0x61);
    IO_Write(0x61,old61 | 128);
    IO_Write(0x64,0xae);
#endif
    return CBRET_NONE;
}

bool CPU_PUSHF(Bitu use32);
void CPU_Push16(uint16_t value);
unsigned char AT_read_60(void);
extern bool pc98_force_ibm_layout;

/* BIOS INT 18h output vs keys:
 *
 *              Unshifted   Shifted     CTRL    Caps    Kana    Kana+Shift
 *              -----------------------------------------------------------
 * ESC          0x001B      0x001B      0x001B  0x001B  0x001B  0x001B
 * STOP         --          --          --      --      --      --
 * F1..F10      <--------------- scan code in upper, zero in lower ------->
 * INS/DEL      <--------------- scan code in upper, zero in lower ------->
 * ROLL UP/DOWN <--------------- scan code in upper, zero in lower ------->
 * COPY         --          --          --      --      --      --
 * HOME CLR     0x3E00      0xAE00      --      --      --      --
 * HELP         <--------------- scan code in upper, zero in lower ------->
 * ARROW KEYS   <--------------- scan code in upper, zero in lower ------->
 * XFER         0x3500      0xA500      0xB500  0x3500  0x3500  0xA500
 * NFER         0x5100      0xA100      0xB100  0x5100  0x5100  0xA100
 * GRPH         --          --          --      --      --      --
 * TAB          0x0F09      0x0F09      0x0F09  0x0F09  0x0F09  0x0F09
 * - / 口       --          0x335F      0x331F  --      0x33DB  0x33DB      Kana+CTRL = 0x331F
 */
static Bitu IRQ1_Handler_PC98(void) {
    unsigned char status;
    unsigned int patience = 32;

    status = IO_ReadB(0x43); /* 8251 status */
    while (status & 2/*RxRDY*/) {
        unsigned char sc_8251 = IO_ReadB(0x41); /* 8251 data */

        bool pressed = !(sc_8251 & 0x80);
        sc_8251 &= 0x7F;

        /* Testing on real hardware shows INT 18h AH=0 returns raw scancode in upper half, ASCII in lower half.
         * Just like INT 16h on IBM PC hardware */
        uint16_t scan_add = sc_8251 << 8U;

        /* NOTES:
         *  - The bitmap also tracks CAPS, and KANA state. It does NOT track NUM state.
         *    The bit corresponding to the key code will show a bit set if in that state.
         *  - The STS byte returned by INT 18h AH=02h seems to match the 14th byte of the bitmap... so far.
         *
         *  STS layout (based on real hardware):
         *
         *    bit[7] = ?
         *    bit[6] = ?
         *    bit[5] = ?
         *    bit[4] = CTRL is down
         *    bit[3] = GRPH is down
         *    bit[2] = KANA engaged
         *    bit[1] = CAPS engaged
         *    bit[0] = SHIFT is down
         */
        uint8_t modflags = mem_readb(0x52A + 0xE);

        bool shift = modflags & 0x01; //bit 0
        bool caps = modflags & 0x02;  //bit 1
        bool kana = modflags & 0x04;  //bit 2
        bool grph = modflags & 0x08;  //bit 3
        bool ctrl = !!(modflags & 0x10);  //bit 4
        //bool caps_capitals = (modflags & 1) ^ ((modflags >> 1) & 1); /* CAPS XOR SHIFT */ UNUSED

        /* According to Neko Project II, the BIOS maintains a "pressed key" bitmap at 0x50:0x2A.
         * Without this bitmap many PC-98 games are unplayable. */
        /* ALSO note that byte 0xE of this array is the "shift" state byte. */
        {
            unsigned int o = 0x52Au + ((unsigned int)sc_8251 >> 3u);
            unsigned char c = mem_readb(o);
            unsigned char b = 1u << (sc_8251 & 7u);

            if (pressed)
                c |= b;
            else
                c &= ~b;

            mem_writeb(o,c);

            /* mirror CTRL+GRAPH+KANA+CAPS+SHIFT at 0x53A which is returned by INT 18h AH=2 */
            if (o == 0x538) mem_writeb(0x53A,c);
        }

        /* NOTES:
         *  - SHIFT/CTRL scan code changes (no scan code emitted if GRPH is held down for these keys)
         *
         *    CTRL apparently takes precedence over SHIFT.
         *
         *      Key       Unshifted      Shifted        CTRL
         *      --------------------------------------------
         *      F1        0x62           0x82           0x92
         *      F2        0x63           0x83           0x93
         *      F3        0x64           0x84           0x94
         *      F4        0x65           0x85           0x95
         *      F5        0x66           0x86           0x96
         *      F6        0x67           0x87           0x97
         *      F7        0x68           0x88           0x98
         *      F8        0x69           0x89           0x99
         *      F9        0x6A           0x8A           0x9A
         *      F10       0x6B           0x8B           0x9B
         *      HOME/CLR  0x3E           0xAE           <none>
         *      XFER      0x35           0xA5           0xB5
         *      NFER      0x51           0xA1           0xB1
         *      VF1       0x52           0xC2           0xD2
         *      VF2       0x53           0xC3           0xD3
         *      VF3       0x54           0xC4           0xD4
         *      VF4       0x55           0xC5           0xD5
         *      VF5       0x56           0xC6           0xD6
         */

        /* FIXME: I'm fully aware of obvious problems with this code so far:
         *        - This is coded around my American keyboard layout
         *        - No support for CAPS or KANA modes.
         *
         *        As this code develops refinements will occur.
         *
         *        - Scan codes will be mapped to unshifted/shifted/kana modes
         *          as laid out on Japanese keyboards.
         *        - This will use a lookup table with special cases such as
         *          modifier keys. */
        //                  Keyboard layout and processing, copied from an actual PC-98 keyboard
        //
        //                  KEY         UNSHIFT SHIFT   CTRL    KANA
        //                  ----------------------------------------
        if (pressed && sc_8251 <= 0x6f) { // skip shift-keys (0x70-0x74)
            if (sc_8251 == 0x60) { // STOP
                // does not pass it on.
                // According to Neko Project II source code, STOP invokes INT 6h
                // which is PC-98's version of the break interrupt IBM maps to INT 1Bh.
                // Obviously defined before Intel decided that INT 6h is the Invalid
                // Opcode exception. Booting PC-98 MS-DOS and looking at the INT 6h
                // interrupt handler in the debugger confirms this.
                /* push an IRET frame pointing at INT 06h. */

                /* we can't just CALLBACK_RunRealInt() here because we're in the
                * middle of an ISR and we need to acknowledge the interrupt to
                * the PIC before we call INT 06h. Funny things happen otherwise,
                * including an unresponsive keyboard. */

                /* I noticed that Neko Project II has the code to emulate this,
                * as a direct call to run a CPU interrupt, but it's commented
                * out for probably the same issue. */
                const uint32_t cb = real_readd(0, 0x06u * 4u);

                CPU_PUSHF(0);
                CPU_Push16((uint16_t)(cb >> 16u));
                CPU_Push16((uint16_t)(cb & 0xFFFFu));
                break;
            }
            else if (sc_8251 == 0x61){
                // COPY (INT5 shall be called)
            }
            if (grph) { /* Alt is being pressed */
                if (scan_to_scanascii_pc98[sc_8251].alt) add_key(scan_to_scanascii_pc98[sc_8251].alt);
            }
            else if (ctrl) { /* Ctrl is being pressed */
                if (scan_to_scanascii_pc98[sc_8251].control) add_key(scan_to_scanascii_pc98[sc_8251].control);
            }
            else if (kana && shift) { // Kana is active + Shift is being pressed
                if (scan_to_scanascii_pc98[sc_8251].kana_shift)add_key(scan_to_scanascii_pc98[sc_8251].kana_shift);
            }
            else if (kana) {
                if (scan_to_scanascii_pc98[sc_8251].kana) add_key(scan_to_scanascii_pc98[sc_8251].kana); // Kana is active
            }
            else if ((sc_8251 >= 0x10 && sc_8251 <= 0x19) ||
                (sc_8251 >= 0x1d && sc_8251 <= 0x25) || (sc_8251 >= 0x29 && sc_8251 <= 0x2f)){
                /* is alphabet*/
                if (shift ^ caps) { /* Upper case letters */
                    add_key(scan_to_scanascii_pc98[sc_8251].shift);
                }
                else {
                    add_key(scan_to_scanascii_pc98[sc_8251].normal);
                }

            }
            else {
                if (!pc98_force_ibm_layout){
                    if (shift){
                        if (scan_to_scanascii_pc98[sc_8251].shift) add_key(scan_to_scanascii_pc98[sc_8251].shift);
                    }
                    else{
                        if (scan_to_scanascii_pc98[sc_8251].normal){
                            add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        }
                    }
                }
                else {
                    switch (sc_8251) {
                    case 0x02:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '@');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x06:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '^');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x07:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '&');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x08:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '*');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x09:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '(');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x0A:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + ')');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x0B:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '_');
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x0C:  // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + '+');
                        else add_key(scan_add + '=');
                        break;
                    case 0x1A: // pc98_force_ibm_layout
                        if (!modflags){
                            add_key(scan_add + '`');
                            break;
                        }
                        break;
                    case 0x26: // pc98_force_ibm_layout
                        if (shift) add_key(scan_add + ':'); // Shift - semicolon = ':'
                        else add_key(scan_to_scanascii_pc98[sc_8251].normal);
                        break;
                    case 0x27: // pc98_force_ibm_layout
                        // quote key
                        if (shift) {
                            add_key(scan_add + '\"');
                            break;
                        }
                        else {
                            add_key(scan_add + '\'');
                            break;
                        }
                    default:
                        if (shift){
                            if (scan_to_scanascii_pc98[sc_8251].shift) add_key(scan_to_scanascii_pc98[sc_8251].shift);
                        }
                        else{
                            if (scan_to_scanascii_pc98[sc_8251].normal){
                                add_key(scan_to_scanascii_pc98[sc_8251].normal);
                            }
                        }
                    }
                }
            }
        }
        if (--patience == 0) break; /* in case of stuck 8251 */
        status = IO_ReadB(0x43); /* 8251 status */
    }

    return CBRET_NONE;
}

static Bitu PCjr_NMI_Keyboard_Handler(void) {
#if 0
    Bitu DEBUG_EnableDebugger(void);
    DEBUG_EnableDebugger();
#endif
    if (IO_ReadB(0x64) & 1) /* while data is available */
        reg_eip++; /* skip over EIP to IRQ1 call through */

    return CBRET_NONE;
}

static Bitu IRQ1_CtrlBreakAfterInt1B(void) {
    return CBRET_NONE;
}

bool isDBCSCP();
static bool IsKanjiCode(uint16_t key)
{
	if(isDBCSCP()) {
		// Kanji
		if((key & 0xff00) == 0xf000 || (key & 0xff00) == 0xf100) {
			return true;
		}
		// Framework II
		if(key == 0x8500 || key == 0x8600 || key == 0x9c00 || key == 0x9e00
		  || key == 0xa500 || key == 0xa600 || key == 0x8a00 || key == 0x8900) {
			return true;
		}
	}
	return false;
}

/* check whether key combination is enhanced or not,
   translate key if necessary */
static bool IsEnhancedKey(uint16_t &key) {
    if (IS_PC98_ARCH)
        return false;

    /* test for special keys (return and slash on numblock) */
    if ((key>>8)==0xe0) {
        if (((key&0xff)==0x0a) || ((key&0xff)==0x0d)) {
            /* key is return on the numblock */
            key=(key&0xff)|0x1c00;
        } else {
            /* key is slash on the numblock */
            key=(key&0xff)|0x3500;
        }
        /* both keys are not considered enhanced keys */
        return false;
    } else if (((key>>8)>0x84) || (((key&0xff)==0xf0) && (key>>8))) {
        /* key is enhanced key (either scancode part>0x84 or
           specially-marked keyboard combination, low part==0xf0) */
        if(!IsKanjiCode(key)) return true;
    }
    /* convert key if necessary (extended keys) */
    if (!IsKanjiCode(key) && (key>>8) && ((key&0xff)==0xe0))  {
        key&=0xff00;
    }
    return false;
}

#if defined(C_SDL2)
#if defined(WIN32) && !defined(HX_DOS)
extern void IME_SetEnable(BOOL state);
extern bool IME_GetEnable();
#elif defined(MACOSX)
extern bool IME_GetEnable();
extern void IME_SetEnable(int state);
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#endif
#endif

extern bool DOS_BreakFlag;
extern bool DOS_BreakConioFlag;

bool int16_unmask_irq1_on_read = true;
bool int16_ah_01_cf_undoc = true;

Bitu INT16_Handler(void) {
    uint16_t temp=0;
    switch (reg_ah) {
    case 0x00: /* GET KEYSTROKE */
        if (int16_unmask_irq1_on_read)
            PIC_SetIRQMask(1,false); /* unmask keyboard */

        // HACK: Make STOP key work
        if (IS_PC98_ARCH && DOS_BreakConioFlag) {
            DOS_BreakConioFlag=false;
            reg_ax=0;
            return CBRET_NONE;
        }

        if ((get_key(temp)) && (!IsEnhancedKey(temp))) {
            /* normal key found, return translated key in ax */
            reg_ax=temp;
        } else {
            /* enter small idle loop to allow for irqs to happen */
            reg_ip+=1;
        }
        break;
    case 0x10: /* GET KEYSTROKE (enhanced keyboards only) */
        if (int16_unmask_irq1_on_read)
            PIC_SetIRQMask(1,false); /* unmask keyboard */

        // HACK: Make STOP key work
        if (IS_PC98_ARCH && DOS_BreakConioFlag) {
            DOS_BreakConioFlag=false;
            reg_ax=0;
            return CBRET_NONE;
        }

        if (get_key(temp)) {
            if (!IS_PC98_ARCH && ((temp&0xff)==0xf0) && (temp>>8)) {
                /* special enhanced key, clear low part before returning key */
                if(!IsKanjiCode(temp)) temp&=0xff00;
            }
            reg_ax=temp;
        } else {
            /* enter small idle loop to allow for irqs to happen */
            reg_ip+=1;
        }
        break;
    case 0x01: /* CHECK FOR KEYSTROKE */
        // enable interrupt-flag after IRET of this int16
        CALLBACK_SIF(true);
        if (int16_unmask_irq1_on_read)
            PIC_SetIRQMask(1,false); /* unmask keyboard */

        for (;;) {
            if (check_key(temp)) {
                if (!IsEnhancedKey(temp)) {
                    /* normal key, return translated key in ax */
                    CALLBACK_SZF(false);
                    if (int16_ah_01_cf_undoc) CALLBACK_SCF(true);
                    reg_ax=temp;
                    break;
                } else {
                    /* remove enhanced key from buffer and ignore it */
                    get_key(temp);
                }
            } else {
                /* no key available */
                CALLBACK_SZF(true);
                if (int16_ah_01_cf_undoc) CALLBACK_SCF(false);
                break;
            }
//          CALLBACK_Idle();
        }
        break;
    case 0x11: /* CHECK FOR KEYSTROKE (enhanced keyboards only) */
        // enable interrupt-flag after IRET of this int16
        CALLBACK_SIF(true);
        if (int16_unmask_irq1_on_read)
            PIC_SetIRQMask(1,false); /* unmask keyboard */

        /* NOTE: The FreeDOS EDIT.COM editor built into DOSBox-X has a problem where if
         *       this call return AX=0 and ZF=0, it will treat it the same as if we had
         *       returned ZF=1 to indicate no scan code. Unfortunately scan code 0x0000
         *       is added to the buffer for CTRL+BREAK, meaning that if you hit CTRL+BREAK
         *       while using EDIT.COM, you effectively disable all keyboard input to the
         *       program until you exit, or trick the program into reading the scan code
         *       to remove it from the buffer.
         *
         * TODO: If you run EDIT.COM on real MS-DOS, does the same problem come up? */

        if (!check_key(temp)) {
            CALLBACK_SZF(true);
        } else {
            CALLBACK_SZF(false);
            if (!IS_PC98_ARCH && ((temp&0xff)==0xf0) && (temp>>8)) {
                /* special enhanced key, clear low part before returning key */
                temp&=0xff00;
            }
            reg_ax=temp;
        }
        break;
    case 0x02:  /* GET SHIFT FLAGS */
        reg_al=mem_readb(BIOS_KEYBOARD_FLAGS1);
        break;
    case 0x03:  /* SET TYPEMATIC RATE AND DELAY */
        if (reg_al == 0x00) { // set default delay and rate
            IO_Write(0x60,0xf3);
            IO_Write(0x60,0x20); // 500 msec delay, 30 cps
        } else if (reg_al == 0x05) { // set repeat rate and delay
            IO_Write(0x60,0xf3);
            IO_Write(0x60,(reg_bh&3)<<5|(reg_bl&0x1f));
        } else {
            LOG(LOG_BIOS,LOG_ERROR)("INT16:Unhandled Typematic Rate Call %2X BX=%X",reg_al,reg_bx);
        }
        break;
    case 0x05:  /* STORE KEYSTROKE IN KEYBOARD BUFFER */
        if (BIOS_AddKeyToBuffer(reg_cx)) reg_al=0;
        else reg_al=1;
        break;
    case 0x12: /* GET EXTENDED SHIFT STATES */
        reg_al = mem_readb(BIOS_KEYBOARD_FLAGS1);
        reg_ah = (mem_readb(BIOS_KEYBOARD_FLAGS2)&0x73)   |
                 ((mem_readb(BIOS_KEYBOARD_FLAGS2)&4)<<5) | // SysReq pressed, bit 7
                 (mem_readb(BIOS_KEYBOARD_FLAGS3)&0x0c);    // Right Ctrl/Alt pressed, bits 2,3
        break;
    case 0x13:
#if (defined(WIN32) && !defined(HX_DOS) || defined(MACOSX)) && !defined(C_SDL2) && defined(SDL_DOSBOX_X_SPECIAL)
#if defined(USE_TTF)
        if((IS_DOSV || ttf_dosv) && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#else
        if(IS_DOSV && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#endif
            if(reg_al == 0x00) {
                if(reg_dl & 0x81)
                    SDL_SetIMValues(SDL_IM_ONOFF, 1, NULL);
                else
                    SDL_SetIMValues(SDL_IM_ONOFF, 0, NULL);
            } else if(reg_al == 0x01) {
                int onoff;
                reg_dl = 0x00;
                if(SDL_GetIMValues(SDL_IM_ONOFF, &onoff, NULL) == NULL) {
                    if(onoff)
                        reg_dl = 0x81;
                }
            }
        }
#elif (defined(WIN32) && !defined(HX_DOS) || defined(MACOSX)) && defined(C_SDL2)
#if defined(USE_TTF)
        if((IS_DOSV || ttf_dosv) && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#else
        if(IS_DOSV && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#endif
            if(reg_al == 0x00) {
                if(reg_dl & 0x81)
                    IME_SetEnable(TRUE);
                else
                    IME_SetEnable(FALSE);
            } else if(reg_al == 0x01) {
                if(IME_GetEnable())
                    reg_dl = 0x81;
                else
                    reg_dl = 0x00;
            }
        }
#endif
        break;
    case 0x14:
#if defined(USE_TTF)
        if((IS_DOSV || ttf_dosv) && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#else
        if(IS_DOSV && IS_DOS_CJK && (DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS)) {
#endif
            if(reg_al == 0x02) {
                // get
                reg_al = fep_line;
            } else if(reg_al == 0x00 || reg_al == 0x01) {
                // set
                fep_line = reg_al;
            }
        }
        break;
    case 0x50:// Set/Get JP/US mode in KBD BIOS
        if (!IS_JEGA_ARCH) break;
        switch (reg_al) {
            case 0x00:
                LOG(LOG_BIOS, LOG_NORMAL)("AX KBD BIOS 5000h is called.");
                if (INT16_AX_SetKBDBIOSMode(reg_bx)) reg_al = 0x00;
                else reg_al = 0x01;
                break;
            case 0x01:
                LOG(LOG_BIOS,LOG_NORMAL)("AX KBD BIOS 5001h is called.");
                reg_bx = INT16_AX_GetKBDBIOSMode();
                reg_al = 0;
                break;
            default:
                LOG(LOG_BIOS,LOG_ERROR)("Unhandled AX Function %X",reg_al);
                reg_al=0x2;//return unknown error
                break;
        }
        break;
    case 0x51:// Get the shift status
        if (!IS_JEGA_ARCH) break;
        if (INT16_AX_GetKBDBIOSMode() != 0x51) break;//exit if not in JP mode
        LOG(LOG_BIOS,LOG_NORMAL)("AX KBD BIOS 51xxh is called.");
        reg_al = mem_readb(BIOS_KEYBOARD_FLAGS1);
        reg_ah = mem_readb(BIOS_KEYBOARD_AX_KBDSTATUS) & 0x02;
        break;
    case 0x55:
        /* Weird call used by some dos apps */
        LOG(LOG_BIOS,LOG_NORMAL)("INT16:55:Word TSR compatible call");
        break;
    case 0xdb:
        // ETen call
        // reg_ax = 8000;
        break;
    case 0xf0:
        // J-3100 beep
        break;
    case 0xf1:
        // J-3100 set shift
        break;
    case 0xf2:
        // J-3100 key click sound
        break;
    case 0xf5:
        // J-3100 set key data
        if(J3_IsJapanese()) {
            if(BIOS_AddKeyToBuffer(reg_bx)) {
                reg_ax = 0x0000;
            } else {
                // buffer full
                reg_ax = 0xffff;
            }
        }
        break;
    case 0xf6:
        // J-3100 check key buffer
        if(J3_IsJapanese()) {
            uint16_t size, head, tail;
            size = mem_readw(BIOS_KEYBOARD_BUFFER_END) - mem_readw(BIOS_KEYBOARD_BUFFER_START);
            head = mem_readw(BIOS_KEYBOARD_BUFFER_HEAD);
            tail = mem_readw(BIOS_KEYBOARD_BUFFER_TAIL);
            reg_ax = 0;
            if(tail > head) {
                reg_ax = (tail - head) / 2;
            } else if(tail < head) {
                reg_ax = (tail + size - head) / 2;
            }
            if(reg_ax == size / 2) {
                reg_ax = 0xffff;
            }
            reg_bx = J3_GetMachineCode();
        }
        break;
    default:
        LOG(LOG_BIOS,LOG_ERROR)("INT16:Unhandled call %02X",reg_ah);
        break;

    }

    return CBRET_NONE;
}

/* The INT16h handler manipulates reg_ip and expects it to work
 * based on the layout of the callback. So to allow calling
 * directly we have to save/restore CS:IP and run the DOS
 * machine. */
Bitu INT16_Handler_Wrap(void) {
    Bitu proc;

    proc = CALLBACK_RealPointer(call_int16);
    CALLBACK_RunRealFarInt(proc >> 16/*seg*/,proc & 0xFFFFU/*off*/);
    return 0;
}

//Keyboard initialisation. src/gui/sdlmain.cpp
extern bool startup_state_numlock;
extern bool startup_state_capslock;
extern bool startup_state_scrlock;

static void InitBiosSegment(void) {
    if (IS_PC98_ARCH) {
        mem_writew(0x524/*tail*/,0x502);
        mem_writew(0x526/*tail*/,0x502);
    }
    else { /* IBM PC */
        /* Setup the variables for keyboard in the bios data segment */
        mem_writew(BIOS_KEYBOARD_BUFFER_START,0x1e);
        mem_writew(BIOS_KEYBOARD_BUFFER_END,0x3e);
        mem_writew(BIOS_KEYBOARD_BUFFER_HEAD,0x1e);
        mem_writew(BIOS_KEYBOARD_BUFFER_TAIL,0x1e);
        uint8_t flag1 = 0;
        uint8_t leds = 16; /* Ack received */

#if 0 /*SDL_VERSION_ATLEAST(1, 2, 14)*/
        //Nothing, mapper handles all.
#else
        if (startup_state_capslock) { flag1|=BIOS_KEYBOARD_FLAGS1_CAPS_LOCK_ACTIVE; leds|=BIOS_KEYBOARD_LEDS_CAPS_LOCK;}
        if (startup_state_numlock)  { flag1|=BIOS_KEYBOARD_FLAGS1_NUMLOCK_ACTIVE; leds|=BIOS_KEYBOARD_LEDS_NUM_LOCK;}
        if (startup_state_scrlock)  { flag1|=BIOS_KEYBOARD_FLAGS1_SCROLL_LOCK_ACTIVE; leds|=BIOS_KEYBOARD_LEDS_SCROLL_LOCK;}
#endif

        mem_writeb(BIOS_KEYBOARD_FLAGS1,flag1);
        mem_writeb(BIOS_KEYBOARD_FLAGS2,0);
        mem_writeb(BIOS_KEYBOARD_FLAGS3,16); /* Enhanced keyboard installed */  
        mem_writeb(BIOS_KEYBOARD_TOKEN,0);
        mem_writeb(BIOS_KEYBOARD_LEDS,leds);
    }
}

void CALLBACK_DeAllocate(Bitu in);

void BIOS_UnsetupKeyboard(void) {
    if (call_int16 != 0) {
        CALLBACK_DeAllocate(call_int16);
        RealSetVec(0x16,0);
        call_int16 = 0;
    }
    if (call_irq1 != 0) {
        CALLBACK_DeAllocate(call_irq1);
        call_irq1 = 0;
    }
    if (call_irq_pcjr_nmi != 0) {
        CALLBACK_DeAllocate(call_irq_pcjr_nmi);
        call_irq_pcjr_nmi = 0;
    }
    if (call_irq6 != 0) {
        CALLBACK_DeAllocate(call_irq6);
        call_irq6 = 0;
    }
    if (irq1_ret_ctrlbreak_callback != 0) {
        CALLBACK_DeAllocate(irq1_ret_ctrlbreak_callback);
        irq1_ret_ctrlbreak_callback = 0;
    }
}

void BIOS_SetupKeyboard(void) {
    /* Init the variables */
    InitBiosSegment();

    if (IS_PC98_ARCH) {
        /* HACK */
        /* Allocate/setup a callback for int 0x16 and for standard IRQ 1 handler */
        call_int16=CALLBACK_Allocate(); 
        CALLBACK_Setup(call_int16,&INT16_Handler,CB_INT16,"Keyboard");
        /* DO NOT set up an INT 16h vector. This exists only for the DOS CONIO emulation. */
    }
    else {
        /* Allocate/setup a callback for int 0x16 and for standard IRQ 1 handler */
        call_int16=CALLBACK_Allocate(); 
        CALLBACK_Setup(call_int16,&INT16_Handler,CB_INT16,"Keyboard");
        RealSetVec(0x16,CALLBACK_RealPointer(call_int16));
    }

    call_irq1=CALLBACK_Allocate();
    if (machine == MCH_PCJR) { /* PCjr keyboard interrupt connected to NMI */
        call_irq_pcjr_nmi=CALLBACK_Allocate();

        CALLBACK_Setup(call_irq_pcjr_nmi,&PCjr_NMI_Keyboard_Handler,CB_IRET,"PCjr NMI Keyboard");

        uint32_t a = CALLBACK_RealPointer(call_irq_pcjr_nmi);

        RealSetVec(0x02/*NMI*/,a);

        /* TODO: PCjr calls INT 48h to convert PCjr scan codes to IBM PC/XT compatible */

	a = ((a >> 16) << 4) + (a & 0xFFFF);
	/* a+0 = callback instruction (4 bytes)
	 * a+4 = iret (1 bytes) */
	phys_writeb(a+5,0x50);		/* push ax */
	phys_writeb(a+6,0x1e);		/* push ds */
	phys_writew(a+7,0xC0C7);	/* mov ax,0x0040    NTS: Do not use PUSH <imm>, that opcode does not exist on the 8086 */
	phys_writew(a+9,0x0040);	/* <---------' */
	phys_writew(a+11,0xD88E);	/* mov ds,ax */
	phys_writew(a+13,0x60E4);	/* in al,60h */
	phys_writew(a+15,0x09CD);	/* int 9h */
	phys_writeb(a+17,0x1f);		/* pop ds */
	phys_writeb(a+18,0x58);		/* pop ax */
	phys_writew(a+19,0x00EB + ((256-21)<<8)); /* jmp a+0 */
    }

    if (IS_PC98_ARCH) {
        CALLBACK_Setup(call_irq1,&IRQ1_Handler_PC98,CB_IRET_EOI_PIC1,Real2Phys(BIOS_DEFAULT_IRQ1_LOCATION),"IRQ 1 Keyboard PC-98");
        RealSetVec(0x09/*IRQ 1*/,BIOS_DEFAULT_IRQ1_LOCATION);
    }
    else {
        CALLBACK_Setup(call_irq1,&IRQ1_Handler,CB_IRQ1,Real2Phys(BIOS_DEFAULT_IRQ1_LOCATION),"IRQ 1 Keyboard");
        RealSetVec(0x09/*IRQ 1*/,BIOS_DEFAULT_IRQ1_LOCATION);
        // pseudocode for CB_IRQ1:
        //  push ax
        //  in al, 0x60
        //  mov ah, 0x4f
        //  stc
        //  int 15
        //  jc skip
        //  callback IRQ1_Handler
        //  label skip:
        //  cli
        //  mov al, 0x20
        //  out 0x20, al
        //  pop ax
        //  iret
        //  cli
        //  mov al, 0x20
        //  out 0x20, al
        //  push bp
        //  int 0x05
        //  pop bp
        //  pop ax
        //  iret
    }

    irq1_ret_ctrlbreak_callback=CALLBACK_Allocate();
    CALLBACK_Setup(irq1_ret_ctrlbreak_callback,&IRQ1_CtrlBreakAfterInt1B,CB_IRQ1_BREAK,"IRQ 1 Ctrl-Break callback");
    // pseudocode for CB_IRQ1_BREAK:
    //  int 1b
    //  cli
    //  callback IRQ1_CtrlBreakAfterInt1B
    //  mov al, 0x20
    //  out 0x20, al
    //  pop ax
    //  iret
}
