local parser = clink.arg.new_parser
local adk_parser = parser(
	{
--buildin
	"ftyrst",
	"pk",
	"meminfo",
	"smartisan-active",
--trace
    "strace",
	"ltrace",
    "debuggerd",
    "debuggerd64",
--wifi
    "wpa_cli",
	"wl",
--net
	"tcpdump",
	"netstat",
	"ip",
	"iptables",
	"ndc",
--coreutils
	"ps",
	"top",
	"printenv",
	"ifconfig",
	"vmstat",
	"lsof",
	"busybox",
	"getevent",
--log
    "logcat",
    "dmesg",
--android
	"getenforce",
	"setenforce",
	"getprop",
	"setprop",
	"start",
	"stop",
	"netcfg",
	"svc",
	"am",
	"dumpsys",
	"dumpstate",
	"screencap",
--reboot
    "reboot",
	"reboot -p",
	"reboot edl",
	"reboot recovery",
	"reboot bootloader"
    },
	"-h"
)         
clink.arg.register_parser("adk", adk_parser)