#pragma once

#ifdef _WIN32
#include <windows.h>
#include <winerror.h>
#include <stdint.h>
#include <ntstatus.h>
#include <winbase.h>
#include <time.h>
#include <devpropdef.h>
#include <devpkey.h>
#include <setupapi.h>

#undef ERROR_SUCCESS
#define ERROR_SUCCESS 0
#define ERROR_HV_INSUFFICIENT_MEMORY ENOMEM

#define strnlen_s(a,b) strlen(a)
#else
#include <stdint.h>
#endif