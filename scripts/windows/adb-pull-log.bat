@echo OFF
echo Pulling...

rd /s /q log
md log

::..\app\adb shell "pgrep tcpdump | xargs kill -9"

..\app\adb pull /data/logs/ log/syslog/
..\app\adb pull /data/misc/wifi/ log/wifi/
..\app\adb pull /sdcard/ip.pcap log/ip.pcap

::..\app\adb shell dmesg > log/dmesg
echo Done
pause