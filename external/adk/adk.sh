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
# This is a simple wrapper for "adb function / shell" */

#function
adk_meminfo ()
{
	echo meminfo
}

adk_input ()
{
	echo input
}

adk_curapk ()
{
PACKAGE=`adb shell dumpsys activity  |grep mFocusedActivity | awk {'print $4'} | sed 's/\(.*\)\/\.\(.*\)/\1/g'`
adb shell pm list packages -f | grep $PACKAGE
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
	meminfo)
		adk_meminfo;;
	curapk)
		adk_curapk;;
	*) adb shell $*;;
esac