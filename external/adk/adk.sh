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

#function
adk_meminfo ()
{
	adb root
	adb wait-for-device
while [ 1 -eq 1 ]
do
	adb shell cat /proc/meminfo
	adb shell cat /proc/pagetypeinfo
	adb shell cat /proc/slabinfo
	adb shell cat /proc/zoneinfo
	adb shell cat /proc/vmallocinfo
	adb shell cat /proc/vmstat
	adb shell cat /proc/meminfo
	adb shell procrank
	adb shell top -n 1
	adb shell free -m
	adb shell sleep 5
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
		adb shell mount -o remount $drive $mountpoint
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
	adb shell pm list packages -f > /tmp/tmplog.pid.$$

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
adb shell pm list packages -f | grep $packages
}

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
	meminfo)
		adk_meminfo;;
	root)
		adk_root;;
	panic)
		adk_panic;;
	listapk)
		adk_listapk;;
	focusedapk)
		adk_focusedapk;;
	*) adb shell $*;;
esac