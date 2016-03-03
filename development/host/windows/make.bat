set WINDDK=C:\WinDDK\7600.16385.1\

%WINDDK%\bin\setenv.bat %WINDDK% x64 WIN7
cd %~dp0\usb\api\ && build.exe build -cbeEIFZ
cd %~dp0\usb\winusb\ && build.exe build -cbeEIFZ
