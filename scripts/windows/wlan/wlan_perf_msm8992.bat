@echo on
echo Wlan debug
set curr_path=%~dp0 
pushd %curr_path%

..\app\adb root

..\app\adb shell "echo 12582912> /proc/sys/net/core/wmem_max"
..\app\adb shell "echo 12582912> /proc/sys/net/core/rmem_max"
..\app\adb shell "echo \"10240 87380 12582912\" > /proc/sys/net/ipv4/tcp_rmem"
..\app\adb shell "echo \"10240 87380 12582912\" > /proc/sys/net/ipv4/tcp_wmem"
..\app\adb shell "echo \"12582912 12582912 12582912\" > /proc/sys/net/ipv4/tcp_mem"

..\app\adb shell "stop thermal-engine"
..\app\adb shell "echo 0 > /sys/module/msm_thermal/core_control/enabled"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu1/cpufreq/scaling_min_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu2/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu2/cpufreq/scaling_min_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu3/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1440000 > /sys/devices/system/cpu/cpu3/cpufreq/scaling_min_freq"
..\app\adb shell "echo 1824000 > /sys/devices/system/cpu/cpu4/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1824000 > /sys/devices/system/cpu/cpu4/cpufreq/scaling_min_freq"
..\app\adb shell "echo 1824000 > /sys/devices/system/cpu/cpu5/cpufreq/scaling_max_freq"
..\app\adb shell "echo 1824000 > /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu1/cpufreq/scaling_governor"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu2/cpufreq/scaling_governor"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu3/cpufreq/scaling_governor"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor"
..\app\adb shell "echo performance > /sys/devices/system/cpu/cpu5/cpufreq/scaling_governor"

popd
pause