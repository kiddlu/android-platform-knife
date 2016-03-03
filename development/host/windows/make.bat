set WINDDK=C:\WinDDK\7600.16385.1\

%WINDDK%\bin\setenv.bat %WINDDK% fre x64 WIN7
cd %~dp0\usb\api\ && build.exe -cbeEIFZ
cd %~dp0\usb\winusb\ && build.exe -cbeEIFZ
