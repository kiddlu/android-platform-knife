::pls download WDK 7.1 from https://www.microsoft.com/en-us/download/details.aspx?id=11800 and install

set WDKROOT=C:\WinDDK\7600.16385.1\
set THIS_PATH=%~dp0

call %THIS_PATH%\setenv.bat %WDKROOT% fre x64 WIN7
::call %THIS_PATH%\setenv.bat %WDKROOT% fre x86 WIN7

cd %THIS_PATH%\usb\api\
build.exe -cbeEIFZ
cd %THIS_PATH%\usb\winusb\
build.exe -cbeEIFZ
cd %THIS_PATH%
