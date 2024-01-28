#include <stdio.h>
#include <string.h>
#include <io.h>

struct DiskInfo {
    /* buffer to be filled by INT 21h function 0x7303h */
    unsigned short size;
    unsigned short version;
    unsigned long sectorsPerCluster;
    unsigned long bytesPerSector;
    unsigned long availableClusters;
    unsigned long totalClusters;
    unsigned long physicalSectorsAvailable;
    unsigned long totalPhysicalSectors;
    unsigned long availableAllocationUnits;
    unsigned long totalAllocationUnits;
    char reserved[8];
};

#ifdef __WATCOMC__
#include <direct.h> /* below: Turbo C dir.h values as comments */
#define MAXPATH  _MAX_PATH  /* 80 */
#define MAXDRIVE _MAX_DRIVE /*  3 */
#define MAXDIR   _MAX_DIR   /* 66 */
#define MAXFILE  _MAX_FNAME /*  9 */
#define MAXEXT   _MAX_EXT   /*  5 */

/* following are defined in <dos.h> for Borland compilers, so alias to OW defines */
/* no alternate for _A_NORMAL */
#define FA_RDONLY _A_RDONLY
#define FA_HIDDEN _A_HIDDEN
#define FA_SYSTEM _A_SYSTEM
#define FA_LABEL  _A_VOLID
#define FA_DIREC  _A_SUBDIR
#define FA_ARCH   _A_ARCH

struct ffblk {
        unsigned short cr_time; /* time of file creation      */
        unsigned short cr_date; /* date of file creation      */
        unsigned short ac_time; /* time of last file access   */
        unsigned short ac_date; /* date of last file access   */
        unsigned short lfnax;   /* DOS LFN search handle      */
        unsigned char  lfnsup;  /* DOS LFN support status     */
        char ff_reserved[10];   /* reserved for use by DOS    */
        char ff_attrib;         /* attribute byte for file    */
        unsigned short ff_ftime;/* time of last write to file */
        unsigned short ff_date; /* date of last write to file */
        unsigned long  ff_fsize;/* length of file in bytes    */
#if defined(__OS2__) || defined(__NT__) || defined(__WATCOM_LFN__)
        char ff_name[256];      /* null-terminated filename   */
#else
        char ff_name[13];       /* null-terminated filename   */
#endif
};

int findfirst(char *pathname, struct ffblk *fblock, int attrib) {
  return((int)_dos_findfirst(pathname,( unsigned )attrib,(struct find_t *)fblock));
}
int findnext(struct ffblk *fblock) {
  return((int)_dos_findnext((struct find_t *)fblock));
}
int findclose(struct ffblk *fblock) {
    return((int)_dos_findclose((struct find_t *)fblock));
}

int getverify(void) {
  return(bdos(0x54,0,0) & 0xff);
}
void setverify(int value) {
  bdos( 0x2e,0,value);
}


struct ftime {
    unsigned ft_tsec  : 5;  /* Two seconds */
    unsigned ft_min   : 6;  /* Minutes */
    unsigned ft_hour  : 5;  /* Hours */
    unsigned ft_day   : 5;  /* Days */
    unsigned ft_month : 4;  /* Months */
    unsigned ft_year  : 7;  /* Year - 1980 */
};
/* bc_time only used to allow get/set ftime to work, ie don't manually set */
union borftime {
    struct {
        unsigned time;
        unsigned date;
    } msc_time;
    struct ftime bc_time;
};
int getftime(int handle, struct ftime *ftimep)
{
  union borftime ftm;
  
  if( _dos_getftime(handle,
                    (unsigned *)&ftm.msc_time.date,
                    (unsigned *)&ftm.msc_time.time 
					) == 0 )
  {
    *ftimep = ftm.bc_time;
	return 0;
  } /* else */
  return -1;
}
int setftime(int handle, struct ftime *ftimep)
{
  union borftime ftm;
  ftm.bc_time = *ftimep;
  return((_dos_setftime(handle,ftm.msc_time.date,ftm.msc_time.time) == 0) ? 0 : -1);
}


#else
#include <dir.h>
#endif

#ifdef __WATCOMC__ /* "dosdate_t" instead of Borland style "date" ... */
#include <time.h>
#define THEDATE struct dosdate_t /* in dos.h */
#define da_year year
#define da_mon month
#define da_day day
#define ti_hour hour
#define ti_minute minute
#define ti_sec second
#define ti_hund hsecond

time_t dostounix(THEDATE *date, struct dostime_t *time)
{
    struct tm unixtime;

    unixtime.tm_sec = time->ti_sec;
    unixtime.tm_min = time->ti_minute;
    unixtime.tm_hour = time->ti_hour;
    unixtime.tm_mday = date->da_day;
    unixtime.tm_mon = date->da_mon - 1;
    unixtime.tm_year = date->da_year - 1900;
    unixtime.tm_isdst = 0;

    return(mktime(&unixtime));
}
#else
#define THEDATE struct date
#endif

/*-------------------------------------------------------------------------*/
/* COMPILER SPECIFICA                                                      */
/*-------------------------------------------------------------------------*/
#if __DJGPP__
  #include <unistd.h>

  /* disable DJGPP filename globbing, so wildcards are handled the DOS-way */
  char **__crt0_glob_function(char *arg) {
    return 0;
  }

  /* convert DJGPP's _fixpath function into Borland C's _fullpath */
  #define _fullpath(buffer, path, buflen) _fixpath(path, buffer)

  /* directory separator is the UNIX-slash */
  #define DIR_SEPARATOR "/"
#else
  /* directory separator is the DOS-backslash */
  #define DIR_SEPARATOR "\\"

  /* constants for "access" function */
  /* (equal with DJGPP but with different values!) */
  #define F_OK 0  /* exists */
  #define R_OK 4  /* readable */
  #define W_OK 2  /* writeable */
  #define X_OK 1  /* executable */
#endif


#ifdef __WATCOMC__
/* WARNING this will override default implementation and signature */
#define _chmod _dos_chmod

/*-------------------------------------------------------------------------*/
/* Works like function chmod/_chmod from Borland                           */
/* if mode is 0 then returns current file attributes (readonly/hidden/...) */
/* if mode is 1 then sets current file attributes                          */
/* returns -1 on error, otherwise returns current file attributes          */
/*-------------------------------------------------------------------------*/
#include <stdarg.h>
int _dos_chmod(const char *path, int mode, ... /* int attrib */)
{
  unsigned m_attrib;

  if (mode) {
    va_list valist;
    va_start(valist, mode);
    m_attrib = va_arg(valist, unsigned);
    va_end(valist);

    if (_dos_setfileattr(path, m_attrib)) {
	  /* set errno properly */
	  return -1;
	}
  } else {
    if (_dos_getfileattr(path, &m_attrib)) {
	  /* set errno properly */
	  return -1;
	}
  }
  return m_attrib;
}
#endif


#ifdef __WATCOMC__
/*-------------------------------------------------------------------------*/
/* Borland compatible get disk free space function.                        */
/*-------------------------------------------------------------------------*/
struct dfree {
    unsigned df_avail;  /* Available clusters */
    unsigned df_total;  /* Total clusters */
    unsigned df_bsec;   /* Bytes per sector */
    unsigned df_sclus;  /* Sectors per cluster */
};

void getdfree(unsigned char drive, struct dfree *dtable)
{
    struct diskfree_t disk;
    
    if(_dos_getdiskfree(drive, &disk) != 0) {
        dtable->df_sclus = 0xFFFF;
        return;
    }
    dtable->df_avail = disk.avail_clusters;
    dtable->df_total = disk.total_clusters;
    dtable->df_bsec = disk.bytes_per_sector;
    dtable->df_sclus = disk.sectors_per_cluster;
}
#endif


/*-------------------------------------------------------------------------*/
/* Works like function strcpy() but stops copying characters into          */
/* destination when the specified maximum number of characters (including  */
/* the terminating null character) is reached to prevent bounds violation. */
/*-------------------------------------------------------------------------*/
char *strmcpy(char *dest,
              const char *src,
              const unsigned int maxlen) {
  unsigned int i, tmp_maxlen;

  tmp_maxlen = maxlen - 1;
  i = 0;
  while ((src[i] != '\0') &&
         (i < tmp_maxlen)) {
    dest[i] = src[i];
    i++;
  }
  dest[i] = '\0';

  return dest;
}


/*-------------------------------------------------------------------------*/
/* Works like function strcat() but stops copying characters into          */
/* destination when the specified maximum number of characters (including  */
/* the terminating null character) is reached to prevent bounds violation. */
/*-------------------------------------------------------------------------*/
char *strmcat(char *dest,
              const char *src,
              const unsigned int maxlen) {
  unsigned int i, tmp_maxlen;
  char *src_ptr;


  tmp_maxlen = maxlen - 1;
  src_ptr = (char *)src;
  i = strlen(dest);
  while ((*src_ptr != '\0') &&
         (i < tmp_maxlen)) {
    dest[i] = *src_ptr;
    src_ptr++;
    i++;
  }
  dest[i] = '\0';

  return dest;
}


/*-------------------------------------------------------------------------*/
/* Converts an US date string ("M/D/Y" or "M-D-Y") into a date structure.  */
/* A two digit year may be specified.                                      */
/*                                                                         */
/* return value:                                                           */
/*  0   date was converted successfully                                    */
/* -1   date is invalid and was not converted                              */
/*-------------------------------------------------------------------------*/
int strtodate(const char *string,
              THEDATE *dt) {
  char *ptr;

  ptr = (char *)string;
  if(isdigit(*ptr)) {
    dt->da_mon = atoi(ptr);
    while (isdigit(*ptr)) ++ptr;
    if ((*ptr != '/') && (*ptr != '-')) {
      return -1;
    }
    ptr++;
    dt->da_day = atoi(ptr);
    while (isdigit(*ptr)) ++ptr;
    if((*ptr != '/') && (*ptr != '-')) {
      return -1;
    }
    ptr++;
    dt->da_year = atoi(ptr);
  }

  if (dt->da_year < 80) {
    dt->da_year = dt->da_year + 2000;
  }
  else if (dt->da_year < 100) {
    dt->da_year = dt->da_year + 1900;
  }

  return 0;
}


/*-------------------------------------------------------------------------*/
/* Checks, if the specified date is valid.                                 */
/*                                                                         */
/* return value:                                                           */
/*  0   date is invalid                                                    */
/* -1   date is valid                                                      */
/*-------------------------------------------------------------------------*/
int datevalid(THEDATE * dt) {
  if ((dt->da_mon < 1) || (dt->da_mon > 12) ||
      (dt->da_day < 1) || (dt->da_day > 31)) {
    return 0;
  }

  switch (dt->da_mon) {
    case 2:
      if ((dt->da_year % 4 == 0) &&
          ((dt->da_year % 100 != 0) || (dt->da_year % 400 == 0))) {
        if (dt->da_day > 29) {
          return 0;
        }
      } else {
        if (dt->da_day > 28) {
          return 0;
        }
      }
      break;
    case 4:
    case 6:
    case 9:
    case 11:
      if (dt->da_day > 30) {
        return 0;
      }
      break;
  }

  return -1;
}


/*-------------------------------------------------------------------------*/
/* Splits the program arguments into file and switch arguments.            */
/*-------------------------------------------------------------------------*/
void classify_args(const int argc,
                   const char **argv,
                   int *fileargc,
                   char **fileargv,
                   int *switchargc,
                   char **switchargv) {
  int i;
  char *argptr;


  *fileargc = 0;
  *switchargc = 0;
  for (i = 1; i < argc; i++) {
    argptr = (char *)argv[i];
    if (argptr[0] == '/' || argptr[0] == '-') {
      /* first character of parameter is '/' or '-' -> switch argument */
      switchargv[*switchargc] = argptr + 1;
      *switchargc = *switchargc + 1;
    }
    else {
      /* file argument */
      fileargv[*fileargc] = argptr;
      *fileargc = *fileargc + 1;
    }
  }
}


/*-------------------------------------------------------------------------*/
/* Appends a trailing directory separator to the path, but only if it is   */
/* missing.                                                                */
/*-------------------------------------------------------------------------*/
char *cat_separator(char *path) {
  int length;


  length = strlen(path);
  if (path[length - 1] != *DIR_SEPARATOR) {
    path[length] = *DIR_SEPARATOR;
    path[length + 1] = '\0';
  }

  return path;
}


/*-------------------------------------------------------------------------*/
/* Checks if the specified path is valid. The pathname may contain a       */
/* trailing directory separator.                                           */
/*-------------------------------------------------------------------------*/
int dir_exists(const char *path) {
  char tmp_path[MAXPATH];
  int i, attrib;


  strmcpy(tmp_path, path, sizeof(tmp_path));
  i = strlen(tmp_path);
  if (i < 3) {
    /* root directory needs trailing backspace -> add it */
    strmcat(tmp_path, DIR_SEPARATOR, sizeof(tmp_path));
  }
  else if (i > 3) {
    /* remove trailing backspace if necessary */
    i = i - 1;
    if (tmp_path[i] == *DIR_SEPARATOR) {
      tmp_path[i] = '\0';
    }
  }

  attrib = _chmod(tmp_path, 0);
  if (attrib == -1 ||
      (attrib & FA_DIREC) == 0) {
    return 0;
  }

  return -1;
}


/*-------------------------------------------------------------------------*/
/* Writes the specified message on the screen and waits for a key input    */
/* only accepting the specified ones. The first character of a key message */
/* is used as input key (eg: "yes" -> 'Y', "no" -> 'N').                   */
/* MODIFIED 9/2005: returns 1/2/3/4 if 1st/2nd/3rd/4th option chosen       */
/*                                                                         */
/* parameters:                                                             */
/*   msg             message text                                          */
/*   yes             text for answer "yes"                                 */
/*   no              text for answer "no"                                  */
/*   overwrite_all   optional; text for answer "overwrite_all" or NULL     */
/*   skip_all        optional; text for answer "skip_all" or NULL          */
/*                                                                         */
/* examples:                                                               */
/*   char ch;                                                              */
/*   ch = confirm("Overwrite file", "Yes", "No", NULL, NULL);              */
/*   ch = confirm("Overwrite file", "Yes", "No", "All", NULL);             */
/*   ch = confirm("Overwrite file", "Yes", "No",                           */
/*                "Overwrite all", "Skip all");                            */
/*-------------------------------------------------------------------------*/
char confirm(const char *msg,
             const char *yes,
             const char *no,
             const char *overwrite_all,
             const char *skip_all) {
  char msg_tmp[128],
       ch;


  strmcpy(msg_tmp, msg, sizeof(msg_tmp));
  strmcat(msg_tmp, " (", sizeof(msg_tmp));
  strmcat(msg_tmp, yes, sizeof(msg_tmp));
  strmcat(msg_tmp, "/", sizeof(msg_tmp));
  strmcat(msg_tmp, no, sizeof(msg_tmp));
  if (overwrite_all != NULL &&
      overwrite_all[0] != '\0') {
    strmcat(msg_tmp, "/", sizeof(msg_tmp));
    strmcat(msg_tmp, overwrite_all, sizeof(msg_tmp));
  }
  if (skip_all != NULL &&
      skip_all[0] != '\0') {
    strmcat(msg_tmp, "/", sizeof(msg_tmp));
    strmcat(msg_tmp, skip_all, sizeof(msg_tmp));
  }
  strmcat(msg_tmp, ")? ", sizeof(msg_tmp));

  do {
    printf(msg_tmp);
    scanf("%c", &ch);
    fflush(stdin);
    ch = toupper(ch);
    if (ch == yes[0]) { return 1; }
    if (ch == no[0]) { return 2; }
    if ((overwrite_all != NULL) && (ch == overwrite_all[0])) { return 3; }
    if ((skip_all != NULL) && (ch == skip_all[0])) { return 4; }
  } while (1);

  /* never reached: return 0; */
}


/*-------------------------------------------------------------------------*/
/* Copies the source into the destination file including file attributes   */
/* and timestamp.                                                          */
/*-------------------------------------------------------------------------*/
void copy_file(const char *src_filename,
               const char *dest_filename,
               const char return_on_error,
               const char keep_attrib,
               const char reset_arch) {
  FILE *src_file,
       *dest_file;
  char buffer[16384];
  unsigned int buffersize;
  int readsize,
      fileattrib;
  struct ftime filetime;


  /* open source file */
  src_file = fopen(src_filename, "rb");
  if (src_file == NULL) {
    printf("%s - %s\n", catgets(cat, 25, 0, "Cannot open source file"), src_filename);
    if (return_on_error) {
      return;
    }
    else {
      exit(30);
    }
  }

  /* open destination file */
  dest_file = fopen(dest_filename, "wb");
  if (dest_file == NULL) {
    printf("%s - %s\n", catgets(cat, 25, 1, "Cannot create destination file"), dest_filename);
    fclose(src_file);
    if (return_on_error) {
      return;
    }
    else {
      exit(29);
    }
  }

  /* copy file data */
  buffersize = sizeof(buffer);
  readsize = fread(buffer, sizeof(char), buffersize, src_file);
  while (readsize > 0) {
    if (fwrite(buffer, sizeof(char), readsize, dest_file) != readsize) {
      printf("%s - %s\n", catgets(cat, 25, 2, "Write error on destination file"), dest_filename);
      fclose(src_file);
      fclose(dest_file);
      if (return_on_error) {
        return;
      }
      else {
        exit(29);
      }
    }
    readsize = fread(buffer, sizeof(char), buffersize, src_file);
  }

  /* copy file timestamp */
  getftime(fileno(src_file), &filetime);
  setftime(fileno(dest_file), &filetime);

  /* close files */
  fclose(src_file);
  fclose(dest_file);

  /* copy file attributes */
  fileattrib = _chmod(src_filename, 0);
  if(!keep_attrib) {
      fileattrib = fileattrib & FA_ARCH;
  }
  if(reset_arch){
     fileattrib = fileattrib & !FA_ARCH;
  }
  _chmod(dest_filename, 1, fileattrib);
}

unsigned int getDOSVersion() {
    union REGS regs;

    regs.h.ah = 0x30;  // Function code for get DOS version
    intdos(&regs, &regs);

    // AL register will contain the DOS major version
    // AH register will contain the DOS minor version
    if (regs.h.al == 0) {
        regs.h.al++;
    }
    return (regs.h.al << 8) | regs.h.ah;
}

unsigned int get_extendedDiskInfo (char driveNumber, struct DiskInfo *diskInfo) {
    union REGS regs;
    struct SREGS segregs;
    char driveString[4];

	if(driveNumber < 1 || driveNumber > 26) return 0xFFFF;

    // Set up buffer for result
    segread(&segregs);  /* Get the segment of DS */
    regs.x.di = FP_OFF(diskInfo);
    segregs.es = FP_SEG(diskInfo);

    // Convert drive number to ASCIZ string
    sprintf(driveString, "%c:\\", 'A' + driveNumber - 1);

    // Set up registers
    regs.h.ah = 0x73;     /* Function code for get free disk space */
    regs.h.al = 0x03;     /* Sub-function code for get disk information */
    regs.x.cx = sizeof(struct DiskInfo);
    regs.x.dx = FP_OFF(driveString);
    segregs.ds = FP_SEG(driveString);

    // Perform the interrupt
    intdosx(&regs, &regs, &segregs);
	return regs.x.ax;
}
