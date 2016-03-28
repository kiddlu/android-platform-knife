@echo OFF
set DIR_PATH=%~dp0

set ADB_EXE=%DIR_PATH%\apps\adb.exe
set FASTBOOT_EXE=%DIR_PATH%\apps\fastboot.exe
set IMG2SIMG_EXE=%DIR_PATH%\apps\img2simg.exe
set PACKSPARSEIMG_EXE=%DIR_PATH%\apps\packsparseimg.exe
set BUSYBOX_EXE=%DIR_PATH%\apps\busybox.exe

set FLASH_FULL=0
set FLASH_USERDATA=0

%ADB_EXE% reboot bootloader

rem AP
%FASTBOOT_EXE% flash xbl xbl.elf
%FASTBOOT_EXE% flash xblbak xbl.elf
%FASTBOOT_EXE% flash aboot emmc_appsboot.mbn
%FASTBOOT_EXE% flash abootbak emmc_appsboot.mbn
%FASTBOOT_EXE% flash boot boot.img
%FASTBOOT_EXE% flash recovery recovery.img

%PACKSPARSEIMG_EXE% -t system -o %TMP%\ -x rawprogram_unsparse4.xml
%IMG2SIMG_EXE% %TMP%\system.raw %TMP%\system.img
%FASTBOOT_EXE% flash system %TMP%\system.img
del %TMP%\system.img %TMP%\system.raw

%PACKSPARSEIMG_EXE% -t cache -o %TMP%\ -x rawprogram_unsparse0.xml
%IMG2SIMG_EXE% %TMP%\cache.raw %TMP%\cache.img
%FASTBOOT_EXE% flash cache %TMP%\cache.img
del %TMP%\cache.img %TMP%\cache.raw

rem OEM
%FASTBOOT_EXE% flash tz tz.mbn
%FASTBOOT_EXE% flash tzbak tz.mbn
%FASTBOOT_EXE% flash rpm rpm.mbn
%FASTBOOT_EXE% flash rpmbak rpm.mbn
%FASTBOOT_EXE% flash hyp hyp.mbn
%FASTBOOT_EXE% flash hypbak hyp.mbn
%FASTBOOT_EXE% flash pmic pmic.elf
%FASTBOOT_EXE% flash pmicbak pmic.elf
%FASTBOOT_EXE% flash modem NON-HLOS.bin
%FASTBOOT_EXE% flash bluetooth BTFM.bin
%FASTBOOT_EXE% flash keymaster keymaster.mbn
%FASTBOOT_EXE% flash keymasterbak keymaster.mbn
%FASTBOOT_EXE% flash sec sec.dat
%FASTBOOT_EXE% flash cmnlib cmnlib.mbn
%FASTBOOT_EXE% flash cmnlibbak cmnlib.mbn
%FASTBOOT_EXE% flash cmnlib64 cmnlib64.mbn
%FASTBOOT_EXE% flash cmnlib64bak cmnlib64.mbn
%FASTBOOT_EXE% flash dsp adspso.bin
%FASTBOOT_EXE% flash mdtp mdtp.img
%FASTBOOT_EXE% flash devcfg devcfg.mbn
%FASTBOOT_EXE% flash devcfgbak devcfg.mbn
%FASTBOOT_EXE% flash cdt cdt.bin

rem full flash
if %FLASH_PERSIST% == 1 (
	%FASTBOOT_EXE% flash persist persist.img
)
if %FLASH_USERDATA% == 1 (
	%PACKSPARSEIMG_EXE% -t userdata -o %TMP%\ -x rawprogram_unsparse0.xml
	%IMG2SIMG_EXE% %TMP%\userdata.raw %TMP%\userdata.img	
	%FASTBOOT_EXE% flash userdata %TMP%\userdata.img
	del %TMP%\userdata.img %TMP%\userdata.raw
)

rem reboot
%FASTBOOT_EXE% reboot

rem do master clean if not flash userdata partition
if not %FLASH_USERDATA% == 1 (
	%ADB_EXE% wait-for-device
	%BUSYBOX_EXE% sleep 20
	%ADB_EXE% shell am broadcast -a android.intent.action.MASTER_CLEAR
)

echo ###################################
echo ###### Fastboot Flash Done!! ######
echo ###################################

pause