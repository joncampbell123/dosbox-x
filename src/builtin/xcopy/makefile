# Makefile for XCOPY (using Borland C or Turbo C)
# Based on the CHOICE makefile by Tom Ehlert

# compression (for final executable)
# set 
#   UPX=-rem 
# if you dont want to UPX choice
# if you use upx: --8086 for 8086 compatibility, --best for smallest

UPX=-rem
#UPX=upx -9


# set where build files (including intermediate ones) placed
OUTDIR=..\BIN


# common commands needed, e.g. make directory & remove files
MKDIR=mkdir
MV=copy
RM=del


############# WATCOM ########################
# -s remove stack overflow checks, -0 8086 instructions only
# -bt=DOS target DOS (e.g. if compiling from other OS)
# -wx max warnings, -fm generate map file
# -we treat warnings as errors, -zl no default libs
# -ms memory model (s=SMALL), -zq quiet mode
# -fe=xxx executable name (must be last)
# -o a=relax alias checking s=for size, -zp# 1=byte align
#CC=wcl
#CFLAGS=-oas -bt=DOS -zp1 -s -ms -0 -wx -we -zq -fm -fe=$*
# needed for wmake if phony target
#PHONY=.SYMBOLIC

############# TURBO_C ########################
# -w warn -M create map -f- no floating point -Z register optimize
# -O jump optimize -k- no standard stack frame -K unsigned char
# -exxx executable name (name must be part of option in CFLAGS)
# -mt tiny (default is small -ms)
# -N stack checking -a- byte alignment  -ln no default libs
# -lt create .com file -lx no map file ...
# -v- quiet mode
# -nPATH to place resulting files
#CC=bcc
CC=tcc
CFLAGS=-d -v- -w -M -f- -Z -a- -O -k- -K -ln -ms -e$*
PHONY=


CFILES=xcopy.c kitten.c prf.c


# targets:

all: xcopy.exe

# no space between -e and find (set exe file name)!
# upx either xcopy.com or xcopy.exe, depends...

xcopy.exe: $(CFILES) makefile shared.inc kitten.h
	-$(MKDIR) $(OUTDIR)
	$(CC) $(CFLAGS) $(CFILES)
	$(UPX) xcopy.exe
	$(MV) xcopy.exe $(OUTDIR)


clean: $(PHONY)
	-$(RM) *.obj 
	-$(RM) *.map 
	-$(RM) xcopy.exe
	-$(RM) $(OUTDIR)\*.obj 
	-$(RM) $(OUTDIR)\*.map


distclean: $(PHONY) clean
	-$(RM) $(OUTDIR)\xcopy.exe
