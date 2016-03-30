#!/bin/sh

DIR_PATH=`readlink -f $0 | xargs dirname`

ADB_EXE=$DIR_PATH/apps/adb
FASTBOOT_EXE=$DIR_PATH/apps/fastboot
IMG2SIMG_EXE=$DIR_PATH/apps/img2simg
PACKSPARSEIMG_EXE=$DIR_PATH/apps/packsparseimg
#BUSYBOX_EXE=$DIR_PATH/apps/busybox

FLASH_FULL=0
FLASH_USERDATA=0
FLASH_MASTERCLEAN=0

if [ $UID -ne 0 ]; then
    echo "Superuser privileges are required to run this script."
    echo "e.g. \"sudo $0\""
    exit 1
fi

$ADB_EXE wait-for-device
$ADB_EXE reboot bootloader

# AP
$FASTBOOT_EXE flash xbl xbl.elf
$FASTBOOT_EXE flash xbl xbl.elf
$FASTBOOT_EXE flash aboot emmc_appsboot.mbn
$FASTBOOT_EXE flash abootbak emmc_appsboot.mbn
$FASTBOOT_EXE flash boot boot.img
$FASTBOOT_EXE flash recovery recovery.img

$PACKSPARSEIMG_EXE -t system -o /tmp/ -x rawprogram_unsparse4.xml
$IMG2SIMG_EXE /tmp/system.raw /tmp/system.img
$FASTBOOT_EXE flash system /tmp/system.img
rm -f /tmp/system.img /tmp/system.raw

$PACKSPARSEIMG_EXE -t cache -o  /tmp/ -x rawprogram_unsparse4.xml
$IMG2SIMG_EXE  /tmp/cache.raw  /tmp/cache.img
$FASTBOOT_EXE% flash cache  /tmp/cache.img
rm -f  /tmp/cache.img  /tmp/cache.raw

# OEM 
$FASTBOOT_EXE flash tz tz.mbn
$FASTBOOT_EXE flash tzbak tz.mbn
$FASTBOOT_EXE flash rpm rpm.mbn
$FASTBOOT_EXE flash rpmbak rpm.mbn
$FASTBOOT_EXE flash hyp hyp.mbn
$FASTBOOT_EXE flash hypbak hyp.mbn
$FASTBOOT_EXE flash pmic pmic.elf
$FASTBOOT_EXE flash pmicbak pmic.elf
$FASTBOOT_EXE flash modem NON-HLOS.bin
$FASTBOOT_EXE flash bluetooth BTFM.bin
$FASTBOOT_EXE flash keymaster keymaster.mbn
$FASTBOOT_EXE flash keymasterbak keymaster.mbn
$FASTBOOT_EXE flash sec sec.dat
$FASTBOOT_EXE flash cmnlib cmnlib.mbn
$FASTBOOT_EXE flash cmnlibbak cmnlib.mbn
$FASTBOOT_EXE flash cmnlib64 cmnlib64.mbn
$FASTBOOT_EXE flash cmnlib64bak cmnlib64.mbn
$FASTBOOT_EXE flash dsp adspso.bin
$FASTBOOT_EXE flash mdtp mdtp.img
$FASTBOOT_EXE flash devcfg devcfg.mbn
$FASTBOOT_EXE flash devcfgbak devcfg.mbn
$FASTBOOT_EXE flash cdt cdt.bin

# Full Flash
if [ "$FLASH_FULL" == 1 ]; then
	$FASTBOOT_EXE flash persist persist.img
fi
if [ "$FLASH_USERDATA" == 1 ]; then
	$PACKSPARSEIMG_EXE% -t userdata -o /tmp/ -x rawprogram_unsparse0.xml
	$IMG2SIMG_EXE% /tmp/userdata.raw /tmp/userdata.img	
	$FASTBOOT_EXE% flash userdata /tmp/userdata.img
	rm -f /tmp/userdata.img /tmp/userdata.raw
fi

# reboot
$FASTBOOT_EXE reboot

# maybe should do master clean if not flash userdata partition
if [ "$FLASH_MASTERCLEAN" == 1 ]; then
#$ADB_EXE% wait-for-device
#$ADB_EXE shell getprop sys.boot_completed > "%TMP%\boot_completed"
#set /p BOOTFLAG=<"%TMP%\boot_completed"
#if %BOOTFLAG% == 0 (
#	echo waiting...
#	%BUSYBOX_EXE% sleep 5
#	goto LOOP
#)
#$ADB_EXE shell am broadcast -a android.intent.action.MASTER_CLEAR
fi

echo "###################################"
echo "###### Fastboot Flash Done!! ######"
echo "###################################"