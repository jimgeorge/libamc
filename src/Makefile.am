AM_CPPFLAGS = -Wall
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libamc.la
libamc_la_SOURCES = serial.c amc.c amc.h crc.c crc.h
libamc_la_LDFLAGS = -version-info 0:1:0

# Include files to install
libamcincludedir = $(includedir)/amc
libamcinclude_HEADERS = amc.h

# Include files that are part of the source, but not installed
noinst_HEADERS = serial.h

CLEANFILES = *~
