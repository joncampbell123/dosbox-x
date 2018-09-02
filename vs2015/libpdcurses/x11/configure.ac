dnl Process this file with autoconf to produce a configure script.

AC_INIT([PDCurses], [3.6], [wmcbrine@gmail.com], [PDCurses])
AC_CONFIG_SRCDIR([pdcx11.h])

AC_SUBST(prefix)

AC_PROG_CC

AC_CONFIG_HEADER(config.h)
dnl Checks for system first
AC_CANONICAL_SYSTEM([])

on_qnx=no
case "$target" in
	*hp-hpux*)
		SYS_DEFS="-D_HPUX_SOURCE"
		;;
	*dec-osf*)
		SYS_DEFS="-D_XOPEN_SOURCE_EXTENDED"
		;;
	*pc-sco*)
		SYS_DEFS="-UM_XENIX -b elf"
		;;
	*qnx*)
		on_qnx=yes
		SYS_DEFS="-Q"
		;;
	*)
		;;
esac
AC_SUBST(SYS_DEFS)

MH_CHECK_MAX_SIGNALS(NSIG __sys_nsig)

dnl Check for other programs.
AC_PROG_RANLIB
AC_PROG_INSTALL
AC_PROG_MAKE_SET

dnl ensure that the system has System V IPC support
MH_IPC

if test $ac_cv_header_stdc != yes; then
	AC_MSG_ERROR([Need ANSI C headers])
fi

dnl Checks for libraries.

dnl we check for dlfcn.h or dl.h so we know if we can build shared objects
AC_CHECK_HEADERS(fcntl.h \
	sys/time.h \
	sys/select.h \
	dlfcn.h \
	dl.h
)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_TIME
MH_CHECK_LIB(socket nls)
AC_SUBST(MH_EXTRA_LIBS)
MH_CHECK_CC_O

dnl Checks for library functions.
AC_CHECK_FUNCS(vsscanf usleep poll vsnprintf)

dnl Check for X includes and X libraries
AC_PATH_X
MH_CHECK_X_INC
MH_CHECK_X_LIB
MH_CHECK_X_HEADERS(DECkeysym.h Sunkeysym.h xpm.h)
MH_CHECK_X_TYPEDEF(XPointer)
dnl
dnl extra for xpm library
if test $ac_cv_header_xpm_h = yes; then
	MH_XLIBS="$MH_XLIBS -lXpm"
fi
dnl ---------- allow --enable-debug to compile in debug mode ---------
AC_ARG_ENABLE(debug,
	[  --enable-debug          turn on debugging],
	[with_debug=$enableval],
	[with_debug=no],
)
cflags_g="`echo $CFLAGS | grep -c '\-g'`"
cflags_O="`echo $CFLAGS | grep -c '\-O'`"

if test "$with_debug" = yes; then
	if test "$cflags_g" = "0"; then
		CFLAGS="${CFLAGS} -g"
	fi
	if test "$cflags_O" != "0"; then
		CFLAGS="`echo ${CFLAGS} | sed -e s/-O.//`"
	fi
	CFLAGS="${CFLAGS} -DPDCDEBUG"
else
	if test "$cflags_O" = "0"; then
		CFLAGS="${CFLAGS} -O"
	fi
	if test "$cflags_g" != "0"; then
		CFLAGS="`echo ${CFLAGS} | sed -e s/-g//`"
	fi
fi
if test "$ac_cv_prog_CC" = "gcc"; then
	if test "$with_debug" = yes; then
		CFLAGS="${CFLAGS} -Wall"
	else
		CFLAGS="-O2 -Wall -fomit-frame-pointer"
	fi
fi
if test "$on_qnx" = yes; then
	if test "$with_debug" = yes; then
		CFLAGS="-g"
	else
		CFLAGS="-Otax"
	fi
fi

dnl --------------- check for wide character support -----------------
dnl allow --enable-widec to include wide character support
AC_ARG_ENABLE(widec,
	[  --enable-widec          include support for wide characters],
	[with_widec=$enableval],
	[with_widec=no],
)
PDC_WIDE=""
if test "$with_widec" = yes; then
	PDC_WIDE="-DPDC_WIDE"
	SYS_DEFS="$SYS_DEFS $PDC_WIDE"
fi
AC_SUBST(PDC_WIDE)

dnl -------------------- check for XIM support -----------------------
dnl allow --enable-xim to include XIM support
AC_ARG_ENABLE(xim,
	[  --enable-xim            include support for XIM],
	[with_xim=$enableval],
	[with_xim=no],
)
if test "$with_xim" = yes; then
	SYS_DEFS="$SYS_DEFS -DPDC_XIM"
fi

dnl ------------------------ force UTF-8? ----------------------------
dnl allow --enable-force-utf8 to override locale settings
AC_ARG_ENABLE(force-utf8,
	[  --enable-force-utf8     override locale settings; use UTF-8],
	[force_utf8=$enableval],
	[force_utf8=no],
)
if test "$force_utf8" = yes; then
	SYS_DEFS="$SYS_DEFS -DPDC_FORCE_UTF8"
fi

dnl ----------------- check for Purify support -----------------------
dnl allow --enable-purify to enable linking with Purify
AC_ARG_ENABLE(purify,
	[  --enable-purify         link with Purify (TM)],
	[with_purify=$enableval],
	[with_purify=no],
)
if test "$with_purify" = yes; then
	PURIFY="purify"
else
	PURIFY=""
fi
AC_SUBST(PURIFY)

dnl --------------------- check for Xaw3d library --------------------
dnl allow --with-xaw3d to link with PDCurses
AC_ARG_WITH(xaw3d,
	[  --with-xaw3d            link with Xaw3d],
	[with_xaw3d=$withval],
	[with_xaw3d=no],
)
if test "$with_xaw3d" = yes; then
	AC_DEFINE([USE_XAW3D], [1],
		[Define if you want to use Xaw3d library]
	)
fi

dnl --------------------- check for neXtaw library -------------------
dnl allow --with-nextaw to link with PDCurses
AC_ARG_WITH(nextaw,
	[  --with-nextaw           link with neXtaw],
	[with_nextaw=$withval],
	[with_nextaw=no],
)
if test "$with_nextaw" = yes; then
	AC_DEFINE([USE_NEXTAW], [1],
		[Define if you want to use neXtaw library]
	)
fi

dnl -------------- check how to make shared libraries ----------------
dnl Force the ability of shared library usage
MH_SHARED_LIBRARY(XCurses)

AC_CONFIG_FILES([Makefile xcurses-config])
AC_OUTPUT

AC_DEFINE([PDC_MAX_SIGNALS], [],
	[Define as the system defined limit for number of signals]
)

AC_DEFINE([HAVE_DECKEYSYM_H], [],
	[Define if you have the <DECkeySym.h> header file]
)

AC_DEFINE([HAVE_SUNKEYSYM_H], [],
	[Define if you have the <Sunkeysym.h> header file]
)

AC_DEFINE([HAVE_XPM_H], [],
	[Define if you have the <xpm.h> header file]
)

AC_DEFINE([USE_XAW3D], [],
	[Define if you want to use Xaw3d library]
)

AC_DEFINE([USE_NEXTAW], [],
	[Define if you want to use neXtaw library]
)

AC_DEFINE([XPOINTER_TYPEDEFED], [],
	[Define XPointer is typedefed in X11/Xlib.h]
)
