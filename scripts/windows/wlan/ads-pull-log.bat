@echo OFF
echo Pulling...

rd /s /q log
md log

adb pull /data/logs/ log/syslog/
adb pull /data/misc/wifi/ log/wifi/
adb pull /sdcard/ip.pcap log/ip.pcap

echo Done
pause