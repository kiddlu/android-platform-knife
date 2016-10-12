#!/bin/bash

#cygwin
if [ -f "$HOME/.winixrc" ]; then
	source ~/.winixrc
fi

#posix
if [ -f "$HOME/.shrc" ]; then
	source ~/.shrc
fi

# Android Debug Kit
# This is a simple wrapper / script for "adb function / shell" */

################### function ###################
adk_meminfo ()
{
	adb root
	adb wait-for-device
while [ 1 -eq 1 ]
do
	adb shell "cat /proc/meminfo"
	adb shell "cat /proc/pagetypeinfo"
	adb shell "cat /proc/slabinfo"
	adb shell "cat /proc/zoneinfo"
	adb shell "cat /proc/vmallocinfo"
	adb shell "cat /proc/vmstat"
	adb shell "cat /proc/meminfo"
	adb shell "procrank"
	adb shell "top -n 1"
	adb shell "free -m"
	adb shell "sleep 5"
done
}

adk_input ()
{
	echo input
}

adk_root ()
{
	adb root
	adb wait-for-device
	for string in `adb shell mount | grep ro, | awk '{printf ("%s@%s\n",$1, $2) }'`; do
		drive=$(echo $string  |awk -F'@' '$0=$1')
		mountpoint=$(echo $string|awk -F'@' '$0=$2')
		adb shell "mount -o remount $drive $mountpoint"
	done
}

adk_panic ()
{
	adb root
	adb wait-for-device
	adb shell "echo c > /proc/sysrq-trigger"
}

adk_listapk ()
{
	adb shell "pm list packages -f" > /tmp/tmplog.pid.$$

	for dir in '/system/app' '/system/priv-app' '/system/vendor' '/system/framework' '/data/app'; do
		echo 
		echo dir: $dir
		cat /tmp/tmplog.pid.$$ | grep $dir
	done

	rm /tmp/tmplog.pid.$$
}
adk_focusedapk ()
{
packages=`adb shell dumpsys activity  | grep mFocusedActivity | awk {'print $4'} | sed 's/\(.*\)\/\.\(.*\)/\1/g'`
adb shell "pm list packages -f" | grep $packages
}

adk_hexdump()
{
	dump_path="/data/hexdump"
	#blk_path="/dev/block/bootdevice/by-name"
	blk_path=`adb shell mount | grep system | awk '{print $1}' | sed "s/\/system//g"`
	adb root
	adb wait-for-device
	adb shell "mkdir $dump_path"
	for partition in `adb shell ls $blk_path | grep -v "system\|cache\|userdata\|udisk"`; do
		partition=`echo "$partition" | tr -d '\r\n'`
		echo "dd if=$blk_path/$partition of=$dump_path/$partition"
		adb shell "dd if=$blk_path/$partition of=$dump_path/$partition"
	done
	adb shell "sync"
	adb pull  $dump_path .
	adb shell "rm -rf $dump_path"
}

adk_cpu-performance()
{
	adb root
	adb wait-for-device
	adb shell stop thermal-engine
	cpus=0
	cpus=`adb shell cat /proc/cpuinfo | grep processor | wc -l`
	cpus=$((cpus - 1))
	for nb in `seq 0 $cpus`; do
		adb shell "echo performance > /sys/devices/system/cpu/cpu$nb/cpufreq/scaling_governor"
		max_freq=`adb shell cat /sys/devices/system/cpu/cpu$nb/cpufreq/cpuinfo_max_freq`
		adb shell "echo $max_freq > /sys/devices/system/cpu/cpu$nb/cpufreq/scaling_min_freq"
		adb shell "echo $max_freq > /sys/devices/system/cpu/cpu$nb/cpufreq/scaling_max_freq"
	done
}

adk_net-shell()
{
	tcpport=5555
	adb disconnect
	adb shell svc wifi enable
	adb root
	adb wait-for-device
	adb shell setprop service.adb.tcp.port $tcpport
	ipaddr=`adb shell "ifconfig wlan0" | grep "inet addr" | awk {'print $2'} | sed {"s/\(.*\):\(.*\)/\2/g"}`
	adb tcpip $tcpport
	echo $ipaddr:$tcpport
	adb wait-for-device
	adb connect $ipaddr:$tcpport
	adb -s $ipaddr:$tcpport shell
}

adk_flash-dir()
{
	webcgi="http://172.16.2.18/cgi-bin/vmlinux-lookup.cgi"
	version=$(adb shell "cat /proc/version" | grep "Linux version")
	if [ $host_platform == "cygwin" ] ; then
		smb_path=$(curl --data-urlencode "version=$version" $webcgi 2> /dev/null | grep "Flashing binary" -A 1 | tail -1)
		unc_path=$(echo ${smb_path#*smb:})
		for string in `net use | grep "Microsoft Windows Network" | awk '{printf ("%s@%s\n",$2, $3)}'`; do
			drive=$(echo $string  |awk -F'@' '$0=$1')
			map_point=$(echo $string|awk -F'@' '$0=$2'| sed "s/\\\/\//g")
			echo $unc_path | grep $map_point > /dev/null
			if [ $? == 0 ]; then
				#echo $drive $map_point $win_path
				map_point_regex=$(echo $map_point | sed "s/\//\\\\\//g")
				drive_regex=$(echo $drive | sed "s/\:/\\\:/g")
				win_path=$(echo "$unc_path" | sed "s/$map_point_regex/$drive_regex/g")
				echo $win_path | tee /dev/console | tr '\n' ' ' | clip
			fi
		done
	fi
}

adk_symbol-dir()
{
	webcgi="http://172.16.2.18/cgi-bin/vmlinux-lookup.cgi"
	version=$(adb shell "cat /proc/version" | grep "Linux version")
	if [ $host_platform == "cygwin" ] ; then
		smb_path=$(curl --data-urlencode "version=$version" $webcgi 2> /dev/null | grep "kernel symbols" -A 1 | tail -1)
		unc_path=$(echo ${smb_path#*smb:})
		for string in `net use | grep "Microsoft Windows Network" | awk '{printf ("%s@%s\n",$2, $3)}'`; do
			drive=$(echo $string  |awk -F'@' '$0=$1')
			map_point=$(echo $string|awk -F'@' '$0=$2'| sed "s/\\\/\//g")
			echo $unc_path | grep $map_point > /dev/null
			if [ $? == 0 ]; then
				#echo $drive $map_point $win_path
				map_point_regex=$(echo $map_point | sed "s/\//\\\\\//g")
				drive_regex=$(echo $drive | sed "s/\:/\\\:/g")
				win_path=$(echo "$unc_path" | sed "s/$map_point_regex/$drive_regex/g")
				echo $win_path | tee /dev/console | tr '\n' ' ' | clip
			fi
		done
	fi
}

adk_fix-usb()
{
	if [ $host_platform == "cygwin" ]; then
		outpath=$USERPROFILE
	else
		outpath=$HOME
	fi
	adb kill-server
	curl https://raw.githubusercontent.com/kiddlu/adbusbini/master/adb_usb.ini > $outpath/.android/adb_usb.ini
	adb start-server
}

adk_usb-diag()
{
	adb root
	adb wait-for-device
}

adk_pmap-all()
{
	for pid in `adb shell "ps" | awk '{print $2}' `; do
		cmdline=`adb shell cat /proc/$pid/cmdline`
		if [ -n "$cmdline" ]; then
			adb shell pmap $pid
		fi
	done
}
################### main ###################

host_platform=""
case `uname` in
    Linux) host_platform=linux ;;
    FreeBSD) host_platform=fbsd ;;
    *CYGWIN*) host_platform=cygwin ;;
    *MINGW*) host_platform=mingw ;;
    Darwin) host_platform=darwin ;;
esac

if [ $# -lt 1 ] ; then 
	echo "Android Debug Kit"
	echo "adk \"cmd\" to execute"
	exit 1;
fi

case "$1" in  
	ftyrst)
		adb shell am broadcast -a android.intent.action.MASTER_CLEAR;;
	smartisan-active)
		adb shell am start -n com.smartisanos.setupwizard/com.smartisanos.setupwizard.SetupWizardCompleteActivity;;
	smartisan-launcher)
		adb shell am start -n com.smartisanos.launcher/com.smartisanos.launcher.Launcher;;
	hexdump)
		adk_hexdump;;
	meminfo)
		adk_meminfo;;
	flash-dir)
		adk_flash-dir;;
	symbol-dir)
		adk_symbol-dir;;
	fix-usb)
		adk_fix-usb;;
	usb-diag)
		adk_usb-diag;;
	pmap-all)
		adk_pmap-all;;
	root)
		adk_root;;
	cpu-performance)
		adk_cpu-performance;;
	panic)
		adk_panic;;
	listapk)
		adk_listapk;;
	focusedapk)
		adk_focusedapk;;
	net-shell)
		adk_net-shell;;
	*) adb shell $*;;
esac