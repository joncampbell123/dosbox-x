#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <cstring>
#include <fstream>
#include "SDL.h"
#include "menu.h"
#include "shell.h"
#include "cross.h"
#include "mapper.h"
#include "logging.h"
#include "build_timestamp.h"
#ifdef WIN32
#include "direct.h"
#endif
#if defined (__APPLE__)
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
#include "vs2015/zlib/contrib/minizip/zip.c"
#include "vs2015/zlib/contrib/minizip/unzip.c"
#include "vs2015/zlib/contrib/minizip/ioapi.c"
#if !defined(HX_DOS)
#include "../libs/tinyfiledialogs/tinyfiledialogs.h"
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
void GFX_LosingFocus(void), MAPPER_ReleaseAllKeys(void);
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
    GFX_LosingFocus();
    try
    {
        LOG_MSG("Loading state from slot: %d", (int)currentSlot + 1);
        SaveState::instance().load(currentSlot);
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

namespace Util {
std::string compress(const std::string& input) { //throw (SaveState::Error)
	if (input.empty())
		return input;

	const uLong bufferSize = ::compressBound((uLong)input.size());

	std::string output;
	output.resize(bufferSize);

	uLongf actualSize = bufferSize;
	if (::compress2(reinterpret_cast<Bytef*>(&output[0]), &actualSize,
					reinterpret_cast<const Bytef*>(input.c_str()), (uLong)input.size(), Z_BEST_SPEED) != Z_OK)
		throw SaveState::Error("Compression failed!");

	output.resize(actualSize);

	//save size of uncompressed data
	const size_t uncompressedSize = input.size(); //save size of uncompressed data
	output.resize(output.size() + sizeof(uncompressedSize)); //won't trigger a reallocation
	::memcpy(&output[0] + output.size() - sizeof(uncompressedSize), &uncompressedSize, sizeof(uncompressedSize));

	return std::string(&output[0], output.size()); //strip reserved space
}

std::string decompress(const std::string& input) { //throw (SaveState::Error)
	if (input.empty())
		return input;

	//retrieve size of uncompressed data
	size_t uncompressedSize = 0;
	::memcpy(&uncompressedSize, &input[0] + input.size() - sizeof(uncompressedSize), sizeof(uncompressedSize));

	std::string output;
	output.resize(uncompressedSize);

	uLongf actualSize = (uLongf)uncompressedSize;
	if (::uncompress(reinterpret_cast<Bytef*>(&output[0]), &actualSize,
					 reinterpret_cast<const Bytef*>(input.c_str()), (uLong)(input.size() - sizeof(uncompressedSize))) != Z_OK)
		throw SaveState::Error("Decompression failed!");

	output.resize(actualSize); //should be superfluous!

	return output;
}
}

inline void SaveState::RawBytes::set(const std::string& stream) {
	bytes = stream;
	isCompressed = false;
	dataExists   = true;
}

inline std::string SaveState::RawBytes::get() const { //throw (Error){
	if (isCompressed)
		(Util::decompress(bytes)).swap(bytes);
	isCompressed = false;
	return bytes;
}

inline void SaveState::RawBytes::compress() const { //throw (Error)
	if (!isCompressed)
		(Util::compress(bytes)).swap(bytes);
	isCompressed = true;
}

inline bool SaveState::RawBytes::dataAvailable() const {
	return dataExists;
}

#define CASESENSITIVITY (0)
#define MAXFILENAME (256)

int mymkdir(const char* dirname)
{
    int ret=0;
#ifdef _WIN32
    ret = _mkdir(dirname);
#elif unix
    ret = mkdir (dirname,0775);
#elif __APPLE__
    ret = mkdir (dirname,0775);
#endif
    return ret;
}

int makedir(const char *newdir)
{
  char *buffer ;
  char *p;
  int  len = (int)strlen(newdir);

  if (len <= 0)
    return 0;

  buffer = (char*)malloc(len+1);
        if (buffer==NULL)
        {
                printf("Error allocating memory\n");
                return UNZ_INTERNALERROR;
        }
  strcpy(buffer,newdir);

  if (buffer[len-1] == '/') {
    buffer[len-1] = '\0';
  }
  if (mymkdir(buffer) == 0)
    {
      free(buffer);
      return 1;
    }

  p = buffer+1;
  while (1)
    {
      char hold;

      while(*p && *p != '\\' && *p != '/')
        p++;
      hold = *p;
      *p = 0;
      if ((mymkdir(buffer) == -1) && (errno == ENOENT))
        {
          printf("couldn't create directory %s\n",buffer);
          free(buffer);
          return 0;
        }
      if (hold == 0)
        break;
      *p++ = hold;
    }
  free(buffer);
  return 1;
}

void change_file_date(const char *filename, uLong dosdate, tm_unz tmu_date)
{
    (void)dosdate;
#ifdef _WIN32
  HANDLE hFile;
  FILETIME ftm,ftLocal,ftCreate,ftLastAcc,ftLastWrite;

  hFile = CreateFileA(filename,GENERIC_READ | GENERIC_WRITE,
                      0,NULL,OPEN_EXISTING,0,NULL);
  GetFileTime(hFile,&ftCreate,&ftLastAcc,&ftLastWrite);
  DosDateTimeToFileTime((WORD)(dosdate>>16),(WORD)dosdate,&ftLocal);
  LocalFileTimeToFileTime(&ftLocal,&ftm);
  SetFileTime(hFile,&ftm,&ftLastAcc,&ftm);
  CloseHandle(hFile);
#else
#if defined(unix) || defined(__APPLE__)
  struct utimbuf ut;
  struct tm newdate;
  newdate.tm_sec = tmu_date.tm_sec;
  newdate.tm_min=tmu_date.tm_min;
  newdate.tm_hour=tmu_date.tm_hour;
  newdate.tm_mday=tmu_date.tm_mday;
  newdate.tm_mon=tmu_date.tm_mon;
  if (tmu_date.tm_year > 1900)
      newdate.tm_year=tmu_date.tm_year - 1900;
  else
      newdate.tm_year=tmu_date.tm_year ;
  newdate.tm_isdst=-1;

  ut.actime=ut.modtime=mktime(&newdate);
  utime(filename,&ut);
#endif
#endif
}

int do_extract_currentfile(unzFile uf, const int* popt_extract_without_path, int* popt_overwrite, const char* password, const char *savename=NULL)
{
    char filename_inzip[256];
    char* filename_withoutpath;
    char* p;
    int err=UNZ_OK;
    FILE *fout=NULL;
    void* buf;
    uInt size_buf;

    unz_file_info64 file_info;
    uLong ratio=0;
    err = unzGetCurrentFileInfo64(uf,&file_info,filename_inzip,sizeof(filename_inzip),NULL,0,NULL,0);

    (void)ratio;

    if (err!=UNZ_OK)
    {
        printf("error %d with zipfile in unzGetCurrentFileInfo\n",err);
        return err;
    }

    size_buf = 8192;
    buf = (void*)malloc(size_buf);
    if (buf==NULL)
    {
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
    }

    p = filename_withoutpath = filename_inzip;
    while ((*p) != '\0')
    {
        if (((*p)=='/') || ((*p)=='\\'))
            filename_withoutpath = p+1;
        p++;
    }

    if ((*filename_withoutpath)=='\0')
    {
        if ((*popt_extract_without_path)==0)
        {
            printf("creating directory: %s\n",filename_inzip);
            mymkdir(filename_inzip);
        }
    }
    else
    {
        const char* write_filename;
        int skip=0;

        if (savename!=NULL)
            write_filename = savename;
        else if ((*popt_extract_without_path)==0)
            write_filename = filename_inzip;
        else
            write_filename = filename_withoutpath;

        err = unzOpenCurrentFilePassword(uf,password);
        if (err!=UNZ_OK)
        {
            printf("error %d with zipfile in unzOpenCurrentFilePassword\n",err);
        }

        if (((*popt_overwrite)==0) && (err==UNZ_OK))
        {
            char rep=0;
            FILE* ftestexist;
            ftestexist = FOPEN_FUNC(write_filename,"rb");
            if (ftestexist!=NULL)
            {
                fclose(ftestexist);
                do
                {
                    char answer[128];
                    int ret;

                    printf("The file %s exists. Overwrite ? [y]es, [n]o, [A]ll: ",write_filename);
                    ret = scanf("%1s",answer);
                    if (ret != 1)
                    {
                       exit(EXIT_FAILURE);
                    }
                    rep = answer[0] ;
                    if ((rep>='a') && (rep<='z'))
                        rep -= 0x20;
                }
                while ((rep!='Y') && (rep!='N') && (rep!='A'));
            }

            if (rep == 'N')
                skip = 1;

            if (rep == 'A')
                *popt_overwrite=1;
        }

        if ((skip==0) && (err==UNZ_OK))
        {
            fout=FOPEN_FUNC(write_filename,"wb");
            /* some zipfile don't contain directory alone before file */
            if ((fout==NULL) && ((*popt_extract_without_path)==0) &&
                                (filename_withoutpath!=(char*)filename_inzip))
            {
                char c=*(filename_withoutpath-1);
                *(filename_withoutpath-1)='\0';
                makedir(write_filename);
                *(filename_withoutpath-1)=c;
                fout=FOPEN_FUNC(write_filename,"wb");
            }

            if (fout==NULL)
            {
                printf("error opening %s\n",write_filename);
            }
        }

        if (fout!=NULL)
        {
            printf(" extracting: %s\n",write_filename);

            do
            {
                err = unzReadCurrentFile(uf,buf,size_buf);
                if (err<0)
                {
                    printf("error %d with zipfile in unzReadCurrentFile\n",err);
                    break;
                }
                if (err>0)
                    if (fwrite(buf,err,1,fout)!=1)
                    {
                        printf("error in writing extracted file\n");
                        err=UNZ_ERRNO;
                        break;
                    }
            }
            while (err>0);
            if (fout)
                    fclose(fout);

            if (err==0)
                change_file_date(write_filename,file_info.dosDate,
                                 file_info.tmu_date);
        }

        if (err==UNZ_OK)
        {
            err = unzCloseCurrentFile (uf);
            if (err!=UNZ_OK)
            {
                printf("error %d with zipfile in unzCloseCurrentFile\n",err);
            }
        }
        else
            unzCloseCurrentFile(uf); /* don't lose the error */
    }

    free(buf);
    return err;
}

int do_extract(unzFile uf, int opt_extract_without_path, int opt_overwrite, const char* password)
{
    uLong i;
    unz_global_info64 gi;
    int err;
    FILE* fout=NULL;

    (void)fout;

    err = unzGetGlobalInfo64(uf,&gi);
    if (err!=UNZ_OK) {
        printf("error %d with zipfile in unzGetGlobalInfo \n",err);
        return 0;
    }

    for (i=0;i<gi.number_entry;i++)
    {
        if (do_extract_currentfile(uf,&opt_extract_without_path,
                                      &opt_overwrite,
                                      password) != UNZ_OK)
            break;

        if ((i+1)<gi.number_entry)
        {
            err = unzGoToNextFile(uf);
            if (err!=UNZ_OK)
            {
                printf("error %d with zipfile in unzGoToNextFile\n",err);
                break;
            }
        }
    }

    return 0;
}

int do_extract_onefile(unzFile uf, const char* filename, int opt_extract_without_path, int opt_overwrite, const char* password, const char *savename=NULL)
{
    int err = UNZ_OK;
    (void)err;
    if (unzLocateFile(uf,filename,CASESENSITIVITY)!=UNZ_OK)
    {
        printf("file %s not found in the zipfile\n",filename);
        return 2;
    }

    if (do_extract_currentfile(uf,&opt_extract_without_path,
                                      &opt_overwrite,
                                      password, savename) == UNZ_OK)
        return 0;
    else
        return 1;
}

int my_miniunz(char ** savefile, const char * savefile2, const char * savedir, char* savename = NULL) {
    const char *zipfilename=NULL;
    const char *filename_to_extract=NULL;
    const char *password=NULL;
    char filename_try[MAXFILENAME+16] = "";
    int ret_value=0;
    int opt_do_extract=1;
    int opt_do_extract_withoutpath=0;
    int opt_overwrite=0;
    int opt_extractdir=0;
    const char *dirname=NULL;
    unzFile uf=NULL;

		opt_do_extract = opt_do_extract_withoutpath = 1;
		opt_overwrite=1;
		opt_extractdir=1;
		dirname=savedir;
        zipfilename = (const char *)savefile;
        filename_to_extract = savefile2;

    if (zipfilename!=NULL)
    {

#        ifdef USEWIN32IOAPI
        zlib_filefunc64_def ffunc;
#        endif

        strncpy(filename_try, zipfilename,MAXFILENAME-1);
        /* strncpy doesnt append the trailing NULL, of the string is too long. */
        filename_try[ MAXFILENAME ] = '\0';

#        ifdef USEWIN32IOAPI
        fill_win32_filefunc64A(&ffunc);
        uf = unzOpen2_64(zipfilename,&ffunc);
#        else
        uf = unzOpen64(zipfilename);
#        endif
    }

    if (uf==NULL)
    {
        //printf("Cannot open %s\n",zipfilename,zipfilename);
        return 1;
    }
    //printf("%s opened\n",filename_try);

	if (opt_do_extract==1)
    {
		char cCurrentPath[FILENAME_MAX];
		char *ret=getcwd(cCurrentPath, sizeof(cCurrentPath));
        if (opt_extractdir && chdir(dirname))
        {
          printf("Error changing into %s, aborting\n", dirname);
          exit(-1);
        }

        if (filename_to_extract == NULL)
            ret_value = do_extract(uf, opt_do_extract_withoutpath, opt_overwrite, password);
        else
            ret_value = do_extract_onefile(uf, filename_to_extract, opt_do_extract_withoutpath, opt_overwrite, password, savename);
		if (ret!=NULL) chdir(cCurrentPath);
    }

    unzClose(uf);

    return ret_value;
}

#ifdef _WIN32
uLong filetime(char *f, tm_zip *tmzip, uLong *dt)
{
  int ret = 0;
  {
      FILETIME ftLocal;
      HANDLE hFind;
      WIN32_FIND_DATAA ff32;

      hFind = FindFirstFileA(f,&ff32);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
        FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
        FindClose(hFind);
        ret = 1;
      }
  }
  return ret;
}
#else
#if defined(unix) || defined(__APPLE__)
uLong filetime(char *f, tm_zip *tmzip, uLong *dt)
{
    (void)dt;
  int ret=0;
  struct stat s;        /* results of stat() */
  struct tm* filedate;
  time_t tm_t=0;

  if (strcmp(f,"-")!=0)
  {
    char name[MAXFILENAME+1];
    int len = strlen(f);
    if (len > MAXFILENAME)
      len = MAXFILENAME;

    strncpy(name, f,MAXFILENAME-1);
    /* strncpy doesnt append the trailing NULL, of the string is too long. */
    name[ MAXFILENAME ] = '\0';

    if (name[len - 1] == '/')
      name[len - 1] = '\0';
    /* not all systems allow stat'ing a file with / appended */
    if (stat(name,&s)==0)
    {
      tm_t = s.st_mtime;
      ret = 1;
    }
  }
  filedate = localtime(&tm_t);

  tmzip->tm_sec  = filedate->tm_sec;
  tmzip->tm_min  = filedate->tm_min;
  tmzip->tm_hour = filedate->tm_hour;
  tmzip->tm_mday = filedate->tm_mday;
  tmzip->tm_mon  = filedate->tm_mon ;
  tmzip->tm_year = filedate->tm_year;

  return ret;
}
#else
uLong filetime(char *f, tm_zip *tmzip, uLong *dt)
{
    return 0;
}
#endif
#endif

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

int getFileCrc(const char* filenameinzip,void*buf,unsigned long size_buf,unsigned long* result_crc)
{
    unsigned long calculate_crc=0;
    int err=ZIP_OK;
    FILE * fin = FOPEN_FUNC(filenameinzip,"rb");

    unsigned long size_read = 0;
    unsigned long total_read = 0;
    if (fin==NULL)
       err = ZIP_ERRNO;

    if (err == ZIP_OK)
        do
        {
            err = ZIP_OK;
            size_read = (int)fread(buf,1,size_buf,fin);
            if (size_read < size_buf)
                if (feof(fin)==0)
            {
                printf("error in reading %s\n",filenameinzip);
                err = ZIP_ERRNO;
            }

            if (size_read>0)
                calculate_crc = crc32(calculate_crc,(const Bytef*)buf,size_read);
            total_read += size_read;

        } while ((err == ZIP_OK) && (size_read>0));

    if (fin)
        fclose(fin);

    *result_crc=calculate_crc;
    printf("file %s crc %lx\n", filenameinzip, calculate_crc);
    return err;
}

int isLargeFile(const char* filename)
{
  int largeFile = 0;
  ZPOS64_T pos = 0;
  FILE* pFile = FOPEN_FUNC(filename, "rb");

  if(pFile != NULL)
  {
    int n = FSEEKO_FUNC(pFile, 0, SEEK_END);
    pos = FTELLO_FUNC(pFile);
    (void)n;

                printf("File : %s is %lld bytes\n", filename, pos);

    if(pos >= 0xffffffff)
     largeFile = 1;

                fclose(pFile);
  }

 return largeFile;
}

int my_minizip(char ** savefile, char ** savefile2, char* savename=NULL) {
    int opt_overwrite=0;
    int opt_compress_level=Z_DEFAULT_COMPRESSION;
    int opt_exclude_path=savename==NULL?1:0;
    int zipfilenamearg = 0;
    (void)zipfilenamearg;
    //char filename_try[MAXFILENAME16];
    int err=0;
    int size_buf=0;
    void* buf=NULL;
    const char* password=NULL;

	opt_overwrite = 2;
	opt_compress_level = 9;

    size_buf = 16384;
    buf = (void*)malloc(size_buf);
    if (buf==NULL)
    {
        //printf("Error allocating memory\n");
        return ZIP_INTERNALERROR;
    }

    {
        zipFile zf;
        int errclose;
#        ifdef USEWIN32IOAPI
        zlib_filefunc64_def ffunc;
        fill_win32_filefunc64A(&ffunc);
        zf = zipOpen2_64(savefile,(opt_overwrite==2) ? 2 : 0,NULL,&ffunc);
#        else
        zf = zipOpen64(savefile,(opt_overwrite==2) ? 2 : 0);
#        endif

        if (zf == NULL)
        {
            //printf("error opening %s\n",savefile);
            err= ZIP_ERRNO;
        }
        else
            //printf("creating %s\n",savefile);

            {
                FILE *fin = NULL;
                int size_read;
                char* filenameinzip = (char *)savefile2;
                const char *savefilenameinzip;
                zip_fileinfo zi;
                unsigned long crcFile=0;
                int zip64 = 0;

                zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
                zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
                zi.dosDate = 0;
                zi.internal_fa = 0;
                zi.external_fa = 0;
                filetime(filenameinzip,&zi.tmz_date,&zi.dosDate);

                if ((password != NULL) && (err==ZIP_OK))
                    err = getFileCrc(filenameinzip,buf,size_buf,&crcFile);

                zip64 = isLargeFile(filenameinzip);

                                                         /* The path name saved, should not include a leading slash. */
               /*if it did, windows/xp and dynazip couldn't read the zip file. */
                 savefilenameinzip = savename == NULL ? filenameinzip : savename;
                 while( savefilenameinzip[0] == '\\' || savefilenameinzip[0] == '/' )
                 {
                     savefilenameinzip++;
                 }

                 /*should the zip file contain any path at all?*/
                 if( opt_exclude_path )
                 {
                     const char *tmpptr;
                     const char *lastslash = 0;
                     for( tmpptr = savefilenameinzip; *tmpptr; tmpptr++)
                     {
                         if( *tmpptr == '\\' || *tmpptr == '/')
                         {
                             lastslash = tmpptr;
                         }
                     }
                     if( lastslash != NULL )
                     {
                         savefilenameinzip = lastslash+1; // base filename follows last slash.
                     }
                 }

                 /**/
                err = zipOpenNewFileInZip3_64(zf,savefilenameinzip,&zi,
                                 NULL,0,NULL,0,NULL /* comment*/,
                                 (opt_compress_level != 0) ? Z_DEFLATED : 0,
                                 opt_compress_level,0,
                                 /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
                                 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
                                 password,crcFile, zip64);

                if (err != ZIP_OK) {
                    //printf("error in opening %s in zipfile\n",filenameinzip);
				}
                else
                {
                    fin = fopen64(filenameinzip,"rb");
                    if (fin==NULL)
                    {
                        err=ZIP_ERRNO;
                        //printf("error in opening %s for reading\n",filenameinzip);
                    }
                }

                if (err == ZIP_OK)
                    do
                    {
                        err = ZIP_OK;
                        size_read = (int)fread(buf,1,size_buf,fin);
                        if (size_read < size_buf)
                            if (feof(fin)==0)
                        {
                            //printf("error in reading %s\n",filenameinzip);
                            err = ZIP_ERRNO;
                        }

                        if (size_read>0)
                        {
                            err = zipWriteInFileInZip (zf,buf,size_read);
                            if (err<0)
                            {
                                //printf("error in writing %s in the zipfile\n",
                                //                 filenameinzip);
                            }

                        }
                    } while ((err == ZIP_OK) && (size_read>0));

                if (fin)
                    fclose(fin);

                if (err<0)
                    err=ZIP_ERRNO;
                else
                {
                    err = zipCloseFileInZip(zf);
                    if (err!=ZIP_OK) {
                        //printf("error in closing %s in the zipfile\n",
                        //            filenameinzip);
					}
                }
            }
        errclose = zipClose(zf,NULL);
        if (errclose != ZIP_OK) {
            //printf("error in closing %s\n",savefile);
		}
    }

    free(buf);
    return 0;
}

int flagged_backup(char *zip);
int flagged_restore(char* zip);

void SaveState::save(size_t slot) { //throw (Error)
	if (slot >= SLOT_COUNT*MAX_PAGE)  return;
	SDL_PauseAudio(0);
	bool save_err=false;
	if((MEM_TotalPages()*4096/1024/1024)>1024) {
		LOG_MSG("Stopped. 1 GB is the maximum memory size for saving/loading states.");
		notifyError("Unsupported memory size for saving states.", false);
		return;
	}
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
        char *new_remark = tinyfd_inputBox("Save state", "Please enter remark for the state (optional; 30 characters maximum). Click the Cancel button to cancel the saving.", " ");
        if (new_remark==NULL) return;
        new_remark=trim(new_remark);
        if (strlen(new_remark)>30) new_remark[30]=0;
        save_remark = new_remark;
    }
#endif
	bool create_version=false;
	bool create_title=false;
	bool create_memorysize=false;
	bool create_machinetype=false;
	bool create_timestamp=false;
	bool create_saveremark=false;
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
	remove(save.c_str());
	std::ofstream file (save.c_str());
	file << "";
	file.close();
	try {
		for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
			std::ostringstream ss;
			i->second.comp.getBytes(ss);
			i->second.rawBytes[slot].set(ss.str());
			
			//LOG_MSG("Component is %s",i->first.c_str());

			if(!create_version) {
				std::string tempname = temp+"DOSBox-X_Version";
				std::ofstream emulatorversion (tempname.c_str(), std::ofstream::binary);
				emulatorversion << "DOSBox-X " << VERSION << " (" << SDL_STRING << ")" << std::endl << GetPlatform(true) << std::endl << UPDATED_STR;
				create_version=true;
				emulatorversion.close();
			}

			if(!create_title) {
				std::string tempname = temp+"Program_Name";
				std::ofstream programname (tempname.c_str(), std::ofstream::binary);
				programname << RunningProgram;
				create_title=true;
				programname.close();
			}

			if(!create_memorysize) {
				std::string tempname = temp+"Memory_Size";
				std::ofstream memorysize (tempname.c_str(), std::ofstream::binary);
				memorysize << MEM_TotalPages();
				create_memorysize=true;
				memorysize.close();
			}

			if(!create_machinetype) {
				std::string tempname = temp+"Machine_type";
				std::ofstream machinetype (tempname.c_str(), std::ofstream::binary);
				machinetype << getType();
				create_machinetype=true;
				machinetype.close();
			}

			if(!create_timestamp) {
				std::string tempname = temp+"Time_Stamp";
				std::ofstream timestamp (tempname.c_str(), std::ofstream::binary);
				timestamp << getTime(true);
				create_timestamp=true;
				timestamp.close();
			}

			if(!create_saveremark) {
				std::string tempname = temp+"Save_Remark";
				std::ofstream saveremark (tempname.c_str(), std::ofstream::binary);
				saveremark << std::string(save_remark);
				create_saveremark=true;
				saveremark.close();
			}

			std::string realtemp;
			realtemp = temp + i->first;
			std::ofstream outfile (realtemp.c_str(), std::ofstream::binary);
			outfile << (Util::compress(ss.str()));
			//compress all other saved states except position "slot"
			//const std::vector<RawBytes>& rb = i->second.rawBytes;
			//std::for_each(rb.begin(), rb.begin() + slot, std::mem_fun_ref(&RawBytes::compress));
			//std::for_each(rb.begin() + slot + 1, rb.end(), std::mem_fun_ref(&RawBytes::compress));
			outfile.close();
			ss.clear();
			if(outfile.fail()) {
				LOG_MSG("Save failed! - %s", realtemp.c_str());
				save_err=true;
				remove(save.c_str());
				goto delete_all;
			}
		}
	}
	catch (const std::bad_alloc&) {
		LOG_MSG("Save failed! Out of Memory!");
		save_err=true;
		remove(save.c_str());
		goto delete_all;
	}

	for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		my_minizip((char **)save.c_str(), (char **)save2.c_str());
	}
	save2=temp+"DOSBox-X_Version";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Program_Name";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Memory_Size";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Machine_Type";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Time_Stamp";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
	save2=temp+"Save_Remark";
	my_minizip((char **)save.c_str(), (char **)save2.c_str());
    if (!dos_kernel_disabled) flagged_backup((char *)save.c_str());

delete_all:
	for (CompEntry::iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		remove(save2.c_str());
	}
	save2=temp+"DOSBox-X_Version";
	remove(save2.c_str());
	save2=temp+"Program_Name";
	remove(save2.c_str());
	save2=temp+"Memory_Size";
	remove(save2.c_str());
	save2=temp+"Machine_Type";
	remove(save2.c_str());
	save2=temp+"Time_Stamp";
	remove(save2.c_str());
	save2=temp+"Save_Remark";
	remove(save2.c_str());
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
    confres=false;
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
    GUI_Shortcut(23+ind);
    MAPPER_ReleaseAllKeys();
    GFX_LosingFocus();
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
	SDL_PauseAudio(0);
	extern const char* RunningProgram;
	bool read_version=false;
	bool read_title=false;
	bool read_memorysize=false;
	bool read_machinetype=false;
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
	std::string save=use_save_file&&savefilename.size()?savefilename:temp+slotname.str()+".sav";
	std::ifstream check_slot;
	check_slot.open(save.c_str(), std::ifstream::in);
	if(check_slot.fail()) {
		LOG_MSG("No saved slot - %d (%s)",(int)slot+1,save.c_str());
		notifyError(use_save_file&&savefilename.size()?"The selected save file is currently empty.":"The selected save slot is an empty slot.", false);
		load_err=true;
		return;
	}

	for (CompEntry::const_iterator i = components.begin(); i != components.end(); ++i) {
		std::filebuf * fb;
		std::ifstream ss;
		std::ifstream check_file;
		fb = ss.rdbuf();
		
		//LOG_MSG("Component is %s",i->first.c_str());

		my_miniunz((char **)save.c_str(),i->first.c_str(),temp.c_str());

		if(!read_version) {
			my_miniunz((char **)save.c_str(),"DOSBox-X_Version",temp.c_str());
			std::ifstream check_version;
			int length = 8;

			std::string tempname = temp+"DOSBox-X_Version";
			check_version.open(tempname.c_str(), std::ifstream::in);
			if(check_version.fail()) {
				savestatecorrupt("DOSBox-X_Version");
				load_err=true;
				goto delete_all;
			}
			check_version.seekg (0, std::ios::end);
			length = (int)check_version.tellg();
			check_version.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_version.read (buffer, length);
			check_version.close();
			buffer[length]='\0';
			char *p=strrchr(buffer, '\n');
			if (p!=NULL) *p=0;
			std::string emulatorversion = std::string("DOSBox-X ") + VERSION + std::string(" (") + SDL_STRING + std::string(")\n") + GetPlatform(true);
			if (p==NULL||strcasecmp(buffer,emulatorversion.c_str())) {
				if(!force_load_state&&!loadstateconfirm(0)) {
					LOG_MSG("Aborted. Check your DOSBox-X version: %s",buffer);
					load_err=true;
					goto delete_all;
				}
			}
			read_version=true;
		}

		if(!read_title) {
			my_miniunz((char **)save.c_str(),"Program_Name",temp.c_str());
			std::ifstream check_title;
			int length = 8;

			std::string tempname = temp+"Program_Name";
			check_title.open(tempname.c_str(), std::ifstream::in);
			if(check_title.fail()) {
				savestatecorrupt("Program_Name");
				load_err=true;
				goto delete_all;
			}
			check_title.seekg (0, std::ios::end);
			length = (int)check_title.tellg();
			check_title.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_title.read (buffer, length);
			check_title.close();
			if (!length||(size_t)length!=strlen(RunningProgram)||strncmp(buffer,RunningProgram,length)) {
				if(!force_load_state&&!loadstateconfirm(1)) {
					buffer[length]='\0';
					LOG_MSG("Aborted. Check your program name: %s",buffer);
					load_err=true;
					goto delete_all;
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
			read_title=true;
		}

		if(!read_memorysize) {
			my_miniunz((char **)save.c_str(),"Memory_Size",temp.c_str());
			std::fstream check_memorysize;
			int length = 8;

			std::string tempname = temp+"Memory_Size";
			check_memorysize.open(tempname.c_str(), std::ifstream::in);
			if(check_memorysize.fail()) {
				savestatecorrupt("Memory_Size");
				load_err=true;
				goto delete_all;
			}
			check_memorysize.seekg (0, std::ios::end);
			length = (int)check_memorysize.tellg();
			check_memorysize.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_memorysize.read (buffer, length);
			check_memorysize.close();
			char str[10];
			itoa((int)MEM_TotalPages(), str, 10);
			if(!length||(size_t)length!=strlen(str)||strncmp(buffer,str,length)) {
				if(!force_load_state&&!loadstateconfirm(2)) {
					buffer[length]='\0';
					int size=atoi(buffer)*4096/1024/1024;
					LOG_MSG("Aborted. Check your memory size: %d MB", size);
					load_err=true;
					goto delete_all;
				}
			}
			read_memorysize=true;
		}

		if(!read_machinetype) {
			my_miniunz((char **)save.c_str(),"Machine_Type",temp.c_str());
			std::ifstream check_machinetype;
			int length = 8;

			std::string tempname = temp+"Machine_Type";
			check_machinetype.open(tempname.c_str(), std::ifstream::in);
			if(check_machinetype.fail()) {
				savestatecorrupt("Machine_Type");
				load_err=true;
				goto delete_all;
			}
			check_machinetype.seekg (0, std::ios::end);
			length = (int)check_machinetype.tellg();
			check_machinetype.seekg (0, std::ios::beg);

			char * const buffer = (char*)alloca( (length+1) * sizeof(char)); // char buffer[length];
			check_machinetype.read (buffer, length);
			check_machinetype.close();
			char str[20];
			strcpy(str, getType().c_str());
			if(!length||(size_t)length!=strlen(str)||strncmp(buffer,str,length)) {
				if(!force_load_state&&!loadstateconfirm(3)) {
					LOG_MSG("Aborted. Check your machine type: %s",buffer);
					load_err=true;
					goto delete_all;
				}
			}
			read_machinetype=true;
		}

		std::string realtemp;
		realtemp = temp + i->first;
		check_file.open(realtemp.c_str(), std::ifstream::in);
		check_file.close();
		if(check_file.fail()) {
			savestatecorrupt(i->first.c_str());
			load_err=true;
			goto delete_all;
		}

		clearline=true;
		fb->open(realtemp.c_str(),std::ios::in | std::ios::binary);
		std::string str((std::istreambuf_iterator<char>(ss)), std::istreambuf_iterator<char>());
		std::stringstream mystream;
		mystream << (Util::decompress(str));
		i->second.comp.setBytes(mystream);
		if (mystream.rdbuf()->in_avail() != 0 || mystream.eof()) { //basic consistency check
			savestatecorrupt(i->first.c_str());
			load_err=true;
			goto delete_all;
		}
		//compress all other saved states except position "slot"
		//const std::vector<RawBytes>& rb = i->second.rawBytes;
		//std::for_each(rb.begin(), rb.begin() + slot, std::mem_fun_ref(&RawBytes::compress));
		//std::for_each(rb.begin() + slot + 1, rb.end(), std::mem_fun_ref(&RawBytes::compress));
		fb->close();
		mystream.clear();
        if (!dos_kernel_disabled) flagged_restore((char *)save.c_str());
	}
delete_all:
	std::string save2;
	for (CompEntry::const_iterator i = components.begin(); i != components.end(); ++i) {
		save2=temp+i->first;
		remove(save2.c_str());
	}
	save2=temp+"DOSBox-X_Version";
	remove(save2.c_str());
	save2=temp+"Program_Name";
	remove(save2.c_str());
	save2=temp+"Memory_Size";
	remove(save2.c_str());
	save2=temp+"Machine_Type";
	remove(save2.c_str());
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
	my_miniunz((char **)save.c_str(),"Program_Name",temp.c_str());
	std::ifstream check_title;
	int length = 8;
	std::string tempname = temp+"Program_Name";
	check_title.open(tempname.c_str(), std::ifstream::in);
	if (check_title.fail()) {
		remove(tempname.c_str());
		return "";
	}
	check_title.seekg (0, std::ios::end);
	length = (int)check_title.tellg();
	check_title.seekg (0, std::ios::beg);
	char * const buffer1 = (char*)alloca( (length+1) * sizeof(char));
	check_title.read (buffer1, length);
	check_title.close();
	remove(tempname.c_str());
	buffer1[length]='\0';
    std::string ret=nl?"Program: "+(!strlen(buffer1)?"-":std::string(buffer1))+"\n":"[Program: "+std::string(buffer1)+"]";
	my_miniunz((char **)save.c_str(),"Time_Stamp",temp.c_str());
    length=18;
	tempname = temp+"Time_Stamp";
	check_title.open(tempname.c_str(), std::ifstream::in);
	if (check_title.fail()) {
		remove(tempname.c_str());
		return ret;
	}
	check_title.seekg (0, std::ios::end);
	length = (int)check_title.tellg();
	check_title.seekg (0, std::ios::beg);
	char * const buffer2 = (char*)alloca( (length+1) * sizeof(char));
	check_title.read (buffer2, length);
	check_title.close();
	remove(tempname.c_str());
	buffer2[length]='\0';
    if (strlen(buffer2)) ret+=nl?"Timestamp: "+(!strlen(buffer2)?"-":std::string(buffer2))+"\n":" ("+std::string(buffer2);
	my_miniunz((char **)save.c_str(),"Save_Remark",temp.c_str());
    length=30;
	tempname = temp+"Save_Remark";
	check_title.open(tempname.c_str(), std::ifstream::in);
	if (check_title.fail()) {
		remove(tempname.c_str());
		return ret+(!nl?")":"");
	}
	check_title.seekg (0, std::ios::end);
	length = (int)check_title.tellg();
	check_title.seekg (0, std::ios::beg);
	char * const buffer3 = (char*)alloca( (length+1) * sizeof(char));
	check_title.read (buffer3, length);
	check_title.close();
	remove(tempname.c_str());
	buffer3[length]='\0';
    if (strlen(buffer3)) ret+=nl?"Remark: "+(!strlen(buffer3)?"-":std::string(buffer3))+"\n":" - "+std::string(buffer3)+")";
    else if (!nl) ret+=")";
	return ret;
}
