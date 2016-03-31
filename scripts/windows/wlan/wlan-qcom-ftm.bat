@echo OFF

..\apps\adb root
..\apps\adb wait-for-device
..\apps\adb shell svc wifi disable
..\apps\adb shell insmod /system/lib/module/wlan.ko mode=5
..\apps\adb shell ftmdaemon -n