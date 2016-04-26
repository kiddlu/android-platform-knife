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

if [ $# -lt 1 ] ; then 
	echo "Android Debug Kit"
	echo "\nadk \"cmd\" to execute\n"
	exit 1;
fi

case "$1" in  
	ftyrst)
		adb shell am broadcast -a android.intent.action.MASTER_CLEAR;;
	smartisan-active)
		adb shell am start -n com.smartisanos.setupwizard/com.smartisanos.setupwizard.SetupWizardCompleteActivity;;
	*) echo "no target for $1" >&2;;
esac