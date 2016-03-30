@echo OFF

set PID=1

::..\app\adb shell "pgrep tcpdump | xargs kill -9"

.\apps\adb shell strace -T -C -p %PID% > strace-%PID%.txt