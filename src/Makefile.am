AM_CPPFLAGS = -Wall
ACLOCAL_AMFLAGS = -I m4

lib_LTLIBRARIES = libamc.la
libyam_la_SOURCES = serial.c modbus.c modbus.h
libyam_la_LDFLAGS = -version-info 0:1:0

# Include files to install
libyamincludedir = $(includedir)/amc
libyaminclude_HEADERS = amc.h

# Include files that are part of the source, but not installed
noinst_HEADERS = serial.h

CLEANFILES = *~
