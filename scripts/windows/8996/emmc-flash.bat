::  This is edl flash batch file for surabaya/colombo.
::  Make sure you phone can "adb reboot edl" to edl mode,
::  or enter edl mode by yourself.Enjoy your caffee time.

::  Base on QFIL 2.0.0.2

@echo off
set DIR_PATH=%~dp0

set ADB_EXE=%DIR_PATH%\apps\adb.exe
set BUSYBOX_EXE=%DIR_PATH%\apps\busybox.exe
set FHLOADER_EXE=%DIR_PATH%\apps\fh_loader.exe
set SAHARA_EXE=%DIR_PATH%\apps\QSaharaServer.exe
set ENUMCOM_EXE=%DIR_PATH%\apps\enumcom.exe
set EMMCDL_EXE=%DIR_PATH%\apps\emmcdl.exe

pushd %TMP%

%ADB_EXE% devices | %BUSYBOX_EXE% sed -n 2p | %BUSYBOX_EXE% grep device >nul
if "%ERRORLEVEL%"=="0" (
	%ADB_EXE% reboot edl
	%BUSYBOX_EXE% sleep 2
)

set LOOP_TIME=0
:LOOP
set COM_NUMBER=

::for /f "delims=" %%i in (' %EMMCDL_EXE% -l | %BUSYBOX_EXE% grep QDLoader  | %BUSYBOX_EXE% awk {"print $5"} | %BUSYBOX_EXE% sed "s/(//g" | %BUSYBOX_EXE% sed "s/)//g" ') do (set COM_NUMBER=%%i)
for /f "delims=" %%i in ('%ENUMCOM_EXE% ^| %BUSYBOX_EXE% grep QCUSB ^| %BUSYBOX_EXE% awk {"print $2"}') do (set COM_NUMBER=%%i)
%BUSYBOX_EXE% echo %COM_NUMBER%

if "%COM_NUMBER%"=="" (
	%BUSYBOX_EXE% echo "wait for COM(9008)"
	%BUSYBOX_EXE% sleep 2

	set /a LOOP_TIME+=1
	if %LOOP_TIME%==30 (
		goto NODEVICE
	) else (
		goto LOOP
	)
)

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram_unsparse0.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch0.xml
%BUSYBOX_EXE% sleep 1

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram1.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch1.xml
%BUSYBOX_EXE% sleep 1

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram2.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch2.xml
%BUSYBOX_EXE% sleep 1

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram3.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch3.xml
%BUSYBOX_EXE% sleep 1

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram_unsparse4.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch4.xml
%BUSYBOX_EXE% sleep 1

%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x rawprogram5.xml
%EMMCDL_EXE% -p %COM_NUMBER% -MemoryName ufs -f prog_ufs_firehose_8996_ddr.elf -x patch5.xml
%BUSYBOX_EXE% sleep 1

rem REST
%FHLOADER_EXE% --port=\\.\%COM_NUMBER% --noprompt --showpercentagecomplete --zlpawarehost=1 --memoryname=UFS --setactivepartition=1 --reset
goto END

:NODEVICE
echo ###################################
echo ######   No Device Found!!   ######
echo ###################################
echo.

popd
pause
exit /b 1

:END
echo ###################################
echo ######  EDL Flash Done!!!  ########
echo ###################################
echo.

popd
pause