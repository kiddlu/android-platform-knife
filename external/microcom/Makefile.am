PROGRAM = microcom
AUTOMAKE_OPTIONS = gnu

INCLUDES = -I$(top_srcdir)

bin_PROGRAMS = $(PROGRAM)
microcom_SOURCES = autodet.c help.c  microcom.c  mux.c  script.c debug.h  microcom.h  script.h

ChangeLog.tmp:
	git log --format="%ai %aN %n%n%x09* %B%n" >ChangeLog.tmp

changelog: ChangeLog.tmp
