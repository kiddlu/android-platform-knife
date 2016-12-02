/*================================================================================================
 *
 * FILE:        kickstart_log.c
 *
 * DESCRIPTION:
 *    This module implements the logic for logging EVENT, INFO, WARNING, ERROR messages
 *
 *
 *        Copyright © 2009-2012 Qualcomm Technologies Incorporated.
 *               All Rights Reserved.
 *            QUALCOMM Proprietary/GTDR
 *===============================================================================================
 *
 *
 *  kickstart_log.c : This module implements the logic for logging EVENT, INFO, WARNING, ERROR
 *  messages
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/kickstart_log.c#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD		who		why
 *  -----------------------------------------------------------------------------
 *  2010-09-28       ng      Added command mode support
 *
 *  Copyright 2012 by Qualcomm Technologies, Incorporated.  All Rights Reserved.
 *
 *==========================================================================================
 */
#include "common_protocol_defs.h"
#include "kickstart_log.h"
#ifdef LINUXPC
#include <stdarg.h>
#endif

/*===========================================================================
 *  FUNCTION:  kickstart_log
 *
 *  DESCRIPTION
 *  Kickstart logger, prints the function name, line number and the logs
 *
 *  PARAMETERS
 *  func name      - function name
 *  line_number    - line number
 *  format         - variable list of arguments
 *
 *  RETURN VALUE
 *  NONE
 *
 *  SIDE EFFECTS
 *  NONE
 *  ===========================================================================*/
void kickstart_log (int log_level, const char *func_name, int line_number, const char *format, ...)
{
   va_list args;
   char log[LOG_BUFFER_SIZE];

#ifdef WINDOWSPC
   time_t current_time;
   struct tm *local_time;

   current_time = time(NULL);
   local_time = localtime(&current_time);
#endif

   va_start (args, format);
   vsnprintf(log, sizeof(log), format, args);
   va_end (args);

    if ((log_level == LOG_ERROR) ||
        log_level == LOG_WARN ||
        log_level == LOG_STATUS ||
        kickstart_options.verbose > 0) {
#ifdef WINDOWSPC
		printf("\n%02d:%02d:%02d: ", local_time->tm_hour, local_time->tm_min, local_time->tm_sec);
#endif
        switch (log_level) {
        case LOG_ERROR:
            LOGE("ERROR: function: %s:%d %s\n", func_name, line_number, log);
            break;

        case LOG_INFO:
            LOGI("INFO: function: %s:%d %s\n", func_name, line_number, log);
            break;

        case LOG_WARN:
            LOGI("WARNING: function: %s:%d %s\n", func_name, line_number, log);
            break;

        case LOG_EVENT:
            LOGI("EVENT: %s\n", log);
            break;

        case LOG_STATUS:
            LOGE("%s\n", log);
            break;
        }
    }
}
