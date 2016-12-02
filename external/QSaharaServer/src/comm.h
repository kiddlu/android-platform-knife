/*
 * Copyright (C) 2012 Qualcomm Technologies, Inc. All rights reserved.
 *                 Qualcomm Technologies Proprietary/GTDR
 *
 * All data and information contained in or disclosed by this document is
 * confidential and proprietary information of Qualcomm Technologies, Inc. and all
 * rights therein are expressly reserved.  By accepting this material the
 * recipient agrees that this material and the information contained therein
 * is held in confidence and in trust and will not be used, copied, reproduced
 * in whole or in part, nor its contents revealed in any manner to others
 * without the express written permission of Qualcomm Technologies, Inc.
 *
 *
 *  comm.h : Defines protocol states, and send/receive functions and parameters.
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/comm.h#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD		who		why
 *  -----------------------------------------------------------------------------
 *  2010-09-28       ng      Added command mode support
 *  2010-10-18       ab      Added memory debug mode support
 *
 *  Copyright 2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
 *
 *==========================================================================================
 */


#ifndef COMM_H
#define COMM_H

#include "common_protocol_defs.h"

typedef enum {
    USING_USB,
    USING_UART,
    USING_SDIO,
    USING_PCIE
} transport_medium_t;


typedef enum {
    SAHARA_PROTOCOL,
    DLOAD_PROTOCOL
} protocol_type_t;

typedef struct  {
    /* name of current port */
    char *port_name;

    /* handle to COM port */
    PORT_HANDLE port_fd;

    size_t total_bytes_sent;

    size_t total_bytes_recd;

    transport_medium_t transport_medium;

    protocol_type_t protocol;

    int rx_timeout;

    unsigned int sleep_btwn_reads_usec;

    size_t MAX_TO_READ;
    size_t MAX_TO_WRITE;
	size_t BYTES_TO_PRINT;

} com_port_t;

extern com_port_t com_port;

void port_disconnect ();
bool port_connect (char *port_name);
bool tx_data (byte *buffer, size_t bytes_to_send);
bool rx_data (byte *buffer, size_t bytes_to_read, size_t *bytes_read);

#endif
