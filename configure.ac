dnl Autoconf script for libamnc

AC_INIT(libamc, 0.1.0, jgeorge@engr.colostate.edu)
AC_CONFIG_SRCDIR([src/amc.c])
AC_CONFIG_MACRO_DIR([m4])
AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE
#AM_DISABLE_STATIC
dnl Remove me once debug is done
AM_DISABLE_SHARED

# Silent mode make (use make V=1 to override)
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T

AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

AC_HEADER_STDC
AC_CHECK_HEADERS([termios.h unistd.h endian.h poll.h errno.h fcntl.h])

AC_C_CONST
AC_CHECK_FUNCS([bzero strtol ntohs htons poll])
AC_PROG_CXX
AC_PROG_RANLIB

AC_OUTPUT([
	Makefile
	src/Makefile
	tests/Makefile
	libamc.pc
])

