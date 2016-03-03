@echo on
echo Wlan debug
set curr_path=%~dp0 
pushd %curr_path%

..\app\adb root
..\app\adb shell "sleep 3"
..\app\adb remount
..\app\adb push /bin/wl /system/bin/wl
..\app\adb push ./bin/wpa_cli /system/bin/wpa_cli
..\app\adb push ./bin/tcpdump /system/bin/tcpdump

..\app\adb shell "chmod 777 /system/bin/wl"
..\app\adb shell "chmod 777 /system/bin/wpa_cli"
..\app\adb shell "chmod 777 /system/bin/tcpdump"

..\app\adb shell "echo 10 > /sys/module/bcmdhd/parameters/dhd_console_ms"
..\app\adb shell "echo 7 > /sys/module/bcmdhd/parameters/dhd_msg_level"
..\app\adb shell "echo 7 > /sys/module/bcmdhd/parameters/wl_dbg_level"

echo Wating...
..\app\adb shell "svc wifi disable"
..\app\adb shell "sleep 1"
..\app\adb shell "svc wifi enable"
..\app\adb shell "sleep 5"

..\app\adb shell "wpa_cli -i wlan0 -p /data/misc/wifi/sockets/ log_level EXCESSIVE"

..\app\adb shell "dhdutil -i wlan0 dconpoll 10"
..\app\adb shell "dhdutil -i wlan0 wdtick 1"

..\app\adb shell "tcpdump -i wlan0 -s 0 -w /sdcard/ip.pcap"

popd
::pause