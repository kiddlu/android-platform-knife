@echo on
echo Wlan debug
set curr_path=%~dp0 
pushd %curr_path%

..\app\adb root
..\app\adb shell "sleep 3"
..\app\adb remount
..\app\adb push ./bin/iwpriv /system/bin/iwpriv
..\app\adb push ./bin/wpa_cli /system/bin/wpa_cli
..\app\adb push ./bin/tcpdump /system/bin/tcpdump

..\app\adb shell "chmod 777 /system/bin/iwpriv"
..\app\adb shell "chmod 777 /system/bin/wpa_cli"
..\app\adb shell "chmod 777 /system/bin/tcpdump"

echo Wating...
..\app\adb shell "svc wifi disable"
..\app\adb shell "sleep 1"
..\app\adb shell "svc wifi enable"
..\app\adb shell "sleep 5"

..\app\adb shell "wpa_cli -i wlan0 -p /data/misc/wifi/sockets/ log_level EXCESSIVE"

..\app\adb shell "iwpriv wlan0 dump 211 32 1"
..\app\adb shell "iwpriv wlan0 dump 13 0 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 1 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 2 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 3 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 4 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 5 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 6 6 1"
..\app\adb shell "iwpriv wlan0 dump 13 7 6 1"

..\app\adb shell "iwpriv wlan0 setwlandbg 0 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 1 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 2 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 3 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 4 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 5 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 6 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 7 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 8 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 9 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 10 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 11 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 12 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 13 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 14 9 1"
..\app\adb shell "iwpriv wlan0 setwlandbg 15 9 1"

..\app\adb shell "tcpdump -i wlan0 -s 0 -w /sdcard/ip.pcap"

popd
::pause