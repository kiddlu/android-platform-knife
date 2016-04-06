local parser = clink.arg.new_parser
local adk_parser = parser(
	{
--buildin
	"ftyrst",
	"pk",
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
--system
	"getevent",
	"lsof",
	"getenforce",
	"setenforce",
	"getopt",
	"setopt",
	"start",
	"stop",
	"netcfg",
	"svc",
	"am",
	"screencap",
--coreutils
	"ps",
	"top",
	"ifconfig",
	"vmstat",
	"busybox",
--log
    "logcat",
    "dmesg",
--reboot
    "reboot",
	"reboot edl",
	"reboot recovery",
	"reboot bootloader"
    },
	"-h"
)         
clink.arg.register_parser("adk", adk_parser)