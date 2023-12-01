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
 *
 *  Heavy improvements like PC-98 and LFN support by the DOSBox-X Team
 *  With major works from joncampbell123 and Wengier
 */


#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <sys/stat.h>

#include "control.h"
#include "dosbox.h"
#include "dos_inc.h"
#include "bios_disk.h"
#include "bios.h"
#include "logging.h"
#include "mem.h"
#include "paging.h"
#include "callback.h"
#include "regs.h"
#include "timer.h"
#include "menu.h"
#include "menudef.h"
#include "mapper.h"
#include "drives.h"
#include "setup.h"
#include "support.h"
#include "parport.h"
#include "serialport.h"
#include "dos_network.h"
#include "render.h"
#include "jfont.h"
#include "../ints/int10.h"
#include "pic.h"
#include "sdlmain.h"
#if defined(WIN32)
#include "../dos/cdrom.h"
#if !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
#include <process.h>
#include <shellapi.h>
#include <shlwapi.h>
#endif
#include <winsock.h>
#else
#include <unistd.h>
#endif

#include <output/output_ttf.h>

static bool first_run=true;
bool sync_time = false, manualtime = false;
extern std::string log_dev_con_str;
extern const char* RunningProgram;
extern bool use_quick_reboot, j3100_start;
extern bool enable_config_as_shell_commands;
extern bool checkwat, loadlang, pcibus_enable;
extern bool log_int21, log_fileio, pipetmpdev;
extern bool chinasea;
#if defined(USE_TTF)
extern bool ttf_dosv;
#endif
extern int tryconvertcp, autofixwarn;
extern int lfn_filefind_handle, result_errorcode;
extern uint16_t customcp_to_unicode[256];
int customcp = 0, altcp = 0;
unsigned long totalc, freec;
uint16_t countryNo = 0;
Bitu INT29_HANDLER(void);
Bitu INT28_HANDLER(void);
bool isDBCSCP();
uint32_t BIOS_get_PC98_INT_STUB(void);
uint16_t GetDefaultCP(void);
void ResolvePath(std::string& in);
void SwitchLanguage(int oldcp, int newcp, bool confirm);
void makestdcp950table(), makeseacp951table();
std::string GetDOSBoxXPath(bool withexe=false);
extern std::string prefix_local, prefix_overlay;

int ascii_toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c + 'A' - 'a';

    return c;
}

bool shiftjis_lead_byte(int c) {
    if ((((unsigned char)c & 0xE0) == 0x80) ||
        (((unsigned char)c & 0xE0) == 0xE0))
        return true;

    return false;
}

char * DBCS_upcase(char * str) {
    for (char* idx = str; *idx ; ) {
        if ((IS_PC98_ARCH && shiftjis_lead_byte(*idx)) || (isDBCSCP() && isKanji1_gbk(*idx))) {
            /* Shift-JIS is NOT ASCII and should not be converted to uppercase like ASCII.
             * The trailing byte can be mistaken for ASCII */
            idx++;
            if (*idx != 0) idx++;
        }
        else {
            *idx = ascii_toupper(*reinterpret_cast<unsigned char*>(idx));
            idx++;
        }
    }

    return str;
}

static Bitu DOS_25Handler_Actual(bool fat32);
static Bitu DOS_26Handler_Actual(bool fat32);

unsigned char cpm_compat_mode = CPM_COMPAT_MSDOS5;

bool dos_in_hma = true;
bool dos_umb = true;
bool DOS_BreakFlag = false;
bool DOS_BreakConioFlag = false;
bool enable_dbcs_tables = true;
bool enable_share_exe = true;
bool enable_filenamechar = true;
bool shell_keyboard_flush = true;
bool enable_network_redirector = true;
bool force_conversion = false;
bool hidenonrep = true;
bool rsize = false;
bool reqwin = false;
bool packerr = false;
bool incall = false;
int file_access_tries = 0;
int dos_initial_hma_free = 34*1024;
int dos_sda_size = 0x560;
int dos_clipboard_device_access;
const char *dos_clipboard_device_name;
const char dos_clipboard_device_default[]="CLIP$";

int maxfcb=100;
int maxdrive=1;
int enablelfn=-1;
int fat32setver=-1;
bool uselfn, winautorun=false;
extern int infix, log_dev_con;
extern bool int15_wait_force_unmask_irq, shellrun, logging_con, ctrlbrk;
extern bool winrun, startcmd, startwait, startquiet, starttranspath, i4dos;
extern bool startup_state_numlock, mountwarning, clipboard_dosapi;
std::string startincon;

uint32_t dos_hma_allocator = 0; /* physical memory addr */

Bitu XMS_EnableA20(bool enable);
Bitu XMS_GetEnabledA20(void);
bool XMS_IS_ACTIVE();
bool XMS_HMA_EXISTS();
void MSG_Init(void);
void JFONT_Init(void);
void SetNumLock(void);
void InitFontHandle(void);
void ShutFontHandle(void);
void DOSBox_SetSysMenu(void);
void runRescan(const char *str);
bool GFX_GetPreventFullscreen(void);

bool DOS_IS_IN_HMA() {
	if (dos_in_hma && XMS_IS_ACTIVE() && XMS_HMA_EXISTS())
		return true;

	return false;
}

uint32_t DOS_HMA_LIMIT() {
	if (dos.version.major < 5) return 0; /* MS-DOS 5.0+ only */
	if (!DOS_IS_IN_HMA()) return 0;
	return (0x110000 - 16); /* 1MB + 64KB - 16 bytes == (FFFF:FFFF + 1) == (0xFFFF0 + 0xFFFF + 1) == 0x10FFF0 */
}

uint32_t DOS_HMA_FREE_START() {
	if (dos.version.major < 5) return 0; /* MS-DOS 5.0+ only */
	if (!DOS_IS_IN_HMA()) return 0;

	if (dos_hma_allocator == 0) {
		dos_hma_allocator = 0x110000u - 16u - (unsigned int)dos_initial_hma_free;
		LOG(LOG_DOSMISC,LOG_DEBUG)("Starting HMA allocation from physical address 0x%06x (FFFF:%04x)",
			dos_hma_allocator,(dos_hma_allocator+0x10u)&0xFFFFu);
	}

	return dos_hma_allocator;
}

uint32_t DOS_HMA_GET_FREE_SPACE() {
	uint32_t start;

	if (dos.version.major < 5) return 0; /* MS-DOS 5.0+ only */
	if (!DOS_IS_IN_HMA()) return 0;
	start = DOS_HMA_FREE_START();
	if (start == 0) return 0;
	return (DOS_HMA_LIMIT() - start);
}

void DOS_HMA_CLAIMED(uint16_t bytes) {
	uint32_t limit = DOS_HMA_LIMIT();

	if (limit == 0) E_Exit("HMA allocatiom bug: Claim function called when HMA allocation is not enabled");
	if (dos_hma_allocator == 0) E_Exit("HMA allocatiom bug: Claim function called without having determined start");
	dos_hma_allocator += bytes;
	if (dos_hma_allocator > limit) E_Exit("HMA allocation bug: Exceeded limit");
}

uint16_t DOS_INFOBLOCK_SEG=0x80;	// sysvars (list of lists)
uint16_t DOS_CONDRV_SEG=0xa0;
uint16_t DOS_CONSTRING_SEG=0xa8;
uint16_t DOS_SDA_SEG=0xb2;		// dos swappable area
uint16_t DOS_SDA_SEG_SIZE=0x560;  // WordPerfect 5.1 consideration (emendelson)
uint16_t DOS_SDA_OFS=0;
uint16_t DOS_CDS_SEG=0x108;
uint16_t DOS_MEM_START=0x158;	 // regression to r3437 fixes nascar 2 colors
uint16_t minimum_mcb_segment=0x70;
uint16_t minimum_mcb_free=0x70;
uint16_t minimum_dos_initial_private_segment=0x70;

uint16_t DOS_PRIVATE_SEGMENT=0;//0xc800;
uint16_t DOS_PRIVATE_SEGMENT_END=0;//0xd000;

Bitu DOS_PRIVATE_SEGMENT_Size=0x800;	// 32KB (0x800 pages), mainline DOSBox behavior

bool enable_dummy_device_mcb = true;

extern unsigned int MAXENV;// = 32768u;
extern unsigned int ENV_KEEPFREE;// = 83;

#if defined(USE_TTF)
#include "char512.h"
extern int wpType;
static uint16_t WP5chars = 0;
static uint16_t WPvga512CHMhandle = -1;
static bool WPvga512CHMcheck = false;
#endif

DOS_Block dos;
DOS_InfoBlock dos_infoblock;

extern int bootdrive;
extern bool force_sfn, dos_kernel_disabled;

uint16_t DOS_Block::psp() const {
	if (dos_kernel_disabled) {
		LOG_MSG("BUG: DOS kernel is disabled (booting a guest OS), and yet somebody is still asking for DOS's current PSP segment\n");
		return 0x0000;
	}

	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetPSP();
}

void DOS_Block::psp(uint16_t _seg) const {
	if (dos_kernel_disabled) {
		LOG_MSG("BUG: DOS kernel is disabled (booting a guest OS), and yet somebody is still attempting to change DOS's current PSP segment\n");
		return;
	}

	DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetPSP(_seg);
}

RealPt DOS_Block::dta() const {
	if (dos_kernel_disabled) {
		LOG_MSG("BUG: DOS kernel is disabled (booting a guest OS), and yet somebody is still asking for DOS's DTA (disk transfer address)\n");
		return 0;
	}

	return DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).GetDTA();
}

void DOS_Block::dta(RealPt _dta) const {
	if (dos_kernel_disabled) {
		LOG_MSG("BUG: DOS kernel is disabled (booting a guest OS), and yet somebody is still attempting to change DOS's DTA (disk transfer address)\n");
		return;
	}

	DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDTA(_dta);
}

#define DOS_COPYBUFSIZE 0x10000
uint8_t dos_copybuf[DOS_COPYBUFSIZE];
#ifdef WIN32
uint16_t	NetworkHandleList[127];uint8_t dos_copybuf_second[DOS_COPYBUFSIZE];
#endif

static uint16_t ias_handle;
static uint16_t mskanji_handle;
static uint16_t ibmjp_handle;
static uint16_t avsdrv_handle;

static bool hat_flag[] = {
//            a     b     c     d     e      f      g      h
	false, true, true, true, true, true, false,   false, false,
//       i      j     k     l      m     n     o      p     q
	 false, false, true, true, false, true, true, false, true,
//      r      s      t      u     v     w     x     y     z
	 true, false, false, false, true, true, true, true, true
};

bool CheckHat(uint8_t code)
{
#if defined(USE_TTF)
	if(IS_JEGA_ARCH || IS_DOSV || ttf_dosv) {
#else
	if(IS_JEGA_ARCH || IS_DOSV) {
#endif
		if(code <= 0x1a) {
			return hat_flag[code];
		}
	}
	return false;
}

void DOS_SetError(uint16_t code) {
	dos.errorcode=code;
}

static constexpr uint8_t DATE_MDY = 0;
static constexpr uint8_t DATE_DMY = 1;
static constexpr uint8_t DATE_YMD = 2;

static constexpr uint8_t TIME_12H = 0;
static constexpr uint8_t TIME_24H = 1;

static constexpr uint8_t SEP_SPACE  = 0x20; // ( )
static constexpr uint8_t SEP_APOST  = 0x27; // (')
static constexpr uint8_t SEP_COMMA  = 0x2c; // (,)
static constexpr uint8_t SEP_DASH   = 0x2d; // (-)
static constexpr uint8_t SEP_PERIOD = 0x2e; // (.)
static constexpr uint8_t SEP_SLASH  = 0x2f; // (/)
static constexpr uint8_t SEP_COLON  = 0x3a; // (:)

typedef struct CountryInfo {
	COUNTRYNO country_number;
	uint8_t   date_format;
	uint8_t   date_separator;
	uint8_t   time_format;
	uint8_t   time_separator;
	uint8_t   thousands_separator;
	uint8_t   decimal_separator;
} CountryInfo;

static const std::vector<CountryInfo> COUNTRY_INFO = {
	//                           | Date fmt | Date separ | Time fmt | Time separ | 1000 separ | Dec separ  |
	{ COUNTRYNO::United_States   , DATE_MDY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // en_US
	{ COUNTRYNO::Candian_French  , DATE_YMD , SEP_DASH   , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fr_CA.
	{ COUNTRYNO::Latin_America   , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // es_419
	{ COUNTRYNO::Russia          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // ru_RU
	{ COUNTRYNO::Greece          , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // el_GR
	{ COUNTRYNO::Netherlands     , DATE_DMY , SEP_DASH   , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // nl_NL
	{ COUNTRYNO::Belgium         , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fr_BE
	{ COUNTRYNO::France          , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fr_FR
	{ COUNTRYNO::Spain           , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // es_ES
	{ COUNTRYNO::Hungary         , DATE_YMD , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // hu_HU
	{ COUNTRYNO::Yugoslavia      , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // sr_RS/sr_ME/hr_HR/sk_SK/bs_BA/mk_MK
	{ COUNTRYNO::Italy           , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // it_IT
	{ COUNTRYNO::Romania         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // ro_RO
	{ COUNTRYNO::Switzerland     , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_APOST  , SEP_PERIOD }, // ??_CH
	{ COUNTRYNO::Czech_Slovak    , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // cs_CZ
	{ COUNTRYNO::Austria         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // de_AT
	{ COUNTRYNO::United_Kingdom  , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // en_GB
	{ COUNTRYNO::Denmark         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // da_DK
	{ COUNTRYNO::Sweden          , DATE_YMD , SEP_DASH   , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // sv_SE
	{ COUNTRYNO::Norway          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // nn_NO
	{ COUNTRYNO::Poland          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // pl_PL
	{ COUNTRYNO::Germany         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // de_DE
	{ COUNTRYNO::Argentina       , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // es_AR
	{ COUNTRYNO::Brazil          , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // pt_BR
	{ COUNTRYNO::Malaysia        , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // ms_MY
	{ COUNTRYNO::Australia       , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // en_AU
	{ COUNTRYNO::Philippines     , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // fil_PH
	{ COUNTRYNO::Singapore       , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // ms_SG
	{ COUNTRYNO::Kazakhstan      , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // kk_KZ
	{ COUNTRYNO::Japan           , DATE_YMD , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // ja_JP
	{ COUNTRYNO::South_Korea     , DATE_YMD , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // ko_KR
	{ COUNTRYNO::Vietnam         , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // vi_VN
	{ COUNTRYNO::China           , DATE_YMD , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // zh_CN
	{ COUNTRYNO::Turkey          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // tr_TR
	{ COUNTRYNO::India           , DATE_DMY , SEP_SLASH  , TIME_12H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // hi_IN
	{ COUNTRYNO::Niger           , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fr_NE
	{ COUNTRYNO::Benin           , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fr_BJ
	{ COUNTRYNO::Nigeria         , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // en_NG
	{ COUNTRYNO::Faeroe_Islands  , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // fo_FO
	{ COUNTRYNO::Portugal        , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // pt_PT
	{ COUNTRYNO::Iceland         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // is_IS
	{ COUNTRYNO::Albania         , DATE_DMY , SEP_PERIOD , TIME_12H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // sq_AL
	{ COUNTRYNO::Malta           , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // mt_MT
	{ COUNTRYNO::Finland         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // fi_FI
	{ COUNTRYNO::Bulgaria        , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // bg_BG
	{ COUNTRYNO::Lithuania       , DATE_YMD , SEP_DASH   , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // lt_LT
	{ COUNTRYNO::Latvia          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // lv_LV
	{ COUNTRYNO::Estonia         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // et_EE
	{ COUNTRYNO::Armenia         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // hy_AM
	{ COUNTRYNO::Belarus         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // be_BY
	{ COUNTRYNO::Ukraine         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // uk_UA
	{ COUNTRYNO::Serbia          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // sr_RS
	{ COUNTRYNO::Montenegro      , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // sr_ME
	{ COUNTRYNO::Croatia         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // hr_HR
	{ COUNTRYNO::Slovenia        , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // sk_SK
	{ COUNTRYNO::Bosnia          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // bs_BA
	{ COUNTRYNO::Macedonia       , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // mk_MK
	{ COUNTRYNO::Taiwan          , DATE_YMD , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // zh_TW
	{ COUNTRYNO::Arabic          , DATE_DMY , SEP_PERIOD , TIME_12H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // ar_??
	{ COUNTRYNO::Israel          , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // he_IL
	{ COUNTRYNO::Mongolia        , DATE_YMD , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_COMMA  , SEP_PERIOD }, // mn_MN
	{ COUNTRYNO::Tadjikistan     , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // tg_TJ
	{ COUNTRYNO::Turkmenistan    , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // tk_TM
	{ COUNTRYNO::Azerbaijan      , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_PERIOD , SEP_COMMA  }, // az_AZ
	{ COUNTRYNO::Georgia         , DATE_DMY , SEP_PERIOD , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // ka_GE
	{ COUNTRYNO::Kyrgyzstan      , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // ky_KG
	{ COUNTRYNO::Uzbekistan      , DATE_DMY , SEP_SLASH  , TIME_24H , SEP_COLON  , SEP_SPACE  , SEP_COMMA  }, // uz_UZ
	//                           | Date fmt | Date separ | Time fmt | Time separ | 1000 separ | Dec separ  |
};

void DOS_SetCountry(uint16_t country_number) // XXX check where this is called, why always sets to US locale (is US country info OK?)
{
	if (dos.tables.country == NULL)
		return;

	// Select country information structure
	CountryInfo country_info = COUNTRY_INFO[0];
	for (auto &current : COUNTRY_INFO) {
		if (current.country_number != country_number) continue;

		country_info = current;
		break;
	}

	uint8_t *p;

	// Date format
	p  = dos.tables.country;
	*p = country_info.date_format;

	// Date separation character
	p  = dos.tables.country + 11;
	*p = country_info.date_separator;

	// 12-hour or 24-hour clock
	p  = dos.tables.country + 17;
	*p = country_info.time_format;

	// Time separation character
	p  = dos.tables.country + 13;
	*p = country_info.time_separator;

	// Thousand and decimal separators
	p  = dos.tables.country + 7;
	*p = country_info.thousands_separator;
	p  = dos.tables.country + 9;
	*p = country_info.decimal_separator;
}

const uint8_t DOS_DATE_months[] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

uint8_t GetMonthDays(uint8_t month) {
    return DOS_DATE_months[month];
}

void DOS_AddDays(uint8_t days) {
	dos.date.day += days;
	uint8_t monthlimit = GetMonthDays(dos.date.month);

	if(dos.date.day > monthlimit) {
		if(dos.date.month==2 && dos.date.year%4==0 && (dos.date.year%100!=0 || dos.date.year%400==0)) {
			// leap year
			if(dos.date.day > 29) {
				dos.date.month++;
				dos.date.day -= 29;
			}
		} else {
			//not leap year
			dos.date.month++;
			dos.date.day -= monthlimit;
		}
		if(dos.date.month > 12) {
			// year over
			dos.date.month = 1;
			dos.date.year++;
		}
	}
}

#define DATA_TRANSFERS_TAKE_CYCLES 1
#define DOS_OVERHEAD 1

#ifndef DOSBOX_CPU_H
#include "cpu.h"
#endif

// TODO: Make this configurable.
//       (This can be controlled with the settings "hard drive data rate limit" and "floppy drive data rate limit")
//       Additionally, allow this to vary per-drive so that
//       Drive D: can be as slow as a 2X IDE CD-ROM drive in PIO mode
//       Drive C: can be as slow as a IDE drive in PIO mode and
//       Drive A: can be as slow as a 3.5" 1.44MB floppy disk
//
// This fixes MS-DOS games that crash or malfunction if the disk I/O is too fast.
// This also fixes "380 volt" and prevents the "city animation" from loading too fast for it's music timing (and getting stuck)
int disk_data_rate = 2100000;    // 2.1MBytes/sec mid 1990s IDE PIO hard drive without SMARTDRV
int floppy_data_rate;

void diskio_delay(Bits value/*bytes*/, int type = -1) {
    if ((type == 0 && floppy_data_rate != 0) || (type != 0 && disk_data_rate != 0)) {
        double scalar;
        double endtime;

        if(type == 0) { // Floppy
            scalar = (double)value / floppy_data_rate; 
            endtime = PIC_FullIndex() + (scalar * 1000);
        }
        else { // Hard drive or CD-ROM
            scalar = (double)value / disk_data_rate;
            endtime = PIC_FullIndex() + (scalar * 1000);
        }

        /* MS-DOS will most likely enable interrupts in the course of
         * performing disk I/O */
        CPU_STI();

        do {
            CALLBACK_Idle();
        } while (PIC_FullIndex() < endtime);
    }
}

static void diskio_delay_drive(uint8_t drive, uint16_t delay) {
	if(drive < DOS_DRIVES) {
		bool floppy = (drive < 2);
		if(IS_PC98_ARCH) {
			uint8_t id = Drives[drive]->GetMediaByte();
			floppy = (id == 0xfe) || (id == 0xf0);
		}
		if(floppy)	diskio_delay(delay, 0);
		else		diskio_delay(delay);
	}
}

static void diskio_delay_handle(uint16_t reg_handle, uint16_t delay)
{
	uint8_t handle = RealHandle(reg_handle);
	if(handle != 0xff && Files[handle]) {
		diskio_delay_drive(Files[handle]->GetDrive(), delay);
	}
}

static inline void overhead() {
	reg_ip += 2;
}

#define BCD2BIN(x)	((((unsigned int)(x) >> 4u) * 10u) + ((x) & 0x0fu))
#define BIN2BCD(x)	((((x) / 10u) << 4u) + (x) % 10u)
extern bool date_host_forced;
extern bool dos_a20_disable_on_exec;

static Bitu DOS_21Handler(void);
void XMS_DOS_LocalA20DisableIfNotEnabled(void);
void XMS_DOS_LocalA20DisableIfNotEnabled_XMSCALL(void);
void DOS_Int21_7139(char *name1, const char *name2);
void DOS_Int21_713a(char *name1, const char *name2);
void DOS_Int21_713b(char *name1, const char *name2);
void DOS_Int21_7141(char *name1, const char *name2);
void DOS_Int21_7143(char *name1, const char *name2);
void DOS_Int21_7147(char *name1, const char *name2);
void DOS_Int21_714e(char *name1, char *name2);
void DOS_Int21_714f(const char *name1, const char *name2);
void DOS_Int21_7156(char *name1, char *name2);
void DOS_Int21_7160(char *name1, char *name2);
void DOS_Int21_716c(char *name1, const char *name2);
void DOS_Int21_71a0(char *name1, char *name2);
void DOS_Int21_71a1(const char *name1, const char *name2);
void DOS_Int21_71a6(const char *name1, const char *name2);
void DOS_Int21_71a7(const char *name1, const char *name2);
void DOS_Int21_71a8(char* name1, const char* name2);
void DOS_Int21_71aa(char* name1, const char* name2);
Bitu DEBUG_EnableDebugger(void);
void runMount(const char *str);
bool Network_IsNetworkResource(const char * filename);
void CALLBACK_RunRealInt_retcsip(uint8_t intnum,Bitu &cs,Bitu &ip);

#define DOSNAMEBUF 256
char appname[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII], appargs[CTBUF];
//! \brief Is a DOS program running ? (set by INT21 4B/4C)
bool dos_program_running = false;
bool DOS_BreakINT23InProgress = false;

void DOS_InitClock() {
    if (IS_PC98_ARCH) {
        /* TODO */
    }
    else {
        /* initialize date from BIOS */
        reg_ah = 4;
        reg_cx = reg_dx = 0;
        CALLBACK_RunRealInt(0x1a);
        dos.date.year=BCD2BIN(reg_cl);
        dos.date.month=BCD2BIN(reg_dh);
        dos.date.day=BCD2BIN(reg_dl);
        if (reg_ch >= 0x19 && reg_ch <= 0x20) dos.date.year += BCD2BIN(reg_ch) * 100;
        else dos.date.year += 1900;
        if (dos.date.year < 1980) dos.date.year += 100;
    }
}

void DOS_PrintCBreak() {
	/* print ^C <newline> */
	uint16_t n = 4;
	const char *nl = "^C\r\n";
	DOS_WriteFile(STDOUT,(const uint8_t*)nl,&n);
}

bool DOS_BreakTest(bool print=true) {
	if (DOS_BreakFlag) {
		bool terminate = true;
		bool terminint23 = false;
		Bitu segv,offv;

		/* print ^C on the console */
		if (print) DOS_PrintCBreak();

		DOS_BreakFlag = false;
		DOS_BreakConioFlag = false;

		offv = mem_readw((0x23*4)+0);
		segv = mem_readw((0x23*4)+2);
		if (segv != 0) {
			/* NTS: DOS calls are allowed within INT 23h! */
			Bitu save_sp = reg_sp;

			/* set carry flag */
			reg_flags |= 1;

			/* invoke INT 23h */
			/* NTS: Some DOS programs provide their own INT 23h which then calls INT 21h AH=0x4C
			 *      inside the handler! Set a flag so that if that happens, the termination
			 *      handler will throw us an exception to force our way back here after
			 *      termination completes!
			 *
			 *      This fixes: PC Mix compiler PCL.EXE
			 *
			 *      2023/09/28: Some basic debugging with MS-DOS 6.22 shows the INT 23h handler
			 *                  installed by COMMAND.COM does the same thing (INT 21h AH=0x4C)
			 *                  which is normally still there unless the DOS application itself
			 *                  replaces the vector.
			 *
			 *      FIXME: This is an ugly hack! */
			try {
				DOS_BreakINT23InProgress = true;
				CALLBACK_RunRealInt(0x23);
				DOS_BreakINT23InProgress = false;
			}
			catch (int x) {
				if (x == 0) {
					DOS_BreakINT23InProgress = false;
					terminint23 = true;
				}
				else {
					LOG_MSG("Unexpected code in INT 23h termination exception\n");
					abort();
				}
			}

			/* if the INT 23h handler did not already terminate itself... */
			if (!terminint23) {
				/* if it returned with IRET, or with RETF and CF=0, don't terminate */
				if (reg_sp == save_sp || (reg_flags & 1) == 0) {
					terminate = false;
					LOG_MSG("Note: DOS handler does not wish to terminate\n");
				}
				else {
					/* program does not wish to continue. it used RETF. pop the remaining flags off */
					LOG_MSG("Note: DOS handler does wish to terminate\n");
				}

				if (reg_sp != save_sp) reg_sp += 2;
			}
		}
		else {
			/* Old comment: "HACK: DOSBox's shell currently does not assign INT 23h"
			 * 2023/09/28: The DOSBox command shell now installs a handler, therefore
			 *             a null vector is now something to warn about. */
			LOG_MSG("WARNING: INT 23h CTRL+C vector is NULL\n");
		}

		if (terminate) {
			LOG_MSG("Note: DOS break terminating program\n");
			DOS_Terminate(dos.psp(),false,0);
			*appname=0;
			*appargs=0;
			return false;
		}
		else if (terminint23) {
			LOG_MSG("Note: DOS break handler terminated program for us.\n");
			return false;
		}
	}

	return true;
}

void DOS_BreakAction() {
	DOS_BreakFlag = true;
    DOS_BreakConioFlag = false;
}

/* unmask IRQ 0 automatically on disk I/O functions.
 * there exist old DOS games and demos that rely on very selective IRQ masking,
 * but, their code also assumes that calling into DOS or the BIOS will unmask the IRQ.
 *
 * This fixes "Rebel by Arkham" which masks IRQ 0-7 (PIC port 21h) in a VERY stingy manner!
 *
 *    Pseudocode (early in demo init):
 *
 *             in     al,21h
 *             or     al,3Bh        ; mask IRQ 0, 1, 3, 4, and 5
 *             out    21h,al
 *
 *    Later:
 *
 *             mov    ah,3Dh        ; open file
 *             ...
 *             int    21h
 *             ...                  ; demo apparently assumes that INT 21h will unmask IRQ 0 when reading, because ....
 *             in     al,21h
 *             or     al,3Ah        ; mask IRQ 1, 3, 4, and 5
 *             out    21h,al
 *
 * The demo runs fine anyway, but if we do not unmask IRQ 0 at the INT 21h call, the timer never ticks and the
 * demo does not play any music (goldplay style, of course).
 *
 * This means several things. One is that a disk cache (which may provide the file without using INT 13h) could
 * mysteriously prevent the demo from playing music. Future OS changes, where IRQ unmasking during INT 21h could
 * not occur, would also prevent it from working. I don't know what the programmer was thinking, but side
 * effects like that are not to be relied on!
 *
 * On the other hand, perhaps masking the keyboard (IRQ 1) was intended as an anti-debugger trick? You can't break
 * into the demo if you can't trigger the debugger, after all! The demo can still poll the keyboard controller
 * for ESC or whatever.
 *
 * --J.C. */
bool disk_io_unmask_irq0 = true;

void XMS_DOS_LocalA20EnableIfNotEnabled(void);
void XMS_DOS_LocalA20EnableIfNotEnabled_XMSCALL(void);

typedef struct {
	UINT16 size_of_structure;
	UINT16 structure_version;
	UINT32 sectors_per_cluster;
	UINT32 bytes_per_sector;
	UINT32 available_clusters_on_drive;
	UINT32 total_clusters_on_drive;
	UINT32 available_sectors_on_drive;
	UINT32 total_sectors_on_drive;
	UINT32 available_allocation_units;
	UINT32 total_allocation_units;
	UINT8 reserved[8];
} ext_space_info_t;

char res1[CROSS_LEN] = {0}, res2[CROSS_LEN], *result;
const char * TranslateHostPath(const char * arg, bool next = false) {
    result = next ? res2: res1;
    if (!starttranspath||!*arg) {
        strcpy(result, arg);
        return result;
    }
    std::string args = arg;
    size_t spos = 0, epos = 0, lastpos;
    while (1) {
        lastpos = spos;
        spos += epos+args.substr(lastpos+epos).find_first_not_of(' ');
        if (spos<lastpos+epos) {
            strcat(result, args.substr(lastpos+epos).c_str());
            break;
        }
        if (!lastpos&&!epos) strcpy(result, args.substr(0, spos).c_str());
        else strcat(result, args.substr(lastpos+epos, spos-lastpos-epos).c_str());
        if (arg[spos] == '"') {
            epos = args.substr(spos+1).find("\" ");
            if (epos == std::string::npos) epos = args.size() - spos;
            else epos += 2;
        } else {
            epos = args.substr(spos).find_first_of(' ');
            if (epos == std::string::npos) epos = args.size() - spos;
        }
        uint8_t drive;
        char fullname[DOS_PATHLENGTH];
        std::string name = args.substr(spos, epos);
        if (DOS_MakeName(name.c_str(), fullname, &drive) && (!strncmp(Drives[drive]->GetInfo(),"local ",6) || !strncmp(Drives[drive]->GetInfo(),"CDRom ",6))) {
           localDrive *ldp = dynamic_cast<localDrive*>(Drives[drive]);
           Overlay_Drive *odp = dynamic_cast<Overlay_Drive*>(Drives[drive]);
           std::string hostname = "";
           if (odp) hostname = odp->GetHostName(fullname);
           else if (ldp) hostname = ldp->GetHostName(fullname);
           if (hostname.size()) {
               if (arg[spos] == '"') strcat(result, "\"");
               strcat(result, hostname.c_str());
               if (arg[spos] == '"') strcat(result, "\"");
           } else
               strcat(result, name.c_str());
        } else
            strcat(result, name.c_str());
    }
    return result;
}

#if defined (WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
intptr_t hret=0;
void EndRunProcess() {
    if(hret) {
        DWORD exitCode;
        GetExitCodeProcess((HANDLE)hret, &exitCode);
        if (exitCode==STILL_ACTIVE)
            TerminateProcess((HANDLE)hret, 0);
    }
    ctrlbrk=false;
}

void HostAppRun() {
    char comline[256], *p=comline;
    char winDirCur[512], winDirNew[512], winName[256], dir[CROSS_LEN+15];
    char *fullname=appname;
    uint8_t drive;
    if (!DOS_MakeName(fullname, winDirNew, &drive)) return;
    bool net = false;
#if !defined(__MINGW32__) || defined(__MINGW64_VERSION_MAJOR)
    if (Network_IsNetworkResource(fullname)) {
        net = true;
        strcpy(winName, fullname);
    }
#endif
    if (GetCurrentDirectory(512, winDirCur)&&(net||!strncmp(Drives[drive]->GetInfo(),"local ",6)||!strncmp(Drives[drive]->GetInfo(),"CDRom ",6))) {
        bool useoverlay=false;
        Overlay_Drive *odp = dynamic_cast<Overlay_Drive*>(Drives[drive]);
        if (odp != NULL && !net) {
            strcpy(winName, odp->getOverlaydir());
            strcat(winName, winDirNew);
            struct stat tempstat;
            if (stat(winName,&tempstat)==0 && (tempstat.st_mode & S_IFDIR)==0)
                useoverlay=true;
        }
        if (!useoverlay && !net) {
            strcpy(winName, Drives[drive]->GetBaseDir());
            strcat(winName, winDirNew);
            if (!PathFileExists(winName)) {
                bool olfn=uselfn;
                uselfn=true;
                if (DOS_GetSFNPath(fullname,dir,true)&&DOS_MakeName(("\""+std::string(dir)+"\"").c_str(), winDirNew, &drive)) {
                    strcpy(winName, Drives[drive]->GetBaseDir());
                    strcat(winName, winDirNew);
                }
                uselfn=olfn;
            }
        }
        if (!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"local ",6)||!strncmp(Drives[DOS_GetDefaultDrive()]->GetInfo(),"CDRom ",6)) {
            Overlay_Drive *ddp = dynamic_cast<Overlay_Drive*>(Drives[DOS_GetDefaultDrive()]);
            strcpy(winDirNew, ddp!=NULL?ddp->getOverlaydir():Drives[DOS_GetDefaultDrive()]->GetBaseDir());
            strcat(winDirNew, Drives[DOS_GetDefaultDrive()]->curdir);
            if (!PathFileExists(winDirNew)) {
                bool olfn=uselfn;
                uselfn=true;
                if (DOS_GetCurrentDir(0,dir,true)) {
                    strcpy(winDirNew, ddp!=NULL?ddp->getOverlaydir():Drives[DOS_GetDefaultDrive()]->GetBaseDir());
                    strcat(winDirNew, dir);
                }
                uselfn=olfn;
            }
        } else {
            strcpy(winDirNew, useoverlay?odp->getOverlaydir():Drives[drive]->GetBaseDir());
            strcat(winDirNew, Drives[drive]->curdir);
        }
        if (SetCurrentDirectory(winDirNew)||net) {
            SHELLEXECUTEINFO lpExecInfo;
            strcpy(comline, appargs);
            strcpy(comline, trim((char *)TranslateHostPath(p)));
            if (!startquiet) {
                char msg[]="Now run it as a Windows application...\r\n";
                uint16_t s = (uint16_t)strlen(msg);
                DOS_WriteFile(STDOUT,(uint8_t*)msg,&s);
            }
            DWORD temp = (DWORD)SHGetFileInfo(winName,0,NULL,0,SHGFI_EXETYPE);
            if (temp==0) temp = (DWORD)SHGetFileInfo((std::string(winDirNew)+"\\"+std::string(fullname)).c_str(),0,NULL,0,SHGFI_EXETYPE);
            if (HIWORD(temp)==0 && LOWORD(temp)==0x4550) { // Console applications
                lpExecInfo.cbSize  = sizeof(SHELLEXECUTEINFO);
                lpExecInfo.fMask=SEE_MASK_DOENVSUBST|SEE_MASK_NOCLOSEPROCESS;
                lpExecInfo.hwnd = NULL;
                lpExecInfo.lpVerb = "open";
                lpExecInfo.lpDirectory = NULL;
                lpExecInfo.nShow = SW_SHOW;
                lpExecInfo.hInstApp = (HINSTANCE) SE_ERR_DDEFAIL;
                strcpy(dir, "/C \"");
                strcat(dir, winName);
                strcat(dir, " ");
                strcat(dir, comline);
                strcat(dir, " & echo( & echo The command execution is completed. & pause\"");
                lpExecInfo.lpFile = "CMD.EXE";
                lpExecInfo.lpParameters = dir;
                ShellExecuteEx(&lpExecInfo);
                hret = (intptr_t)lpExecInfo.hProcess;
            } else {
                char qwinName[258];
                sprintf(qwinName,"\"%s\"",winName);
                hret = _spawnl(P_NOWAIT, winName, qwinName, comline, NULL);
            }
            SetCurrentDirectory(winDirCur);
            if (startwait && hret > 0) {
                int count=0;
                ctrlbrk=false;
                DWORD exitCode = 0;
                GetExitCodeProcess((HANDLE)hret, &exitCode);
                while (GetExitCodeProcess((HANDLE)hret, &exitCode) && exitCode == STILL_ACTIVE) {
                    CALLBACK_Idle();
                    if (ctrlbrk) {
                        uint8_t c;uint16_t n=1;
                        DOS_ReadFile (STDIN,&c,&n);
                        if (c == 3) {
                            char msg[]="^C\r\n";
                            uint16_t s = (uint16_t)strlen(msg);
                            DOS_WriteFile(STDOUT,(uint8_t*)msg,&s);
                        }
                        EndRunProcess();
                        exitCode=0;
                        break;
                    }
                    if (++count==20000&&!startquiet) {
                        char msg[]="(Press Ctrl+C to exit immediately)\r\n";
                        uint16_t s = (uint16_t)strlen(msg);
                        DOS_WriteFile(STDOUT,(uint8_t*)msg,&s);
                    }
                }
                dos.return_code = exitCode&255;
                dos.return_mode = 0;
                hret = 0;
            } else if (hret > 0)
                hret = 0;
            else
                hret = errno;
            DOS_SetError((uint16_t)hret);
            hret=0;
            return;
        } else if (startquiet) {
            char msg[]="This program cannot be run in DOS mode.\r\n";
            uint16_t s = (uint16_t)strlen(msg);
            DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
        }
    } else if (startquiet) {
        char msg[]="This program cannot be run in DOS mode.\r\n";
        uint16_t s = (uint16_t)strlen(msg);
        DOS_WriteFile(STDERR,(uint8_t*)msg,&s);
    }
}
#endif

#define IAS_DEVICE_HANDLE 0x1a50
#define MSKANJI_DEVICE_HANDLE 0x1a51
#define IBMJP_DEVICE_HANDLE 0x1a52
#define AVSDRV_DEVICE_HANDLE 0x1a53

/* called by shell to flush keyboard buffer right before executing the program to avoid
 * having the Enter key in the buffer to confuse programs that act immediately on keyboard input. */
void DOS_FlushSTDIN(void) {
	LOG_MSG("Flush STDIN");
	uint8_t handle=RealHandle(STDIN);
	if (handle!=0xFF && Files[handle] && Files[handle]->IsName("CON")) {
		uint8_t c;uint16_t n;
		while (DOS_GetSTDINStatus()) {
			n=1; DOS_ReadFile(STDIN,&c,&n);
		}
	}
}

static Bitu DOS_21Handler(void) {
    bool unmask_irq0 = false;

    /* NTS to ognjenmi: Your INT 21h logging patch was modified to log ALL INT 21h calls (the original
     *                  placement put it after the ignore case below), and is now conditional on
     *                  whether INT 21h logging is enabled. Also removed unnecessary copying of reg_al
     *                  and reg_ah to auto type variables. */
    if (log_int21) {
        LOG(LOG_DOSMISC, LOG_DEBUG)("Executing interrupt 21, ah=%x, al=%x", reg_ah, reg_al);
    }

    /* Real MS-DOS behavior:
     *   If HIMEM.SYS is loaded and CONFIG.SYS says DOS=HIGH, DOS will load itself into the HMA area.
     *   To prevent crashes, the INT 21h handler down below will enable the A20 gate before executing
     *   the DOS kernel. */
    if (DOS_IS_IN_HMA()) {
        if (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM))) /* virtual 8086 mode */
            XMS_DOS_LocalA20EnableIfNotEnabled_XMSCALL();
        else
            XMS_DOS_LocalA20EnableIfNotEnabled();
    }

    if (((reg_ah != 0x50) && (reg_ah != 0x51) && (reg_ah != 0x62) && (reg_ah != 0x64)) && (reg_ah<0x6c)) {
        DOS_PSP psp(dos.psp());
        psp.SetStack(RealMake(SegValue(ss),reg_sp-18));
        /* Save registers */
        real_writew(SegValue(ss), reg_sp - 18, reg_ax);
        real_writew(SegValue(ss), reg_sp - 16, reg_bx);
        real_writew(SegValue(ss), reg_sp - 14, reg_cx);
        real_writew(SegValue(ss), reg_sp - 12, reg_dx);
        real_writew(SegValue(ss), reg_sp - 10, reg_si);
        real_writew(SegValue(ss), reg_sp - 8, reg_di);
        real_writew(SegValue(ss), reg_sp - 6, reg_bp);
        real_writew(SegValue(ss), reg_sp - 4, SegValue(ds));
        real_writew(SegValue(ss), reg_sp - 2, SegValue(es));
    }

    if (reg_ah == 0x06) {
        /* does not check CTRL+BREAK. Some DOS programs do not expect to be interrupted with INT 23h if they read */
        /* keyboard input through this and may cause system instability if terminated. This fixes PC-98 text editor
         * VZ.EXE which will leave it's INT 6h handler in memory if interrupted this way, for example. */
        /* See also:
         *   [http://www.ctyme.com/intr/rb-2558.htm] INT 21h AH=6
         *   [http://www.ctyme.com/intr/rb-2559.htm] INT 21h AH=6 DL=FFh
         */
    }
    else {
        if (((reg_ah >= 0x01 && reg_ah <= 0x0C) || (reg_ah != 0 && reg_ah != 0x4C && reg_ah != 0x31 && dos.breakcheck)) && !DOS_BreakTest()) return CBRET_NONE;
    }

    char name1[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
    char name2[DOSNAMEBUF+2+DOS_NAMELENGTH_ASCII];
    
    if (reg_ah!=0x4c&&reg_ah!=0x51) {packerr=false;reqwin=false;}
    switch (reg_ah) {
        case 0x00:      /* Terminate Program */
            /* HACK for demoscene prod parties/1995/wired95/surprisecode/w95spcod.zip/WINNERS/SURP-KLF
             *
             * This demo starts off by popping 3 words off the stack (the third into ES to get the top
             * of DOS memory which it then uses to draw into VGA memory). Since SP starts out at 0xFFFE,
             * that means SP wraps around to start popping values out of the PSP segment.
             *
             * Real MS-DOS will also start the demo with SP at 0xFFFE.
             *
             * The demo terminates with INT 20h.
             *
             * This code will fail since the stack pointer must wrap back around to read the segment,
             * unless we read by popping. */
            if (reg_sp > 0xFFFA) {
                LOG(LOG_DOSMISC,LOG_WARN)("DOS:INT 20h/INT 21h AH=00h WARNING, process terminated where stack pointer wrapped around 64K");

                uint16_t f_ip = CPU_Pop16();
                uint16_t f_cs = CPU_Pop16();
                uint16_t f_flags = CPU_Pop16();

                (void)f_flags;
                (void)f_ip;

                LOG(LOG_DOSMISC,LOG_DEBUG)("DOS:INT 20h/INT 21h AH=00h recovered CS segment %04x",f_cs);

                DOS_Terminate(f_cs,false,0);
            } else if (dos.version.major >= 7 && uselfn && mem_readw(SegPhys(ss)+reg_sp) >=0x2700 && mem_readw(SegPhys(ss)+reg_sp+2)/0x100 == 0x90 && dos.psp()/0x100 >= 0xCC)
                /* Wengier: This case fixes the bug that DIR /S from MS-DOS 7+ could crash hard within DOSBox-X. With this change it should now work properly. */
                DOS_Terminate(dos.psp(),false,0);
			else
                DOS_Terminate(real_readw(SegValue(ss),reg_sp+2),false,0);

            if (DOS_BreakINT23InProgress) throw int(0); /* HACK: Ick */
            dos_program_running = false;
            *appname=0;
            *appargs=0;
            break;
        case 0x01:      /* Read character from STDIN, with echo */
            {   
                uint8_t c;uint16_t n=1;
                dos.echo=true;
                DOS_ReadFile(STDIN,&c,&n);
                if (c == 3) {
                    DOS_BreakAction();
                    if (!DOS_BreakTest()) return CBRET_NONE;
                }
                reg_al=c;
                dos.echo=false;
            }
            break;
        case 0x02:      /* Write character to STDOUT */
            {
                uint8_t c=reg_dl;uint16_t n=1;
                DOS_WriteFile(STDOUT,&c,&n);
                //Not in the official specs, but happens nonetheless. (last written character)
                reg_al=(c==9)?0x20:c; //strangely, tab conversion to spaces is reflected here
            }
            break;
        case 0x03:      /* Read character from STDAUX */
            {
                uint16_t port = real_readw(0x40,0);
                if(port!=0 && serialports[0]) {
                    uint8_t status;
                    // RTS/DTR on
                    IO_WriteB((Bitu)port + 4u, 0x3u);
                    serialports[0]->Getchar(&reg_al, &status, true, 0xFFFFFFFF);
                }
            }
            break;
        case 0x04:      /* Write Character to STDAUX */
            {
                uint16_t port = real_readw(0x40,0);
                if(port!=0 && serialports[0]) {
                    // RTS/DTR on
                    IO_WriteB((Bitu)port + 4u, 0x3u);
                    serialports[0]->Putchar(reg_dl,true,true, 0xFFFFFFFF);
                    // RTS off
                    IO_WriteB((Bitu)port + 4u, 0x1u);
                }
            }
            break;
        case 0x05:      /* Write Character to PRINTER */
            {
                for(unsigned int i = 0; i < 3; i++) {
                    // look up a parallel port
                    if(parallelPortObjects[i] != NULL) {
                        parallelPortObjects[i]->Putchar(reg_dl);
                        break;
                    }
                }
                break;
            }
        case 0x06:      /* Direct Console Output / Input */
            switch (reg_dl) {
                case 0xFF:  /* Input */
                    {   
                        //Simulate DOS overhead for timing sensitive games
                        //MM1
                        overhead();
                        //TODO Make this better according to standards
                        if (!DOS_GetSTDINStatus()) {
                            reg_al=0;
                            CALLBACK_SZF(true);
                            break;
                        }
                        uint8_t c;uint16_t n=1;
                        DOS_ReadFile(STDIN,&c,&n);
                        reg_al=c;
                        CALLBACK_SZF(false);
                        break;
                    }
                default:
                    {
                        uint8_t c = reg_dl;uint16_t n = 1;
                        dos.direct_output=true;
                        DOS_WriteFile(STDOUT,&c,&n);
                        dos.direct_output=false;
                        reg_al=c;
                    }
                    break;
            }
            break;
        case 0x07:      /* Character Input, without echo */
            {
                uint8_t c;uint16_t n=1;
                DOS_ReadFile (STDIN,&c,&n);
                reg_al=c;
                break;
            }
        case 0x08:      /* Direct Character Input, without echo (checks for breaks officially :)*/
            {
                uint8_t c;uint16_t n=1;
                DOS_ReadFile (STDIN,&c,&n);
                if (c == 3) {
                    DOS_BreakAction();
                    if (!DOS_BreakTest()) return CBRET_NONE;
                }
                reg_al=c;
                break;
            }
        case 0x09:      /* Write string to STDOUT */
            {
                uint8_t c=0;uint16_t n=1;
                PhysPt buf=SegPhys(ds)+reg_dx;
                std::string str="";
                if (mem_readb(buf)=='T')
                    while ((c=mem_readb(buf++))!='$') {
                        str+=std::string(1, c);
                        if (str.length()>42) break;
                    }
                buf=SegPhys(ds)+reg_dx;
                reqwin=(str.length()==42&&!strncmp(str.c_str(),"This program cannot be run in DOS mode.",39))||(!strncmp(str.c_str(),"This program requires Microsoft Windows.",40))||(str.length()==38&&!strncmp(str.c_str(),"This program must be run under Win32",36));
                if (!winautorun||!reqwin||control->SecureMode()||!startquiet)
                    while ((c=mem_readb(buf++))!='$')
                        DOS_WriteFile(STDOUT,&c,&n);
                reg_al=c;
            }
            break;
        case 0x0a:      /* Buffered Input */
            {
                //TODO ADD Break checkin in STDIN but can't care that much for it
                PhysPt data=SegPhys(ds)+reg_dx;
                uint8_t free=mem_readb(data);
                uint8_t read=0;uint8_t c;uint16_t n=1;
                if (!free) break;
                free--;
                for(;;) {
                    if (!DOS_BreakTest()) return CBRET_NONE;
                    DOS_ReadFile(STDIN,&c,&n);
                    if (n == 0)				// End of file
                        E_Exit("DOS:0x0a:Redirected input reached EOF");
                    if (c == 10)			// Line feed
                        continue;
                    if (c == 8) {           // Backspace
                        if (read) { //Something to backspace.
                            Bitu flag = 0;
                            for(uint8_t pos = 0 ; pos < read ; pos++) {
                                c = mem_readb(data + pos + 2);
                                if(flag == 1) {
                                    flag = 2;
                                } else {
                                    flag = 0;
                                    if(isKanji1(c)) {
                                        flag = 1;
                                    }
#if defined(USE_TTF)
                                    if(IS_JEGA_ARCH || IS_DOSV || ttf_dosv) {
#else
                                    if(IS_JEGA_ARCH || IS_DOSV) {
#endif
                                        if(CheckHat(c)) {
                                            flag = 2;
                                        }
                                    }
                                }
                            }
                            // STDOUT treats backspace as non-destructive.
                            if(flag == 0) {
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                c = ' '; DOS_WriteFile(STDOUT,&c,&n);
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                --read;
                            } else if(flag == 1) {
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                c = ' '; DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                read -= 2;
                            } else if(flag == 2) {
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                c = ' '; DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                c = 8;   DOS_WriteFile(STDOUT,&c,&n);
                                         DOS_WriteFile(STDOUT,&c,&n);
                                --read;
                            }
                        }
                        continue;
                    }
                    if (c == 3) {   // CTRL+C
                        DOS_BreakAction();
                        if (!DOS_BreakTest()) return CBRET_NONE;
                    }
#if defined(USE_TTF)
                    if ((IS_JEGA_ARCH || IS_DOSV || ttf_dosv) && c == 7) {
#else
                    if ((IS_JEGA_ARCH || IS_DOSV) && c == 7) {
#endif
                        DOS_WriteFile(STDOUT, &c, &n);
                        continue;
                    }
                    if (read == free && c != 13) {      // Keyboard buffer full
                        uint8_t bell = 7;
                        DOS_WriteFile(STDOUT, &bell, &n);
                        continue;
                    }
                    DOS_WriteFile(STDOUT,&c,&n);
                    mem_writeb(data+read+2,c);
                    if (c==13) 
                        break;
                    read++;
                }
                mem_writeb(data+1,read);
                break;
            }
        case 0x0b:      /* Get STDIN Status */
            if (!DOS_GetSTDINStatus()) {reg_al=0x00;}
            else {reg_al=0xFF;}
            //Simulate some overhead for timing issues
            //Tankwar menu (needs maybe even more)
            overhead();
            break;
        case 0x0c:      /* Flush Buffer and read STDIN call */
            {
                /* flush buffer if STDIN is CON */
                uint8_t handle=RealHandle(STDIN);
                if (handle!=0xFF && Files[handle] && Files[handle]->IsName("CON")) {
                    uint8_t c;uint16_t n;
                    while (DOS_GetSTDINStatus()) {
                        n=1;    DOS_ReadFile(STDIN,&c,&n);
                    }
                }
                switch (reg_al) {
                    case 0x1:
                    case 0x6:
                    case 0x7:
                    case 0x8:
                    case 0xa:
                        { 
                            uint8_t oldah=reg_ah;
                            reg_ah=reg_al;
                            DOS_21Handler();
                            reg_ah=oldah;
                        }
                        break;
                    default:
                        //              LOG_ERROR("DOS:0C:Illegal Flush STDIN Buffer call %d",reg_al);
                        reg_al=0;
                        break;
                }
            }
            break;
            //TODO Find out the values for when reg_al!=0
            //TODO Hope this doesn't do anything special
        case 0x0d:      /* Disk Reset */
            //Sure let's reset a virtual disk
            break;  
        case 0x0e:      /* Select Default Drive */
            DOS_SetDefaultDrive(reg_dl);
            {
                int drive=maxdrive>=1&&maxdrive<=26?maxdrive:1;
                for (uint16_t i=drive;i<DOS_DRIVES;i++) if (Drives[i]) drive=i+1;
                reg_al=drive;
            }
            break;
        case 0x0f:      /* Open File using FCB */
            if(DOS_FCBOpen(SegValue(ds),reg_dx)){
                reg_al=0;
            }else{
                reg_al=0xff;
            }
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x0f FCB-fileopen used, result:al=%d",reg_al);
            break;
        case 0x10:      /* Close File using FCB */
            if(DOS_FCBClose(SegValue(ds),reg_dx)){
                reg_al=0;
            }else{
                reg_al=0xff;
            }
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x10 FCB-fileclose used, result:al=%d",reg_al);
            break;
        case 0x11:      /* Find First Matching File using FCB */
            if(DOS_FCBFindFirst(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x11 FCB-FindFirst used, result:al=%d",reg_al);
            break;
        case 0x12:      /* Find Next Matching File using FCB */
            if(DOS_FCBFindNext(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x12 FCB-FindNext used, result:al=%d",reg_al);
            break;
        case 0x13:      /* Delete File using FCB */
            if (DOS_FCBDeleteFile(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x16 FCB-Delete used, result:al=%d",reg_al);
            break;
        case 0x14:      /* Sequential read from FCB */
            reg_al = DOS_FCBRead(SegValue(ds),reg_dx,0);
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x14 FCB-Read used, result:al=%d",reg_al);
            break;
        case 0x15:      /* Sequential write to FCB */
            reg_al=DOS_FCBWrite(SegValue(ds),reg_dx,0);
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x15 FCB-Write used, result:al=%d",reg_al);
            break;
        case 0x16:      /* Create or truncate file using FCB */
            if (DOS_FCBCreate(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x16 FCB-Create used, result:al=%d",reg_al);
            break;
        case 0x17:      /* Rename file using FCB */     
            if (DOS_FCBRenameFile(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            break;
        case 0x18:      /* NULL Function for CP/M compatibility or Extended rename FCB */
            goto default_fallthrough;
        case 0x19:      /* Get current default drive */
            reg_al = DOS_GetDefaultDrive();
            break;
        case 0x1a:      /* Set Disk Transfer Area Address */
            dos.dta(RealMakeSeg(ds, reg_dx));
            break;
        case 0x1b:      /* Get allocation info for default drive */ 
            if (!DOS_GetAllocationInfo(0,&reg_cx,&reg_al,&reg_dx)) reg_al=0xff;
            break;
        case 0x1c:      /* Get allocation info for specific drive */
            if (!DOS_GetAllocationInfo(reg_dl,&reg_cx,&reg_al,&reg_dx)) reg_al=0xff;
            break;
        case 0x1d:      /* NULL Function for CP/M compatibility or Extended rename FCB */
            goto default_fallthrough;
        case 0x1e:      /* NULL Function for CP/M compatibility or Extended rename FCB */
            goto default_fallthrough;
        case 0x1f: /* Get drive parameter block for default drive */
            goto case_0x32_fallthrough;
        case 0x20:      /* NULL Function for CP/M compatibility or Extended rename FCB */
            goto default_fallthrough;
        case 0x21:      /* Read random record from FCB */
            {
                uint16_t toread=1;
                reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,&toread,true);
            }
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x21 FCB-Random read used, result:al=%d",reg_al);
            break;
        case 0x22:      /* Write random record to FCB */
            {
                uint16_t towrite=1;
                reg_al=DOS_FCBRandomWrite(SegValue(ds),reg_dx,&towrite,true);
            }
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x22 FCB-Random write used, result:al=%d",reg_al);
            break;
        case 0x23:      /* Get file size for FCB */
            if (DOS_FCBGetFileSize(SegValue(ds),reg_dx)) reg_al = 0x00;
            else reg_al = 0xFF;
            break;
        case 0x24:      /* Set Random Record number for FCB */
            DOS_FCBSetRandomRecord(SegValue(ds),reg_dx);
            break;
        case 0x25:      /* Set Interrupt Vector */
            // Magical Girl Pretty Sammy
            // Patch sound driver bugs. Swap the order of "mov sp" and "mov ss".
            if(IS_PC98_ARCH && reg_al == 0x60 && !strcmp(RunningProgram, "SNDCDDRV")
              && real_readd(SegValue(ds), reg_dx + 47) == 0x0fa6268b && real_readd(SegValue(ds), reg_dx + 52) == 0x0fa4168e) {
                real_writed(SegValue(ds), reg_dx + 47, 0x0fa4168e);
                real_writed(SegValue(ds), reg_dx + 52, 0x0fa6268b);
            }
            RealSetVec(reg_al, RealMakeSeg(ds, reg_dx));
            break;
        case 0x26:      /* Create new PSP */
            /* TODO: DEBUG.EXE/DEBUG.COM as shipped with MS-DOS seems to reveal a bug where,
             *       when DEBUG.EXE calls this function and you're NOT loading a program to debug,
             *       the CP/M CALL FAR instruction's offset field will be off by 2. When does
             *       that happen, and how do we emulate that? */
            DOS_NewPSP(reg_dx, DOS_PSP(dos.psp()).GetSize());
            reg_al = 0xf0;    /* al destroyed */
            break;
        case 0x27:      /* Random block read from FCB */
            reg_al = DOS_FCBRandomRead(SegValue(ds),reg_dx,&reg_cx,false);
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x27 FCB-Random(block) read used, result:al=%d",reg_al);
            break;
        case 0x28:      /* Random Block write to FCB */
            reg_al=DOS_FCBRandomWrite(SegValue(ds),reg_dx,&reg_cx,false);
            LOG(LOG_FCB,LOG_NORMAL)("DOS:0x28 FCB-Random(block) write used, result:al=%d",reg_al);
            break;
        case 0x29:      /* Parse filename into FCB */
            {   
                uint8_t difference;
                char string[1024];
                MEM_StrCopy(SegPhys(ds)+reg_si,string,1023); // 1024 toasts the stack
                reg_al=FCB_Parsename(SegValue(es),reg_di,reg_al ,string, &difference);
                reg_si+=difference;
            }
            LOG(LOG_FCB,LOG_NORMAL)("DOS:29:FCB Parse Filename, result:al=%d",reg_al);
            break;
        case 0x2a:      /* Get System Date */
            // use BIOS to get system date
            if (IS_PC98_ARCH) {
                CPU_Push16(reg_ax);
                CPU_Push16(reg_bx);
                CPU_Push16(SegValue(es));
                reg_sp -= 6;

                reg_ah = 0;     // get time
                reg_bx = reg_sp;
                SegSet16(es,SegValue(ss));
                CALLBACK_RunRealInt(0x1c);

                uint32_t memaddr = ((uint32_t)SegValue(es) << 4u) + reg_bx;

                reg_sp += 6;
                SegSet16(es,CPU_Pop16());
                reg_bx = CPU_Pop16();
                reg_ax = CPU_Pop16();

                reg_cx = 1900u + BCD2BIN(mem_readb(memaddr+0u));                  // year
                if (reg_cx < 1980u) reg_cx += 100u;
                reg_dh = BCD2BIN((unsigned int)mem_readb(memaddr+1) >> 4u);
                reg_dl = BCD2BIN(mem_readb(memaddr+2));
                reg_al = BCD2BIN(mem_readb(memaddr+1) & 0xFu);

                dos.date.year=reg_cx;
                dos.date.month=reg_dh;
                dos.date.day=reg_dl;
            }
            else {
                // Real hardware testing: DOS appears to read the CMOS once at startup and then the date
                // is stored and counted internally. It does not read via INT 1Ah. This means it is possible
                // for DOS and the BIOS 1Ah/CMOS to have totally different time and date!
                reg_cx = dos.date.year;
                reg_dh = dos.date.month;
                reg_dl = dos.date.day;

                // calculate day of week (we could of course read it from CMOS, but never mind)
                /* Use Zeller's congruence */
                uint16_t year = reg_cx, month = reg_dh, day = reg_dl;
                int8_t weekday;
                if(month <= 2) {
                    year--;
                    month += 12;
                }
                weekday = (year + year / 4u  - year / 100u + year / 400u + (13u * month + 8u) / 5u + day) % 7;
                reg_al = weekday < 0 ? weekday + 7 : weekday;
                /* Sunday=0, Monday=1, ... */
            }
            break;
        case 0x2b:      /* Set System Date */
            {
                // unfortunately, BIOS does not return whether succeeded
                // or not, so do a sanity check first

                int maxday[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}; /* cannot be const, see maxday[1]++ increment below! */

                if (reg_cx % 4 == 0 && (reg_cx % 100 != 0 || reg_cx % 400 == 0))
                    maxday[1]++;

                if (reg_cx < 1980 || reg_cx > 9999 || reg_dh < 1 || reg_dh > 12 || reg_dl < 1 || reg_dl > maxday[reg_dh - 1])
                {
                    reg_al = 0xff;              // error!
                    break;                      // done
                }

                if (IS_PC98_ARCH) {
                    /* TODO! */
                }
                else {
                    uint16_t cx = reg_cx;

                    CPU_Push16(reg_ax);
                    CPU_Push16(reg_cx);
                    CPU_Push16(reg_dx);

                    reg_ah = 5;
                    reg_ch = BIN2BCD(cx / 100);     // century
                    reg_cl = BIN2BCD(cx % 100);     // year
                    reg_dh = BIN2BCD(reg_dh);       // month
                    reg_dl = BIN2BCD(reg_dl);       // day

                    CALLBACK_RunRealInt(0x1a);

                    reg_dx = CPU_Pop16();
                    reg_cx = CPU_Pop16();
                    reg_ax = CPU_Pop16();

                    reg_al = 0;                     // OK
                }
            }
            dos.date.year=reg_cx;
            dos.date.month=reg_dh;
            dos.date.day=reg_dl;
            reg_al=0;
            if (sync_time) {manualtime=true;mainMenu.get_item("sync_host_datetime").check(false).refresh_item(mainMenu);}
            break;
        case 0x2c: {    /* Get System Time */
            // use BIOS to get RTC time
            if (IS_PC98_ARCH) {
                CPU_Push16(reg_ax);
                CPU_Push16(reg_bx);
                CPU_Push16(SegValue(es));
                reg_sp -= 6;

                reg_ah = 0;     // get time
                reg_bx = reg_sp;
                SegSet16(es,SegValue(ss));
                CALLBACK_RunRealInt(0x1c);

                uint32_t memaddr = ((PhysPt)SegValue(es) << 4u) + reg_bx;

                reg_sp += 6;
                SegSet16(es,CPU_Pop16());
                reg_bx = CPU_Pop16();
                reg_ax = CPU_Pop16();

                reg_ch = BCD2BIN(mem_readb(memaddr+3));     // hours
                reg_cl = BCD2BIN(mem_readb(memaddr+4));     // minutes
                reg_dh = BCD2BIN(mem_readb(memaddr+5));     // seconds

                reg_dl = 0;
            }
            else {
                // It turns out according to real hardware, that DOS reads the date and time once on startup
                // and then relies on the BIOS_TIMER counter after that for time, and caches the date. So
                // the code prior to the April 2023 change was correct after all.
                reg_ax=0; // get time
                CALLBACK_RunRealInt(0x1a);
                if(reg_al) DOS_AddDays(reg_al);
                reg_ah=0x2c;

                Bitu ticks=((Bitu)reg_cx<<16)|reg_dx;
                Bitu time=(Bitu)((100.0/((double)PIT_TICK_RATE/65536.0)) * (double)ticks);

                reg_dl=(uint8_t)((Bitu)time % 100); // 1/100 seconds
                time/=100;
                reg_dh=(uint8_t)((Bitu)time % 60); // seconds
                time/=60;
                reg_cl=(uint8_t)((Bitu)time % 60); // minutes
                time/=60;
                reg_ch=(uint8_t)((Bitu)time % 24); // hours
            }

            //Simulate DOS overhead for timing-sensitive games
            //Robomaze 2
            overhead();
            break;
        }
        case 0x2d:      /* Set System Time */
            {
                // unfortunately, BIOS does not return whether succeeded
                // or not, so do a sanity check first
                if (reg_ch > 23 || reg_cl > 59 || reg_dh > 59 || reg_dl > 99)
                {
                    reg_al = 0xff;      // error!
                    break;              // done
                }

                if (IS_PC98_ARCH) {
                    /* TODO! */
                }
                else {
                    // timer ticks every 55ms
                    const uint32_t csec = (((((reg_ch * 60u) + reg_cl) * 60u + reg_dh) * 100u) + reg_dl);
                    const uint32_t ticks = (uint32_t)((double)csec * ((double)PIT_TICK_RATE/6553600.0));

                    CPU_Push16(reg_ax);
                    CPU_Push16(reg_cx);
                    CPU_Push16(reg_dx);

                    // use BIOS to set RTC time
                    reg_ah = 3;     // set RTC time
                    reg_ch = BIN2BCD(reg_ch);       // hours
                    reg_cl = BIN2BCD(reg_cl);       // minutes
                    reg_dh = BIN2BCD(reg_dh);       // seconds
                    reg_dl = 0;                     // no DST

                    CALLBACK_RunRealInt(0x1a);

                    // use BIOS to update clock ticks to sync time
                    // could set directly, but setting is safer to do via dedicated call (at least in theory)
                    reg_ah = 1;     // set system time
                    reg_cx = (uint16_t)(ticks >> 16);
                    reg_dx = (uint16_t)(ticks & 0xffff);

                    CALLBACK_RunRealInt(0x1a);

                    reg_dx = CPU_Pop16();
                    reg_cx = CPU_Pop16();
                    reg_ax = CPU_Pop16();

                    reg_al = 0;                     // OK
                }
            }
            if (sync_time) {manualtime=true;mainMenu.get_item("sync_host_datetime").check(false).refresh_item(mainMenu);}
            break;
        case 0x2e:      /* Set Verify flag */
            dos.verify=(reg_al==1);
            break;
        case 0x2f:      /* Get Disk Transfer Area */
            SegSet16(es,RealSeg(dos.dta()));
            reg_bx=RealOff(dos.dta());
            break;
        case 0x30:      /* Get DOS Version */
            if (reg_al==0) {
                if(IS_J3100) {
                    reg_bh=0x29;     /* Fake Toshiba DOS */
                } else {
                    reg_bh=0xFF;     /* Fake Microsoft DOS */
                }
            }
            if (reg_al==1 && DOS_IS_IN_HMA()) reg_bh=0x10;      /* DOS is in HMA? */
            reg_al=dos.version.major;
            reg_ah=dos.version.minor;
            /* Serialnumber */
            reg_bl=0x00;
            reg_cx=0x0000;
            break;
        case 0x31:      /* Terminate and stay resident */
            // Important: This service does not set the carry flag!
            //
            // Fix for PC-98 game "Yu No". One of the TSRs used by the game, PLAY6.EXE,
            // frees it's own PSP segment before using this TSR system call to remain
            // resident in memory. On real PC-98 MS-DOS, INT 21h AH=31h appears to
            // change the freed segment back into an allocated segment before resizing
            // as directed by the number of paragraphs in DX. In order for PLAY6.EXE to
            // do it's job properly without crashing the game we have to do the same.
            //
            // [Issue #3452]
            //
            // I have not yet verified if IBM PC MS-DOS does the same thing. It probably
            // does. However I have yet to see anything in the IBM PC world do something
            // crazy like that. --Jonathan C.
            {
                DOS_MCB psp_mcb(dos.psp()-1);
                if (psp_mcb.GetPSPSeg() == 0/*free block?*/) {
                    /* say so in the log, and then change it into an allocated block */
                    LOG(LOG_DOSMISC,LOG_DEBUG)("Calling program asking to terminate and stay resident has apparently freed it's own PSP segment");
                    psp_mcb.SetPSPSeg(dos.psp());
                }
            }

            DOS_ResizeMemory(dos.psp(),&reg_dx);
            DOS_Terminate(dos.psp(),true,reg_al);
            if (DOS_BreakINT23InProgress) throw int(0); /* HACK: Ick */
            dos_program_running = false;
            *appname=0;
            *appargs=0;
            break;
        case 0x32: /* Get drive parameter block for specific drive */
            {   /* Officially a dpb should be returned as well. The disk detection part is implemented */
                case_0x32_fallthrough:
                uint8_t drive=reg_dl;
                if (!drive || reg_ah==0x1f) drive = DOS_GetDefaultDrive();
                else drive--;
                if (drive < DOS_DRIVES && Drives[drive] && !Drives[drive]->isRemovable()) {
                    reg_al = 0x00;
                    SegSet16(ds,dos.tables.dpb);
                    reg_bx = drive*dos.tables.dpb_size;
                    if (mem_readw(SegPhys(ds)+reg_bx+0x1F)==0xFFFF) {
                        uint32_t bytes_per_sector,sectors_per_cluster,total_clusters,free_clusters;
                        if (DOS_GetFreeDiskSpace32(reg_dl,&bytes_per_sector,&sectors_per_cluster,&total_clusters,&free_clusters))
                            mem_writew(SegPhys(ds)+reg_bx+0x1F,free_clusters);
                    }
                    LOG(LOG_DOSMISC,LOG_NORMAL)("Get drive parameter block.");
                } else {
                    reg_al=0xff;
                }
            }
            break;
        case 0x33:      /* Extended Break Checking */
            switch (reg_al) {
                case 0:reg_dl=dos.breakcheck;break;         /* Get the breakcheck flag */
                case 1:dos.breakcheck=(reg_dl>0);break;     /* Set the breakcheck flag */
                case 2:{bool old=dos.breakcheck;dos.breakcheck=(reg_dl>0);reg_dl=old;}break;
                case 3: /* Get cpsw */
                       /* Fallthrough */
                case 4: /* Set cpsw */
                       LOG(LOG_DOSMISC,LOG_ERROR)("Someone playing with cpsw %x",reg_ax);
                       break;
		case 5:reg_dl=3;break; /* Drive C: (TODO: Make configurable, or else initially set to Z: then re-set to the first drive mounted */
                case 6:                                         /* Get true version number */
                       reg_bl=dos.version.major;
                       reg_bh=dos.version.minor;
                       reg_dl=dos.version.revision;
                       reg_dh=DOS_IS_IN_HMA()?0x10:0x00;                       /* Dos in HMA?? */
                       break;
                case 7:
                       break;
                default:
                       LOG(LOG_DOSMISC,LOG_ERROR)("Weird 0x33 call %2X",reg_al);
                       reg_al =0xff;
                       break;
            }
            break;
        case 0x34:      /* Get INDos Flag */
            SegSet16(es,DOS_SDA_SEG);
            reg_bx=DOS_SDA_OFS + 0x01;
            break;
        case 0x35:      /* Get interrupt vector */
            reg_bx=real_readw(0,((uint16_t)reg_al)*4);
            SegSet16(es,real_readw(0,((uint16_t)reg_al)*4+2));
            break;
        case 0x36:      /* Get Free Disk Space */
            {
                uint16_t bytes,clusters,free;
                uint8_t sectors;
                if (DOS_GetFreeDiskSpace(reg_dl,&bytes,&sectors,&clusters,&free)) {
                    reg_ax=sectors;
                    reg_bx=free;
                    reg_cx=bytes;
                    reg_dx=clusters;
                } else {
                    uint8_t drive=reg_dl;
                    if (drive==0) drive=DOS_GetDefaultDrive();
                    else drive--;
                    if (drive<2) {
                        // floppy drive, non-present drivesdisks issue floppy check through int24
                        // (critical error handler); needed for Mixed up Mother Goose (hook)
                        //                  CALLBACK_RunRealInt(0x24);
                    }
                    reg_ax=0xffff;  // invalid drive specified
                }
            }
            break;
        case 0x37:      /* Get/Set Switch char Get/Set Availdev thing */
            //TODO  Give errors for these functions to see if anyone actually uses this shit-
            switch (reg_al) {
                case 0:
                    reg_al=0;reg_dl=0x2f;break;  /* always return '/' like dos 5.0+ */
                case 1:
                    LOG(LOG_DOSMISC,LOG_DEBUG)("DOS:0x37:Attempted to set switch char");
                    reg_al=0;break;
                case 2:
                    reg_al=0;reg_dl=0xff;break;  /* AVAILDEV \DEV\ prefix optional */
                case 3:
                    LOG(LOG_DOSMISC,LOG_DEBUG)("DOS:0x37:Attempted to set AVAILDEV \\DEV\\ prefix use");
                    reg_al=0;break;
            }
            break;
        case 0x38:                  /* Set Country Code */  
            if (reg_al==0) {        /* Get country specific information */
                PhysPt dest = SegPhys(ds)+reg_dx;
                MEM_BlockWrite(dest,dos.tables.country,0x18);
				reg_al = countryNo<0xff?countryNo:0xff;
				reg_bx = countryNo;
                CALLBACK_SCF(false);
                break;
			} else if (reg_dx == 0xffff) { /* Set country code */
				countryNo = reg_al==0xff?reg_bx:reg_al;
				DOS_SetCountry(countryNo);
				reg_ax = 0;
				CALLBACK_SCF(false);
				break;
            }
            CALLBACK_SCF(true);
            break;
        case 0x39:      /* MKDIR Create directory */
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			force_sfn = true;
            if (DOS_MakeDir(name1)) {
                reg_ax=0x05;    /* ax destroyed */
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
			force_sfn = false;
            break;
        case 0x3a:      /* RMDIR Remove directory */
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			force_sfn = true;
            if  (DOS_RemoveDir(name1)) {
                reg_ax=0x05;    /* ax destroyed */
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
                LOG(LOG_DOSMISC,LOG_NORMAL)("Remove dir failed on %s with error %X",name1,dos.errorcode);
            }
			force_sfn = false;
            break;
        case 0x3b:      /* CHDIR Set current directory */
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
            if  (DOS_ChangeDir(name1)) {
                reg_ax=0x00;    /* ax destroyed */
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x3c:      /* CREATE Create or truncate file */
			force_sfn = true;
            unmask_irq0 |= disk_io_unmask_irq0;
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
            if (DOS_CreateFile(name1,reg_cx,&reg_ax)) {
                diskio_delay_handle(reg_ax, 2048);
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
			force_sfn = false;
            break;
        case 0x3d:      /* OPEN Open existing file */
		{
            unmask_irq0 |= disk_io_unmask_irq0;
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
#if defined(USE_TTF)
            if((IS_DOSV || ttf_dosv) && IS_DOS_JAPANESE) {
#else
            if(IS_DOSV && IS_DOS_JAPANESE) {
#endif
                char *name_start = name1;
                if(name1[0] == '@' && name1[1] == ':') {
                    name_start += 2;
                }
                if(DOS_CheckExtDevice(name_start, false) == 0) {
                    if(dos.im_enable_flag) {
                        if((DOSV_GetFepCtrl() & DOSV_FEP_CTRL_IAS) && !strncmp(name_start, "$IBMAIAS", 8)) {
                            ias_handle = IAS_DEVICE_HANDLE;
                            reg_ax = IAS_DEVICE_HANDLE;
                            force_sfn = false;
                            CALLBACK_SCF(false);
                            break;
                        } else if((DOSV_GetFepCtrl() & DOSV_FEP_CTRL_MSKANJI) && !strncmp(name_start, "MS$KANJI", 8)) {
                            mskanji_handle = MSKANJI_DEVICE_HANDLE;
                            reg_ax = MSKANJI_DEVICE_HANDLE;
                            force_sfn = false;
                            CALLBACK_SCF(false);
                            break;
                        }
                    }
#if defined(USE_TTF)
                    if(!strncmp(name_start, "$IBMAFNT", 8) || (ttf_dosv && !strncmp(name_start, "$IBMADSP", 8))) {
#else
                    if(!strncmp(name_start, "$IBMAFNT", 8)) {
#endif
                        ibmjp_handle = IBMJP_DEVICE_HANDLE;
                        reg_ax = IBMJP_DEVICE_HANDLE;
                        force_sfn = false;
                        CALLBACK_SCF(false);
                        break;
                    }
                }
            }
            if(IS_PC98_ARCH) {
                if(!strncmp(name1, "AVSDRV$$", 8)) {
                    avsdrv_handle = AVSDRV_DEVICE_HANDLE;
                    reg_ax = AVSDRV_DEVICE_HANDLE;
                    force_sfn = false;
                    CALLBACK_SCF(false);
                    break;
                }
            }
			uint8_t oldal=reg_al;
			force_sfn = true;
            if (DOS_OpenFile(name1,reg_al,&reg_ax)) {
#if defined(USE_TTF)
                if (ttf.inUse&&wpType==1) {
                    int len = (int)strlen(name1);
                    if (len > 10 && !strcmp(name1+len-11, "\\VGA512.CHM"))  // Test for VGA512.CHM
                        WPvga512CHMhandle = reg_ax;							// Save handle
                }
#endif
                diskio_delay_handle(reg_ax, 1024);
				force_sfn = false;
                CALLBACK_SCF(false);
            } else {
				force_sfn = false;
				if (uselfn&&DOS_OpenFile(name1,oldal,&reg_ax)) {
					diskio_delay_handle(reg_ax, 1024);
					CALLBACK_SCF(false);
				} else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
            }
            force_sfn = false;
            break;
		}
        case 0x3e:      /* CLOSE Close file */
        {
            if(ias_handle != 0 && ias_handle == reg_bx) {
                ias_handle = 0;
                CALLBACK_SCF(false);
                break;
            } else if(mskanji_handle != 0 && mskanji_handle == reg_bx) {
                mskanji_handle = 0;
                CALLBACK_SCF(false);
                break;
            } else if(ibmjp_handle != 0 && ibmjp_handle == reg_bx) {
                ibmjp_handle = 0;
                CALLBACK_SCF(false);
                break;
            } else if(avsdrv_handle != 0 && avsdrv_handle == reg_bx) {
                avsdrv_handle = 0;
                CALLBACK_SCF(false);
                break;
            }
            uint8_t handle = RealHandle(reg_bx);
            uint8_t drive = (handle != 0xff && Files[handle]) ? Files[handle]->GetDrive() : DOS_DRIVES;
            unmask_irq0 |= disk_io_unmask_irq0;
            if (DOS_CloseFile(reg_bx, false, &reg_al)) {
#if defined(USE_TTF)
                if (ttf.inUse&&reg_bx == WPvga512CHMhandle)
                    WPvga512CHMhandle = -1;
#endif
                diskio_delay_drive(drive, 512);
                /* al destroyed with pre-close refcount from sft */
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        }
        case 0x3f:      /* READ Read from file or device */
            unmask_irq0 |= disk_io_unmask_irq0;
            /* TODO: If handle is STDIN and not binary do CTRL+C checking */
            { 
                uint16_t toread=reg_cx;
                uint32_t handle = RealHandle(reg_bx);
                bool fRead = false;

                /* if the offset and size exceed the end of the 64KB segment,
                 * truncate the read according to observed MS-DOS 5.0 behavior
                 * where the actual byte count read is 64KB minus (reg_dx % 16).
                 *
                 * This is needed for "Dark Purpose" to read it's DAT file
                 * correctly, which calls INT 21h AH=3Fh with DX=0004h and CX=FFFFh
                 * and will mis-render it's fonts, images, and color palettes
                 * if we do not do this.
                 *
                 * Ref: http://files.scene.org/get/mirrors/hornet/demos/1995/d/darkp.zip */
                if (((uint32_t)toread+(uint32_t)reg_dx) > 0xFFFFUL && (reg_dx & 0xFU) != 0U) {
                    uint16_t nuread = (uint16_t)(0x10000UL - (reg_dx & 0xF)); /* FIXME: If MS-DOS 5.0 truncates it any farther I need to know! */

                    if (nuread > toread) nuread = toread;
                    LOG_MSG("INT 21h READ warning: DX=%04xh CX=%04xh exceeds 64KB, truncating to %04xh",reg_dx,toread,nuread);
                    toread = nuread;
                }

                dos.echo=true;
                if(handle >= DOS_FILES || !Files[handle] || !Files[handle]->IsOpen()) {
                    DOS_SetError(DOSERR_INVALID_HANDLE);
                }
                else if(Files[handle]->GetInformation() & EXT_DEVICE_BIT) {
                    fRead = !(((DOS_ExtDevice*)Files[handle])->CallDeviceFunction(4, 26, SegValue(ds), reg_dx, toread) & 0x8000);
#if defined(USE_TTF)
                    if(fRead && ttf.inUse && reg_bx == WPvga512CHMhandle)
                        MEM_BlockRead(SegPhys(ds) + reg_dx, dos_copybuf, toread);
#endif
                }
                else
                {
                    if((fRead = DOS_ReadFile(reg_bx, dos_copybuf, &toread))) {
                        MEM_BlockWrite(SegPhys(ds) + reg_dx, dos_copybuf, toread);
                        diskio_delay_handle(reg_bx, toread);
                    }
                }

                if (fRead) {
                    reg_ax=toread;
#if defined(USE_TTF)
                    if (ttf.inUse && reg_bx == WPvga512CHMhandle) {
                        if (toread == 26 || toread == 2) {
                            if (toread == 2)
                                WP5chars = *(uint16_t*)dos_copybuf;
                            WPvga512CHMcheck = true;
                        }
                        else if (WPvga512CHMcheck) {
                            if (WP5chars) {
                                memmove(dos_copybuf+2, dos_copybuf, toread);
                                *(uint16_t*)dos_copybuf = WP5chars;
                                WP5chars = 0;
                                WPset512(dos_copybuf, toread+2);
                            } else
                                WPset512(dos_copybuf, toread);
                            WPvga512CHMhandle = -1;
                            WPvga512CHMcheck = false;
                        }
                    }
#endif
                    CALLBACK_SCF(false);
                }
                else if (dos.errorcode==77) {
					DOS_BreakAction();
					if (!DOS_BreakTest()) {
						dos.echo = false;
						return CBRET_NONE;
					} else {
						reg_ax=dos.errorcode;
						CALLBACK_SCF(true);
					}
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
                dos.echo=false;
                break;
            }
        case 0x40:                  /* WRITE Write to file or device */
            unmask_irq0 |= disk_io_unmask_irq0;
            {
                uint16_t towrite=reg_cx;
                bool fWritten;

                /* if the offset and size exceed the end of the 64KB segment,
                 * truncate the write according to observed MS-DOS 5.0 READ behavior
                 * where the actual byte count written is 64KB minus (reg_dx % 16).
                 *
                 * This is copy-paste of AH=3Fh read handling because it's likely
                 * that MS-DOS probably does the same with write as well, though
                 * this has not yet been confirmed. --J.C. */
                if (((uint32_t)towrite+(uint32_t)reg_dx) > 0xFFFFUL && (reg_dx & 0xFU) != 0U) {
                    uint16_t nuwrite = (uint16_t)(0x10000UL - (reg_dx & 0xF)); /* FIXME: If MS-DOS 5.0 truncates it any farther I need to know! */

                    if (nuwrite > towrite) nuwrite = towrite;
                    LOG_MSG("INT 21h WRITE warning: DX=%04xh CX=%04xh exceeds 64KB, truncating to %04xh",reg_dx,towrite,nuwrite);
                    towrite = nuwrite;
                }

                MEM_BlockRead(SegPhys(ds)+reg_dx,dos_copybuf,towrite);
                packerr=reg_bx==2&&towrite==22&&!strncmp((char *)dos_copybuf,"Packed file is corrupt",towrite);
                fWritten = (packerr && !(i4dos && !shellrun) && (!autofixwarn || (autofixwarn == 2 && infix == 0) || (autofixwarn == 1 && infix == 1)));
                if(!fWritten)
                {
                    uint32_t handle = RealHandle(reg_bx);

                    if(handle >= DOS_FILES || !Files[handle] || !Files[handle]->IsOpen()) {
                        DOS_SetError(DOSERR_INVALID_HANDLE);
                    }
                    else if(Files[handle]->GetInformation() & EXT_DEVICE_BIT) {
                        fWritten = !(((DOS_ExtDevice*)Files[handle])->CallDeviceFunction(8, 26, SegValue(ds), reg_dx, towrite) & 0x8000);
                    }
                    else {
                        if((fWritten = DOS_WriteFile(reg_bx, dos_copybuf, &towrite))) {
                            diskio_delay_handle(reg_bx, towrite);
                        }
                    }
                }
                if (fWritten) {
                    reg_ax=towrite;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
                break;
            }
        case 0x41:                  /* UNLINK Delete file */
            unmask_irq0 |= disk_io_unmask_irq0;
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			force_sfn = true;
            if (DOS_UnlinkFile(name1)) {
                diskio_delay_drive((name1[1] == ':') ? toupper(name1[0]) - 'A' : DOS_GetDefaultDrive(), 1024);
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
			force_sfn = false;
            break;
        case 0x42:                  /* LSEEK Set current file position */
            unmask_irq0 |= disk_io_unmask_irq0;
            {
                uint32_t pos=((uint32_t)reg_cx << 16u) + reg_dx;
                if (DOS_SeekFile(reg_bx,&pos,reg_al)) {
                    reg_dx=(uint16_t)((unsigned int)pos >> 16u);
                    reg_ax=(uint16_t)(pos & 0xFFFF);
                    diskio_delay_handle(reg_bx, 32);
                    CALLBACK_SCF(false);
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
                break;
            }
        case 0x43:                  /* Get/Set file attributes */
            unmask_irq0 |= disk_io_unmask_irq0;
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
            switch (reg_al) {
                case 0x00:              /* Get */
                    {
                        uint16_t attr_val=reg_cx;
                        if (DOS_GetFileAttr(name1,&attr_val)) {
                            reg_cx=attr_val;
                            reg_ax=attr_val; /* Undocumented */   
                            CALLBACK_SCF(false);
                        } else {
                            CALLBACK_SCF(true);
                            reg_ax=dos.errorcode;
                        }
                        break;
                    }
                case 0x01:              /* Set */
                    if (DOS_SetFileAttr(name1,reg_cx)) {
                        reg_ax=0x202;   /* ax destroyed */
                        CALLBACK_SCF(false);
                    } else {
                        CALLBACK_SCF(true);
                        reg_ax=dos.errorcode;
                    }
                    break;
                default:
                    LOG(LOG_DOSMISC,LOG_ERROR)("DOS:0x43:Illegal subfunction %2X",reg_al);
                    reg_ax=1;
                    CALLBACK_SCF(true);
                    break;
            }
            break;
        case 0x44:                  /* IOCTL Functions */
            if(ias_handle != 0 && ias_handle == reg_bx) {
                if(reg_al == 0) {
                    reg_dx = 0x0080;
                    CALLBACK_SCF(false);
                    break;
                }
            } else if(mskanji_handle != 0 && mskanji_handle == reg_bx) {
                if(reg_al == 0) {
                    reg_dx = 0x0080;
                    CALLBACK_SCF(false);
                } else if(reg_al == 2 && reg_cx == 4) {
                    real_writew(SegValue(ds), reg_dx, DOSV_GetFontHandlerOffset(DOSV_MSKANJI_API));
                    real_writew(SegValue(ds), reg_dx + 2, CB_SEG);
                    reg_ax = 4;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = 1;
                    CALLBACK_SCF(true);
                }
                break;
            }
            if (DOS_IOCTL()) {
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x45:                  /* DUP Duplicate file handle */
            if (DOS_DuplicateEntry(reg_bx,&reg_ax)) {
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x46:                  /* DUP2,FORCEDUP Force duplicate file handle */
            if (DOS_ForceDuplicateEntry(reg_bx,reg_cx)) {
                reg_ax=reg_cx; //Not all sources agree on it.
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x47:                  /* CWD Get current directory */
			if (DOS_GetCurrentDir(reg_dl,name1,false)) {
                MEM_BlockWrite(SegPhys(ds)+reg_si,name1,(Bitu)(strlen(name1)+1));   
                reg_ax=0x0100;
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x48:                  /* Allocate memory */
            {
                uint16_t size=reg_bx;uint16_t seg;
                if (DOS_AllocateMemory(&seg,&size)) {
                    reg_ax=seg;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax=dos.errorcode;
                    reg_bx=size;
                    CALLBACK_SCF(true);
                }
                break;
            }
        case 0x49:                  /* Free memory */
            if (DOS_FreeMemory(SegValue(es))) {
                CALLBACK_SCF(false);
            } else {            
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x4a:                  /* Resize memory block */
            {
                uint16_t size=reg_bx;
                if (DOS_ResizeMemory(SegValue(es),&size)) {
                    reg_ax=SegValue(es);
                    CALLBACK_SCF(false);
                } else {            
                    reg_ax=dos.errorcode;
                    reg_bx=size;
                    CALLBACK_SCF(true);
                }
                break;
            }
        case 0x4b:                  /* EXEC Load and/or execute program */
            {
                result_errorcode = 0;
                MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);

                /* A20 hack for EXEPACK'd executables */
                if (dos_a20_disable_on_exec) {
                    if (cpu.pmode && ((GETFLAG_IOPL<cpu.cpl) || GETFLAG(VM))) {
                        /* We're running in virtual 8086 mode. Ideally the protected mode kernel would virtualize
                         * port 92h, but it seems Windows 3.1 does not do that. Shutting off the A20 gate in protected
                         * mode will cause the kernel to CRASH. However Windows 3.1 does intercept HIMEM.SYS calls
                         * to enable/disable the A20 gate, so we can accomplish the same effect that way instead and
                         * remain compatible with Windows 3.1. */
                        XMS_DOS_LocalA20DisableIfNotEnabled_XMSCALL();
                    }
                    else {
                        /* We're in real mode. Do it directly. */
                        XMS_DOS_LocalA20DisableIfNotEnabled();
                    }
                    dos_a20_disable_on_exec=false;
                }

                LOG(LOG_EXEC,LOG_NORMAL)("Execute %s %d",name1,reg_al);
                DOS_ParamBlock block(SegPhys(es)+reg_bx);
                block.LoadData();
                CommandTail ctail;
                MEM_BlockRead(Real2Phys(block.exec.cmdtail),&ctail,sizeof(CommandTail));
                if (DOS_Execute(name1,SegPhys(es)+reg_bx,reg_al)) {
                    strcpy(appname, name1);
                    strncpy(appargs, ctail.buffer, sizeof(appargs));
                    if(ctail.count < sizeof(appargs))
                        appargs[ctail.count] = 0;
                    else
                        appname[sizeof(appargs) - 1] = 0;
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
                dos_program_running = true;
            }
            break;
            //TODO Check for use of execution state AL=5
        case 0x4c:                  /* EXIT Terminate with return code */
            DOS_Terminate(dos.psp(),false,reg_al);
            if (result_errorcode) {
                dos.return_code = result_errorcode;
                result_errorcode = 0;
            }
            if (DOS_BreakINT23InProgress) throw int(0); /* HACK: Ick */
#if defined (WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
            if (winautorun&&reqwin&&*appname&&!control->SecureMode())
                HostAppRun();
            reqwin=false;
#endif
            dos_program_running = false;
            *appname=0;
            *appargs=0;
            break;
        case 0x4d:                  /* Get Return code */
            reg_al=dos.return_code;/* Officially read from SDA and clear when read */
            reg_ah=dos.return_mode;
            CALLBACK_SCF(false);
            break;
        case 0x4e:                  /* FINDFIRST Find first matching file */
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
			lfn_filefind_handle=LFN_FILEFIND_NONE;
            if (DOS_FindFirst(name1,reg_cx)) {
                CALLBACK_SCF(false);
                reg_ax=0;           /* Undocumented */
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;       
        case 0x4f:                  /* FINDNEXT Find next matching file */
            if (DOS_FindNext()) {
                CALLBACK_SCF(false);
                /* reg_ax=0xffff;*/         /* Undocumented */
                reg_ax=0;               /* Undocumented:Qbix Willy beamish */
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;      
        case 0x50:                  /* Set current PSP */
            dos.psp(reg_bx);
            break;
        case 0x51:                  /* Get current PSP */
            reg_bx=dos.psp();
            break;
        case 0x52: {                /* Get list of lists */
            uint8_t count=2; // floppy drives always counted
            while (count<DOS_DRIVES && Drives[count] && !Drives[count]->isRemovable()) count++;
            dos_infoblock.SetBlockDevices(count);
            RealPt addr=dos_infoblock.GetPointer();
            SegSet16(es,RealSeg(addr));
            reg_bx=RealOff(addr);
            LOG(LOG_DOSMISC,LOG_NORMAL)("Call is made for list of lists - let's hope for the best");
            break; }
            //TODO Think hard how shit this is gonna be
            //And will any game ever use this :)
        case 0x53:                  /* Translate BIOS parameter block to drive parameter block */
            E_Exit("Unhandled Dos 21 call %02X",reg_ah);
            break;
        case 0x54:                  /* Get verify flag */
            reg_al=dos.verify?1:0;
            break;
        case 0x55:                  /* Create Child PSP*/
            DOS_ChildPSP(reg_dx,reg_si);
            dos.psp(reg_dx);
            reg_al=0xf0;    /* al destroyed */
            break;
        case 0x56:                  /* RENAME Rename file */
			force_sfn = true;
            MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
            MEM_StrCopy(SegPhys(es)+reg_di,name2,DOSNAMEBUF);
            if (DOS_Rename(name1,name2)) {
                CALLBACK_SCF(false);            
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
			force_sfn = false;
            break;      
        case 0x57:                  /* Get/Set File's Date and Time */
            if (reg_al == 0x00) {
                if (DOS_GetFileDate(reg_bx, &reg_cx, &reg_dx)) {
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
            } else if (reg_al == 0x01) {
                if (DOS_SetFileDate(reg_bx,reg_cx,reg_dx)) {
                    CALLBACK_SCF(false);
                } else {
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                }
            } else {
                LOG(LOG_DOSMISC,LOG_ERROR)("DOS:57:Unsupported subfunction %X",reg_al);
            }
            break;
        case 0x58:                  /* Get/Set Memory allocation strategy */
            switch (reg_al) {
                case 0:                 /* Get Strategy */
                    reg_ax=DOS_GetMemAllocStrategy();
                    CALLBACK_SCF(false);
                    break;
                case 1:                 /* Set Strategy */
                    if (DOS_SetMemAllocStrategy(reg_bx)) CALLBACK_SCF(false);
                    else {
                        reg_ax=1;
                        CALLBACK_SCF(true);
                    }
                    break;
                case 2:                 /* Get UMB Link Status */
                    reg_al=dos_infoblock.GetUMBChainState()&1;
                    CALLBACK_SCF(false);
                    break;
                case 3:                 /* Set UMB Link Status */
                    if (DOS_LinkUMBsToMemChain(reg_bx)) CALLBACK_SCF(false);
                    else {
                        reg_ax=1;
                        CALLBACK_SCF(true);
                    }
                    break;
                default:
                    LOG(LOG_DOSMISC,LOG_ERROR)("DOS:58:Not Supported Set//Get memory allocation call %X",reg_al);
                    reg_ax=1;
                    CALLBACK_SCF(true);
            }
            break;
        case 0x59:                  /* Get Extended error information */
            reg_ax=dos.errorcode;
            if (dos.errorcode==DOSERR_FILE_NOT_FOUND || dos.errorcode==DOSERR_PATH_NOT_FOUND) {
                reg_bh=8;   //Not Found error class (Road Hog)
            } else {
                reg_bh=0;   //Unspecified error class
            }
            reg_bl=1;   //Retry retry retry
            reg_ch=0;   //Unknown error locus
            CALLBACK_SCF(false); //undocumented
            break;
        case 0x5a:                  /* Create temporary file */
            {
                uint16_t handle;
                MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
                if (DOS_CreateTempFile(name1,&handle)) {
                    reg_ax=handle;
                    MEM_BlockWrite(SegPhys(ds)+reg_dx,name1,(Bitu)(strlen(name1)+1));
                    CALLBACK_SCF(false);
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
            }
            break;
        case 0x5b:                  /* Create new file */
            {
                MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
                uint16_t handle;
                if (DOS_OpenFile(name1,0,&handle)) {
                    DOS_CloseFile(handle);
                    DOS_SetError(DOSERR_FILE_ALREADY_EXISTS);
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                    break;
                }
                if (DOS_CreateFile(name1,reg_cx,&handle)) {
                    reg_ax=handle;
                    CALLBACK_SCF(false);
                } else {
                    reg_ax=dos.errorcode;
                    CALLBACK_SCF(true);
                }
                break;
            }
        case 0x5c:  {       /* FLOCK File region locking */
            /* ert, 20100711: Locking extensions */
            uint32_t pos=((unsigned int)reg_cx << 16u) + reg_dx;
            uint32_t size=((unsigned int)reg_si << 16u) + reg_di;
            //LOG_MSG("LockFile: BX=%d, AL=%d, POS=%d, size=%d", reg_bx, reg_al, pos, size);
            if (!enable_share_exe) {
               DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
               reg_ax = dos.errorcode;
               CALLBACK_SCF(true);
            } else if (DOS_LockFile(reg_bx,reg_al,pos, size)) {
                reg_ax=0;
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
            }
        case 0x5d:                  /* Network Functions */
            if(reg_al == 0x06) {
                /* FIXME: I'm still not certain, @emendelson, why this matters so much
                 *        to WordPerfect 5.1 and 6.2 and why it causes problems otherwise.
                 *        DOSBox and DOSBox-X only use the first 0x1A bytes anyway. */
                SegSet16(ds,DOS_SDA_SEG);
                reg_si = DOS_SDA_OFS;
                reg_cx = DOS_SDA_SEG_SIZE;  // swap if in dos
                reg_dx = 0x1a;  // swap always (NTS: Size of DOS SDA structure in dos_inc)
                CALLBACK_SCF(false);
                LOG(LOG_DOSMISC,LOG_NORMAL)("Get SDA, Let's hope for the best!");
            } else {
                LOG(LOG_DOSMISC,LOG_ERROR)("DOS:5D:Unsupported subfunction %X",reg_al);
            }
            break;
        case 0x5e:                  /* Network and printer functions */
        {
            bool net = false;
            if (!reg_al && !control->SecureMode()) {
                if (enable_network_redirector)
                    net = true;
                else {
                    reg_ax = 0x1100;
                    CALLBACK_RunRealInt(0x2f);
                    net = reg_al == 0xFF;
                    reg_ax = 0x5e00;
                }
            }
            if (net) {	// Get machine name
#if defined(WIN32)
                DWORD size = DOSNAMEBUF;
                GetComputerName(name1, &size);
                if (size)
#else
                int result = gethostname(name1, DOSNAMEBUF);
                if (!result)
#endif
                {
                    strcat(name1, "               ");									// Simply add 15 spaces
                    if (!strcmp(RunningProgram, "4DOS") || (reg_ip == 0xeb31 && (reg_sp == 0xc25e || reg_sp == 0xc26e))) {	// 4DOS expects it to be 0 terminated (not documented)
                        name1[16] = 0;
                        MEM_BlockWrite(SegPhys(ds)+reg_dx, name1, 17);
                    } else {
                        name1[15] = 0;													// ASCIIZ
                        MEM_BlockWrite(SegPhys(ds)+reg_dx, name1, 16);
                    }
                    reg_cx = 0x1ff;														// 01h name valid, FFh NetBIOS number for machine name
                    CALLBACK_SCF(false);
                    break;
                }
            }
            reg_al = 1;
            CALLBACK_SCF(true);
            break;
        }
        case 0x5f:                  /* Network redirection */
#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
            switch(reg_al)
            {
                case    0x34:   //Set pipe state
                    if(Network_SetNamedPipeState(reg_bx,reg_cx,reg_ax))
                        CALLBACK_SCF(false);
                    else    CALLBACK_SCF(true);
                    break;
                case    0x35:   //Peek pipe
                    {
                        uint16_t  uTmpSI=reg_si;
                        if(Network_PeekNamedPipe(reg_bx,
                                    dos_copybuf,reg_cx,
                                    reg_cx,reg_si,reg_dx,
                                    reg_di,reg_ax))
                        {
                            MEM_BlockWrite(SegPhys(ds)+uTmpSI,dos_copybuf,reg_cx);
                            CALLBACK_SCF(false);
                        }
                        else    CALLBACK_SCF(true);
                    }
                    break;
                case    0x36:   //Transcate pipe
                    //Inbuffer:the buffer to be written to pipe
                    MEM_BlockRead(SegPhys(ds)+reg_si,dos_copybuf_second,reg_cx);

                    if(Network_TranscateNamedPipe(reg_bx,
                                dos_copybuf_second,reg_cx,
                                dos_copybuf,reg_dx,
                                reg_cx,reg_ax))
                    {
                        //Outbuffer:the buffer to receive data from pipe
                        MEM_BlockWrite(SegPhys(es)+reg_di,dos_copybuf,reg_cx);
                        CALLBACK_SCF(false);
                    }
                    else    CALLBACK_SCF(true);
                    break;
                default:
                    reg_ax=0x0001;          //Failing it
                    CALLBACK_SCF(true);
                    break;
            }
#else
            reg_ax=0x0001;          //Failing it
            CALLBACK_SCF(true);
#endif
            break; 
        case 0x60:                  /* Canonicalize filename or path */
            MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
            if (DOS_Canonicalize(name1,name2)) {
                MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));   
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x61:                  /* Unused (reserved for network use) */
            goto default_fallthrough;
        case 0x62:                  /* Get Current PSP Address */
            reg_bx=dos.psp();
            break;
        case 0x63:                  /* DOUBLE BYTE CHARACTER SET */
            if(reg_al == 0 && dos.tables.dbcs != 0) {
                SegSet16(ds,RealSeg(dos.tables.dbcs));
                reg_si=RealOff(dos.tables.dbcs) + 2;        
                reg_al = 0;
                CALLBACK_SCF(false); //undocumented
            } else reg_al = 0xff; //Doesn't officially touch carry flag
            break;
        case 0x64:                  /* Set device driver lookahead flag */
            LOG(LOG_DOSMISC,LOG_NORMAL)("set driver look ahead flag");
            break;
        case 0x65:                  /* Get extended country information and a lot of other useless shit*/
            { /* Todo maybe fully support this for now we set it standard for USA */ 
                LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:65:Extended country information call %X",reg_ax);
                if((reg_al <=  0x07) && (reg_cx < 0x05)) {
                    DOS_SetError(DOSERR_FUNCTION_NUMBER_INVALID);
                    CALLBACK_SCF(true);
                    break;
                }
                Bitu len = 0; /* For 0x21 and 0x22 */
                PhysPt data=SegPhys(es)+reg_di;
                switch (reg_al) {
                    case 0x01:
                        mem_writeb(data + 0x00,reg_al);
                        mem_writew(data + 0x01,0x26);
						if (!countryNo) {
                            if (IS_PC98_ARCH)
                                countryNo = 81;
#if defined(WIN32)
							else
                            {
                                char buffer[128];
                                if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_ICOUNTRY, buffer, 128)) {
								countryNo = uint16_t(atoi(buffer));
								DOS_SetCountry(countryNo);
                                }
							}
#endif
							if (!countryNo) {
                                const char *layout = DOS_GetLoadedLayout();
                                if (layout == NULL)
                                    countryNo = COUNTRYNO::United_States;
                                else if (country_code_map.find(layout) != country_code_map.end())
                                    countryNo = country_code_map.find(layout)->second;
                                else
                                    countryNo = COUNTRYNO::United_States;
                                DOS_SetCountry(countryNo);
							}
						}
						mem_writew(data + 0x03, countryNo);
                        if(reg_cx > 0x06 ) mem_writew(data+0x05,dos.loaded_codepage);
                        if(reg_cx > 0x08 ) {
                            Bitu amount = (reg_cx>=0x29u)?0x22u:(reg_cx-7u);
                            MEM_BlockWrite(data + 0x07,dos.tables.country,amount);
                            reg_cx=(reg_cx>=0x29)?0x29:reg_cx;
                        }
                        CALLBACK_SCF(false);
                        break;
                    case 0x05: // Get pointer to filename terminator table
                        mem_writeb(data + 0x00, reg_al);
                        mem_writed(data + 0x01, dos.tables.filenamechar);
                        reg_cx = 5;
                        CALLBACK_SCF(false);
                        break;
                    case 0x02: // Get pointer to uppercase table
                    case 0x04: // Get pointer to filename uppercase table
                        mem_writeb(data + 0x00, reg_al);
                        mem_writed(data + 0x01, dos.tables.upcase);
                        reg_cx = 5;
                        CALLBACK_SCF(false);
                        break;
                    case 0x06: // Get pointer to collating sequence table
                        mem_writeb(data + 0x00, reg_al);
                        mem_writed(data + 0x01, dos.tables.collatingseq);
                        reg_cx = 5;
                        CALLBACK_SCF(false);
                        break;
                    case 0x03: // Get pointer to lowercase table
                    case 0x07: // Get pointer to double byte char set table
                        if (dos.tables.dbcs != 0) {
                            mem_writeb(data + 0x00, reg_al);
                            mem_writed(data + 0x01, dos.tables.dbcs); //used to be 0
                            reg_cx = 5;
                            CALLBACK_SCF(false);
                        }
                        break;
                    case 0x20: /* Capitalize Character */
                        {
                            int in  = reg_dl;
                            int out = toupper(in);
                            reg_dl  = (uint8_t)out;
                        }
                        CALLBACK_SCF(false);
                        break;
                    case 0x21: /* Capitalize String (cx=length) */
                    case 0x22: /* Capatilize ASCIZ string */
                        data = SegPhys(ds) + reg_dx;
                        if(reg_al == 0x21) len = reg_cx; 
                        else len = mem_strlen(data); /* Is limited to 1024 */

                        if(len > DOS_COPYBUFSIZE - 1) E_Exit("DOS:0x65 Buffer overflow");
                        else if(len) {
                            MEM_BlockRead(data,dos_copybuf,len);
                            dos_copybuf[len] = 0;
                            //No upcase as String(0x21) might be multiple asciz strings
                            for (Bitu count = 0; count < len;count++)
                                dos_copybuf[count] = (uint8_t)toupper(*reinterpret_cast<unsigned char*>(dos_copybuf+count));
                            MEM_BlockWrite(data,dos_copybuf,len);
                        }
                        CALLBACK_SCF(false);
                        break;
                    case 0x23: /* Determine if character represents yes/no response (MS-DOS 4.0+) */
                        /* DL = character
                         * DH = second char of double-byte char if DBCS */
                        /* response: CF=1 if error (what error?) or CF=0 and AX=response
                         *
                         * response values 0=no 1=yes 2=neither */
                        /* FORMAT.COM and FDISK.EXE rely on this call after prompting the user */
                        {
                            unsigned int c;

                            if (IS_PC98_ARCH)
                                c = reg_dx; // DBCS
                            else
                                c = reg_dl; // SBCS

                            if (tolower(c) == MSG_Get("INT21_6523_YESNO_CHARS")[0])
                                reg_ax = 1;/*yes*/
                            else if (tolower(c) == MSG_Get("INT21_6523_YESNO_CHARS")[1])
                                reg_ax = 0;/*no*/
                            else
                                reg_ax = 2;/*neither*/
                        }
                        CALLBACK_SCF(false);
                        break;
                    default:
                        E_Exit("DOS:0x65:Unhandled country information call %2X",reg_al);   
                }
                break;
            }
        case 0x66:                  /* Get/Set global code page table  */
            switch (reg_al)
            {
                case 1:
                    LOG(LOG_DOSMISC,LOG_NORMAL)("Getting global code page table");
                    reg_bx=reg_dx=dos.loaded_codepage;
                    CALLBACK_SCF(false);
                    break;
                case 2:
                {
#if defined(USE_TTF)
                    if (!ttf.inUse)
#endif
                    {
                        LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Setting code page table is not supported for non-TrueType font output");
                        CALLBACK_SCF(true);
                        dos.errorcode = 2;
                        reg_ax = dos.errorcode;
                        break;
                    }
                    if (!isSupportedCP(reg_bx) || IS_PC98_ARCH || IS_JEGA_ARCH || IS_DOSV)
                    {
                        LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Invalid codepage %d", reg_bx);
                        CALLBACK_SCF(true);
                        dos.errorcode = 2;
                        reg_ax = dos.errorcode;
                        break;
                    }
                    int cpbak = dos.loaded_codepage;
                    dos.loaded_codepage = reg_bx;
#if defined(USE_TTF)
                    setTTFCodePage();
#endif
                    if (loadlang) {
                        MSG_Init();
#if DOSBOXMENU_TYPE == DOSBOXMENU_HMENU
                        mainMenu.unbuild();
                        mainMenu.rebuild();
                        if (!GFX_GetPreventFullscreen()) {
                            if (menu.toggle) DOSBox_SetMenu(); else DOSBox_NoMenu();
                        }
#endif
                        DOSBox_SetSysMenu();
                    }
                    if (isDBCSCP()) {
                        ShutFontHandle();
                        InitFontHandle();
                        JFONT_Init();
                        Section_prop * ttf_section = static_cast<Section_prop *>(control->GetSection("ttf"));
                        const char *font = ttf_section->Get_string("font");
                        if (!font || !*font) {
                            ttf_reset();
#if C_PRINTER
                            if (printfont) UpdateDefaultPrinterFont();
#endif
                        }
                    }
                    SetupDBCSTable();
                    runRescan("-A -Q");
                    incall = true;
                    SwitchLanguage(cpbak, dos.loaded_codepage, false);
                    incall = false;
                    CALLBACK_SCF(false);
                    break;
                }
                default:
                    dos.errorcode = 1;
                    reg_ax = dos.errorcode;
                    CALLBACK_SCF(true);
                    LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Invalid code page subfunction requested");
                    break;
            }
            break;
        case 0x67:                  /* Set handle count */
            /* Weird call to increase amount of file handles needs to allocate memory if >20 */
            {
                DOS_PSP psp(dos.psp());
                psp.SetNumFiles(reg_bx);
                CALLBACK_SCF(false);
                break;
            }
        case 0x68:                  /* FFLUSH Commit file */
            case_0x68_fallthrough:
            if(DOS_FlushFile(reg_bl)) {
                reg_ah = 0x68;
                CALLBACK_SCF(false);
            } else {
                reg_ax = dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x69:                  /* Get/Set disk serial number */
            {
                uint16_t old_cx=reg_cx;
                switch(reg_al)      {
                    case 0x00:              /* Get */
                        LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:Get Disk serial number");
                        reg_cl=0x66;// IOCTL function
                        break;
                    case 0x01:              /* Set */
                        LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:Set Disk serial number");
                        reg_cl=0x46;// IOCTL function
                        break;
                    default:
                        E_Exit("DOS:Illegal Get Serial Number call %2X",reg_al);
                }
                reg_ch=0x08;    // IOCTL category: disk drive
                reg_ax=0x440d;  // Generic block device request
                DOS_21Handler();
                reg_cx=old_cx;
                break;
            }
        case 0x6a:                  /* Commit file */
            // Note: Identical to AH=68h in DOS 5.0-6.0; not known whether this is the case in DOS 4.x
            goto case_0x68_fallthrough;
        case 0x6b:                  /* NULL Function */
            goto default_fallthrough;
        case 0x6c:                  /* Extended Open/Create */
            MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
            if (DOS_OpenFileExtended(name1,reg_bx,reg_cx,reg_dx,&reg_ax,&reg_cx)) {
                CALLBACK_SCF(false);
            } else {
                reg_ax=dos.errorcode;
                CALLBACK_SCF(true);
            }
            break;
        case 0x6d:                  /* ROM - Find first ROM program */
            LOG(LOG_DOSMISC, LOG_ERROR)("DOS:ROM - Find first ROM program not implemented");
            goto default_fallthrough;
        case 0x6e:                  /* ROM - Find next ROM program */
            LOG(LOG_DOSMISC, LOG_ERROR)("DOS:ROM - Find next ROM program not implemented");
            goto default_fallthrough;
        case 0x6f:                  /* ROM functions */
            LOG(LOG_DOSMISC, LOG_ERROR)("DOS:6F ROM functions not implemented");
            goto default_fallthrough;
        case 0x71:                  /* Unknown probably 4dos detection */
            LOG(LOG_DOSMISC,LOG_NORMAL)("DOS:MS-DOS 7+ long file name support call %2X",reg_al);
            if (!uselfn) {
                    reg_ax=0x7100;
                    CALLBACK_SCF(true); //Check this! What needs this ? See default case
                    break;
            }
            switch(reg_al)          {
                    case 0x39:              /* LFN MKDIR */
							DOS_Int21_7139(name1, name2);
                            break;
                    case 0x3a:              /* LFN RMDIR */
							DOS_Int21_713a(name1, name2);
                            break;
                    case 0x3b:              /* LFN CHDIR */
							DOS_Int21_713b(name1, name2);
                            break;
                    case 0x41:              /* LFN UNLINK */
							DOS_Int21_7141(name1, name2);
                            break;
                    case 0x43:              /* LFN ATTR */
							DOS_Int21_7143(name1, name2);
                            break;
                    case 0x47:              /* LFN PWD */
							DOS_Int21_7147(name1, name2);
                            break;
                    case 0x4e:              /* LFN FindFirst */
							DOS_Int21_714e(name1, name2);
                            break;           
                    case 0x4f:              /* LFN FindNext */
							DOS_Int21_714f(name1, name2);
                            break;
                    case 0x56:              /* LFN Rename */
							DOS_Int21_7156(name1, name2);
                            break;         
                    case 0x60:              /* LFN GetName */
							DOS_Int21_7160(name1, name2);
                            break;
                    case 0x6c:              /* LFN Create */
							DOS_Int21_716c(name1, name2);
                            break;
                    case 0xa0:              /* LFN VolInfo */
							DOS_Int21_71a0(name1, name2);
                            break;
                    case 0xa1:              /* LFN FileClose */
							DOS_Int21_71a1(name1, name2);
							break;
                    case 0xa6:              /* LFN GetFileInfoByHandle */
							DOS_Int21_71a6(name1, name2);
							break;
                    case 0xa7:              /* LFN TimeConv */
							DOS_Int21_71a7(name1, name2);
                            break;
                    case 0xa8:              /* LFN GenSFN */
							DOS_Int21_71a8(name1, name2);
                            break;
					case 0xaa:              /* LFN Subst */
							DOS_Int21_71aa(name1, name2);
							break;
					case 0xa9:              /* LFN Server Create */
							reg_ax=0x7100; // unimplemented (not very useful)
                    default:
                            reg_ax=0x7100;
                            CALLBACK_SCF(true); //Check this! What needs this ? See default case
            }
            break;
		case 0x73:
			if (dos.version.major < 7) { // MS-DOS 7+ only for AX=73xxh
				CALLBACK_SCF(true);
				reg_ax=0x7300;
			} else if (reg_al==0 && reg_cl<2) {
				/* Drive locking and flushing */
				reg_al = reg_cl;
				reg_ah = 0;
				CALLBACK_SCF(false);
			} else if (reg_al==2) {
				/* Get extended DPB */
				uint32_t ptr = SegPhys(es)+reg_di;
				uint8_t drive;

				/* AX=7302h
				 * DL=drive
				 * ES:DI=buffer to return data into
				 * CX=length of buffer (Windows 9x uses 0x3F)
				 * SI=??? */

				if (reg_dl != 0) /* 1=A: 2=B: ... */
					drive = reg_dl - 1;
				else /* 0=default */
					drive = DOS_GetDefaultDrive();

				if (drive < DOS_DRIVES && Drives[drive] && !Drives[drive]->isRemovable() && reg_cx >= 0x3F) {
					fatDrive *fdp;
					FAT_BootSector::bpb_union_t bpb;
					if (!strncmp(Drives[drive]->GetInfo(),"fatDrive ",9)) {
						fdp = dynamic_cast<fatDrive*>(Drives[drive]);
						if (fdp != NULL) {
							bpb=fdp->GetBPB();
							if (bpb.is_fat32()) {
								unsigned char tmp[24];

								mem_writew(ptr+0x00,0x3D);                                  // length of data (Windows 98)
								/* first 24 bytes after len is DPB */
								{
									const uint32_t srcptr = (dos.tables.dpb << 4) + (drive*dos.tables.dpb_size);
									MEM_BlockRead(srcptr,tmp,24);
									MEM_BlockWrite(ptr+0x02,tmp,24);
								}
								uint32_t bytes_per_sector,sectors_per_cluster,total_clusters,free_clusters,tfree;
								rsize=true;
								totalc=freec=0;
								if (DOS_GetFreeDiskSpace32(reg_dl,&bytes_per_sector,&sectors_per_cluster,&total_clusters,&free_clusters))
									tfree = freec?freec:free_clusters;
								else
									tfree=0xFFFFFFFF;
								rsize=false;
								mem_writeb(ptr+0x1A,0x00);      // dpb flags
								mem_writed(ptr+0x1B,0xFFFFFFFF);// ptr to next DPB if Windows 95 magic SI signature (TODO)
								mem_writew(ptr+0x1F,2);         // cluster to start searching when writing (FIXME)
								mem_writed(ptr+0x21,tfree);// number of free clusters
								mem_writew(ptr+0x25,bpb.v32.BPB_ExtFlags);
								mem_writew(ptr+0x27,bpb.v32.BPB_FSInfo);
								mem_writew(ptr+0x29,bpb.v32.BPB_BkBootSec);
								mem_writed(ptr+0x2B,fdp->GetFirstClusterOffset()); /* apparently cluster offset relative to the disk not volume */
								mem_writed(ptr+0x2F,fdp->GetHighestClusterNumber());
								mem_writed(ptr+0x33,bpb.v32.BPB_FATSz32);
								mem_writed(ptr+0x37,bpb.v32.BPB_RootClus);
								mem_writed(ptr+0x3B,2);         // cluster to start searching when writing (FIXME)

								CALLBACK_SCF(false);
								break;
							}
						}
					}

					reg_ax=0x18;//FIXME
					CALLBACK_SCF(true);
				} else {
					reg_ax=0x18;//FIXME
					CALLBACK_SCF(true);
				}
			} else if (reg_al==3) {
				/* Get extended free disk space */
				MEM_StrCopy(SegPhys(ds)+reg_dx,name1,reg_cx);
				if (name1[1]==':'&&name1[2]=='\\')
					reg_dl=name1[0]-'A'+1;
				else {
					reg_ax=0xffff;
					CALLBACK_SCF(true);
					break;
				}
				uint32_t bytes_per_sector,sectors_per_cluster,total_clusters,free_clusters;
				rsize=true;
				totalc=freec=0;
				if (DOS_GetFreeDiskSpace32(reg_dl,&bytes_per_sector,&sectors_per_cluster,&total_clusters,&free_clusters))
				{
					ext_space_info_t *info = new ext_space_info_t;
					info->size_of_structure = sizeof(ext_space_info_t);
					info->structure_version = 0;
					info->sectors_per_cluster = sectors_per_cluster;
					info->bytes_per_sector = bytes_per_sector;
					info->available_clusters_on_drive = freec?freec:free_clusters;
					info->total_clusters_on_drive = totalc?totalc:total_clusters;
					info->available_sectors_on_drive = sectors_per_cluster * (freec?freec:free_clusters);
					info->total_sectors_on_drive = sectors_per_cluster * (totalc?totalc:total_clusters);
					info->available_allocation_units = freec?freec:free_clusters;
					info->total_allocation_units = totalc?totalc:total_clusters;
					MEM_BlockWrite(SegPhys(es)+reg_di,info,sizeof(ext_space_info_t));
					delete info;
					reg_ax=0;
					CALLBACK_SCF(false);
				}
				else
				{
					reg_ax=dos.errorcode;
					CALLBACK_SCF(true);
				}
				rsize=false;
			} else if (reg_al == 5 && reg_cx == 0xFFFF && (dos.version.major > 7 || dos.version.minor >= 10)) {
				/* MS-DOS 7.1+ (Windows 95 OSR2+) FAT32 extended disk read/write */
				reg_al = reg_dl - 1; /* INT 25h AL 0=A: 1=B:   This interface DL 1=A: 2=B: */
				if (reg_si & 1)
					DOS_26Handler_Actual(true/*fat32*/); /* writing */
				else
					DOS_25Handler_Actual(true/*fat32*/); /* reading */

				/* CF needs to be returned on stack or else it's lost */
				CALLBACK_SCF(!!(reg_flags & FLAG_CF));
			} else {
				LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Unhandled call %02X al=%02X (MS-DOS 7.x function)",reg_ah,reg_al);
				CALLBACK_SCF(true);
				reg_ax=0xffff;//FIXME
			}
			break;
		case 0xE0:
        case 0xEF:                  /* Used in Ancient Art Of War CGA */
        default:
            default_fallthrough:
            if (reg_ah < 0x6b) LOG(LOG_DOSMISC,LOG_ERROR)("DOS:Unhandled call %02X al=%02X. Set al to default of 0",reg_ah,reg_al); //Less errors. above 0x6c the functions are simply always skipped, only al is zeroed, all other registers untouched
            reg_al=0x00; /* default value */
            break;
    }

    /* if INT 21h involves any BIOS calls that need the timer, emulate the fact that the
     * BIOS might unmask IRQ 0 as part of the job (especially INT 13h disk I/O).
     *
     * Some DOS games & demos mask interrupts at the PIC level in a stingy manner that
     * apparently assumes DOS/BIOS will unmask some when called.
     *
     * Examples:
     *   Rebel by Arkham (without this fix, timer interrupt will not fire during demo and therefore music will not play). */
    if (unmask_irq0)
        PIC_SetIRQMask(0,false); /* Enable system timer */

    return CBRET_NONE;
}

static Bitu BIOS_1BHandler(void) {
    mem_writeb(BIOS_CTRL_BREAK_FLAG,0x00);

    // MS-DOS installs an INT 1Bh handler that sets the "break" flag and then returns immediately.
    // MS-DOS 6.22's interrupt handler is literally two instructions:
    //      MOV BYTE PTR CS:[000Ch],03h
    //      IRET

    /* take note (set flag) and return */
    /* FIXME: Don't forget that on "BOOT" this handler should be unassigned, though having it assigned
     *        to the guest OS causes no harm. */
    DOS_BreakFlag = true;
    DOS_BreakConioFlag = true;
    return CBRET_NONE;
}

static Bitu DOS_20Handler(void) {
	reg_ah=0x00;
	DOS_21Handler();
	return CBRET_NONE;
}

static Bitu DOS_CPMHandler(void) {
	// Convert a CPM-style call to a normal DOS call
	uint16_t flags=CPU_Pop16();
	CPU_Pop16();
	uint16_t caller_seg=CPU_Pop16();
	uint16_t caller_off=CPU_Pop16();
	CPU_Push16(flags);
	CPU_Push16(caller_seg);
	CPU_Push16(caller_off);
	if (reg_cl>0x24) {
		reg_al=0;
		return CBRET_NONE;
	}
	reg_ah=reg_cl;
	return DOS_21Handler();
}

static Bitu DOS_27Handler(void) {
	// Terminate & stay resident
	uint16_t para = (reg_dx/16)+((reg_dx % 16)>0);
	uint16_t psp = dos.psp(); //mem_readw(SegPhys(ss)+reg_sp+2);
	if (DOS_ResizeMemory(psp,&para)) {
		DOS_Terminate(psp,true,0);
		if (DOS_BreakINT23InProgress) throw int(0); /* HACK: Ick */
	}
	return CBRET_NONE;
}

static uint16_t DOS_SectorAccess(bool read) {
	fatDrive * drive = (fatDrive *)Drives[reg_al];
	uint16_t bufferSeg = SegValue(ds);
	uint16_t bufferOff = reg_bx;
	uint16_t sectorCnt = reg_cx;
	uint32_t sectorNum = (uint32_t)reg_dx + drive->partSectOff;
	uint32_t sectorEnd = drive->getSectorCount() + drive->partSectOff;
	uint8_t sectorBuf[512];
	Bitu i;

	if (sectorCnt == 0xffff) { // large partition form
		bufferSeg = real_readw(SegValue(ds),reg_bx + 8);
		bufferOff = real_readw(SegValue(ds),reg_bx + 6);
		sectorCnt = real_readw(SegValue(ds),reg_bx + 4);
		sectorNum = real_readd(SegValue(ds),reg_bx + 0) + drive->partSectOff;
	} else if (sectorEnd > 0xffff) return 0x0207; // must use large partition form

	while (sectorCnt--) {
		if (sectorNum >= sectorEnd) return 0x0408; // sector not found
		if (read) {
			if (drive->readSector(sectorNum++,&sectorBuf)) return 0x0408;
			for (i=0;i<512;i++) real_writeb(bufferSeg,bufferOff++,sectorBuf[i]);
		} else {
			for (i=0;i<512;i++) sectorBuf[i] = real_readb(bufferSeg,bufferOff++);
			if (drive->writeSector(sectorNum++,&sectorBuf)) return 0x0408;
		}
	}
	return 0;
}

static Bitu DOS_25Handler_Actual(bool fat32) {
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->isRemovable()) {
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else if (strncmp(Drives[reg_al]->GetInfo(),"fatDrive",8) == 0) {
		reg_ax = DOS_SectorAccess(true);
		SETFLAGBIT(CF,reg_ax != 0);
	} else {
		DOS_Drive *drv = Drives[reg_al];
		/* assume drv != NULL */
		uint32_t sector_size = drv->GetSectorSize();
		uint32_t sector_count = drv->GetSectorCount();
		PhysPt ptr = PhysMake(SegValue(ds),reg_bx);
		uint32_t req_count = reg_cx;
		uint32_t sector_num = reg_dx;

		/* For < 32MB drives.
		 *  AL = drive
		 *  CX = sector count (not 0xFFFF)
		 *  DX = sector number
		 *  DS:BX = pointer to disk transfer area
		 *
		 * For >= 32MB drives.
		 *
		 *  AL = drive
		 *  CX = 0xFFFF
		 *  DS:BX = disk read packet
		 *
		 *  Disk read packet:
		 *    +0 DWORD = sector number
		 *    +4 WORD = sector count
		 *    +6 DWORD = disk transfer area
		 */
		if (sector_count != 0 && sector_size != 0) {
			unsigned char tmp[2048];
			const char *method;

			if (sector_size > sizeof(tmp)) {
				reg_ax = 0x8002;
				SETFLAGBIT(CF,true);
				return CBRET_NONE;
			}

			if (sector_count > 0xFFFF && req_count != 0xFFFF) {
				reg_ax = 0x0207; // must use CX=0xFFFF API for > 64KB segment partitions
				SETFLAGBIT(CF,true);
				return CBRET_NONE;
			}

			if (fat32) {
				sector_num = mem_readd(ptr+0);
				req_count = mem_readw(ptr+4);
				uint32_t p = mem_readd(ptr+6);
				ptr = PhysMake(p >> 16u,p & 0xFFFFu);
				method = "Win95/FAT32";
			}
			else if (req_count == 0xFFFF) {
				sector_num = mem_readd(ptr+0);
				req_count = mem_readw(ptr+4);
				uint32_t p = mem_readd(ptr+6);
				ptr = PhysMake(p >> 16u,p & 0xFFFFu);
				method = ">=32MB";
			}
			else {
				method = "<32MB";
			}

			if (fat32) {
				LOG(LOG_DOSMISC,LOG_DEBUG)("INT 21h AX=7305h READ: sector=%lu count=%lu ptr=%lx method='%s'",
						(unsigned long)sector_num,
						(unsigned long)req_count,
						(unsigned long)ptr,
						method);
			}
			else {
				LOG(LOG_DOSMISC,LOG_DEBUG)("INT 25h READ: sector=%lu count=%lu ptr=%lx method='%s'",
						(unsigned long)sector_num,
						(unsigned long)req_count,
						(unsigned long)ptr,
						method);
			}

			SETFLAGBIT(CF,false);
			reg_ax = 0;

			while (req_count > 0) {
				uint8_t res = drv->Read_AbsoluteSector_INT25(sector_num,tmp);
				if (res != 0) {
					reg_ax = 0x8002;
					SETFLAGBIT(CF,true);
					break;
				}

				for (unsigned int i=0;i < (unsigned int)sector_size;i++)
					mem_writeb(ptr+i,tmp[i]);

				req_count--;
				sector_num++;
				ptr += sector_size;
			}

			return CBRET_NONE;
		}

		/* MicroProse installer hack, inherited from DOSBox SVN, as a fallback if INT 25h emulation is not available for the drive. */
		if (reg_cx == 1 && reg_dx == 0 && reg_al >= 2) {
			// write some BPB data into buffer for MicroProse installers
			real_writew(SegValue(ds),reg_bx+0x1c,0x3f); // hidden sectors
			SETFLAGBIT(CF,false);
			reg_ax = 0;
		} else {
			LOG(LOG_DOSMISC,LOG_NORMAL)("int 25 called but not as disk detection drive %u",reg_al);
			reg_ax = 0x8002;
			SETFLAGBIT(CF,true);
		}
	}
	return CBRET_NONE;
}

static Bitu DOS_25Handler(void) {
	return DOS_25Handler_Actual(false);
}

static Bitu DOS_26Handler_Actual(bool fat32) {
	if (reg_al >= DOS_DRIVES || !Drives[reg_al] || Drives[reg_al]->isRemovable()) {	
		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	} else if (strncmp(Drives[reg_al]->GetInfo(),"fatDrive",8) == 0) {
		reg_ax = DOS_SectorAccess(false);
		SETFLAGBIT(CF,reg_ax != 0);
	} else {
		DOS_Drive *drv = Drives[reg_al];
		/* assume drv != NULL */
		uint32_t sector_size = drv->GetSectorSize();
		uint32_t sector_count = drv->GetSectorCount();
		PhysPt ptr = PhysMake(SegValue(ds),reg_bx);
		uint32_t req_count = reg_cx;
		uint32_t sector_num = reg_dx;

		/* For < 32MB drives.
		 *  AL = drive
		 *  CX = sector count (not 0xFFFF)
		 *  DX = sector number
		 *  DS:BX = pointer to disk transfer area
		 *
		 * For >= 32MB drives.
		 *
		 *  AL = drive
		 *  CX = 0xFFFF
		 *  DS:BX = disk read packet
		 *
		 *  Disk read packet:
		 *    +0 DWORD = sector number
		 *    +4 WORD = sector count
		 *    +6 DWORD = disk transfer area
		 */
		if (sector_count != 0 && sector_size != 0) {
			unsigned char tmp[2048];
			const char *method;

			if (sector_size > sizeof(tmp)) {
				reg_ax = 0x8002;
				SETFLAGBIT(CF,true);
				return CBRET_NONE;
			}

			if (sector_count > 0xFFFF && req_count != 0xFFFF) {
				reg_ax = 0x0207; // must use CX=0xFFFF API for > 64KB segment partitions
				SETFLAGBIT(CF,true);
				return CBRET_NONE;
			}

			if (fat32) {
				sector_num = mem_readd(ptr+0);
				req_count = mem_readw(ptr+4);
				uint32_t p = mem_readd(ptr+6);
				ptr = PhysMake(p >> 16u,p & 0xFFFFu);
				method = "Win95/FAT32";
			}
			else if (req_count == 0xFFFF) {
				sector_num = mem_readd(ptr+0);
				req_count = mem_readw(ptr+4);
				uint32_t p = mem_readd(ptr+6);
				ptr = PhysMake(p >> 16u,p & 0xFFFFu);
				method = ">=32MB";
			}
			else {
				method = "<32MB";
			}

			if (fat32) {
				LOG(LOG_DOSMISC,LOG_DEBUG)("INT 21h AX=7305h WRITE: sector=%lu count=%lu ptr=%lx method='%s'",
						(unsigned long)sector_num,
						(unsigned long)req_count,
						(unsigned long)ptr,
						method);
			}
			else {
				LOG(LOG_DOSMISC,LOG_DEBUG)("INT 26h WRITE: sector=%lu count=%lu ptr=%lx method='%s'",
						(unsigned long)sector_num,
						(unsigned long)req_count,
						(unsigned long)ptr,
						method);
			}

			SETFLAGBIT(CF,false);
			reg_ax = 0;

			while (req_count > 0) {
				for (unsigned int i=0;i < (unsigned int)sector_size;i++)
					tmp[i] = mem_readb(ptr+i);

				uint8_t res = drv->Write_AbsoluteSector_INT25(sector_num,tmp);
				if (res != 0) {
					reg_ax = 0x8002;
					SETFLAGBIT(CF,true);
					break;
				}

				req_count--;
				sector_num++;
				ptr += sector_size;
			}

			return CBRET_NONE;
		}

		reg_ax = 0x8002;
		SETFLAGBIT(CF,true);
	}
	return CBRET_NONE;
}

static Bitu DOS_26Handler(void) {
	return DOS_26Handler_Actual(false);
}

bool enable_collating_uppercase = true;
bool keep_private_area_on_boot = false;
bool private_always_from_umb = false;
bool private_segment_in_umb = true;
uint16_t DOS_IHSEG = 0;

// NOTE about 0x70 and PC-98 emulation mode:
//
// I don't know exactly how things differ in NEC's PC-98 MS-DOS, but,
// according to some strange code in Touhou Project that's responsible
// for blanking the text layer, there's a "row count" variable at 0x70:0x12
// that holds (number of rows - 1). Leaving that byte value at zero prevents
// the game from clearing the screen (which also exposes the tile data and
// overdraw of the graphics layer). A value of zero instead just causes the
// first text character row to be filled in, not the whole visible text layer.
//
// Pseudocode of the routine:
//
// XOR AX,AX
// MOV ES,AX
// MOV AL,ES:[0712h]            ; AX = BYTE [0x70:0x12] zero extend (ex. 0x18 == 24)
// INC AX                       ; AX++                              (ex. becomes 0x19 == 25)
// MOV DX,AX
// SHL DX,1
// SHL DX,1                     ; DX *= 4
// ADD DX,AX                    ; DX += AX     equiv. DX = AX * 5
// MOV CL,4h
// SHL DX,CL                    ; DX <<= 4     equiv. DX = AX * 0x50  or  DX = AX * 80
// ...
// MOV AX,0A200h
// MOV ES,AX
// MOV AX,(solid black overlay block attribute)
// MOV CX,DX
// REP STOSW
//
// When the routine is done, the graphics layer is obscured by text character cells that
// represent all black (filled in) so that the game can later "punch out" the regions
// of the graphics layer it wants you to see. TH02 relies on this as well to flash the
// screen and open from the center to show the title screen. During gameplay, the text
// layer is used to obscure sprite overdraw when a sprite is partially off-screen as well
// as hidden tile data on the right hand half of the screen that the game read/write
// copies through the GDC pattern/tile registers to make the background. When the text
// layer is not present it's immediately apparent that the sprite renderer makes no attempt
// to clip sprites within the screen, but instead relies on the text overlay to hide the
// overdraw.
//
// this means that on PC-98 one of two things are true. either:
//  - NEC's variation of MS-DOS loads the base kernel higher up (perhaps at 0x80:0x00?)
//    and the BIOS data area lies from 0x40:00 to 0x7F:00
//
//    or
//
//  - NEC's variation loads at 0x70:0x00 (same as IBM PC MS-DOS) and Touhou Project
//    is dead guilty of reaching directly into MS-DOS kernel memory to read
//    internal variables it shouldn't be reading directly!
//
// Ick...

Bitu MEM_PageMask(void);
void dos_ver_menu(bool start);
void update_dos_ems_menu(void);
void DOS_GetMemory_reset();
void DOS_GetMemory_Choose();
void INT10_SetCursorPos_viaRealInt(uint8_t row, uint8_t col, uint8_t page);
void INT10_WriteChar_viaRealInt(uint8_t chr, uint8_t attr, uint8_t page, uint16_t count, bool showattr);
void INT10_ScrollWindow_viaRealInt(uint8_t rul, uint8_t cul, uint8_t rlr, uint8_t clr, int8_t nlines, uint8_t attr, uint8_t page);

extern bool dos_con_use_int16_to_detect_input;
extern bool dbg_zero_on_dos_allocmem, addovl;

bool set_ver(char *s) {
	s=trim(s);
	int major=isdigit(*s)?strtoul(s,(char**)(&s),10):-1;
	if (major>=0&&major<100) {
		if (*s == '.' || *s == ' ') {
			s++;
			if (isdigit(*s)&&*(s-1)=='.'&&strlen(s)>2) *(s+2)=0;
			int minor=isdigit(*s)?(*(s-1)=='.'&&strlen(s)==1?10:1)*strtoul(s,(char**)(&s),10):-1;
			while (minor>99) minor/=10;
			if (minor>=0&&minor<100&&(major||minor)) {
				dos.version.minor=minor;
				dos.version.major=major;
				return true;
			}
		} else if (!*s&&major) {
			dos.version.minor=0;
			dos.version.major=major;
			return true;
		}
	}
	return false;
}

#define NUMBER_ANSI_DATA 10

struct INT29H_DATA {
	uint8_t lastwrite;
	struct {
		bool esc;
		bool sci;
		bool enabled;
		uint8_t attr;
		uint8_t data[NUMBER_ANSI_DATA];
		uint8_t numberofarg;
		int8_t savecol;
		int8_t saverow;
		bool warned;
		bool key;
	} ansi;
	uint16_t keepcursor;
} int29h_data;

static void ClearAnsi29h(void)
{
	for(uint8_t i = 0 ; i < NUMBER_ANSI_DATA ; i++) {
		int29h_data.ansi.data[i] = 0;
	}
	int29h_data.ansi.esc = false;
	int29h_data.ansi.sci = false;
	int29h_data.ansi.numberofarg = 0;
	int29h_data.ansi.key = false;
}

static Bitu DOS_29Handler(void)
{
	uint16_t tmp_ax = reg_ax;
	uint16_t tmp_bx = reg_bx;
	uint16_t tmp_cx = reg_cx;
	uint16_t tmp_dx = reg_dx;
	Bitu i;
	uint8_t col,row,page;
	uint16_t ncols,nrows;
	uint8_t tempdata;
    if (log_dev_con) {
        if (log_dev_con_str.size() >= 255 || reg_al == '\n' || reg_al == 27) {
            logging_con = true;
            LOG_MSG(log_dev_con==2?"%s":"DOS CON: %s",log_dev_con_str.c_str());
            logging_con = false;
            log_dev_con_str.clear();
        }
        if (reg_al != '\n' && reg_al != '\r')
            log_dev_con_str += (char)reg_al;
    }
	if(!int29h_data.ansi.esc) {
		if(reg_al == '\033') {
			/*clear the datastructure */
			ClearAnsi29h();
			/* start the sequence */
			int29h_data.ansi.esc = true;
		} else if(reg_al == '\t' && !dos.direct_output) {
			/* expand tab if not direct output */
			page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
			do {
				if(CheckAnotherDisplayDriver()) {
					reg_ah = 0x0e;
					reg_al = ' ';
					CALLBACK_RunRealInt(0x10);
				} else {
					if(int29h_data.ansi.enabled)
						INT10_TeletypeOutputAttr(' ', int29h_data.ansi.attr, true);
					else
						INT10_TeletypeOutput(' ', 7);
				}
				col = CURSOR_POS_COL(page);
			} while(col % 8);
			int29h_data.lastwrite = reg_al;
		} else {
			bool scroll = false;
			/* Some sort of "hack" now that '\n' doesn't set col to 0 (int10_char.cpp old chessgame) */
			if((reg_al == '\n') && (int29h_data.lastwrite != '\r')) {
				reg_ah = 0x0e;
				reg_al = '\r';
				CALLBACK_RunRealInt(0x10);
			}
			reg_ax = tmp_ax;
			int29h_data.lastwrite = reg_al;
			/* use ansi attribute if ansi is enabled, otherwise use DOS default attribute*/
			page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
			col = CURSOR_POS_COL(page);
			row = CURSOR_POS_ROW(page);
			ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
			nrows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS);
			if(reg_al == 0x0d) {
				col = 0;
			} else if(reg_al == 0x0a) {
				if(row < nrows) {
					row++;
				} else {
					scroll = true;
				}
			} else if(reg_al == 0x08) {
				if(col > 0) {
					col--;
				}
			} else {
				reg_ah = 0x09;
				reg_bh = page;
				reg_bl = int29h_data.ansi.attr;
				reg_cx = 1;
				CALLBACK_RunRealInt(0x10);

				col++;
				if(col >= ncols) {
					col = 0;
					if(row < nrows) {
						row++;
					} else {
						scroll = true;
					}
				}
			}
			reg_ah = 0x02;
			reg_bh = page;
			reg_dl = col;
			reg_dh = row;
			CALLBACK_RunRealInt(0x10);
			if (scroll) {
				reg_bh = 0x07;
				reg_ax = 0x0601;
				reg_cx = 0x0000;
				reg_dl = (uint8_t)(ncols - 1);
				reg_dh = (uint8_t)nrows;
				CALLBACK_RunRealInt(0x10);
			}
		}
	} else if(!int29h_data.ansi.sci) {
		switch(reg_al) {
		case '[':
			int29h_data.ansi.sci = true;
			break;
		case '7': /* save cursor pos + attr */
		case '8': /* restore this  (Wonder if this is actually used) */
		case 'D':/* scrolling DOWN*/
		case 'M':/* scrolling UP*/
		default:
			LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unknown char %c after a esc", reg_al); /*prob () */
			ClearAnsi29h();
			break;
		}
	} else {
		/*ansi.esc and ansi.sci are true */
		page = real_readb(BIOSMEM_SEG, BIOSMEM_CURRENT_PAGE);
		if(int29h_data.ansi.key) {
			if(reg_al == '"') {
				int29h_data.ansi.key = false;
			} else {
				if(int29h_data.ansi.numberofarg < NUMBER_ANSI_DATA) {
					int29h_data.ansi.data[int29h_data.ansi.numberofarg++] = reg_al;
				}
			}
		} else {
			switch(reg_al) {
				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					int29h_data.ansi.data[int29h_data.ansi.numberofarg] = 10 * int29h_data.ansi.data[int29h_data.ansi.numberofarg] + (reg_al - '0');
					break;
				case ';': /* till a max of NUMBER_ANSI_DATA */
					int29h_data.ansi.numberofarg++;
					break;
				case 'm':               /* SGR */
					for(i = 0 ; i <= int29h_data.ansi.numberofarg ; i++) {
						int29h_data.ansi.enabled = true;
						switch(int29h_data.ansi.data[i]) {
						case 0: /* normal */
							int29h_data.ansi.attr = 0x07;//Real ansi does this as well. (should do current defaults)
							int29h_data.ansi.enabled = false;
							break;
						case 1: /* bold mode on*/
							int29h_data.ansi.attr |= 0x08;
							break;
						case 4: /* underline */
							LOG(LOG_IOCTL,LOG_NORMAL)("ANSI:no support for underline yet");
							break;
						case 5: /* blinking */
							int29h_data.ansi.attr |= 0x80;
							break;
						case 7: /* reverse */
							int29h_data.ansi.attr = 0x70;//Just like real ansi. (should do use current colors reversed)
							break;
						case 30: /* fg color black */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x0;
							break;
						case 31:  /* fg color red */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x4;
							break;
						case 32:  /* fg color green */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x2;
							break;
						case 33: /* fg color yellow */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x6;
							break;
						case 34: /* fg color blue */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x1;
							break;
						case 35: /* fg color magenta */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x5;
							break;
						case 36: /* fg color cyan */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x3;
							break;
						case 37: /* fg color white */
							int29h_data.ansi.attr &= 0xf8;
							int29h_data.ansi.attr |= 0x7;
							break;
						case 40:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x0;
							break;
						case 41:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x40;
							break;
						case 42:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x20;
							break;
						case 43:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x60;
							break;
						case 44:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x10;
							break;
						case 45:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x50;
							break;
						case 46:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x30;
							break;
						case 47:
							int29h_data.ansi.attr &= 0x8f;
							int29h_data.ansi.attr |= 0x70;
							break;
						default:
							break;
						}
					}
					ClearAnsi29h();
					break;
				case 'f':
				case 'H':/* Cursor Pos*/
					if(!int29h_data.ansi.warned) { //Inform the debugger that ansi is used.
						int29h_data.ansi.warned = true;
						LOG(LOG_IOCTL,LOG_WARN)("ANSI SEQUENCES USED");
					}
					ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
					nrows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1;
					/* Turn them into positions that are on the screen */
					if(int29h_data.ansi.data[0] == 0) int29h_data.ansi.data[0] = 1;
					if(int29h_data.ansi.data[1] == 0) int29h_data.ansi.data[1] = 1;
					if(int29h_data.ansi.data[0] > nrows) int29h_data.ansi.data[0] = (uint8_t)nrows;
					if(int29h_data.ansi.data[1] > ncols) int29h_data.ansi.data[1] = (uint8_t)ncols;
					INT10_SetCursorPos_viaRealInt(--(int29h_data.ansi.data[0]), --(int29h_data.ansi.data[1]), page); /*ansi=1 based, int10 is 0 based */
					ClearAnsi29h();
					break;
					/* cursor up down and forward and backward only change the row or the col not both */
				case 'A': /* cursor up*/
					col = CURSOR_POS_COL(page) ;
					row = CURSOR_POS_ROW(page) ;
					tempdata = (int29h_data.ansi.data[0] ? int29h_data.ansi.data[0] : 1);
					if(tempdata > row) row = 0;
					else row -= tempdata;
					INT10_SetCursorPos_viaRealInt(row, col, page);
					ClearAnsi29h();
					break;
				case 'B': /*cursor Down */
					col = CURSOR_POS_COL(page) ;
					row = CURSOR_POS_ROW(page) ;
					nrows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1;
					tempdata = (int29h_data.ansi.data[0] ? int29h_data.ansi.data[0] : 1);
					if(tempdata + static_cast<Bitu>(row) >= nrows) row = nrows - 1;
					else row += tempdata;
					INT10_SetCursorPos_viaRealInt(row, col, page);
					ClearAnsi29h();
					break;
				case 'C': /*cursor forward */
					col = CURSOR_POS_COL(page);
					row = CURSOR_POS_ROW(page);
					ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
					tempdata = (int29h_data.ansi.data[0] ? int29h_data.ansi.data[0] : 1);
					if(tempdata + static_cast<Bitu>(col) >= ncols) col = ncols - 1;
					else col += tempdata;
					INT10_SetCursorPos_viaRealInt(row, col, page);
					ClearAnsi29h();
					break;
				case 'D': /*Cursor Backward  */
					col = CURSOR_POS_COL(page);
					row = CURSOR_POS_ROW(page);
					tempdata=(int29h_data.ansi.data[0] ? int29h_data.ansi.data[0] : 1);
					if(tempdata > col) col = 0;
					else col -= tempdata;
					INT10_SetCursorPos_viaRealInt(row, col, page);
					ClearAnsi29h();
					break;
				case 'J': /*erase screen and move cursor home*/
					if(int29h_data.ansi.data[0] == 0) int29h_data.ansi.data[0] = 2;
					if(int29h_data.ansi.data[0] != 2) {/* every version behaves like type 2 */
						LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: esc[%dJ called : not supported handling as 2", int29h_data.ansi.data[0]);
					}
					INT10_ScrollWindow_viaRealInt(0, 0, 255, 255, 0, int29h_data.ansi.attr, page);
					ClearAnsi29h();
					INT10_SetCursorPos_viaRealInt(0, 0, page);
					break;
				case 'I': /* RESET MODE */
					LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: set/reset mode called(not supported)");
					ClearAnsi29h();
					break;
				case 'u': /* Restore Cursor Pos */
					INT10_SetCursorPos_viaRealInt(int29h_data.ansi.saverow, int29h_data.ansi.savecol, page);
					ClearAnsi29h();
					break;
				case 's': /* SAVE CURSOR POS */
					int29h_data.ansi.savecol = CURSOR_POS_COL(page);
					int29h_data.ansi.saverow = CURSOR_POS_ROW(page);
					ClearAnsi29h();
					break;
				case 'K': /* erase till end of line (don't touch cursor) */
					col = CURSOR_POS_COL(page);
					row = CURSOR_POS_ROW(page);
					ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
					INT10_WriteChar_viaRealInt(' ', int29h_data.ansi.attr, page, ncols - col, true); //Use this one to prevent scrolling when end of screen is reached
					//for(i = col;i<(Bitu) ncols; i++) INT10_TeletypeOutputAttr(' ',ansi.attr,true);
					INT10_SetCursorPos_viaRealInt(row, col, page);
					ClearAnsi29h();
					break;
				case 'M': /* delete line (NANSI) */
					row = CURSOR_POS_ROW(page);
					ncols = real_readw(BIOSMEM_SEG, BIOSMEM_NB_COLS);
					nrows = real_readb(BIOSMEM_SEG, BIOSMEM_NB_ROWS) + 1;
					INT10_ScrollWindow_viaRealInt(row, 0, nrows - 1, ncols - 1, int29h_data.ansi.data[0] ? -int29h_data.ansi.data[0] : -1, int29h_data.ansi.attr, 0xFF);
					ClearAnsi29h();
					break;
				case '>':
					break;
				case '"':
					if(!int29h_data.ansi.key) {
						int29h_data.ansi.key = true;
						int29h_data.ansi.numberofarg = 0;
					}
					break;
				case 'h':
				case 'l':
				case 'p':
					if(J3_IsJapanese()) {
						if(reg_al == 'h') {
							if(int29h_data.ansi.data[int29h_data.ansi.numberofarg] == 5) {
								// disable cursor
								int29h_data.keepcursor = real_readw(BIOSMEM_SEG, BIOSMEM_CURSOR_TYPE);
								INT10_SetCursorShape(0x20, 0x0f);
							}
						} else if(reg_al == 'l') {
							if(int29h_data.ansi.data[int29h_data.ansi.numberofarg] == 5) {
								// enable cursor
								INT10_SetCursorShape(int29h_data.keepcursor >> 8, int29h_data.keepcursor & 0xff);
							}
						} else if(reg_al == 'p') {
							/* reassign keys (needs strings) */
							uint16_t src, dst;
							i = 0;
							if(int29h_data.ansi.data[i] == 0) {
								i++;
								src = int29h_data.ansi.data[i++] << 8;
							} else {
								src = int29h_data.ansi.data[i++];
							}
							if(int29h_data.ansi.data[i] == 0) {
								i++;
								dst = int29h_data.ansi.data[i++] << 8;
							} else {
								dst = int29h_data.ansi.data[i++];
							}
							DOS_SetConKey(src, dst);
						}
						ClearAnsi29h();
						break;
					}
				case 'i':/* printer stuff */
				default:
					LOG(LOG_IOCTL,LOG_NORMAL)("ANSI: unhandled char %c in esc[", reg_al);
					ClearAnsi29h();
					break;
			}
		}
	}
	reg_ax = tmp_ax;
	reg_bx = tmp_bx;
	reg_cx = tmp_cx;
	reg_dx = tmp_dx;

	return CBRET_NONE;
}

void IPX_Setup(Section*);

class DOS:public Module_base{
private:
	CALLBACK_HandlerObject callback[9];
	RealPt int30,int31;

public:
    void DOS_Write_HMA_CPM_jmp(void) {
        // HMA mirror of CP/M entry point.
        // this is needed for "F01D:FEF0" to be a valid jmp whether or not A20 is enabled
        if (dos_in_hma &&
            cpm_compat_mode != CPM_COMPAT_OFF &&
            cpm_compat_mode != CPM_COMPAT_DIRECT) {
            LOG(LOG_DOSMISC,LOG_DEBUG)("Writing HMA mirror of CP/M entry point");

            Bitu was_a20 = XMS_GetEnabledA20();

            XMS_EnableA20(true);

            mem_writeb(0x1000C0,(uint8_t)0xea);		// jmpf
            mem_unalignedwrited(0x1000C0+1,callback[8].Get_RealPointer());

            if (!was_a20) XMS_EnableA20(false);
        }
    }

    uint32_t DOS_Get_CPM_entry_direct(void) {
        return callback[8].Get_RealPointer();
    }

	DOS(Section* configuration):Module_base(configuration){
        const Section_prop* section = static_cast<Section_prop*>(configuration);

        ::disk_data_rate = section->Get_int("hard drive data rate limit");
        ::floppy_data_rate = section->Get_int("floppy drive data rate limit");
        if (::disk_data_rate < 0) {
            if (pcibus_enable)
                ::disk_data_rate = 8333333; /* Probably an average IDE data rate for mid 1990s PCI IDE controllers in PIO mode */
            else
                ::disk_data_rate = 3500000; /* Probably an average IDE data rate for early 1990s ISA IDE controllers in PIO mode */
        }
        if(::floppy_data_rate < 0) {
            ::floppy_data_rate = 22400; // 175 kbps
        }
        std::string prefix = section->Get_string("special operation file prefix");
        if (prefix.size()) prefix_local = prefix + prefix_local.substr(3), prefix_overlay = prefix + prefix_overlay.substr(3);

		maxfcb=100;
		DOS_FILES=200;
		Section_prop *config_section = static_cast<Section_prop *>(control->GetSection("config"));
		if (config_section != NULL && !control->opt_noconfig && !control->opt_securemode && !control->SecureMode()) {
			DOS_FILES = (unsigned int)config_section->Get_int("files");
			if (DOS_FILES<8) DOS_FILES=8;
			else if (DOS_FILES>255) DOS_FILES=255;
			maxfcb = (int)config_section->Get_int("fcbs");
			if (maxfcb<1) maxfcb=1;
			else if (maxfcb>255) maxfcb=255;
			char *dosopt = (char *)config_section->Get_string("dos"), *r=strchr(dosopt, ',');
			if (r==NULL) {
				if (!strcasecmp(trim(dosopt), "high")) dos_in_hma=true;
				else if (!strcasecmp(trim(dosopt), "low")) dos_in_hma=false;
				if (!strcasecmp(trim(dosopt), "umb")) dos_umb=true;
				else if (!strcasecmp(trim(dosopt), "noumb")) dos_umb=false;
			} else {
				*r=0;
				if (!strcasecmp(trim(dosopt), "high")) dos_in_hma=true;
				else if (!strcasecmp(trim(dosopt), "low")) dos_in_hma=false;
				*r=',';
				if (!strcasecmp(trim(r+1), "umb")) dos_umb=true;
				else if (!strcasecmp(trim(r+1), "noumb")) dos_umb=false;
			}
			const char *lastdrive = config_section->Get_string("lastdrive");
			if (strlen(lastdrive)==1&&lastdrive[0]>='a'&&lastdrive[0]<='z')
				maxdrive=lastdrive[0]-'a'+1;
			const char *dosbreak = config_section->Get_string("break");
			if (!strcasecmp(dosbreak, "on"))
				dos.breakcheck=true;
			else if (!strcasecmp(dosbreak, "off"))
				dos.breakcheck=false;
#if defined(WIN32)
			const char *numlock = config_section->Get_string("numlock");
			if ((!strcasecmp(numlock, "off")&&startup_state_numlock) || (!strcasecmp(numlock, "on")&&!startup_state_numlock))
				SetNumLock();
#endif
		}
        char *r;
#if defined(WIN32)
        unsigned int cp = GetACP();
        const char *cstr = (control->opt_noconfig || !config_section) ? "" : (char *)config_section->Get_string("country");
        r = (char *)strchr(cstr, ',');
        if ((r==NULL || !*(r+1) || atoi(trim(r+1)) == cp || (atoi(trim(r+1)) == 951 && cp == 950)) && GetDefaultCP() == 437) {
            if (cp == 950 && !chinasea) makestdcp950table();
            if (cp == 951 && chinasea) makeseacp951table();
            tryconvertcp = (r==NULL || !*(r+1)) ? 1 : 2;
        }
#endif

        dos_sda_size = section->Get_int("dos sda size");
        shell_keyboard_flush = section->Get_bool("command shell flush keyboard buffer");
		enable_network_redirector = section->Get_bool("network redirector");
		enable_dbcs_tables = section->Get_bool("dbcs");
		enable_share_exe = section->Get_bool("share");
		hidenonrep = section->Get_bool("hidenonrepresentable");
		enable_filenamechar = section->Get_bool("filenamechar");
		file_access_tries = section->Get_int("file access tries");
		dos_initial_hma_free = section->Get_int("hma free space");
        minimum_mcb_free = section->Get_hex("minimum mcb free");
		minimum_mcb_segment = section->Get_hex("minimum mcb segment");
		private_segment_in_umb = section->Get_bool("private area in umb");
		enable_collating_uppercase = section->Get_bool("collating and uppercase");
		private_always_from_umb = section->Get_bool("kernel allocation in umb");
		minimum_dos_initial_private_segment = section->Get_hex("minimum dos initial private segment");
		dos_con_use_int16_to_detect_input = section->Get_bool("con device use int 16h to detect keyboard input");
		dbg_zero_on_dos_allocmem = section->Get_bool("zero memory on int 21h memory allocation");
		MAXENV = (unsigned int)section->Get_int("maximum environment block size on exec");
		ENV_KEEPFREE = (unsigned int)section->Get_int("additional environment block size on exec");
		enable_dummy_device_mcb = section->Get_bool("enable dummy device mcb");
		int15_wait_force_unmask_irq = section->Get_bool("int15 wait force unmask irq");
        disk_io_unmask_irq0 = section->Get_bool("unmask timer on disk io");
        mountwarning = section->Get_bool("mountwarning");
        if (winrun) {
            Section* tsec = control->GetSection("dos");
            tsec->HandleInputline("startcmd=true");
            tsec->HandleInputline("dos clipboard device enable=true");
        }
        startcmd = section->Get_bool("startcmd");
        startincon = section->Get_string("startincon");
        const char *dos_clipboard_device_enable = section->Get_string("dos clipboard device enable");
		dos_clipboard_device_access = !strcasecmp(dos_clipboard_device_enable, "disabled")?0:(!strcasecmp(dos_clipboard_device_enable, "read")?2:(!strcasecmp(dos_clipboard_device_enable, "write")?3:(!strcasecmp(dos_clipboard_device_enable, "full")||!strcasecmp(dos_clipboard_device_enable, "true")?4:1)));
		dos_clipboard_device_name = section->Get_string("dos clipboard device name");
        clipboard_dosapi = section->Get_bool("dos clipboard api");
        if (control->SecureMode()) clipboard_dosapi = false;
        pipetmpdev = section->Get_bool("pipe temporary device");
        force_conversion=true;
        mainMenu.get_item("clipboard_dosapi").check(clipboard_dosapi).enable(true).refresh_item(mainMenu);
        force_conversion=false;
		if (dos_clipboard_device_access) {
			bool valid=true;
			char ch[]="*? .|<>/\\\"";
			if (!*dos_clipboard_device_name||strlen(dos_clipboard_device_name)>8||!strcasecmp(dos_clipboard_device_name, "con")||!strcasecmp(dos_clipboard_device_name, "nul")||!strcasecmp(dos_clipboard_device_name, "prn"))
				valid=false;
			else for (unsigned int i=0; i<strlen(ch); i++) {
				if (strchr((char *)dos_clipboard_device_name, *(ch+i))!=NULL) {
					valid=false;
					break;
				}
			}
			dos_clipboard_device_name=valid?upcase((char *)dos_clipboard_device_name):(char *)dos_clipboard_device_default;
			LOG(LOG_DOSMISC,LOG_NORMAL)("DOS clipboard device (%s access) is enabled with the name %s\n", dos_clipboard_device_access==1?"dummy":(dos_clipboard_device_access==2?"read":(dos_clipboard_device_access==3?"write":"full")), dos_clipboard_device_name);
            std::string text=mainMenu.get_item("clipboard_device").get_text();
            std::size_t found = text.find(":");
            if (found!=std::string::npos) text = text.substr(0, found);
            force_conversion=true;
            mainMenu.get_item("clipboard_device").set_text(text+": "+std::string(dos_clipboard_device_name)).check(dos_clipboard_device_access==4&&!control->SecureMode()).enable(true).refresh_item(mainMenu);
            force_conversion=false;
		} else {
            force_conversion=true;
            mainMenu.get_item("clipboard_device").enable(false).refresh_item(mainMenu);
            force_conversion=false;
        }
        std::string autofixwarning=section->Get_string("autofixwarning");
        autofixwarn=autofixwarning=="false"||autofixwarning=="0"||autofixwarning=="none"?0:(autofixwarning=="a20fix"?1:(autofixwarning=="loadfix"?2:3));
        char *cpstr = (char *)section->Get_string("customcodepage");
        r=(char *)strchr(cpstr, ',');
        customcp = 0;
        for (int i=0; i<256; i++) customcp_to_unicode[i] = 0;
        if (r!=NULL) {
            *r=0;
            int cp = atoi(trim(cpstr));
            *r=',';
            std::string cpfile = trim(r+1);
            ResolvePath(cpfile);
            FILE* file = fopen(cpfile.c_str(), "r"); /* should check the result */
            std::string exepath = GetDOSBoxXPath();
            if (!file && exepath.size()) file = fopen((exepath+CROSS_FILESPLIT+cpfile).c_str(), "r");
            if (file && cp > 0 && cp != 932 && cp != 936 && cp != 949 && cp != 950 && cp != 951) {
                customcp = cp;
                char line[256], *l=line;
                while (fgets(line, sizeof(line), file)) {
                    l=trim(l);
                    if (!strlen(l)) continue;
                    r = strchr(l, '#');
                    if (r) *r = 0;
                    l=trim(l);
                    if (!strlen(l)||strncasecmp(l, "0x", 2)) continue;
                    r = strchr(l, ' ');
                    if (!r) r = strchr(l, '\t');
                    if (!r) continue;
                    *r = 0;
                    int ind = (int)strtol(l+2, NULL, 16);
                    r = trim(r+1);
                    if (ind>0xFF||strncasecmp(r, "0x", 2)) continue;
                    int map = (int)strtol(r+2, NULL, 16);
                    customcp_to_unicode[ind] = map;
                }
            }
            if (file) fclose(file);
        }
        if (dos_initial_hma_free > 0x10000)
            dos_initial_hma_free = 0x10000;

        std::string cpmcompat = section->Get_string("cpm compatibility mode");

        if (cpmcompat == "")
            cpmcompat = "auto";

        if (cpmcompat == "msdos2")
            cpm_compat_mode = CPM_COMPAT_MSDOS2;
        else if (cpmcompat == "msdos5")
            cpm_compat_mode = CPM_COMPAT_MSDOS5;
        else if (cpmcompat == "direct")
            cpm_compat_mode = CPM_COMPAT_DIRECT;
        else if (cpmcompat == "auto")
            cpm_compat_mode = CPM_COMPAT_MSDOS5; /* MS-DOS 5.x is default */
        else
            cpm_compat_mode = CPM_COMPAT_OFF;

        /* FIXME: Boot up an MS-DOS system and look at what INT 21h on Microsoft's MS-DOS returns
         *        for SDA size and location, then use that here.
         *
         *        Why does this value matter so much to WordPerfect 5.1? */
        if (dos_sda_size == 0)
            DOS_SDA_SEG_SIZE = 0x560;
        else if (dos_sda_size < 0x1A)
            DOS_SDA_SEG_SIZE = 0x1A;
        else if (dos_sda_size > 32768)
            DOS_SDA_SEG_SIZE = 32768;
        else
            DOS_SDA_SEG_SIZE = (dos_sda_size + 0xF) & (~0xF); /* round up to paragraph */

        /* msdos 2.x and msdos 5.x modes, if HMA is involved, require us to take the first 256 bytes of HMA
         * in order for "F01D:FEF0" to work properly whether or not A20 is enabled. Our direct mode doesn't
         * jump through that address, and therefore doesn't need it. */
        if (dos_in_hma &&
            cpm_compat_mode != CPM_COMPAT_OFF &&
            cpm_compat_mode != CPM_COMPAT_DIRECT) {
            LOG(LOG_DOSMISC,LOG_DEBUG)("DOS: CP/M compatibility method with DOS in HMA requires mirror of entry point in HMA.");
            if (dos_initial_hma_free > 0xFF00) {
                dos_initial_hma_free = 0xFF00;
                LOG(LOG_DOSMISC,LOG_DEBUG)("DOS: CP/M compatibility method requires reduction of HMA free space to accommodate.");
            }
        }

		if ((int)MAXENV < 0) MAXENV = 65535;
		if ((int)ENV_KEEPFREE < 0) ENV_KEEPFREE = 1024;

		LOG(LOG_DOSMISC,LOG_DEBUG)("DOS: MAXENV=%u ENV_KEEPFREE=%u",MAXENV,ENV_KEEPFREE);

		if (ENV_KEEPFREE < 83)
			LOG_MSG("DOS: ENV_KEEPFREE is below 83 bytes. DOS programs that rely on undocumented data following the environment block may break.");

		if (dbg_zero_on_dos_allocmem) {
			LOG_MSG("Debug option enabled: INT 21h memory allocation will always clear memory block before returning\n");
		}

		if (minimum_mcb_segment > 0x8000) minimum_mcb_segment = 0x8000; /* FIXME: Clip against available memory */

        /* we make use of the DOS_GetMemory() function for the dynamic allocation */
        if (private_always_from_umb) {
            DOS_GetMemory_Choose(); /* the pool starts in UMB */
            if (minimum_mcb_segment == 0)
                DOS_MEM_START = IS_PC98_ARCH ? 0x80 : 0x70; /* funny behavior in some games suggests the MS-DOS kernel loads a bit higher on PC-98 */
            else
                DOS_MEM_START = minimum_mcb_segment;

            if (DOS_MEM_START < 0x40)
                LOG_MSG("DANGER, DANGER! DOS_MEM_START has been set to within the interrupt vector table! Proceed at your own risk!");
            else if (DOS_MEM_START < 0x50)
                LOG_MSG("WARNING: DOS_MEM_START has been assigned to the BIOS data area! Proceed at your own risk!");
            else if (DOS_MEM_START < 0x51)
                LOG_MSG("WARNING: DOS_MEM_START has been assigned to segment 0x50, which some programs may use as the Print Screen flag");
            else if (DOS_MEM_START < 0x80 && IS_PC98_ARCH)
                LOG_MSG("CAUTION: DOS_MEM_START is less than 0x80 which may cause problems with some DOS games or applications relying on PC-98 BIOS state");
            else if (DOS_MEM_START < 0x70)
                LOG_MSG("CAUTION: DOS_MEM_START is less than 0x70 which may cause problems with some DOS games or applications");
        }
        else {
            if (minimum_dos_initial_private_segment == 0)
                DOS_PRIVATE_SEGMENT = IS_PC98_ARCH ? 0x80 : 0x70; /* funny behavior in some games suggests the MS-DOS kernel loads a bit higher on PC-98 */
            else
                DOS_PRIVATE_SEGMENT = minimum_dos_initial_private_segment;

            if (DOS_PRIVATE_SEGMENT < 0x50)
                LOG_MSG("DANGER, DANGER! DOS_PRIVATE_SEGMENT has been set too low!");
            if (DOS_PRIVATE_SEGMENT < 0x80 && IS_PC98_ARCH)
                LOG_MSG("DANGER, DANGER! DOS_PRIVATE_SEGMENT has been set too low for PC-98 emulation!");

            if (MEM_TotalPages() > 0x9C)
                DOS_PRIVATE_SEGMENT_END = 0x9C00;
            else
                DOS_PRIVATE_SEGMENT_END = (uint16_t)((MEM_TotalPages() << (12 - 4)) - 1); /* NTS: Remember DOSBox's implementation reuses the last paragraph for UMB linkage */
        }

        LOG(LOG_DOSMISC,LOG_DEBUG)("DOS kernel structures will be allocated from pool 0x%04x-0x%04x",
                DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);

        DOS_IHSEG = DOS_GetMemory(1,"DOS_IHSEG");

        /* DOS_INFOBLOCK_SEG contains the entire List of Lists, though the INT 21h call returns seg:offset with offset nonzero */
	/* NTS: DOS_GetMemory() allocation sizes are in PARAGRAPHS (16-byte units) not bytes */
	/* NTS: DOS_INFOBLOCK_SEG must be 0x20 paragraphs, CONDRV_SEG 0x08 paragraphs, and CONSTRING_SEG 0x0A paragraphs.
	 *      The total combined size of INFOBLOCK+CONDRV+CONSTRING must be 0x32 paragraphs.
	 *      SDA_SEG must be located at INFOBLOCK_SEG+0x32, so that the current PSP segment parameter is exactly at
	 *      memory location INFOBLOCK_SEG:0x330. The reason for this has to do with Microsoft "Genuine MS-DOS detection"
	 *      code in QuickBasic 7.1 and other programs designed to thwart DR-DOS at the time.
	 *
	 *      This is probably why DOSBox SVN hardcoded segments in the first place.
	 *
	 *      See also:
	 *
	 *      [https://www.os2museum.com/wp/how-to-void-your-valuable-warranty/]
         *      [https://www.os2museum.com/files/drdos_detect.txt]
         *      [https://www.os2museum.com/wp/about-that-warranty/]
         *      [https://github.com/joncampbell123/dosbox-x/issues/3626]
	 */
        DOS_INFOBLOCK_SEG = DOS_GetMemory(0x20,"DOS_INFOBLOCK_SEG");	// was 0x80
        DOS_CONDRV_SEG = DOS_GetMemory(0x08,"DOS_CONDRV_SEG");		// was 0xA0
        DOS_CONSTRING_SEG = DOS_GetMemory(0x0A,"DOS_CONSTRING_SEG");	// was 0xA8
        DOS_SDA_SEG = DOS_GetMemory(DOS_SDA_SEG_SIZE>>4,"DOS_SDA_SEG");		// was 0xB2  (0xB2 + 0x56 = 0x108)
        DOS_SDA_OFS = 0;
        DOS_CDS_SEG = DOS_GetMemory(0x10,"DOS_CDA_SEG");		// was 0x108

		LOG(LOG_DOSMISC,LOG_DEBUG)("DOS kernel alloc:");
		LOG(LOG_DOSMISC,LOG_DEBUG)("   IHSEG:        seg 0x%04x",DOS_IHSEG);
		LOG(LOG_DOSMISC,LOG_DEBUG)("   infoblock:    seg 0x%04x",DOS_INFOBLOCK_SEG);
		LOG(LOG_DOSMISC,LOG_DEBUG)("   condrv:       seg 0x%04x",DOS_CONDRV_SEG);
		LOG(LOG_DOSMISC,LOG_DEBUG)("   constring:    seg 0x%04x",DOS_CONSTRING_SEG);
		LOG(LOG_DOSMISC,LOG_DEBUG)("   SDA:          seg 0x%04x:0x%04x %u bytes",DOS_SDA_SEG,DOS_SDA_OFS,DOS_SDA_SEG_SIZE);
		LOG(LOG_DOSMISC,LOG_DEBUG)("   CDS:          seg 0x%04x",DOS_CDS_SEG);
		LOG(LOG_DOSMISC,LOG_DEBUG)("[private segment @ this point 0x%04x-0x%04x mem=0x%04lx]",
			DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END,
			(unsigned long)(MEM_TotalPages() << (12 - 4)));

		callback[0].Install(DOS_20Handler,CB_IRET,"DOS Int 20");
		callback[0].Set_RealVec(0x20);

		callback[1].Install(DOS_21Handler,CB_INT21,"DOS Int 21");
		callback[1].Set_RealVec(0x21);
	//Pseudo code for int 21
	// sti
	// callback 
	// iret
	// retf  <- int 21 4c jumps here to mimic a retf Cyber

		callback[2].Install(DOS_25Handler,CB_RETF_STI,"DOS Int 25");
		callback[2].Set_RealVec(0x25);

		callback[3].Install(DOS_26Handler,CB_RETF_STI,"DOS Int 26");
		callback[3].Set_RealVec(0x26);

		callback[4].Install(DOS_27Handler,CB_IRET,"DOS Int 27");
		callback[4].Set_RealVec(0x27);

		if (section->Get_bool("dos idle api")) {
			callback[5].Install(INT28_HANDLER,CB_INT28,"DOS idle");
		} else {
			callback[5].Install(NULL,CB_IRET,"DOS idle");
		}
		callback[5].Set_RealVec(0x28);

		if (IS_PC98_ARCH) {
			// PC-98 also has INT 29h but the behavior of some games suggest that it is handled
			// the same as CON device output. Apparently the reason Touhou Project has been unable
			// to clear the screen is that it uses INT 29h to directly send ANSI codes rather than
			// standard I/O calls to write to the CON device.
			callback[6].Install(INT29_HANDLER,CB_IRET,"CON Output Int 29");
#if defined(USE_TTF)
		} else if (IS_DOSV || ttf_dosv) {
#else
		} else if (IS_DOSV) {
#endif
			int29h_data.ansi.attr = 0x07;
			callback[6].Install(DOS_29Handler,CB_IRET,"CON Output Int 29");
		} else if (section->Get_bool("ansi.sys")) { // NTS: DOS CON device is not yet initialized, therefore will not return correct value of "is ANSI.SYS installed"?
			// ANSI.SYS is installed, and it does hook INT 29h as well. Route INT 29h through it.
			callback[6].Install(INT29_HANDLER,CB_IRET,"CON Output Int 29");
		} else {
			// Use the DOSBox SVN callback code that takes the byte and shoves it off to INT 10h.
			// NTS: This is where DOSBox SVN is wrong. DOSBox SVN treats INT 29h as a straight call
			//      to INT 10h BUT it also emulates ANSI.SYS and escape codes.
			callback[6].Install(NULL,CB_INT29,"CON Output Int 29");
			// pseudocode for CB_INT29:
			//	push ax
			//	mov ah, 0x0e
			//	int 0x10
			//	pop ax
			//	iret
		}
		callback[6].Set_RealVec(0x29);

        if (!IS_PC98_ARCH) {
            /* DOS installs a handler for INT 1Bh */
            callback[7].Install(BIOS_1BHandler,CB_IRET,"BIOS 1Bh MS-DOS handler");
            callback[7].Set_RealVec(0x1B);
        }

		callback[8].Install(DOS_CPMHandler,CB_CPM,"DOS/CPM Int 30-31");
		int30=RealGetVec(0x30);
		int31=RealGetVec(0x31);
		mem_writeb(0x30*4,(uint8_t)0xea);		// jmpf
		mem_unalignedwrited(0x30*4+1,callback[8].Get_RealPointer());
		// pseudocode for CB_CPM:
		//	pushf
		//	... the rest is like int 21

        if (IS_PC98_ARCH) {
            /* Any interrupt vector pointing to the INT stub in the BIOS must be rewritten to point to a JMP to the stub
             * residing in the DOS segment (60h) because some PC-98 resident drivers use segment 60h as a check for
             * installed vs uninstalled (MUSIC.COM, Peret em Heru) */
            uint16_t sg = DOS_GetMemory(1/*paragraph*/,"INT stub trampoline");
            PhysPt sgp = (PhysPt)sg << (PhysPt)4u;

            /* Re-base the pointer so the segment is 0x60 */
            uint32_t veco = sgp - 0x600;
            if (veco >= 0xFFF0u) E_Exit("INT stub trampoline out of bounds");
            uint32_t vecp = RealMake(0x60,(uint16_t)veco);

            mem_writeb(sgp+0,0xEA);
            mem_writed(sgp+1,BIOS_get_PC98_INT_STUB());

            for (unsigned int i=0;i < 0x100;i++) {
                uint32_t vec = RealGetVec(i);

                if (vec == BIOS_get_PC98_INT_STUB())
                    mem_writed(i*4,vecp);
            }
        }

        /* NTS: HMA support requires XMS. EMS support may switch on A20 if VCPI emulation requires the odd megabyte */
        if ((!dos_in_hma || !section->Get_bool("xms")) && (MEM_A20_Enabled() || strcmp(section->Get_string("ems"),"false") != 0) &&
            cpm_compat_mode != CPM_COMPAT_OFF && cpm_compat_mode != CPM_COMPAT_DIRECT) {
            /* hold on, only if more than 1MB of RAM and memory access permits it */
            if (MEM_TotalPages() > 0x100 && MEM_PageMask() > 0xff/*more than 20-bit decoding*/) {
                LOG(LOG_DOSMISC,LOG_WARN)("DOS not in HMA or XMS is disabled. This may break programs using the CP/M compatibility call method if the A20 gate is switched on.");
            }
        }

		DOS_SetupFiles();								/* Setup system File tables */
		DOS_SetupDevices();							/* Setup dos devices */
		DOS_SetupTables();

		/* move the private segment elsewhere to avoid conflict with the MCB structure.
		 * either set to 0 to cause the decision making to choose an upper memory address,
		 * or allocate an additional private area and start the MCB just after that */
		if (!private_always_from_umb) {
			DOS_MEM_START = DOS_GetMemory(0,"DOS_MEM_START");		// was 0x158 (pass 0 to alloc nothing, get the pointer)

			DOS_GetMemory_reset();
			DOS_PRIVATE_SEGMENT = 0;
			DOS_PRIVATE_SEGMENT_END = 0;
			if (!private_segment_in_umb) {
				/* If private segment is not being placed in UMB, then it must follow the DOS kernel. */
				unsigned int seg;
				unsigned int segend;

				seg = DOS_MEM_START;
				DOS_MEM_START += (uint16_t)DOS_PRIVATE_SEGMENT_Size;
				segend = DOS_MEM_START;

				if (segend >= (MEM_TotalPages() << (12 - 4)))
					E_Exit("Insufficient room for private area");

				DOS_PRIVATE_SEGMENT = seg;
				DOS_PRIVATE_SEGMENT_END = segend;
				DOS_MEM_START = DOS_PRIVATE_SEGMENT_END;
				DOS_GetMemory_reset();
				LOG_MSG("Private area, not stored in UMB on request, occupies 0x%04x-0x%04x [dynamic]\n",
					DOS_PRIVATE_SEGMENT,DOS_PRIVATE_SEGMENT_END-1);
			}
		}

		if (minimum_mcb_segment != 0) {
			if (DOS_MEM_START < minimum_mcb_segment)
				DOS_MEM_START = minimum_mcb_segment;
		}

		LOG(LOG_DOSMISC,LOG_DEBUG)("   mem start:    seg 0x%04x",DOS_MEM_START);

		/* carry on setup */
		DOS_SetupMemory();								/* Setup first MCB */

        /* NTS: The reason PC-98 has a higher minimum free is that the MS-DOS kernel
         *      has a larger footprint in memory, including fixed locations that
         *      some PC-98 games will read directly, and an ANSI driver.
         *
         *      Some PC-98 games will have problems if loaded below a certain
         *      threshold as well.
         *
         *        Valkyrie: 0xE10 is not enough for the game to run. If a specific
         *                  FM music selection is chosen, the remaining memory is
         *                  insufficient for the game to start the battle.
         *
         *      The default assumes a DOS kernel and lower memory region of 32KB,
         *      which might be a reasonable compromise so far.
         *
         * NOTES: A minimum mcb free value of at least 0xE10 is needed for Windows 3.1
         *        386 enhanced to start, else it will complain about insufficient memory (?).
         *        To get Windows 3.1 to run, either set "minimum mcb free=e10" or run
         *        "LOADFIX" before starting Windows 3.1 */

        /* NTS: There is a mysterious memory corruption issue with some DOS games
         *      and applications when they are loaded at or around segment 0x800.
         *      This should be looked into. In the meantime, setting the MCB
         *      start segment before or after 0x800 helps to resolve these issues.
         *      It also puts DOSBox-X at parity with main DOSBox SVN behavior. */
        if (minimum_mcb_free == 0)
            minimum_mcb_free = IS_PC98_ARCH ? 0x800 : 0x700;
        else if (minimum_mcb_free < minimum_mcb_segment)
            minimum_mcb_free = minimum_mcb_segment;

        LOG(LOG_DOSMISC,LOG_DEBUG)("   min free:     seg 0x%04x",minimum_mcb_free);

        if (DOS_MEM_START < minimum_mcb_free) {
            uint16_t sg=0,tmp;

            dos.psp(8); // DOS ownership

            tmp = 1; // start small
            if (DOS_AllocateMemory(&sg,&tmp)) {
                if (sg < minimum_mcb_free) {
                    LOG(LOG_DOSMISC,LOG_DEBUG)("   min free pad: seg 0x%04x",sg);
                }
                else {
                    DOS_FreeMemory(sg);
                    sg = 0;
                }
            }
            else {
                sg=0;
            }

            if (sg != 0 && sg < minimum_mcb_free) {
                tmp = minimum_mcb_free - sg;
                if (!DOS_ResizeMemory(sg,&tmp)) {
                    LOG(LOG_DOSMISC,LOG_DEBUG)("    WARNING: cannot resize min free pad");
                }
            }
        }

#if C_IPX
	IPX_Setup(NULL);
#endif

		DOS_SetupPrograms();
		DOS_SetupMisc();							/* Some additional dos interrupts */
		ZDRIVE_NUM = 25;
		DOS_SDA(DOS_SDA_SEG,DOS_SDA_OFS).SetDrive(25); /* Else the next call gives a warning. */
		DOS_SetDefaultDrive(25);

        if (IS_JEGA_ARCH) {
            INT10_AX_SetCRTBIOSMode(0x51);
            INT16_AX_SetKBDBIOSMode(0x51);
        } else if (IS_J3100) {
            INT60_J3_Setup();
        }
#if defined(USE_TTF)
		if(IS_DOSV || ttf_dosv) {
#else
		if(IS_DOSV) {
#endif
			DOSV_Setup();
			if(IS_J3100 && j3100_start) {
				INT10_SetVideoMode(0x74);
				SetTrueVideoMode(0x74);
			} else if(IS_DOSV) {
				INT10_DOSV_SetCRTBIOSMode(0x03);
			}
		}

        const char *keepstr = section->Get_string("keep private area on boot");
        if (!strcasecmp(keepstr, "true")||!strcasecmp(keepstr, "1")) keep_private_area_on_boot = 1;
        else if (!strcasecmp(keepstr, "false")||!strcasecmp(keepstr, "0")) keep_private_area_on_boot = 0;
        else keep_private_area_on_boot = addovl;
		dos.version.major=5;
		dos.version.minor=0;
		dos.direct_output=false;
		dos.internal_output=false;

        std::string fat32setverstr = section->Get_string("fat32setversion");
        if (fat32setverstr=="auto") fat32setver=1;
        else if (fat32setverstr=="manual") fat32setver=0;
        else fat32setver=-1;

		std::string lfn = section->Get_string("lfn");
		if (lfn=="true"||lfn=="1") enablelfn=1;
		else if (lfn=="false"||lfn=="0") enablelfn=0;
		else if (lfn=="autostart") enablelfn=-2;
		else enablelfn=-1;

        force_conversion=true;
        mainMenu.get_item("dos_lfn_auto").check(enablelfn==-1).enable(true).refresh_item(mainMenu);
        mainMenu.get_item("dos_lfn_enable").check(enablelfn==1).enable(true).refresh_item(mainMenu);
        mainMenu.get_item("dos_lfn_disable").check(enablelfn==0).enable(true).refresh_item(mainMenu);
        force_conversion=false;

        const char *ver = section->Get_string("ver");
		if (*ver) {
			if (set_ver((char *)ver)) {
				/* warn about unusual version numbers */
				if (dos.version.major >= 10 && dos.version.major <= 30) {
					LOG_MSG("WARNING, DOS version %u.%u: the major version is set to a "
						"range that may cause some DOS programs to think they are "
						"running from within an OS/2 DOS box.",
						dos.version.major, dos.version.minor);
				}
				else if (dos.version.major == 0 || dos.version.major > 8 || dos.version.minor > 90)
					LOG_MSG("WARNING: DOS version %u.%u is unusual, may confuse DOS programs",
						dos.version.major, dos.version.minor);
			}
		}
        force_conversion=true;
        dos_ver_menu(true);
        mainMenu.get_item("dos_ver_edit").enable(true).refresh_item(mainMenu);
        update_dos_ems_menu();
        force_conversion=false;

        /* settings */
        if (first_run) {
            const Section_prop * section=static_cast<Section_prop *>(control->GetSection("dos"));
            use_quick_reboot = section->Get_bool("quick reboot");
            enable_config_as_shell_commands = section->Get_bool("shell configuration as commands");
            startwait = section->Get_bool("startwait");
            startquiet = section->Get_bool("startquiet");
            starttranspath = section->Get_bool("starttranspath");
            winautorun=startcmd;
            first_run=false;
        }
        force_conversion=true;
#if !defined(HX_DOS)
        mainMenu.get_item("mapper_quickrun").enable(true).refresh_item(mainMenu);
#endif
        mainMenu.get_item("mapper_rescanall").enable(true).refresh_item(mainMenu);
        mainMenu.get_item("enable_a20gate").enable(true).refresh_item(mainMenu);
        mainMenu.get_item("quick_reboot").check(use_quick_reboot).refresh_item(mainMenu);
        mainMenu.get_item("shell_config_commands").check(enable_config_as_shell_commands).enable(true).refresh_item(mainMenu);
        mainMenu.get_item("limit_hdd_rate").check(::disk_data_rate).enable(true).refresh_item(mainMenu);
        mainMenu.get_item("limit_floppy_rate").check(::floppy_data_rate).enable(true).refresh_item(mainMenu);
#if defined(WIN32) && !defined(HX_DOS)
        mainMenu.get_item("dos_win_autorun").check(winautorun).enable(true).refresh_item(mainMenu);
#endif
#if defined(WIN32) && !defined(HX_DOS) || defined(LINUX) || defined(MACOSX)
        mainMenu.get_item("dos_win_transpath").check(starttranspath).enable(
#if defined(WIN32) && !defined(HX_DOS)
        true
#else
        startcmd
#endif
        ).refresh_item(mainMenu);
        mainMenu.get_item("dos_win_wait").check(startwait).enable(
#if defined(WIN32) && !defined(HX_DOS)
        true
#else
        startcmd
#endif
        ).refresh_item(mainMenu);
        mainMenu.get_item("dos_win_quiet").check(startquiet).enable(
#if defined(WIN32) && !defined(HX_DOS)
        true
#else
        startcmd
#endif
        ).refresh_item(mainMenu);
#endif
        force_conversion=false;

        if (IS_PC98_ARCH) {
            void PC98_InitDefFuncRow(void);
            PC98_InitDefFuncRow();

            real_writeb(0x60,0x113,0x01); /* 25-line mode */
        }
        *appname=0;
        *appargs=0;
	}
	~DOS(){
		infix=-1;
#if defined(WIN32) && !defined(HX_DOS) && !defined(_WIN32_WINDOWS)
		if (startwait) {
			void EndStartProcess();
			EndStartProcess();
			EndRunProcess();
		}
		force_conversion=true;
#endif
#if defined(WIN32) && !defined(HX_DOS)
		mainMenu.get_item("dos_win_autorun").enable(false).refresh_item(mainMenu);
#endif
#if defined(WIN32) && !defined(HX_DOS) || defined(LINUX) || defined(MACOSX)
		mainMenu.get_item("dos_win_transpath").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_win_wait").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_win_quiet").enable(false).refresh_item(mainMenu);
#endif
		mainMenu.get_item("dos_lfn_auto").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_lfn_enable").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_lfn_disable").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ver_330").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ver_500").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ver_622").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ver_710").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ver_edit").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ems_true").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ems_board").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ems_emm386").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("dos_ems_false").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("enable_a20gate").enable(false).refresh_item(mainMenu);
#if !defined(HX_DOS)
		mainMenu.get_item("mapper_quickrun").enable(false).refresh_item(mainMenu);
#endif
		mainMenu.get_item("mapper_rescanall").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("shell_config_commands").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("clipboard_device").enable(false).refresh_item(mainMenu);
		mainMenu.get_item("clipboard_dosapi").enable(false).refresh_item(mainMenu);
		force_conversion=false;
		/* NTS: We do NOT free the drives! The OS may use them later! */
		void DOS_ShutdownFiles();
		DOS_ShutdownFiles();
		void DOS_ShutdownDevices(void);
		DOS_ShutdownDevices();
		RealSetVec(0x30,int30);
		RealSetVec(0x31,int31);
	}
};

static DOS* test = NULL;

void DOS_Write_HMA_CPM_jmp(void) {
    assert(test != NULL);
    test->DOS_Write_HMA_CPM_jmp();
}

uint32_t DOS_Get_CPM_entry_direct(void) {
    assert(test != NULL);
    return test->DOS_Get_CPM_entry_direct();
}

void DOS_ShutdownFiles() {
	if (Files != NULL) {
		for (Bitu i=0;i<DOS_FILES;i++) {
			if (Files[i] != NULL) {
				delete Files[i];
				Files[i] = NULL;
			}
		}
		delete[] Files;
		Files = NULL;
	}
}

void DOS_ShutdownDrives() {
	for (uint16_t i=0;i<DOS_DRIVES;i++) {
		if (Drives[i] != NULL) {
			if (DriveManager::UnmountDrive(i) == 0)
				Drives[i] = NULL; /* deletes drive but does not set to NULL because surrounding code does that */
			else if (i != ZDRIVE_NUM)
				LOG(LOG_DOSMISC,LOG_DEBUG)("Failed to unmount drive %c",i+'A'); /* probably drive Z: , UnMount() always returns nonzero */
		}

		if (Drives[i] != NULL) { /* just in case */
			delete Drives[i];
			Drives[i] = NULL;
		}
	}

	if (imgDTA != NULL) { /* NTS: Allocated by FAT driver */
		delete imgDTA;
		imgDTA = NULL;
		imgDTASeg = 0;
		imgDTAPtr = 0;
	}
}

void update_pc98_function_row(unsigned char setting,bool force_redraw=false);
void DOS_Casemap_Free();

void DOS_EnableDriveMenu(char drv) {
    if (drv >= 'A' && drv <= 'Z') {
		std::string name;
		bool empty=!dos_kernel_disabled && Drives[drv-'A'] == NULL;
#if defined (WIN32)
		name = std::string("drive_") + drv + "_mountauto";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
#endif
		name = std::string("drive_") + drv + "_mounthd";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountcd";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountfd";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountfro";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountarc";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountimg";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountimgs";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_mountiro";
		mainMenu.get_item(name).enable(empty).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_unmount";
		mainMenu.get_item(name).enable(!dos_kernel_disabled && Drives[drv-'A'] != NULL && (drv-'A') != ZDRIVE_NUM).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_swap";
		mainMenu.get_item(name).enable(!dos_kernel_disabled && Drives[drv-'A'] != NULL && (drv-'A') != ZDRIVE_NUM).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_rescan";
		mainMenu.get_item(name).enable(!dos_kernel_disabled && Drives[drv-'A'] != NULL).refresh_item(mainMenu);
		name = std::string("drive_") + drv + "_info";
		mainMenu.get_item(name).enable(!dos_kernel_disabled && Drives[drv-'A'] != NULL).refresh_item(mainMenu);
		if (drv == 'A' || drv == 'C' || drv == 'D') {
			name = std::string("drive_") + drv + "_boot";
			mainMenu.get_item(name).enable(!dos_kernel_disabled).refresh_item(mainMenu);
			name = std::string("drive_") + drv + "_bootimg";
			mainMenu.get_item(name).enable(!dos_kernel_disabled).refresh_item(mainMenu);
		}
		name = std::string("drive_") + drv + "_saveimg";
		mainMenu.get_item(name).enable(Drives[drv-'A'] != NULL && !dynamic_cast<fatDrive*>(Drives[drv-'A'])).refresh_item(mainMenu);
        if (dos_kernel_disabled || !strcmp(RunningProgram, "LOADLIN")) {
            bool found = false;
            for (int i=0; i<MAX_DISK_IMAGES; i++)
                if (imageDiskList[i] && imageDiskList[i]->ffdd && imageDiskList[i]->drvnum == drv-'A') {
                    found = true;
                    break;
                }
            if (!found) mainMenu.get_item(name).enable(false).refresh_item(mainMenu);
        }
    }
}

void DOS_DoShutDown() {
	if (test != NULL) {
		if (strcmp(RunningProgram, "LOADLIN")) delete test;
		test = NULL;
	}

    if (IS_PC98_ARCH) update_pc98_function_row(0);

    DOS_Casemap_Free();

    for (char drv='A';drv <= 'Z';drv++) DOS_EnableDriveMenu(drv);
}

void DOS_GetMemory_reinit();
void LOADFIX_OnDOSShutdown();

void DOS_ShutDown(Section* /*sec*/) {
	LOADFIX_OnDOSShutdown();
	DOS_DoShutDown();
}

void DOS_OnReset(Section* /*sec*/) {
	LOADFIX_OnDOSShutdown();
	DOS_DoShutDown();
	DOS_GetMemory_reinit();
}

void DOS_Startup(Section* sec) {
    (void)sec;//UNUSED

	if (test == NULL) {
        DOS_GetMemLog.clear();
        DOS_GetMemory_reinit();
        LOG(LOG_DOSMISC,LOG_DEBUG)("Allocating DOS kernel");
		test = new DOS(control->GetSection("dos"));
	}

    force_conversion=true;
    for (char drv='A';drv <= 'Z';drv++) DOS_EnableDriveMenu(drv);
    force_conversion=false;
}

void DOS_RescanAll(bool pressed) {
    if (!pressed) return;
    if (dos_kernel_disabled) return;

    LOG(LOG_DOSMISC,LOG_DEBUG)("Triggering rescan on all drives");
    for(Bitu i =0; i<DOS_DRIVES;i++) {
        if (Drives[i]) Drives[i]->EmptyCache();
    }
}

void DOS_Init() {
	LOG(LOG_DOSMISC,LOG_DEBUG)("Initializing DOS kernel (DOS_Init)");
    LOG(LOG_DOSMISC,LOG_DEBUG)("sizeof(union bootSector) = %u",(unsigned int)sizeof(union bootSector));
    LOG(LOG_DOSMISC,LOG_DEBUG)("sizeof(struct FAT_BootSector) = %u",(unsigned int)sizeof(struct FAT_BootSector));
    LOG(LOG_DOSMISC,LOG_DEBUG)("sizeof(direntry) = %u",(unsigned int)sizeof(direntry));

    /* this code makes assumptions! */
    assert(sizeof(direntry) == 32);
    assert((SECTOR_SIZE_MAX % sizeof(direntry)) == 0);
    assert((MAX_DIRENTS_PER_SECTOR * sizeof(direntry)) == SECTOR_SIZE_MAX);

	AddExitFunction(AddExitFunctionFuncPair(DOS_ShutDown),false);
	AddVMEventFunction(VM_EVENT_RESET,AddVMEventFunctionFuncPair(DOS_OnReset));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_KERNEL,AddVMEventFunctionFuncPair(DOS_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_EXIT_REBOOT_KERNEL,AddVMEventFunctionFuncPair(DOS_ShutDown));
	AddVMEventFunction(VM_EVENT_DOS_SURPRISE_REBOOT,AddVMEventFunctionFuncPair(DOS_OnReset));

    DOSBoxMenu::item *item;

    MAPPER_AddHandler(DOS_RescanAll,MK_nothing,0,"rescanall","Rescan drives",&item);
    item->enable(false).refresh_item(mainMenu);
    item->set_text("Rescan all drives");
    for (char drv='A';drv <= 'Z';drv++) DOS_EnableDriveMenu(drv);
}

void DOS_Int21_7139(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_MakeDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_713a(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if  (DOS_RemoveDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
				LOG(LOG_DOSMISC,LOG_NORMAL)("Remove dir failed on %s with error %X",name1,dos.errorcode);
		}
}

void DOS_Int21_713b(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if  (DOS_ChangeDir(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7141(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_UnlinkFile(name1)) {
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7143(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		switch (reg_bl) {
				case 0x00:                              /* Get */
				{
					uint16_t attr_val=reg_cx;
					if (DOS_GetFileAttr(name1,&attr_val)) {
							reg_cx=attr_val;
							reg_ax=0;
							CALLBACK_SCF(false);
					} else {
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
					}
					break;
				};
				case 0x01:                              /* Set */
					if (DOS_SetFileAttr(name1,reg_cx)) {
							reg_ax=0;
							CALLBACK_SCF(false);
					} else {
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
					}
					break;
				case 0x02:				/* Get compressed file size */
				{
					reg_ax=0;
					reg_dx=0;
					unsigned long size = DOS_GetCompressedFileSize(name1);
					if (size != (unsigned long)(-1l)) {
#if defined (WIN32)
						reg_ax = LOWORD(size);
						reg_dx = HIWORD(size);
#endif
						CALLBACK_SCF(false);
					} else {
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				}
				case 0x03:
				case 0x05:
				case 0x07:
				{
#if defined (WIN32) && !defined(HX_DOS)
					HANDLE hFile = DOS_CreateOpenFile(name1);
					if (hFile != INVALID_HANDLE_VALUE) {
						time_t clock = time(NULL), ttime;
						struct tm *t = localtime(&clock);
						FILETIME time;
						t->tm_isdst = -1;
						t->tm_sec  = (((int)reg_cx) << 1) & 0x3e;
						t->tm_min  = (((int)reg_cx) >> 5) & 0x3f;
						t->tm_hour = (((int)reg_cx) >> 11) & 0x1f;
						t->tm_mday = (int)(reg_di) & 0x1f;
						t->tm_mon  = ((int)(reg_di >> 5) & 0x0f) - 1;
						t->tm_year = ((int)(reg_di >> 9) & 0x7f) + 80;
						ttime=mktime(t);
						LONGLONG ll = Int32x32To64(ttime, 10000000) + 116444736000000000 + (reg_bl==0x07?reg_si*100000:0);
						time.dwLowDateTime = (DWORD) ll;
						time.dwHighDateTime = (DWORD) (ll >> 32);
						if (!SetFileTime(hFile, reg_bl==0x07?&time:NULL,reg_bl==0x05?&time:NULL,reg_bl==0x03?&time:NULL)) {
							CloseHandle(hFile);
							CALLBACK_SCF(true);
							reg_ax=dos.errorcode;
							break;
						}
						CloseHandle(hFile);
						reg_ax=0;
						CALLBACK_SCF(false);
					} else
#endif
					{
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				}
				case 0x04:
				case 0x06:
				case 0x08:
#if !defined(HX_DOS)
					struct stat status;
					if (DOS_GetFileAttrEx(name1, &status)) {
						const struct tm * ltime;
						time_t ttime=reg_bl==0x04?status.st_mtime:reg_bl==0x06?status.st_atime:status.st_ctime;
						if ((ltime=localtime(&ttime))!=0) {
							reg_cx=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
							reg_di=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
						}
						if (reg_bl==0x08)
							reg_si = 0;
						reg_ax=0;
						CALLBACK_SCF(false);
					} else
#endif
					{
						CALLBACK_SCF(true);
						reg_ax=dos.errorcode;
					}
					break;
				default:
						E_Exit("DOS:Illegal LFN Attr call %2X",reg_bl);
		}
}

void DOS_Int21_7147(char *name1, const char *name2) {
    (void)name2;
		DOS_PSP psp(dos.psp());
		psp.StoreCommandTail();
		if (DOS_GetCurrentDir(reg_dl,name1,true)) {
				MEM_BlockWrite(SegPhys(ds)+reg_si,name1,(Bitu)(strlen(name1)+1));
				psp.RestoreCommandTail();
				reg_ax=0;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_714e(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (!DOS_GetSFNPath(name1,name2,false)) {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
			return;
		}
		uint16_t entry;
		uint8_t i,handle=(uint8_t)DOS_FILES;
		for (i=1;i<DOS_FILES;i++) {
			if (!Files[i]) {
				handle=i;
				break;
			}
		}
		if (handle==DOS_FILES) {
			reg_ax=DOSERR_TOO_MANY_OPEN_FILES;
			CALLBACK_SCF(true);
			return;
		}
		if (strlen(name2)>2&&name2[strlen(name2)-2]=='\\'&&name2[strlen(name2)-1]=='*')
			strcat(name2, ".*");
		lfn_filefind_handle=handle;
		bool b=DOS_FindFirst(name2,reg_cx,false);
		lfn_filefind_handle=LFN_FILEFIND_NONE;
		int error=dos.errorcode;
		uint16_t attribute = 0;
		if (!b&&!(strlen(name2)==3&&*(name2+1)==':'&&*(name2+2)=='\\')&&DOS_GetFileAttr(name2, &attribute) && (attribute&DOS_ATTR_DIRECTORY)) {
			strcat(name2,"\\*.*");
			lfn_filefind_handle=handle;
			b=DOS_FindFirst(name2,reg_cx,false);
			lfn_filefind_handle=LFN_FILEFIND_NONE;
			error=dos.errorcode;
		}
		if (b) {
				DOS_PSP psp(dos.psp());
				entry = psp.FindFreeFileEntry();
				if (entry==0xff) {
					reg_ax=DOSERR_TOO_MANY_OPEN_FILES;
					CALLBACK_SCF(true);
					return;
				}
				if (handle>=DOS_DEVICES||!Devices[handle])
					{
					int m=0;
					for (int i=1;i<DOS_DEVICES;i++)
						if (Devices[i]) m=i;
					Files[handle]=new DOS_Device(*Devices[m]);
					}
				else
					Files[handle]=new DOS_Device(*Devices[handle]);
				Files[handle]->AddRef();
				psp.SetFileHandle(entry,handle);
				reg_ax=handle;
				DOS_DTA dta(dos.dta());
				char finddata[CROSS_LEN];
				int c=0;
				MEM_BlockWrite(SegPhys(es)+reg_di,finddata,dta.GetFindData((int)reg_si,finddata,&c));
				reg_cx=c;
				CALLBACK_SCF(false);
		} else {
				dos.errorcode=error;
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_714f(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
		uint8_t handle=(uint8_t)reg_bx;
		if (!handle || handle>=DOS_FILES || !Files[handle]) {
			reg_ax=DOSERR_INVALID_HANDLE;
			CALLBACK_SCF(true);
			return;
		}
		lfn_filefind_handle=handle;
		if (DOS_FindNext()) {
				DOS_DTA dta(dos.dta());
				char finddata[CROSS_LEN];
				int c=0;
				MEM_BlockWrite(SegPhys(es)+reg_di,finddata,dta.GetFindData((int)reg_si,finddata,&c));
				reg_cx=c;
				CALLBACK_SCF(false);
				reg_ax=0x4f00+handle;
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
		lfn_filefind_handle=LFN_FILEFIND_NONE;
}

void DOS_Int21_7156(char *name1, char *name2) {
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		MEM_StrCopy(SegPhys(es)+reg_di,name2+1,DOSNAMEBUF);
		*name2='\"';
		p=name2+strlen(name2);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_Rename(name1,name2)) {
				reg_ax=0;
				CALLBACK_SCF(false);                   
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_7160(char *name1, char *name2) {
        MEM_StrCopy(SegPhys(ds)+reg_si,name1+1,DOSNAMEBUF);
        if (*(name1+1)>=0 && *(name1+1)<32) {
            reg_ax=!*(name1+1)?2:3;
            CALLBACK_SCF(true);
            return;
        }
		bool tail = check_last_split_char(name1 + 1, strlen(name1 + 1), '\\');
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_Canonicalize(name1,name2)) {
				if(reg_cl != 0) {
					strcpy(name1,"\"");
					strcat(name1,name2);
					strcat(name1,"\"");
				}
				switch(reg_cl)          {
						case 0:         // Canonoical path name
								if(tail) strcat(name2, "\\");
								MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
								reg_ax=0;
								CALLBACK_SCF(false);
								break;
						case 1:         // SFN path name
                                checkwat=true;
								if (DOS_GetSFNPath(name1,name2,false)) {
									if(tail) strcat(name2, "\\");
									MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
									reg_ax=0;
									CALLBACK_SCF(false);
								} else {
									reg_ax=2;
									CALLBACK_SCF(true);
								}
                                checkwat=false;
								break;
						case 2:         // LFN path name
								if (DOS_GetSFNPath(name1,name2,true)) {
									if(tail) strcat(name2, "\\");
									MEM_BlockWrite(SegPhys(es)+reg_di,name2,(Bitu)(strlen(name2)+1));
									reg_ax=0;
									CALLBACK_SCF(false);
								} else {
									reg_ax=2;
									CALLBACK_SCF(true);
								}
								break;
						default:
								E_Exit("DOS:Illegal LFN GetName call %2X",reg_cl);
				}
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_716c(char *name1, const char *name2) {
    (void)name2;
		MEM_StrCopy(SegPhys(ds)+reg_si,name1+1,DOSNAMEBUF);
		*name1='\"';
		char *p=name1+strlen(name1);
		while (*p==' '||*p==0) p--;
		*(p+1)='\"';
		*(p+2)=0;
		if (DOS_OpenFileExtended(name1,reg_bx,reg_cx,reg_dx,&reg_ax,&reg_cx)) {
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_71a0(char *name1, char *name2) {
		/* NTS:  Windows Millennium Edition's SETUP program will make this LFN call to
		 *		 canonicalize "C:", except the protected mode kernel does not translate
		 *		 DS:DX and ES:DI from protected mode. So DS:DX correctly points to
		 *		 ASCII-Z string "C:" but when the jump is made back to real mode and
		 *		 our INT 21h is actually called, DS:DX points to unrelated memory and
		 *		 the string we read is gibberish, and ES:DI likewise point to unrelated
		 *		 memory.
		 *
		 *		 If we write to ES:DI, we corrupt memory in a way that quickly causes
		 *		 the setup program to fault and crash (often, a Segment Not Present
		 *		 exception but sometimes worse).
		 *
		 *		 The reason nobody ever encounters this error when installing Windows
		 *		 ME normally from a boot disk is because in pure DOS mode where SETUP
		 *		 is normally run, LFN functions do not exist. This call, INT 21h AX=71A0h,
		 *		 will normally return an error (CF=1) without reading or writing any
		 *		 memory, and SETUP carries on without crashing.
		 *
		 *		 Therefore, if you want to install Windows ME from the DOSBox-X DOS
		 *		 environment, you need to disable Long Filename emulation first before
		 *		 running SETUP.EXE to avoid crashes and instability during the install
		 *		 process. --J.C. */
		MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);

		if (DOS_Canonicalize(name1,name2)) {
				if (reg_cx > 3)
						MEM_BlockWrite(SegPhys(es)+reg_di,"FAT",4);
				reg_ax=0;
				reg_bx=0x4006;
				reg_cx=0xff;
				reg_dx=0x104;
				CALLBACK_SCF(false);
		} else {
				reg_ax=dos.errorcode;
				CALLBACK_SCF(true);
		}
}

void DOS_Int21_71a1(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
		uint8_t handle=(uint8_t)reg_bx;
		if (!handle || handle>=DOS_FILES || !Files[handle]) {
			reg_ax=DOSERR_INVALID_HANDLE;
			CALLBACK_SCF(true);
			return;
		}
		DOS_PSP psp(dos.psp());
		uint16_t entry=psp.FindEntryByHandle(handle);
		if (entry>0&&entry!=0xff) psp.SetFileHandle(entry,0xff);
		if (entry>0&&Files[handle]->RemoveRef()<=0) {
			delete Files[handle];
			Files[handle]=0;
		}
		reg_ax=0;
		CALLBACK_SCF(false);
}

void DOS_Int21_71a6(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
	char buf[64];
	unsigned long serial_number=0x1234,st=0,cdate=0,ctime=0,adate=0,atime=0,mdate=0,mtime=0;
    uint8_t entry = (uint8_t)reg_bx;
    uint8_t handle = 0;
	if (entry>=DOS_FILES) {
		reg_ax=DOSERR_INVALID_HANDLE;
		CALLBACK_SCF(true);
		return;
	}
	DOS_PSP psp(dos.psp());
	for (unsigned int i=0;i<=DOS_FILES;i++)
		if (Files[i] && psp.FindEntryByHandle(i)==entry)
			handle=i;
	if (handle < DOS_FILES && Files[handle] && Files[handle]->name!=NULL) {
		uint8_t drive=Files[handle]->GetDrive();
		if (Drives[drive]) {
			if (!strncmp(Drives[drive]->GetInfo(),"fatDrive ",9)) {
				fatDrive* fdp = dynamic_cast<fatDrive*>(Drives[drive]);
				if (fdp != NULL) serial_number=fdp->GetSerial();
			}
#if defined (WIN32)
			if (!strncmp(Drives[drive]->GetInfo(),"local ",6) || !strncmp(Drives[drive]->GetInfo(),"CDRom ",6)) {
				localDrive* ldp = !strncmp(Drives[drive]->GetInfo(),"local ",6)?dynamic_cast<localDrive*>(Drives[drive]):dynamic_cast<cdromDrive*>(Drives[drive]);
				if (ldp != NULL) serial_number=ldp->GetSerial();
			}
#endif
		}
		struct stat status;
		if (DOS_GetFileAttrEx(Files[handle]->name, &status, Files[handle]->GetDrive())) {
#if !defined(HX_DOS)
			time_t ttime;
			const struct tm * ltime;
			ttime=status.st_ctime;
			if ((ltime=localtime(&ttime))!=0) {
				ctime=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
				cdate=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
			}
			ttime=status.st_atime;
			if ((ltime=localtime(&ttime))!=0) {
				atime=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
				adate=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
			}
			ttime=status.st_mtime;
			if ((ltime=localtime(&ttime))!=0) {
				mtime=DOS_PackTime((uint16_t)ltime->tm_hour,(uint16_t)ltime->tm_min,(uint16_t)ltime->tm_sec);
				mdate=DOS_PackDate((uint16_t)(ltime->tm_year+1900),(uint16_t)(ltime->tm_mon+1),(uint16_t)ltime->tm_mday);
			}
#endif
			sprintf(buf,"%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s%-4s",(char*)&st,(char*)&ctime,(char*)&cdate,(char*)&atime,(char*)&adate,(char*)&mtime,(char*)&mdate,(char*)&serial_number,(char*)&st,(char*)&st,(char*)&st,(char*)&st,(char*)&handle);
			for (int i=32;i<36;i++) buf[i]=0;
			buf[36]=(char)((uint32_t)status.st_size%256);
			buf[37]=(char)(((uint32_t)status.st_size%65536)/256);
			buf[38]=(char)(((uint32_t)status.st_size%16777216)/65536);
			buf[39]=(char)((uint32_t)status.st_size/16777216);
			buf[40]=(char)status.st_nlink;
			for (int i=41;i<47;i++) buf[i]=0;
			buf[52]=0;
			MEM_BlockWrite(SegPhys(ds)+reg_dx,buf,53);
			reg_ax=0;
			CALLBACK_SCF(false);
		} else {
			reg_ax=dos.errorcode;
			CALLBACK_SCF(true);
		}
	} else {
		reg_ax=dos.errorcode;
		CALLBACK_SCF(true);
	}
}

void DOS_Int21_71a7(const char *name1, const char *name2) {
    (void)name1;
    (void)name2;
	switch (reg_bl) {
			case 0x00:
					reg_cl=mem_readb(SegPhys(ds)+reg_si);	//not yet a proper implementation,
					reg_ch=mem_readb(SegPhys(ds)+reg_si+1);	//but MS-DOS 7 and 4DOS DIR should
					reg_dl=mem_readb(SegPhys(ds)+reg_si+4);	//show date/time correctly now
					reg_dh=mem_readb(SegPhys(ds)+reg_si+5);
					reg_bh=0;
					reg_ax=0;
					CALLBACK_SCF(false);
					break;
			case 0x01:
					mem_writeb(SegPhys(es)+reg_di,reg_cl);
					mem_writeb(SegPhys(es)+reg_di+1,reg_ch);
					mem_writeb(SegPhys(es)+reg_di+4,reg_dl);
					mem_writeb(SegPhys(es)+reg_di+5,reg_dh);
					reg_ax=0;
					CALLBACK_SCF(false);
					break;
			default:
					E_Exit("DOS:Illegal LFN TimeConv call %2X",reg_bl);
	}
}

void DOS_Int21_71a8(char* name1, const char* name2) {
    (void)name2;
	if (reg_dh == 0 || reg_dh == 1) {
			MEM_StrCopy(SegPhys(ds)+reg_si,name1,DOSNAMEBUF);
			int i,j=0,o=0;
            char c[13];
            if (reg_dh == 0) memset(c, 0, sizeof(c));
            if (strcmp(name1, ".") && strcmp(name1, "..")) {
                const char* s = strrchr(name1, '.');
                for (i=0;i<8;j++) {
                        if (name1[j] == 0 || (s==NULL?8:s-name1) <= j) {
                            if (reg_dh == 0 && s != NULL) for (int j=0; j<8-i; j++) c[o++] = ' ';
                            break;
                        }
                        while (name1[j]&&(name1[j]<=32||name1[j]==127||name1[j]=='"'||name1[j]=='+'||name1[j]=='='||name1[j]=='.'||name1[j]==','||name1[j]==';'||name1[j]==':'||name1[j]=='<'||name1[j]=='>'||name1[j]=='['||name1[j]==']'||name1[j]=='|'||name1[j]=='\\'||name1[j]=='?'||name1[j]=='*')) j++;
                        c[o++] = toupper(name1[j]);
                        i++;
                }
                if (s != NULL) {
                        s++;
                        if (*s != 0 && reg_dh == 1) c[o++] = '.';
                        j=0;
                        for (i=0;i<3;i++) {
                                if (*(s+i+j) == 0) break;
                                while (*(s+i+j)&&(*(s+i+j)<=32||*(s+i+j)==127||*(s+i+j)=='"'||*(s+i+j)=='+'||*(s+i+j)=='='||*(s+i+j)==','||*(s+i+j)==';'||*(s+i+j)==':'||*(s+i+j)=='<'||*(s+i+j)=='>'||*(s+i+j)=='['||*(s+i+j)==']'||*(s+i+j)=='|'||*(s+i+j)=='\\'||*(s+i+j)=='?'||*(s+i+j)=='*')) j++;
                                c[o++] = toupper(*(s+i+j));
                        }
                }
                assert(o <= 12);
                c[o] = 0;
            } else
                strcpy(c, name1);
			MEM_BlockWrite(SegPhys(es)+reg_di,c,reg_dh==1?strlen(c)+1:11);
			reg_ax=0;
			CALLBACK_SCF(false);
	} else {
			reg_ax=1;
			CALLBACK_SCF(true);
	}
}

void DOS_Int21_71aa(char* name1, const char* name2) {
    (void)name2;
	if (reg_bh<3 && (reg_bl<1 || reg_bl>26)) {
			reg_ax = DOSERR_INVALID_DRIVE;
			CALLBACK_SCF(true);
			return;
	}
	switch (reg_bh) {
		case 0:
		{
			uint8_t drive=reg_bl-1;
			if (drive==DOS_GetDefaultDrive() || Drives[drive] || drive==25) {
				reg_ax = DOSERR_INVALID_DRIVE;
				CALLBACK_SCF(true);
			} else {
				MEM_StrCopy(SegPhys(ds)+reg_dx,name1,DOSNAMEBUF);
				char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
				char temp_str[3] = { 0,0,0 };
				temp_str[0]=(char)('A'+reg_bl-1);
				temp_str[1]=' ';
				strcpy(mountstring,temp_str);
				strcat(mountstring,name1);
				strcat(mountstring," -Q");
				runMount(mountstring);
				if (Drives[drive]) {
					reg_ax=0;
					CALLBACK_SCF(false);
				} else {
					reg_ax=DOSERR_PATH_NOT_FOUND;
					CALLBACK_SCF(true);
				}
			}
			break;
		}
		case 1:
		{
			uint8_t drive=reg_bl-1;
			if (drive==DOS_GetDefaultDrive() || !Drives[drive] || drive==25) {
				reg_ax = DOSERR_INVALID_DRIVE;
				CALLBACK_SCF(true);
			} else {
				char mountstring[DOS_PATHLENGTH+CROSS_LEN+20];
				char temp_str[2] = { 0,0 };
				temp_str[0]=(char)('A'+reg_bl-1);
				strcpy(mountstring,temp_str);
				strcat(mountstring," -Q -U");
				runMount(mountstring);
				if (!Drives[drive]) {
					reg_ax =0;
					CALLBACK_SCF(false);
				} else {
					reg_ax=5;
					CALLBACK_SCF(true);
				}
			}
			break;
		}
		case 2:
		{
			uint8_t drive=reg_bl>0?reg_bl-1:DOS_GetDefaultDrive();
			if (Drives[drive]&&!strncmp(Drives[drive]->GetInfo(),"local directory ",16)) {
				strcpy(name1,Drives[drive]->GetInfo()+16);
				MEM_BlockWrite(SegPhys(ds)+reg_dx,name1,(Bitu)(strlen(name1)+1));
				reg_ax=0;
				CALLBACK_SCF(false);
			} else {
				reg_ax=3;
				CALLBACK_SCF(true);
			}
			break;
		}
		default:
			E_Exit("DOS:Illegal LFN Subst call %2X",reg_bh);
	}
}

//save state support
extern void POD_Save_DOS_Devices( std::ostream& stream );
extern void POD_Save_DOS_DriveManager( std::ostream& stream );
extern void POD_Save_DOS_Files( std::ostream& stream );
extern void POD_Save_DOS_Memory( std::ostream& stream);
extern void POD_Save_DOS_Mscdex( std::ostream& stream );
extern void POD_Save_DOS_Tables( std::ostream& stream );

extern void POD_Load_DOS_Devices( std::istream& stream );
extern void POD_Load_DOS_DriveManager( std::istream& stream );
extern void POD_Load_DOS_Files( std::istream& stream );
extern void POD_Load_DOS_Memory( std::istream& stream );
extern void POD_Load_DOS_Mscdex( std::istream& stream );
extern void POD_Load_DOS_Tables( std::istream& stream );

namespace
{
class SerializeDos : public SerializeGlobalPOD
{
public:
	SerializeDos() : SerializeGlobalPOD("Dos") 
	{}

private:
	virtual void getBytes(std::ostream& stream)
	{
		SerializeGlobalPOD::getBytes(stream);

		//***********************************************
		//***********************************************
		//***********************************************
		// - pure data
		WRITE_POD( &dos_copybuf, dos_copybuf );

		// - pure data
		WRITE_POD( &dos.firstMCB, dos.firstMCB );
		WRITE_POD( &dos.errorcode, dos.errorcode );
		//WRITE_POD( &dos.env, dos.env );
		//WRITE_POD( &dos.cpmentry, dos.cpmentry );
		WRITE_POD( &dos.return_code, dos.return_code );
		WRITE_POD( &dos.return_mode, dos.return_mode );

		WRITE_POD( &dos.current_drive, dos.current_drive );
		WRITE_POD( &dos.verify, dos.verify );
		WRITE_POD( &dos.breakcheck, dos.breakcheck );
		WRITE_POD( &dos.echo, dos.echo );
		WRITE_POD( &dos.direct_output, dos.direct_output );
		WRITE_POD( &dos.internal_output, dos.internal_output );

		WRITE_POD( &dos.loaded_codepage, dos.loaded_codepage );
		WRITE_POD( &dos.version.major, dos.version.major );
		WRITE_POD( &dos.version.minor, dos.version.minor );
		WRITE_POD( &countryNo, countryNo );
		WRITE_POD( &uselfn, uselfn );
		WRITE_POD( &lfn_filefind_handle, lfn_filefind_handle );
		WRITE_POD( &bootdrive, bootdrive );
		WRITE_POD( &dos_kernel_disabled, dos_kernel_disabled );

		POD_Save_DOS_Devices(stream);
		POD_Save_DOS_DriveManager(stream);
		POD_Save_DOS_Files(stream);
		POD_Save_DOS_Memory(stream);
		POD_Save_DOS_Mscdex(stream);
		POD_Save_DOS_Tables(stream);
	}

	virtual void setBytes(std::istream& stream)
	{
		SerializeGlobalPOD::setBytes(stream);

		//***********************************************
		//***********************************************
		//***********************************************
		// - pure data
		READ_POD( &dos_copybuf, dos_copybuf );

		// - pure data
		READ_POD( &dos.firstMCB, dos.firstMCB );
		READ_POD( &dos.errorcode, dos.errorcode );
		//READ_POD( &dos.env, dos.env );
		//READ_POD( &dos.cpmentry, dos.cpmentry );
		READ_POD( &dos.return_code, dos.return_code );
		READ_POD( &dos.return_mode, dos.return_mode );

		READ_POD( &dos.current_drive, dos.current_drive );
		READ_POD( &dos.verify, dos.verify );
		READ_POD( &dos.breakcheck, dos.breakcheck );
		READ_POD( &dos.echo, dos.echo );
		READ_POD( &dos.direct_output, dos.direct_output );
        READ_POD( &dos.internal_output, dos.internal_output );
	
		READ_POD( &dos.loaded_codepage, dos.loaded_codepage );
		READ_POD( &dos.version.major, dos.version.major );
		READ_POD( &dos.version.minor, dos.version.minor );
		READ_POD( &countryNo, countryNo );
		READ_POD( &uselfn, uselfn );
		READ_POD( &lfn_filefind_handle, lfn_filefind_handle );
		READ_POD( &bootdrive, bootdrive );
        bool olddisable = dos_kernel_disabled;
		READ_POD( &dos_kernel_disabled, dos_kernel_disabled );
        if (!olddisable && dos_kernel_disabled) DispatchVMEvent(VM_EVENT_DOS_EXIT_KERNEL);

		POD_Load_DOS_Devices(stream);
		POD_Load_DOS_DriveManager(stream);
		POD_Load_DOS_Files(stream);
		POD_Load_DOS_Memory(stream);
		POD_Load_DOS_Mscdex(stream);
		POD_Load_DOS_Tables(stream);
	}
} dummy;
}
