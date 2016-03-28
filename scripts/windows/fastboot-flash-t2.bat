@echo OFF
set DIR_PATH=%~dp0

set ADB_EXE=%DIR_PATH%\apps\adb.exe
set FASTBOOT_EXE=%DIR_PATH%\apps\fastboot.exe
set IMG2SIMG_EXE=%DIR_PATH%\apps\img2simg.exe
set PACKSPARSEIMG_EXE=%DIR_PATH%\apps\packsparseimg.exe

set FLASH_FULL=0
set FLASH_USERDATA=0

%ADB_EXE% reboot bootloader

rem AP
%FASTBOOT_EXE% flash sbl1 sbl1.mbn
%FASTBOOT_EXE% flash sbl1bak sbl1.mbn
%FASTBOOT_EXE% flash aboot emmc_appsboot.mbn
%FASTBOOT_EXE% flash abootbak emmc_appsboot.mbn
%FASTBOOT_EXE% flash boot boot.img
%FASTBOOT_EXE% flash recovery recovery.img

%PACKSPARSEIMG_EXE% -t system -o %TMP%\
%IMG2SIMG_EXE% %TMP%\system.raw %TMP%\system.img
%FASTBOOT_EXE% flash system %TMP%\system.img
del %TMP%\system.img %TMP%\system.raw

%PACKSPARSEIMG_EXE% -t cache -o %TMP%\
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
%FASTBOOT_EXE% flash pmic pmic.mbn
%FASTBOOT_EXE% flash pmicbak pmic.mbn
%FASTBOOT_EXE% flash modem NON-HLOS.bin
%FASTBOOT_EXE% flash bluetooth BTFM.bin
%FASTBOOT_EXE% flash sdi sdi.mbn
%FASTBOOT_EXE% flash sec sec.dat

rem smartisan
%FASTBOOT_EXE% flash security security.img
%FASTBOOT_EXE% flash alterable alterable.bin
%FASTBOOT_EXE% flash splash splash.img
%FASTBOOT_EXE% flash udisk udisk.bin

rem Full Flash
if %FLASH_FULL% == 1 (
	%FASTBOOT_EXE% flash persist persist.img
	%FASTBOOT_EXE% flash fsg fs_image.tar.gz.mbn.img
	%FASTBOOT_EXE% flash modemst1 dummy_fsg_both0.bin
	%FASTBOOT_EXE% flash modemst2 dummy_fsg_both0.bin
	%FASTBOOT_EXE% flash DDR ddr.bin
)

if %FLASH_USERDATA% == 1 (
	%PACKSPARSEIMG_EXE% -t userdata -o %TMP%\
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