#!/bin/bash

#set -x

usage() {
cat <<USAGE

Usage: checkmem.sh [ -s serialnum ]

if no serialnum, use defautl device

USAGE
}

SERNUM=0

while getopts "hs:" arg; do
    case "$arg" in
        h) usage; exit 0;;
        s) SERNUM="$OPTARG";;
    esac
done
shift $(($OPTIND - 1))

if [ $SERNUM == 0 ]; then
    ADB_CMD="adb"
else
    ADB_CMD="adb -s $SERNUM"
fi

$ADB_CMD root
$ADB_CMD wait-for-device

count=0

while [ 1 -eq 1 ]
do
    echo "########## meminfo ##########"
	  $ADB_CMD shell "cat /proc/meminfo"
    echo "########## pagetypeinfo ##########"
	  $ADB_CMD shell "cat /proc/pagetypeinfo"
    echo "########## slabinfo ##########"
	  $ADB_CMD shell "cat /proc/slabinfo"
    echo "########## zoneinfo ##########"
	  $ADB_CMD shell "cat /proc/zoneinfo"
    echo "########## vmallocinfo ##########"
	  $ADB_CMD shell "cat /proc/vmallocinfo"
    echo "########## vmstat ##########"
	  $ADB_CMD shell "cat /proc/vmstat"
    echo "########## meminfo ##########"
	  $ADB_CMD shell "cat /proc/meminfo"
    echo "########## top ##########"
	  $ADB_CMD shell "top -n 1"
    echo "########## free ##########"
	  $ADB_CMD shell "free -m"
    echo "########## procrank ##########"
	  $ADB_CMD shell "procrank"
    echo "########## dumpsys procstats ##########"
	  $ADB_CMD shell "dumpsys procstats"
    let count=$count+1
    echo "########## Loop $count ##########"
	sleep 5
done

#echo scan > /sys/kernel/debug/kmemleak
#cat /sys/kernel/debug/kmemleak > /data/kmem_leak_$num.txt
