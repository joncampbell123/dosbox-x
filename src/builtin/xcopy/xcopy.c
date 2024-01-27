/***************************************************************************/
/* XCOPY                                                                   */
/*                                                                         */
/* A replacement for the MS DOS(tm) XCOPY command.                         */
/*-------------------------------------------------------------------------*/
/* compatible compilers:                                                   */
/* Borland C++ (tm) v3.0 or higher                                         */
/* [ Support for Turbo C 2.01 and OpenWatcom added 2005 by Eric Auer... ]  */
/* [ Prior to v1.5 use TCC2WAT with OpenWatcom, pieces included in v1.5 ]  */
/*-------------------------------------------------------------------------*/
/* (C)opyright 2001-2003 by Rene Ableidinger (rene.ableidinger@gmx.at)     */
/* Bugfixes and linking to Kitten translation library: Eric Auer 2005      */
/*                                                                         */
/* This program is free software; you can redistribute it and/or modify it */
/* under the terms of the GNU General Public License version 2 as          */
/* published by the Free Software Foundation.                              */
/*                                                                         */
/* This program is distributed in the hope that it will be useful, but     */
/* WITHOUT ANY WARRANTY; without even the implied warranty of              */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU        */
/* General Public License for more details.                                */
/*                                                                         */
/* You should have received a copy of the GNU General Public License along */
/* with this program; if not, write to the Free Software Foundation, Inc., */
/* 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.                */
/***************************************************************************/

/* Downloaded original source from the following link and fixed attribute copy   */
/* https://github.com/FDOS/xcopy/commit/71dd94271057b3a60c6a01d1c3f2c7da856c464c */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <io.h>
/* #include <dir.h> - moved to shared.inc */
#include <dos.h>	/* also has the date / time struct definitions */
#include <sys/stat.h>

#include "kitten.h"     /* Kitten message library */
			/* or use the older catgets.h */
nl_catd cat;            /* message catalog, must be before shared.inc */
  
#include "shared.inc"	/* general compiler specific stuff */
			/* dir.h sets MAXPATH to 80 for DOS */
#define MYMAXPATH 130	/* _fullname wants at least 128 bytes */

#if defined(__TURBOC__) && !defined(__BORLANDC__)
#define _splitpath fnsplit
void _fullpath_tc(char * truename, char * rawname, unsigned int namelen)
{
  union REGS regs;
  struct SREGS sregs;
  if (namelen < 128)
  {
    truename[0] = '\0';
    return;
  }
  regs.x.ax = 0x6000;   /* truename */
  regs.x.si = FP_OFF(rawname);
  sregs.ds = FP_SEG(rawname);
  regs.x.di = FP_OFF(truename);
  sregs.es = FP_SEG(truename);
  intdosx(&regs, &regs, &sregs);
  if (regs.x.cflag != 0)
  {
    truename[0] = '\0';
  }
}
#endif


/*-------------------------------------------------------------------------*/
/* GLOBAL CONSTANTS                                                        */
/*-------------------------------------------------------------------------*/
#define MAXSWITCH 255


/*-------------------------------------------------------------------------*/
/* GLOBAL VARIABLES                                                        */
/*-------------------------------------------------------------------------*/
char switch_archive = 0,
     /* switch_confirm specifies if it should be asked for  */
     /* confirmation to overwrite file if it exists:        */
     /* 0 - don't ask and overwrite file                    */
     /* 1 - don't ask and skip file                         */
     /* 2 - ask (default)                                   */
     switch_confirm = 2,
     switch_continue = 0,
     switch_emptydir = 0,
     switch_fullnames = 0,
     switch_file = 0,
     switch_hidden = 0,
     switch_intodir = 0,
     switch_listmode = 0,
     switch_archive_reset = 0,
     switch_prompt = 0,
     switch_quiet = 0,
     switch_readonly = 0,
     switch_subdir = 0,
     switch_tree = 0,
     switch_verify = 0,
     switch_wait = 0,
     bak_verify = 0,
     switch_keep_attr = 0,
     dest_drive;
long int switch_date = 0;
long file_counter = 0;
int file_found = 0;


/*-------------------------------------------------------------------------*/
/* PROTOTYPES                                                              */
/*-------------------------------------------------------------------------*/
void print_help(void);
void exit_fn(void);
int cyclic_path(const char *src_pathname,
                const char *dest_pathname);
int make_dir(char *path);
void build_filename(char *dest_filename,
                    const char *src_filename,
                    const char *filepattern);
void build_name(char *dest,
                const char *src,
                const char *pattern);
void xcopy_files(const char *src_pathname,
                 const char *src_filename,
                 const char *dest_pathname,
                 const char *dest_filename);
void xcopy_file(const char *src_filename,
                const char *dest_filename);
unsigned int getDOSVersion(void);

/*-------------------------------------------------------------------------*/
/* MAIN-PROGRAM                                                            */
/*-------------------------------------------------------------------------*/
int main(int argc, const char **argv) {
  int fileargc, 
	  switchargc;
  char *fileargv[255],
       *switchargv[255],
       env_prompt[MAXSWITCH],
       tmp_switch[MAXSWITCH] = "",
       src_pathname[MYMAXPATH] = "",
       src_filename[MAXFILE + MAXEXT] = "",
       dest_pathname[MYMAXPATH] = "",
       dest_filename[MAXFILE + MAXEXT] = "",
       *ptr,
       ch;
  int i, length;
  THEDATE dt;
#ifdef __WATCOMC__
  struct dostime_t tm;
#else
  struct time tm;
#endif

  cat = catopen ("xcopy", 0);	/* initialize kitten */

  classify_args(argc, argv, &fileargc, fileargv, &switchargc, switchargv);

  if (fileargc < 1 || switchargv[0] == "?") {
    print_help();
    catclose(cat);
    exit(4);
  }

  if (fileargc > 2) {
    printf("%s\n",catgets(cat, 1, 1, "Invalid number of parameters"));
    catclose(cat);
    exit(4);
  }

  /* activate termination function */
  /* (writes no. of copied files at exit) */
  atexit(exit_fn);

  /* read environment variable COPYCMD to set confirmation switch */
  strmcpy(env_prompt, getenv("COPYCMD"), sizeof(env_prompt));
  if (env_prompt[0] != '\0') {
    strupr(env_prompt);
    if (strcmp(env_prompt, "/Y") == 0)
      /* overwrite existing file(s) */
      switch_confirm = 0;
    else if (strcmp(env_prompt, "/N") == 0)
      /* skip existing file(s) */
      switch_confirm = 1;
    else
      /* ask for confirmation */
      switch_confirm = 2;
  }

  /* get switches */
  for (i = 0; i < switchargc; i++) {
    strmcpy(tmp_switch, switchargv[i], sizeof(tmp_switch));
    strupr(tmp_switch);
    if (strcmp(tmp_switch, "A") == 0)
      switch_archive = -1;
    else if (strcmp(tmp_switch, "C") == 0)
      switch_continue = -1;
    else if (strcmp(tmp_switch, "D") == 0)
      switch_date = -1;
    else if (strncmp(tmp_switch, "D:", 2) == 0) {
      if (strtodate(tmp_switch+2, &dt) != 0 ||
          !datevalid(&dt)) {
        printf("%s\n",catgets(cat, 1, 2, "Invalid date"));
        catclose(cat);
        exit(4);
      }
#ifdef __WATCOMC__
      memset((void *)&tm, 0, sizeof(struct dostime_t));
      /* tm.tm_hour = 0; tm.tm_min = 0; tm.tm_sec = 0; tm.tm_hund = 0; */
#else
      memset((void *)&tm, 0, sizeof(struct time));
      /* tm.ti_hour = 0; tm.ti_min = 0; tm.ti_sec = 0; tm.ti_hund = 0; */
#endif
      switch_date = dostounix(&dt, &tm);
    }
    else if (strcmp(tmp_switch, "E") == 0)
      switch_emptydir = -1;
    else if (strcmp(tmp_switch, "F") == 0)
      switch_fullnames = -1;
    else if (strcmp(tmp_switch, "H") == 0)
      switch_hidden = -1;
    else if (strcmp(tmp_switch, "I") == 0)
      switch_intodir = -1;
    else if (strcmp(tmp_switch, "K") == 0)
      switch_keep_attr = -1;
    else if (strcmp(tmp_switch, "L") == 0)
      switch_listmode = -1;
    else if (strcmp(tmp_switch, "M") == 0)
      switch_archive_reset = -1;
    else if (strcmp(tmp_switch, "N") == 0)
      switch_confirm = 1;
    else if (strcmp(tmp_switch, "P") == 0)
      switch_prompt = -1;
    else if (strcmp(tmp_switch, "Q") == 0)
      switch_quiet = -1;
    else if (strcmp(tmp_switch, "R") == 0)
      switch_readonly = -1;
    else if (strcmp(tmp_switch, "S") == 0)
      switch_subdir = -1;
    else if (strcmp(tmp_switch, "T") == 0)
      switch_tree = -1;
    else if (strcmp(tmp_switch, "V") == 0) {
      switch_verify = -1;
      bak_verify = getverify();
      setverify(1);
    }
    else if (strcmp(tmp_switch, "W") == 0)
      switch_wait = -1;
    else if (strcmp(tmp_switch, "Y") == 0)
      switch_confirm = 0;
    else if (strcmp(tmp_switch, "-Y") == 0)
      switch_confirm = 2;
    else {
      printf("%s - %s\n", catgets(cat, 1, 3, "Invalid switch"), switchargv[i]);
      catclose(cat);
      exit(4);
    }
  }

  /* get source pathname (with trailing backspace) and filename/-pattern */
  length = strlen(fileargv[0]);
  if (length > (MAXPATH - 1)) {
    printf("%s\n", catgets(cat, 1, 4, "Source path too long"));
    catclose(cat);
    exit(4);
  }
#if defined(__TURBOC__) && !defined(__BORLANDC__)
  _fullpath_tc(src_pathname, fileargv[0], MYMAXPATH);
#else
  _fullpath(src_pathname, fileargv[0], MYMAXPATH);
#endif  
  if (src_pathname[0] == '\0') {
    printf("%s\n", catgets(cat, 1, 5, "Invalid source drive specification"));
    catclose(cat);
    exit(4);
  }
  /* check source path */
  if (!dir_exists(src_pathname)) {
    /* source path contains a filename/-pattern -> separate it */
    ptr = strrchr(src_pathname, *DIR_SEPARATOR);
    ptr++;
    strmcpy(src_filename, ptr, sizeof(src_filename));
    *ptr = '\0';
    if (!dir_exists(src_pathname)) {
      printf("%s - %s\n", catgets(cat, 1, 6, "Source path not found"), src_pathname);
      catclose(cat);
      exit(4);
    }
  }
  else {
    /* source is a directory -> filepattern = "*.*" */
    strmcpy(src_filename, "*.*", sizeof(src_filename));
  }
  cat_separator(src_pathname);
  length = strlen(src_pathname);
  if (length > (MAXDRIVE - 1 + MAXDIR - 1)) {
    printf("%s\n", catgets(cat, 1, 7, "Source path too long"));
    catclose(cat);
    exit(4);
  }

  /* get destination pathname (with trailing backspace) and */
  /* filename/-pattern */
  if (fileargc < 2) {
    /* no destination path specified -> use current */
    getcwd(dest_pathname, MAXPATH);
    strmcpy(dest_filename, "*.*", sizeof(dest_filename));
  }
  else {
    /* destination path specified */
    length = strlen(fileargv[1]);
    if (length > (MAXPATH - 1)) {
      printf("%s\n", catgets(cat, 1, 8, "Destination path too long"));
      catclose(cat);
      exit(4);
    }
#if defined(__TURBOC__) && !defined(__BORLANDC__)
    _fullpath_tc(dest_pathname, fileargv[1], MYMAXPATH);
#else
    _fullpath(dest_pathname, fileargv[1], MYMAXPATH);
#endif
    if (dest_pathname[0] == '\0') {
      printf("%s\n", catgets(cat, 1, 9, "Invalid destination drive specification"));
      catclose(cat);
      exit(4);
    }
    /* check destination path */
    if (fileargv[1][length - 1] != *DIR_SEPARATOR &&
        !dir_exists(dest_pathname)) {
      ptr = strrchr(dest_pathname, *DIR_SEPARATOR);
      ptr++;
      strmcpy(dest_filename, ptr, sizeof(dest_filename));
      *ptr = '\0';
      if ((ptr = strchr(dest_filename, '*')) == NULL &&
          (ptr = strchr(dest_filename, '?')) == NULL) {
        /* last destination entry is not a filepattern -> does it specify */
        /* a file or directory? */
        if (((ptr = strchr(src_filename, '*')) == NULL &&
             (ptr = strchr(src_filename, '?')) == NULL) ||
            !switch_intodir) {
          /* source is a single file or switch /I was not specified -> ask */
          /* user if destination should be a file or a directory */
          printf("%s %s %s\n",
            catgets(cat, 1, 10, "Does"),
            dest_filename,
            catgets(cat, 1, 11, "specify a file name"));
          ch = confirm(
            catgets(cat, 1, 12, "or directory name on the target"),
            catgets(cat, 1, 13, "File"),
            catgets(cat, 1, 14, "Directory"), NULL, NULL);
          switch (ch) {
            case 1: /* 'F' */
              /* file */
              switch_file = -1;
              break;
            case 2: /* 'D' */
              /* directory */
              switch_intodir = -1;
              break;
          }
        }
        if (switch_intodir) {
          /* destination is a directory -> filepattern = "*.*" */
          strmcat(dest_pathname, dest_filename, sizeof(dest_pathname));
          strmcpy(dest_filename, "*.*", sizeof(dest_filename));
        }
      }
    }
    else {
      /* destination is a directory -> filepattern = "*.*" */
      strmcpy(dest_filename, "*.*", sizeof(dest_filename));
    }
  }
  cat_separator(dest_pathname);
  length = strlen(dest_pathname);
  if (length > (MAXDRIVE - 1 + MAXDIR - 1)) {
    printf("%s\n",catgets(cat, 1, 15, "Destination path too long"));
    catclose(cat);
    exit(4);
  }

  /* check for cyclic path */
  if ((switch_emptydir || switch_subdir) &&
      cyclic_path(src_pathname, dest_pathname)) {
    printf("%s\n",catgets(cat, 1, 16, "Cannot perform a cyclic copy"));
    catclose(cat);
    exit(4);
  }

  /* get destination drive (1 = A, 2 = B, 3 = C, ...) */
  dest_drive = toupper(dest_pathname[0]) - 64;

  if (switch_wait) {
    printf("%s\n",catgets(cat, 1, 17, "Press enter to continue..."));
    (void)getchar(); /* getch(); would need conio.h */
    fflush(stdin);
  }

  xcopy_files(src_pathname, src_filename, dest_pathname, dest_filename);
  if (!file_found) {
    printf("%s - %s\n",catgets(cat, 1, 18, "File not found"), src_filename);
    catclose(cat);
    exit(1);
  }

  catclose(cat);
  return 0;
}


/*-------------------------------------------------------------------------*/
/* SUB-PROGRAMS                                                            */
/*-------------------------------------------------------------------------*/
void print_help(void) {
  printf("XCOPY v1.5 - Copyright 2001-2003 by Rene Ableidinger (patches 2005: Eric Auer)\n");
  	/* VERSION! */
  printf("%s\n\n", catgets(cat, 2, 1, "Copies files and directory trees."));
  printf("%s\n\n", catgets(cat, 2, 2, "XCOPY source [destination] [/switches]"));
  printf("%s\n", catgets(cat, 2, 3, "  source       Specifies the directory and/or name of file(s) to copy."));
  printf("%s\n", catgets(cat, 2, 4, "  destination  Specifies the location and/or name of new file(s)."));
  printf("%s\n", catgets(cat, 2, 5, "  /A           Copies only files with the archive attribute set and doesn't"));
  printf("%s\n", catgets(cat, 2, 6, "               change the attribute."));
  printf("%s\n", catgets(cat, 2, 7, "  /C           Continues copying even if errors occur."));
  printf("%s\n", catgets(cat, 2, 8, "  /D[:M/D/Y]   Copies only files which have been changed on or after the"));
  printf("%s\n", catgets(cat, 2, 9, "               specified date. When no date is specified, only files which are"));
  printf("%s\n", catgets(cat, 2, 10, "               newer than existing destination files will be copied."));
  printf("%s\n", catgets(cat, 2, 11, "  /E           Copies any subdirectories, even if empty."));
  printf("%s\n", catgets(cat, 2, 12, "  /F           Display full source and destination name."));
  printf("%s\n", catgets(cat, 2, 13, "  /H           Copies hidden and system files as well as unprotected files."));
  printf("%s\n", catgets(cat, 2, 14, "  /I           If destination does not exist and copying more than one file,"));
  printf("%s\n", catgets(cat, 2, 15, "               assume destination is a directory."));
  printf("%s\n", catgets(cat, 2, 16, "  /K           Keep attributes. Otherwise only archive attribute is copied."));
  printf("%s\n", catgets(cat, 2, 17, "  /L           List files without copying them. (simulates copying)"));
  printf("%s\n", catgets(cat, 2, 18, "  /M           Copies only files with the archive attribute set and turns off"));

  if (isatty(1)) {
    printf("-- %s --", catgets(cat, 2, 35, "press enter for more"));
    (void)getchar();
  } /* wait for next page if TTY */

  printf("%s\n", catgets(cat, 2, 19, "               the archive attribute of the source files after copying them."));
  printf("%s\n", catgets(cat, 2, 20, "  /N           Suppresses prompting to confirm you want to overwrite an"));
  printf("%s\n", catgets(cat, 2, 21, "               existing destination file and skips these files."));
  printf("%s\n", catgets(cat, 2, 22, "  /P           Prompts for confirmation before creating each destination file."));
  printf("%s\n", catgets(cat, 2, 23, "  /Q           Quiet mode, don't show copied filenames."));
  printf("%s\n", catgets(cat, 2, 24, "  /R           Overwrite read-only files as well as unprotected files."));
  printf("%s\n", catgets(cat, 2, 25, "  /S           Copies directories and subdirectories except empty ones."));
  printf("%s\n", catgets(cat, 2, 26, "  /T           Creates directory tree without copying files. Empty directories"));
  printf("%s\n", catgets(cat, 2, 27, "               will not be copied. To copy them add switch /E."));
  printf("%s\n", catgets(cat, 2, 28, "  /V           Verifies each new file."));
  printf("%s\n", catgets(cat, 2, 29, "  /W           Waits for a keypress before beginning."));
  printf("%s\n", catgets(cat, 2, 30, "  /Y           Suppresses prompting to confirm you want to overwrite an"));
  printf("%s\n", catgets(cat, 2, 31, "               existing destination file and overwrites these files."));
  printf("%s\n", catgets(cat, 2, 32, "  /-Y          Causes prompting to confirm you want to overwrite an existing"));
  printf("%s\n\n", catgets(cat, 2, 33, "               destination file."));
  printf("%s\n", catgets(cat, 2, 34, "The switch /Y or /N may be preset in the COPYCMD environment variable."));
  printf("%s\n", catgets(cat, 2, 35, "This may be overridden with /-Y on the command line."));
}


/*-------------------------------------------------------------------------*/
/* Gets called by the "exit" command and is used to write a                */
/* status-message.                                                         */
/*-------------------------------------------------------------------------*/
void exit_fn(void) {
  if (switch_verify) {
    /* restore value of verify flag */
    setverify(bak_verify);
  }

  printf("%ld %s\n", file_counter, catgets(cat, 1, 19, "file(s) copied"));
  catclose(cat);
}


/*-------------------------------------------------------------------------*/
/* Checks, if the destination path is a subdirectory of the source path.   */
/*-------------------------------------------------------------------------*/
int cyclic_path(const char *src_pathname,
                const char *dest_pathname) {
  char tmp_dest_pathname[MAXPATH];
  int length;


  /* format pathnames for comparison */
  length = strlen(src_pathname);
  strncpy(tmp_dest_pathname, dest_pathname, length);
  tmp_dest_pathname[length] = '\0';

  return stricmp(src_pathname, tmp_dest_pathname) == 0;
}


/*-------------------------------------------------------------------------*/
/* Creates a directory or a whole directory path. The pathname may contain */
/* a trailing directory separator. If a whole directory path should be     */
/* created and an error occurs then the path string will be truncated      */
/* after the failed directory. (e.g. path=C:\DIR1\DIR2\DIR3 -> error       */
/* creating DIR2 -> path=C:\DIR1\DIR2)                                     */
/*                                                                         */
/* return value:                                                           */
/*  0   directory was created successfully                                 */
/* -1   directory was not created due to error                             */
/*-------------------------------------------------------------------------*/
int make_dir(char *path) {
  int i;
  char tmp_path1[MAXPATH],
       tmp_path2[MAXPATH],
       length,
       mkdir_error;


  if (path[0] == '\0') {
    return -1;
  }

  strmcpy(tmp_path1, path, sizeof(tmp_path1));
  cat_separator(tmp_path1);
  length = strlen(tmp_path1);
  strncpy(tmp_path2, tmp_path1, 3);
  i = 3;
  while (i < length) {
    if (tmp_path1[i] == '\\') {
      tmp_path2[i] = '\0';
      if (!dir_exists(tmp_path2)) {
        mkdir_error = mkdir(tmp_path2);
        if (mkdir_error) {
          path[i] = '\0';
          return mkdir_error;
        }
      }
    }
    tmp_path2[i] = tmp_path1[i];

    i++;
  }

  return 0;
}


/*-------------------------------------------------------------------------*/
/* Uses the source filename and the filepattern (which may contain the     */
/* wildcards '?' and '*' in any possible combination) to create a new      */
/* filename. The source filename may contain a pathname.                   */
/*-------------------------------------------------------------------------*/
void build_filename(char *dest_filename,
                    const char *src_filename,
                    const char *filepattern) {
  char drive[MAXDRIVE],
       dir[MAXDIR],
       filename_file[MAXFILE],
       filename_ext[MAXEXT],
       filepattern_file[MAXFILE],
       filepattern_ext[MAXEXT],
       tmp_filename[MAXFILE],
       tmp_ext[MAXEXT];

  _splitpath(src_filename, drive, dir, filename_file, filename_ext);
  _splitpath(filepattern, drive, dir, filepattern_file, filepattern_ext);
  build_name(tmp_filename, filename_file, filepattern_file);
  build_name(tmp_ext, filename_ext, filepattern_ext);
  strmcpy(dest_filename, drive, MAXPATH);
  strmcat(dest_filename, dir, MAXPATH);
  strmcat(dest_filename, tmp_filename, MAXPATH);
  strmcat(dest_filename, tmp_ext, MAXPATH);
}


void build_name(char *dest,
                const char *src,
                const char *pattern) {
  int i,
      pattern_i,
      src_length,
      pattern_length;


  src_length = strlen(src);
  pattern_length = strlen(pattern);
  i = 0;
  pattern_i = 0;
  while ((i < src_length ||
          (pattern[pattern_i] != '\0' &&
           pattern[pattern_i] != '?' &&
           pattern[pattern_i] != '*')) &&
         pattern_i < pattern_length) {
    switch (pattern[pattern_i]) {
      case '*':
        dest[i] = src[i];
        break;
      case '?':
        dest[i] = src[i];
        pattern_i++;
        break;
      default:
        dest[i] = pattern[pattern_i];
        pattern_i++;
        break;
    }

    i++;
  }
  dest[i] = '\0';
}


/*-------------------------------------------------------------------------*/
/* Searchs through the source directory (and its subdirectories) and calls */
/* function "xcopy_file" for every found file.                             */
/*-------------------------------------------------------------------------*/
void xcopy_files(const char *src_pathname,
                 const char *src_filename,
                 const char *dest_pathname,
                 const char *dest_filename) {
  char filepattern[MAXPATH],
       new_src_pathname[MAXPATH],
       new_dest_pathname[MAXPATH],
       src_path_filename[MAXPATH],
       dest_path_filename[MAXPATH],
       tmp_filename[MAXFILE + MAXEXT],
       tmp_pathname[MAXPATH];
  struct ffblk fileblock;
  int fileattrib,
      done;


  if (switch_emptydir ||
      switch_subdir ||
      switch_tree) {
    /* copy files in subdirectories too */
    strmcpy(filepattern, src_pathname, sizeof(filepattern));
    strmcat(filepattern, "*.*", sizeof(filepattern));
    done = findfirst(filepattern, &fileblock, FA_DIREC);
    while (!done) {
      if ((fileblock.ff_attrib & FA_DIREC) != 0 &&
          strcmp(fileblock.ff_name, ".") != 0 &&
          strcmp(fileblock.ff_name, "..") != 0) {
        /* build source pathname */
        strmcpy(new_src_pathname, src_pathname, sizeof(new_src_pathname));
        strmcat(new_src_pathname, fileblock.ff_name, sizeof(new_src_pathname));
        strmcat(new_src_pathname, DIR_SEPARATOR, sizeof(new_src_pathname));

        /* build destination pathname */
        strmcpy(new_dest_pathname, dest_pathname, sizeof(new_dest_pathname));
        strmcat(new_dest_pathname, fileblock.ff_name, sizeof(new_dest_pathname));
        strmcat(new_dest_pathname, DIR_SEPARATOR, sizeof(new_dest_pathname));

        xcopy_files(new_src_pathname, src_filename,
                    new_dest_pathname, dest_filename);
      }

      done = findnext(&fileblock);
    }
  }

  fileattrib = FA_RDONLY + FA_ARCH;
  if (switch_hidden) {
    /* replace hidden and system files too */
    fileattrib = fileattrib + FA_HIDDEN + FA_SYSTEM;
  }

  /* find first source file */
  strmcpy(filepattern, src_pathname, sizeof(filepattern));
  strmcat(filepattern, src_filename, sizeof(filepattern));
  done = findfirst(filepattern, &fileblock, fileattrib);

  if (!done) {
    file_found = -1;
  }

  /* check if destination directory must be created */
  if ((!done || switch_emptydir) &&
      !dir_exists(dest_pathname)) {
    strmcpy(tmp_pathname, dest_pathname, sizeof(tmp_pathname));
    if (make_dir(tmp_pathname) != 0) {
      printf("%s %s\n", catgets(cat, 1, 20, "Unable to create directory"), tmp_pathname);
      if (switch_continue) {
        return;
      }
      else {
        catclose(cat);
        exit(4);
      }
    }
  }

  /* check, if only directory tree should be created */
  if (switch_tree) {
    return;
  }

  /* copy files */
  while (!done) {
    /* check, if copied files should have archive attribute set */
    if ((switch_archive == 0 && switch_archive_reset == 0) ||
        fileblock.ff_attrib & FA_ARCH) {
      /* build source filename including path */
      strmcpy(src_path_filename, src_pathname, sizeof(src_path_filename));
      strmcat(src_path_filename, fileblock.ff_name, sizeof(src_path_filename));

      /* build destination filename including path */
      strmcpy(dest_path_filename, dest_pathname, sizeof(dest_path_filename));
      build_filename(tmp_filename, fileblock.ff_name, dest_filename);
      strmcat(dest_path_filename, tmp_filename, sizeof(dest_path_filename));

      xcopy_file(src_path_filename, dest_path_filename);
    }

    done = findnext(&fileblock);
  }
}

/*-------------------------------------------------------------------------*/
/* Checks all dependencies of the source and destination file and calls    */
/* function "copy_file".                                                   */
/*-------------------------------------------------------------------------*/
void xcopy_file(const char *src_filename,
                const char *dest_filename) {
  int dest_file_exists;
  int fileattrib;
  struct stat src_statbuf;
  struct stat dest_statbuf;
#if defined(__TURBOC__) && !defined(__BORLANDC__)
  struct diskfree_t disk;
#else
  struct dfree disktable;
#endif
  unsigned long free_diskspace = -1;
  unsigned long bsec = 512;
  char ch;
  char msg_prompt[255];
  unsigned int dos_version = 0;
  struct DiskInfo diskinfo;

  if (switch_prompt) {
    /* ask for confirmation to create file */
    ch = confirm(dest_filename,
      catgets(cat, 3, 2, "Yes"),
      catgets(cat, 3, 3, "No"), NULL, NULL);
    if (ch == 2 /* 'N' */) {
      /* no -> skip file */
      return;
    }
  }

  /* check if source and destination file are equal */
  if (stricmp(src_filename, dest_filename) == 0) {
    printf("%s - %s\n", catgets(cat, 1, 21, "File cannot be copied onto itself"), src_filename);
    catclose(cat);
    exit(4);
  }

  /* check source file for read permission */
  /* (only usefull under an OS with the ability to deny read access) */
  if (access(src_filename, R_OK) != 0) {
    printf("%s - %s\n", catgets(cat, 1, 22, "Read access denied"), src_filename);
    if (switch_continue) {
      return;
    }
    else {
      catclose(cat);
      exit(5);
    }
  }

  /* get info of source and destination file */
  stat((char *)src_filename, &src_statbuf);
  dest_file_exists = !stat((char *)dest_filename, &dest_statbuf);

  dos_version = getDOSVersion();

  /* get amount of free disk space in destination drive */
  if(((dos_version >> 8) > 7) || (((dos_version >> 8) == 7) && ((dos_version & 0xFF) >= 10))){
      //printf("%d %s", dest_drive, dest_drive);
      if(!get_extendedDiskInfo(dest_drive, &diskinfo)){
      	bsec = diskinfo.bytesPerSector;
      	free_diskspace = diskinfo.availableClusters * diskinfo.sectorsPerCluster;
      	//printf("free_diskspace=%lu avail_clusters=%lu,sectors_per_cluster=%lu \n", free_diskspace, diskinfo.availableClusters, diskinfo.sectorsPerCluster);
	  }
	  else {
	    goto int21_36h; /* Try the old DOS method if failed */
	  }
  }
  else {
int21_36h:
#if defined(__TURBOC__) && !defined(__BORLANDC__)
  	if(_dos_getdiskfree(dest_drive, &disk) != 0) {
		printf("Failed to obtain free disk space. Aborting\n");
		exit(39);
  	}
  	bsec = disk.bytes_per_sector;
  	free_diskspace = (unsigned long) disk.avail_clusters * disk.sectors_per_cluster;
  	//printf("dest_drive=%d,free_diskspace=%lu avail_clusters=%u,sectors_per_cluster=%u \n",dest_drive, free_diskspace, disk.avail_clusters, disk.sectors_per_cluster);
#else
	getdfree(dest_drive, &disktable);
	bsec = disktable.df_bsec;
	free_diskspace = (unsigned long) disktable.df_avail * disktable.df_sclus;
	//printf("disktable.df_avail=%d,disktable.df_sclus=%d\n",disktable.df_avail * disktable.df_sclus);
	//free_diskspace = (unsigned long) disktable.df_avail *
	//                 (unsigned long) disktable.df_sclus *
	//                 (unsigned long) disktable.df_bsec;
#endif
  }
  if (dest_file_exists) {
    if (switch_date) {
      if (switch_date < 0) {
        /* check, that only newer files are copied */
        if (src_statbuf.st_mtime <= dest_statbuf.st_mtime) {
          return;
        }
      }
      else {
        /* check, that only files changed on or after the specified date */
        /* are copied                                                    */
        if (src_statbuf.st_mtime < switch_date) {
          return;
        }
      }
    }

    switch (switch_confirm) {
      case 1:
        /* skip file */
        return;
      case 2:
        /* ask for confirmation to overwrite file */
        strmcpy(msg_prompt,
          catgets(cat, 3, 1, "Overwrite"), sizeof(msg_prompt));
        strmcat(msg_prompt, " ", sizeof(msg_prompt));
        strmcat(msg_prompt, dest_filename, sizeof(msg_prompt));
        ch = confirm(msg_prompt,
          catgets(cat, 3, 2, "Yes"),
          catgets(cat, 3, 3, "No"),
          catgets(cat, 3, 4, "Overwrite all"),
          catgets(cat, 3, 5, "Skip all"));
        switch (ch) {
          case 2: /* 'N' */
            /* no -> skip file */
            return;
          case 3: /* 'O' */
            /* overwrite all -> set confirm switch */
            switch_confirm = 0;
            break;
          case 4: /* 'S' */
            /* skip all -> set confirm switch */
            switch_confirm = 1;
            return;
        }
        break;
    }

    /* check free space on destination disk */
    /* *** was wrong, was: "if (src... > free... - dest...) ..." */
    if ( (src_statbuf.st_size > dest_statbuf.st_size) &&
         (((src_statbuf.st_size - dest_statbuf.st_size) / bsec) > free_diskspace) ) {
      printf("%s - %s\n", catgets(cat, 1, 23, "Insufficient disk space in destination path"), dest_filename);
      catclose(cat);
      exit(39);
    }

    /* get file attribute from destination file */
    fileattrib = _chmod(dest_filename, 0);

    /* check destination file for write permission */
    if ((fileattrib & (64 ^ FA_RDONLY)) != 0) {
      if (!switch_readonly) {
        printf("%s - %s\n", catgets(cat, 1, 24, "Write access denied"), dest_filename);
        if (switch_continue) {
          return;
        }
        else {
          catclose(cat);
          exit(5);
        }
      }

      /* check, if file copying should be simulated */
      if (!switch_listmode) {
        /* remove readonly attribute from destination file */
        fileattrib = fileattrib ^ FA_RDONLY;
        _chmod(dest_filename, 1, fileattrib);
      }
    }
  }
  else {
    /* check free space on destination disk */
    unsigned long st_size_sec = (unsigned long)src_statbuf.st_size / bsec;
    
    if (st_size_sec > (unsigned long)free_diskspace) {
      printf("%lu - %lu\n", st_size_sec, free_diskspace);
      printf("%s - %s\n", catgets(cat, 1, 25, "Insufficient disk space in destination path"), dest_filename);
      catclose(cat);
      exit(39);
    }
  }

  if (!switch_quiet) {
    printf("%s %s", catgets(cat, 1, 26, "Copying"), src_filename);
    if (switch_fullnames) {
      printf(" -> %s\n", dest_filename);
    }
    else {
      printf("\n");
    }
  }

  /* check, if file copying should be simulated */
  if (!switch_listmode) {
    copy_file(src_filename, dest_filename, switch_continue, switch_keep_attr, switch_archive_reset);
  }

  file_counter++;
}
