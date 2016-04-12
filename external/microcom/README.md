microcom
========

Microcom is a minicom-like serial terminal emulator with scripting support. 
The requirement for this program was to be small enough to fit on a floppy-based Linux distribution - such as the one from Linux Router Project.  
The original project and its source code was on http://microcom.port5.com/m102.tar.gz (no more available) and found only at http://sources.buildroot.net.
Its content is available under label 1.02.

Features:
=========
- Add/strip line feeds, local echo control
- Hardware/software flow control support
- 8 bit, one stop, no parity
- Support for standard speeds form 1200 to 460800 bps
- Default UNIX tty emulation
- BASIC like script language support
- Distribution: source code, LINUX executable, LRP based floppy
- Automatic modem detection

Please have a look in the provided index.html file for details.
---------------------------------------------------------------

Building:
=========

- To build this program using autotools enter the following commands:
  ./configure && make
For details please refer to autotools documentation and have a look to the configure script's options (./configure --help).

- To build this program without using the autotools just enter the following commands:
  touch config.h && make -f Makefile.old
