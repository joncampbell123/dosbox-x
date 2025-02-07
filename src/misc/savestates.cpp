#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <assert.h>
#include <string>
#include <cstring>
#include <fstream>
#include "SDL.h"
#include "menu.h"
#include "shell.h"
#include "cross.h"
#include "render.h"
#include "mapper.h"
#include "control.h"
#include "logging.h"
#include "mixer.h"
#include "build_timestamp.h"
#ifdef WIN32
#include "direct.h"
#endif
#if defined (__OpenBSD__) || defined (__APPLE__)
#else
#include <malloc.h>
#endif
#if defined(unix) || defined(__APPLE__)
# include <utime.h>
#endif

#define MAXU32 0xffffffff
#include "zip.h"
#include "unzip.h"
#include "ioapi.h"
#include "vs/zlib/contrib/minizip/zip.c"
#include "vs/zlib/contrib/minizip/unzip.c"
#include "vs/zlib/contrib/minizip/ioapi.c"
#include "zipcppstdbuf.h"
#if !defined(HX_DOS)
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
#endif

#ifdef C_SDL2
extern SDL_AudioDeviceID SDL2_AudioDevice; /* valid IDs are 2 or higher, 1 for compat, 0 is never a valid ID */
#endif

extern unsigned int page;
extern int autosave_last[10], autosave_count;
extern std::string autosave_name[10], savefilename;
extern bool use_save_file, clearline, dos_kernel_disabled;
extern const char* RunningProgram;
bool auto_save_state=false;
bool noremark_save_state = false;
bool force_load_state = false;
std::string saveloaderr="";
void refresh_slots(void);
void GFX_LosingFocus(void), GFX_ReleaseMouse(void), MAPPER_ReleaseAllKeys(void), resetFontSize(void);
bool systemmessagebox(char const * aTitle, char const * aMessage, char const * aDialogType, char const * aIconType, int aDefaultButton);

namespace
{
	std::string getTime(bool date=false)
	{
		const time_t current = time(NULL);
		tm* timeinfo;
		timeinfo = localtime(&current); //convert to local time
		char buffer[80];
		if (date)
			::strftime(buffer, 80, "%Y-%m-%d %H:%M", timeinfo);
		else
			::strftime(buffer, 50, "%H:%M:%S", timeinfo);
		return buffer;
	}

	std::string getType() {
		switch (machine) {
			case MCH_HERC:
				return "MCH_HERC";
			case MCH_CGA:
				return "MCH_CGA";
			case MCH_TANDY:
				return "MCH_TANDY";
			case MCH_PCJR:
				return "MCH_PCJR";
			case MCH_EGA:
				return "MCH_EGA";
			case MCH_VGA:
				return "MCH_VGA";
			case MCH_AMSTRAD:
				return "MCH_AMSTRAD";
			case MCH_PC98:
				return "MCH_PC98";
			case MCH_FM_TOWNS:
				return "MCH_FM_TOWNS";
			case MCH_MCGA:
				return "MCH_MCGA";
			case MCH_MDA:
				return "MCH_MDA";
			default:
				return "MCH_OTHER";
		}
	}

	size_t GetGameState();

	class SlotPos
	{
		public:
			SlotPos() : slot(0) {}

			void next()
			{
				++slot;
				slot %= SaveState::SLOT_COUNT*SaveState::MAX_PAGE;
				if (page!=GetGameState()/SaveState::SLOT_COUNT) {
					page=(unsigned int)GetGameState()/SaveState::SLOT_COUNT;
					refresh_slots();
				}
			}

			void previous()
			{
				slot += SaveState::SLOT_COUNT*SaveState::MAX_PAGE - 1;
				slot %= SaveState::SLOT_COUNT*SaveState::MAX_PAGE;
				if (page!=GetGameState()/SaveState::SLOT_COUNT) {
					page=(unsigned int)GetGameState()/SaveState::SLOT_COUNT;
					refresh_slots();
				}
			}

			void set(int value)
			{
				slot = value;
			}

			operator size_t() const
			{
				return slot;
			}
		private:
			size_t slot;
	};

	SlotPos currentSlot;

	void notifyError(const std::string& message, bool log=true)
	{
		if (log) LOG_MSG("%s",message.c_str());
		systemmessagebox("Error",message.c_str(),"ok","error", 1);
	}

	size_t GetGameState(void) {
		return currentSlot;
	}

	void SetGameState(int value) {
		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.set(value);
		if (page!=currentSlot/SaveState::SLOT_COUNT) {
			page=(unsigned int)(currentSlot/SaveState::SLOT_COUNT);
			refresh_slots();
		}
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1,  emptySlot ? "[Empty]" : "");
	}

	void SaveGameState(bool pressed) {
		if (!pressed) return;

		try
		{
			LOG_MSG("Saving state to slot: %d", (int)currentSlot + 1);
			SaveState::instance().save(currentSlot);
			if (page!=GetGameState()/SaveState::SLOT_COUNT)
				SetGameState((int)currentSlot);
			else
				refresh_slots();
		}
		catch (const SaveState::Error& err)
		{
			notifyError(err);
		}
	}


	void LoadGameState(bool pressed) {
		if (!pressed) return;

		//    if (SaveState::instance().isEmpty(currentSlot))
		//    {
		//        LOG_MSG("[%s]: State %d is empty!", getTime().c_str(), currentSlot + 1);
		//        return;
		//    }
		if (!GFX_IsFullscreen()&&render.aspect) GFX_LosingFocus();
		try
		{
			LOG_MSG("Loading state from slot: %d", (int)currentSlot + 1);
			SaveState::instance().load(currentSlot);
#if defined(USE_TTF)
			if (ttf.inUse) resetFontSize();
#endif
		}
		catch (const SaveState::Error& err)
		{
			notifyError(err);
		}
	}

	void NextSaveSlot(bool pressed) {
		if (!pressed) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.next();
		if (currentSlot/SaveState::SLOT_COUNT==page) {
			name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}

	void PreviousSaveSlot(bool pressed) {
		if (!pressed) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.previous();
		if (currentSlot/SaveState::SLOT_COUNT==page) {
			name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
			mainMenu.get_item(name).check(true).refresh_item(mainMenu);
		}

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}

	void LastAutoSaveSlot(bool pressed) {
		if (!pressed) return;
		int index=0;
		for (int i=1; i<10&&i<=autosave_count; i++) if (autosave_name[i].size()&&!strcasecmp(RunningProgram, autosave_name[i].c_str())) index=i;
		if (autosave_last[index]<1) return;

		char name[6]="slot0";
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(false).refresh_item(mainMenu);
		currentSlot.set(autosave_last[index]-1);
		if (page!=currentSlot/SaveState::SLOT_COUNT) {
			page=(unsigned int)(currentSlot/SaveState::SLOT_COUNT);
			refresh_slots();
		}
		name[4]='0'+(char)(currentSlot%SaveState::SLOT_COUNT);
		mainMenu.get_item(name).check(true).refresh_item(mainMenu);

		const bool emptySlot = SaveState::instance().isEmpty(currentSlot);
		LOG_MSG("Active save slot: %d %s", (int)currentSlot + 1, emptySlot ? "[Empty]" : "");
	}
}

std::string GetPlatform(bool save) {
	char platform[30];
	strcpy(platform, 
#if defined(HX_DOS)
			"DOS "
#elif defined(__MINGW32__)
			"MinGW "
#elif defined(WIN32)
			"Windows "
#elif defined(LINUX)
			"Linux "
#elif unix
			"Unix "
#elif defined(MACOSX)
			"macOS "
#elif defined(OS2)
			"OS/2 "
#else
			save?"Other ":""
#endif
	      );
	if (!save) strcat(platform, (std::string(SDL_STRING)+", ").c_str());
#if defined(_M_X64) || defined (_M_AMD64) || defined (_M_ARM64) || defined (_M_IA64) || defined(__ia64__) || defined(__LP64__) || defined(_WIN64) || defined(__x86_64__) || defined(__aarch64__) || defined(__powerpc64__)
	strcat(platform, "64");
#else
	strcat(platform, "32");
#endif
	strcat(platform, save?"-bit build":"-bit");
	return std::string(platform);
}

size_t GetGameState_Run(void) { return GetGameState(); }
void SetGameState_Run(int value) { SetGameState(value); }
void SaveGameState_Run(void) { SaveGameState(true); }
void LoadGameState_Run(void) { LoadGameState(true); }
void NextSaveSlot_Run(void) { NextSaveSlot(true); }
void PreviousSaveSlot_Run(void) { PreviousSaveSlot(true); }
void LastAutoSaveSlot_Run(void) { LastAutoSaveSlot(true); }

void ShowStateInfo(bool pressed) {
	if (!pressed) return;
	std::string message = "Save to: "+(use_save_file&&savefilename.size()?"File "+savefilename:"Slot "+std::to_string(GetGameState_Run()+1))+"\n"+SaveState::instance().getName(GetGameState_Run(), true);
	systemmessagebox("Saved state information", message.c_str(), "ok","info", 1);
}

void AddSaveStateMapper() {
	DOSBoxMenu::item *item;
	MAPPER_AddHandler(SaveGameState, MK_s, MMODHOST,"savestate","Save state", &item);
	item->set_text("Save state");
	MAPPER_AddHandler(LoadGameState, MK_l, MMODHOST,"loadstate","Load state", &item);
	item->set_text("Load state");
	MAPPER_AddHandler(ShowStateInfo, MK_nothing, 0,"showstate","Display state info", &item);
	item->set_text("Display state information");
	MAPPER_AddHandler(PreviousSaveSlot, MK_comma, MMODHOST,"prevslot","Previous save slot", &item);
	item->set_text("Select previous slot");
	MAPPER_AddHandler(NextSaveSlot, MK_period, MMODHOST,"nextslot","Next save slot", &item);
	item->set_text("Select next slot");
}

#ifndef WIN32
char* itoa(int value, char* str, int radix) {
	/**
	 * C++ version 0.4 char* style "itoa":
	 * Written by LukÃ¡s Chmela
	 * Released under GPLv3.
	 */
	// check that the radix if valid
	if (radix < 2 || radix > 36) { *str = '\0'; return str; }

	char* ptr = str, *ptr1 = str, tmp_char;
	int tmp_value;

	do {
		tmp_value = value;
		value /= radix;
		*ptr++ = "zyxwvutsrqponmlkjihgfedcba9876543210123456789abcdefghijklmnopqrstuvwxyz" [35 + (tmp_value - value * radix)];
	} while ( value );

	// Apply negative sign
	if (tmp_value < 0) *ptr++ = '-';
	*ptr-- = '\0';
	while(ptr1 < ptr) {
		tmp_char = *ptr;
		*ptr--= *ptr1;
		*ptr1++ = tmp_char;
	}
	return str;
}
#endif

SaveState& SaveState::instance() {
	static SaveState singleton;
	return singleton;
}

void SaveState::registerComponent(const std::string& uniqueName, Component& comp) {
	components.insert(std::make_pair(uniqueName, CompData(comp)));
}

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)

void zipSetCurrentTime(zip_fileinfo &zi) {
	zi.dosDate = 0;
	zi.internal_fa = 0;
	zi.external_fa = 0;
	zi.tmz_date.tm_sec = 0;
	zi.tmz_date.tm_min = 0;
	zi.tmz_date.tm_hour = 0;
	zi.tmz_date.tm_mday = 0;
	zi.tmz_date.tm_mon = 0;
	zi.tmz_date.tm_year = 0;

	time_t tm_t = time(NULL);
	struct tm* filedate = localtime(&tm_t);
	if (filedate != NULL) {
		zi.tmz_date.tm_sec  = filedate->tm_sec;
		zi.tmz_date.tm_min  = filedate->tm_min;
		zi.tmz_date.tm_hour = filedate->tm_hour;
		zi.tmz_date.tm_mday = filedate->tm_mday;
		zi.tmz_date.tm_mon  = filedate->tm_mon;
		zi.tmz_date.tm_year = filedate->tm_year;
	}
}

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif

int flagged_backup(char *zip);
int flagged_restore(char* zip);

int zipOutOpenFile(zipFile zf,const char *zfname,zip_fileinfo &zi,const bool compress) {
	const int opt_compress_level = compress ? 9 : 0;

	return zipOpenNewFileInZip3_64(zf,zfname,&zi,
		NULL,0,NULL,0,NULL/* comment*/,
		(opt_compress_level != 0) ? Z_DEFLATED : 0,
		opt_compress_level,0,
		-MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
		NULL/*password*/,0/*crcFile*/,1/*zip64*/);
}

void SaveState::save(size_t slot) { //throw (Error)
	if (slot >= SLOT_COUNT*MAX_PAGE)  return;
#ifdef C_SDL2
        SDL_PauseAudioDevice(SDL2_AudioDevice, 0);
#else
        SDL_PauseAudio(0);
#endif
	bool save_err=false;
	if((MEM_TotalPages()*4096/1024/1024)>1024) {
		LOG_MSG("Stopped. 1 GB is the maximum memory size for saving/loading states.");
		notifyError("Unsupported memory size for saving states.", false);
		return;
	}
	bool compresssaveparts = static_cast<Section_prop *>(control->GetSection("dosbox"))->Get_bool("compresssaveparts");
	const char *save_remark = "";
#if !defined(HX_DOS)
	if (auto_save_state)
		save_remark = "Auto-save";
	else if (!noremark_save_state) {
		/* NTS: tinyfd_inputBox() returns a string from an internal statically declared char array.
		 *      It is not necessary to free the return string, but it is important to understand that
		 *      the next call to tinyfd_inputBox() will obliterate the previously returned string.
		 *      See src/libs/tinyfiledialogs/tinyfiledialogs.c line 5069 --J.C. */
		/* NTS: The code was originally written to declare save_remark as char* default assigned to string
		 *      constant "", but GCC (rightfully so) complains you're pointing char* at something that
		 *      is stored const by the compiler. "save_remark" is not modified past this point, so it
		 *      has been changed to const char* and the return value of tinyfd_inputBox() is given to
		 *      a local temporary char* string where the modification can be made, and *then* assigned
		 *      to the const char* string for the rest of this function. */
		bool fs=GFX_IsFullscreen();
		if (fs) GFX_SwitchFullScreen();
		MAPPER_ReleaseAllKeys();
		GFX_LosingFocus();
		GFX_ReleaseMouse();
		char *new_remark = tinyfd_inputBox("Save state", "Please enter remark for the state (optional; 30 characters maximum). Click the Cancel button to cancel the saving.", " ");
		MAPPER_ReleaseAllKeys();
		GFX_LosingFocus();
		if (fs&&!GFX_IsFullscreen()) GFX_SwitchFullScreen();
		if (new_remark==NULL) return;
		new_remark=trim(new_remark);
		if (strlen(new_remark)>30) new_remark[30]=0;
		save_remark = new_remark;
	}
#endif
	int errclose;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path+=CROSS_FILESPLIT;
		path+="save";
		Cross::CreateDir(path);
		path+=CROSS_FILESPLIT;
	}

	std::string temp, save2;
	std::stringstream slotname;
	slotname << slot+1;
	temp=path;
	std::string save=use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav";

	zipFile zf;
	{
		const char *global_comment = "DOSBox-X save state";
		zlib_filefunc64_def ffunc;
#ifdef USEWIN32IOAPI
		fill_win32_filefunc64A(&ffunc);
#else
		fill_fopen64_filefunc(&ffunc);
#endif
		remove(save.c_str());
		zf = zipOpen2_64(save.c_str(),APPEND_STATUS_CREATE,&global_comment,&ffunc);
	}
	if (zf == NULL) { save_err = true; goto done; }

	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"DOSBox-X_Version",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream emulatorversion(&zos);

		emulatorversion << "DOSBox-X " << VERSION << " (" << SDL_STRING << ")" << std::endl << GetPlatform(true) << std::endl << UPDATED_STR;
		/* 2025/01/12: Backwards compat: The old code compressed data to zlib, even though the ZIP support code
		 *             already applies compression. This is to tell the old code that we did not compress the
		 *             data (the ZIP support code did though). */
		emulatorversion << std::endl << "No compression";

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Program_Name",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream programname(&zos);

		programname << RunningProgram;

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Memory_Size",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream memorysize(&zos);

		memorysize << MEM_TotalPages();

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Machine_Type",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream machinetype(&zos);

		machinetype << getType();

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Time_Stamp",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream timestamp(&zos);

		timestamp << getTime(true);

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	{
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,"Save_Remark",zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream saveremark(&zos);

		saveremark << std::string(save_remark);

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}
	for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
		zip_fileinfo zi; zipSetCurrentTime(zi);
		if ((errclose=zipOutOpenFile(zf,i->first.c_str(),zi,compresssaveparts)) != ZIP_OK) { save_err = true; goto done; }
		zip_ostreambuf zos(zf); std::ostream ss(&zos);

		i->second.comp.getBytes(ss);

		if ((errclose=zos.close()) != ZIP_OK) { save_err = true; goto done; }
	}

done:
	if (zf != NULL) {
		errclose = zipClose(zf,NULL);
		if (errclose != ZIP_OK) save_err = true;
	}

	if (!dos_kernel_disabled) flagged_backup((char *)save.c_str());

	if (save_err)
		notifyError("Failed to save the current state.");
	else
		LOG_MSG("[%s]: Saved. (Slot %d)", getTime().c_str(), (int)slot+1);
}

void savestatecorrupt(const char* part) {
	LOG_MSG("Save state corrupted! Program in inconsistent state! - %s", part);
	systemmessagebox("Error","Save state corrupted! Program may not work.","ok","error", 1);
}

bool confres=false;
bool loadstateconfirm(int ind) {
	if (ind<0||ind>4) return false;
	confres=true;
	GUI_Shortcut(23+ind);
	bool ret=confres;
	confres=false;
	return ret;
}

void SaveState::load(size_t slot) const { //throw (Error)
	//	if (isEmpty(slot)) return;
	bool load_err=false;
	if((MEM_TotalPages()*4096/1024/1024)>1024) {
		LOG_MSG("Stopped. 1 GB is the maximum memory size for saving/loading states.");
		notifyError("Unsupported memory size for loading states.", false);
		return;
	}
#ifdef C_SDL2
        SDL_PauseAudioDevice(SDL2_AudioDevice, 0);
#else
        SDL_PauseAudio(0);
#endif
	extern const char* RunningProgram;
	std::string path;
	int err;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if(check_slot.fail()) {
		LOG_MSG("No saved slot - %d (%s)",(int)slot+1,save.c_str());
		notifyError(use_save_file&&savefilename.size()?"The selected save file is currently empty.":"The selected save slot is an empty slot.", false);
		load_err=true;
		return;
	}
	check_slot.close();

	unz_file_info64 file_info;
	unzFile zf;
	{
		zlib_filefunc64_def ffunc;
#ifdef USEWIN32IOAPI
		fill_win32_filefunc64A(&ffunc);
#else
		fill_fopen64_filefunc(&ffunc);
#endif
		zf = unzOpen2_64(save.c_str(),&ffunc);
	}
	if (zf == NULL) { load_err=true; goto done; }

	{
		if ((err=unzLocateFile(zf,"DOSBox-X_Version",1/*case sensitive*/)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzOpenCurrentFile(zf)) != UNZ_OK) { load_err=true; goto done; }
		zip_istreambuf zis(zf);

		char buffer[4096];
		std::streamsize sz = zis.xsgetn((zip_istreambuf::char_type*)buffer,sizeof(buffer)-1); buffer[sz] = 0;

		char *p;
		if (strstr(buffer, "\nNo compression") != NULL) {
			/* 2025/01/12: Removal of this string is required to match version string below even if now ignored */
			p=strrchr(buffer, '\n');
			if (p!=NULL) *p=0;
		}
		p=strrchr(buffer, '\n'); /* Remove i.e. "Linux" */
		if (p!=NULL) *p=0;
		std::string emulatorversion = std::string("DOSBox-X ") + VERSION + std::string(" (") + SDL_STRING + std::string(")");
		if (strcasecmp(buffer,emulatorversion.c_str())) {
			if(!force_load_state&&!loadstateconfirm(0)) {
				LOG_MSG("Aborted. Check your DOSBox-X version: %s",buffer);
				load_err=true;
				goto done;
			}
		}

		if ((err=zis.close()) != ZIP_OK) { load_err=true; goto done; }
	}

	{
		if ((err=unzLocateFile(zf,"Program_Name",1/*case sensitive*/)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzOpenCurrentFile(zf)) != UNZ_OK) { load_err=true; goto done; }
		zip_istreambuf zis(zf);

		char buffer[4096];
		size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer,sizeof(buffer)-1); buffer[length] = 0;

		if (!length||(size_t)length!=strlen(RunningProgram)||strncmp(buffer,RunningProgram,length)) {
			if(!force_load_state&&!loadstateconfirm(1)) {
				buffer[length]='\0';
				LOG_MSG("Aborted. Check your program name: %s",buffer);
				load_err=true;
				goto done;
			}
			if (length<9) {
				static char pname[9];
				if (length) {
					strncpy(pname,buffer,length);
					pname[length]=0;
				} else
					strcpy(pname, "DOSBOX-X");
				RunningProgram=pname;
				void GFX_SetTitle(int32_t cycles, int frameskip, Bits timing, bool paused);
				GFX_SetTitle(-1,-1,-1,false);
			}
		}

		if ((err=zis.close()) != ZIP_OK) { load_err=true; goto done; }
	}

	{
		if ((err=unzLocateFile(zf,"Memory_Size",1/*case sensitive*/)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzOpenCurrentFile(zf)) != UNZ_OK) { load_err=true; goto done; }
		zip_istreambuf zis(zf);

		char buffer[4096];
		size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer,sizeof(buffer)-1); buffer[length] = 0;

		char str[10];
		itoa((int)MEM_TotalPages(), str, 10);
		if(!length||(size_t)length!=strlen(str)||strncmp(buffer,str,length)) {
			if(!force_load_state&&!loadstateconfirm(2)) {
				buffer[length]='\0';
				int size=atoi(buffer)*4096/1024/1024;
				LOG_MSG("Aborted. Check your memory size: %d MB", size);
				load_err=true;
				goto done;
			}
		}

		if ((err=zis.close()) != ZIP_OK) { load_err=true; goto done; }
	}

	{
		if ((err=unzLocateFile(zf,"Machine_Type",1/*case sensitive*/)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzOpenCurrentFile(zf)) != UNZ_OK) { load_err=true; goto done; }
		zip_istreambuf zis(zf);

		char buffer[4096];
		size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer,sizeof(buffer)-1); buffer[length] = 0;

		char str[20];
		strcpy(str, getType().c_str());
		if(!length||(size_t)length!=strlen(str)||strncmp(buffer,str,length)) {
			if(!force_load_state&&!loadstateconfirm(3)) {
				LOG_MSG("Aborted. Check your machine type: %s",buffer);
				load_err=true;
				goto done;
			}
		}

		if ((err=zis.close()) != ZIP_OK) { load_err=true; goto done; }
	}

	for (CompEntry::const_iterator i = components.begin(); i != components.end(); ++i) {
		if ((err=unzLocateFile(zf,i->first.c_str(),1/*case sensitive*/)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) != UNZ_OK) { load_err=true; goto done; }
		if ((err=unzOpenCurrentFile(zf)) != UNZ_OK) { load_err=true; goto done; }
		zip_istreambuf zis(zf); std::istream ss(&zis);

		i->second.comp.setBytes(ss);

		if ((err=zis.close()) != ZIP_OK) { load_err=true; goto done; }
	}

done:
	if (zf != NULL) {
		err = unzClose(zf);
		if (err != UNZ_OK) load_err = true;
	}

	if (!dos_kernel_disabled) flagged_restore((char *)save.c_str());
	if (!load_err) LOG_MSG("[%s]: Loaded. (Slot %d)", getTime().c_str(), (int)slot+1);
}

bool SaveState::isEmpty(size_t slot) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return true;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	return check_slot.fail();
}

void SaveState::removeState(size_t slot) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return;
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if(check_slot.fail()) {
		LOG_MSG("No saved slot - %d (%s)",(int)slot+1,save.c_str());
		notifyError("The selected save slot is an empty slot.", false);
		return;
	}
	if (loadstateconfirm(4)) {
		check_slot.close();
		remove(save.c_str());
		check_slot.open(save.c_str(), std::ifstream::in);
		if (!check_slot.fail()) notifyError("Failed to remove the state in the save slot.");
		if (page!=GetGameState()/SaveState::SLOT_COUNT)
			SetGameState((int)slot);
		else
			refresh_slots();
	}
}

std::string SaveState::getName(size_t slot, bool nl) const {
	if (slot >= SLOT_COUNT*MAX_PAGE) return "["+std::string(MSG_Get("EMPTY_SLOT"))+"]";
	std::string path;
	bool Get_Custom_SaveDir(std::string& savedir);
	if(Get_Custom_SaveDir(path)) {
		path+=CROSS_FILESPLIT;
	} else {
		extern std::string capturedir;
		const size_t last_slash_idx = capturedir.find_last_of("\\/");
		if (std::string::npos != last_slash_idx) {
			path = capturedir.substr(0, last_slash_idx);
		} else {
			path = ".";
		}
		path += CROSS_FILESPLIT;
		path +="save";
		path += CROSS_FILESPLIT;
	}
	std::string temp;
	temp = path;
	std::stringstream slotname;
	slotname << slot+1;
	std::string save=nl&&use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if (check_slot.fail()) return nl?"(Empty state)":"["+std::string(MSG_Get("EMPTY_SLOT"))+"]";
	check_slot.close();

	unzFile zf;
	{
		zlib_filefunc64_def ffunc;
#ifdef USEWIN32IOAPI
		fill_win32_filefunc64A(&ffunc);
#else
		fill_fopen64_filefunc(&ffunc);
#endif
		zf = unzOpen2_64(save.c_str(),&ffunc);
	}
	if (zf == NULL) return "(Error slot)";

	int err;
	std::string ret;
	char buffer1[4096];
	unz_file_info64 file_info;

	if (	(err=unzLocateFile(zf,"Program_Name",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			ret += nl?"Program: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":"[Program: "+std::string(buffer1)+"]";
		}
	}

	if (	(err=unzLocateFile(zf,"Time_Stamp",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			ret += nl?"Timestamp: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":" ("+std::string(buffer1)+")";
		}
	}

	if (	(err=unzLocateFile(zf,"Save_Remark",1/*case sensitive*/)) == UNZ_OK &&
		(err=unzGetCurrentFileInfo64(zf,&file_info,NULL,0,NULL,0,NULL,0)) == UNZ_OK) {
		if ((err=unzOpenCurrentFile(zf)) == UNZ_OK) {
			zip_istreambuf zis(zf);
			size_t length = (size_t)zis.xsgetn((zip_istreambuf::char_type*)buffer1,sizeof(buffer1)-1); buffer1[length] = 0;
			zis.close();
			if (length != 0) ret += nl?"Remark: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":" - "+std::string(buffer1);
		}
	}

	return ret;
}
