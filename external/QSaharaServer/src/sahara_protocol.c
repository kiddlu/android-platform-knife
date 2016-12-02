/*=============================================================================
 *  FILE :
 *  sahara_protocol.c
 *
 *  DESCRIPTION:
 *
 *  Implements the Sahara protocol
 *
 *  Copyright (C) 2012 Qualcomm Technologies, Inc. All rights reserved.
 *                  Qualcomm Technologies Proprietary/GTDR
 *
 *  All data and information contained in or disclosed by this document is
 *  confidential and proprietary information of Qualcomm Technologies, Inc. and all
 *  rights therein are expressly reserved.  By accepting this material the
 *  recipient agrees that this material and the information contained therein
 *  is held in confidence and in trust and will not be used, copied, reproduced
 *  in whole or in part, nor its contents revealed in any manner to others
 *  without the express written permission of Qualcomm Technologies, Inc.
 *  =============================================================================
 *
 *
 *  sahara_protocol.c : Implements the Sahara protocol.
 * ==========================================================================================
 *   $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/QSaharaServer/src/sahara_protocol.c#1 $
 *   $DateTime: 2015/06/04 14:14:52 $
 *   $Author: pwbldsvc $
 *
 *  Edit History:
 *  YYYY-MM-DD       who     why
 *  -----------------------------------------------------------------------------
 *  2010-09-28       ng      Added command mode support
 *  2010-10-18       ab      Added memory debug mode support
 *
 *  Copyright 2012 by Qualcomm Technologies, Inc.  All Rights Reserved.
 *
 *==========================================================================================
 */

#include "common_protocol_defs.h"
#include "comm.h"
#include "sahara_protocol.h"
#include "kickstart_log.h"
#include "kickstart_utils.h"
#ifdef WINDOWSPC
#include "windows_utils.h"
#endif

static bool is_end_of_image_rx (uint32_t command);
static bool send_reset_command ();
static bool is_ack_successful (int status);
static bool start_image_transfer ();
static bool start_image_transfer_64bit();
static bool send_done_packet ();
static bool send_memory_read_packet (uint64_t memory_table_address, uint64_t memory_table_length);
static bool is_valid_memory_table(uint64_t memory_table_size);
static int send_cmd_exec_command (unsigned int command);
static int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y);
void time_throughput_calculate(struct timeval *start_time, struct timeval *end_time, size_t size_bytes);

sahara_data_t sahara_data = {
    NULL,              // rx_buffer
    NULL,              // tx_buffer
    NULL,              // misc_buffer
    SAHARA_WAIT_HELLO, // state
    0,                 // timed_data_size
    -1,                // fd
    -1,                // ram_dump_image
    5,                 // max_ram_dump_retries
    1048576,           // max_ram_dump_read
    SAHARA_MODE_LAST,  // mode
    SAHARA_MODE_LAST,  // prev_mode
    0,                 // command
	false              // ram_dump_64bit
};

static image_mapping_t sahara_mappings[MAX_SAHARA_MAPPINGS];
static int num_sahara_mappings = 0, UsingSaharaVersion = 0;

bool add_sahara_mapping(int id, const char* name) {
	int i;

	dbg(LOG_INFO, "in add_sahara_mapping(%d,'%s')",id,name);

	for (i = 0; i < num_sahara_mappings; i++) {	// not MAX_SAHARA_MAPPINGS
		if (id == sahara_mappings[i].id) {
			sahara_mappings[i].name = name;
			dbg(LOG_INFO, "Just mapped '%s' to '%d' (0x%X), note that i=%d and num_sahara_mappings=%d", name,id,id,i,num_sahara_mappings);
			return true;
		}
	}

	dbg(LOG_INFO, "This is a NEW mapping");

	if (MAX_SAHARA_MAPPINGS == num_sahara_mappings) {
		dbg (LOG_ERROR, "Cannot accept more than %d Sahara mappings", MAX_SAHARA_MAPPINGS);
		return false;
	}
	else {
		sahara_mappings[num_sahara_mappings].id = id;
		sahara_mappings[num_sahara_mappings].name = name;
		num_sahara_mappings++;
    	dbg(LOG_INFO, "Just mapped '%s' to '%d' (0x%X), note that i=%d and num_sahara_mappings=%d", name,id,id,i,num_sahara_mappings);
		return true;
	}
}

bool init_sahara_mapping_list(void) {
	add_sahara_mapping(2,  "amss.mbn");
	add_sahara_mapping(6,  "apps.mbn");
	add_sahara_mapping(8,  "dsp1.mbn");
	add_sahara_mapping(10, "dbl.mbn");
	add_sahara_mapping(11, "osbl.mbn");
	add_sahara_mapping(12, "dsp2.mbn");
	add_sahara_mapping(16, "efs1.mbn");
	add_sahara_mapping(17, "efs2.mbn");
	add_sahara_mapping(20, "efs3.mbn");
	add_sahara_mapping(21, "sbl1.mbn");
	add_sahara_mapping(22, "sbl2.mbn");
	add_sahara_mapping(23, "rpm.mbn");
	add_sahara_mapping(25, "tz.mbn");
	add_sahara_mapping(28, "dsp3.mbn");
	add_sahara_mapping(29, "acdb.mbn");
	add_sahara_mapping(30, "wdt.mbn");
	return add_sahara_mapping(31, "mba.mbn");
};

bool add_input_to_sahara_mapping_list(const char* arg) {
	int image_id = atoi(arg);
	const char* colon_location = strchr(arg, ':');
	//int i;

	if(NULL == colon_location) {
       dbg(LOG_ERROR, "Input not in the form <image_id:image_file_name>");
       return false;
    }

	return add_sahara_mapping(image_id, &arg[colon_location - arg + 1]);
}

const char* get_file_from_sahara_mapping_list(int image_id) {
	int i;
	//show_sahara_mapping_list();

	dbg(LOG_INFO, "num_sahara_mappings = %d",num_sahara_mappings);

	for (i = 0; i < num_sahara_mappings; i++) 
	{
		dbg(LOG_INFO, "(%d) Looking for image %d, sahara_mappings[%d].id = %d",i,image_id,i,sahara_mappings[i].id );

		if (image_id == sahara_mappings[i].id)
		{
			dbg(LOG_INFO, "Matched!!! - '%s'",sahara_mappings[i].name);
			return find_file(sahara_mappings[i].name);
		}
	}
	return NULL;
}

void show_sahara_mapping_list() {
	int i;
	printf("Sahara mappings:\n");
	for (i = 0; i < num_sahara_mappings; i++) {
		printf("%d: %s\n", sahara_mappings[i].id, sahara_mappings[i].name);
	}
}

bool sahara_tx_data (size_t bytes_to_send)
{
    return tx_data(sahara_data.tx_buffer, bytes_to_send);
}

bool sahara_rx_data(size_t bytes_to_read) 
{
    //bool retval = false;
    sahara_packet_header* command_packet_header = NULL;
    size_t temp_bytes_read = 0, bytes_read = 0;

    // Treat byte_to_read 0 as asking for a command packet, since the caller anyway cannot predict the size
    // of the incoming command. We read 8 bytes below, which is the command followed by the length
    if (0 == bytes_to_read) 
	{
        if (false == rx_data(sahara_data.rx_buffer, sizeof(sahara_packet_header), &temp_bytes_read)) 
		{	// sahara_packet_header is 4 byte command, 4 byte packet length
            return false;
        }
        else 
		{
			command_packet_header = (sahara_packet_header *) sahara_data.rx_buffer;
			dbg(LOG_INFO, "Read %d bytes, Header indicates command %d and packet length %d bytes", temp_bytes_read,command_packet_header->command,command_packet_header->length);

            bytes_read += temp_bytes_read;
            if (bytes_read != sizeof(sahara_packet_header)) 
			{
                dbg(LOG_ERROR, "Unable to read packet header. Only read %d bytes.", temp_bytes_read);
                return false;
            }
            command_packet_header = (sahara_packet_header *) sahara_data.rx_buffer;

            dbg(LOG_INFO, "Command packet length %d", command_packet_header->length);
            dbg(LOG_INFO, "SAHARA_RAW_BUFFER_SIZE length %d", SAHARA_RAW_BUFFER_SIZE);

            if (command_packet_header->length > SAHARA_RAW_BUFFER_SIZE) 
			{
                dbg(LOG_ERROR, "Command packet length %d too large to fit", command_packet_header->length);
                return false;
            }
            bytes_to_read = command_packet_header->length;
        }
    }
    while (bytes_read < bytes_to_read) {
        if (false == rx_data(sahara_data.rx_buffer + bytes_read, bytes_to_read - bytes_read, &temp_bytes_read))
            return false;
        else
            bytes_read += temp_bytes_read;
    }
    return true;
}

void MoveStringBackwards(char *sz)
{
	unsigned int i,j = strlen(sz);

	for(i=1;i<j;i++)
	{
		sz[i-1] = sz[i];
	}
	sz[i-1] = '\0'; //NULL;
}

unsigned int FindLastSlash(char *sz)
{
	unsigned int i,j = strlen(sz);
	unsigned int LastSlash = 0;

	for(i=1;i<j;i++)
	{
		if(sz[i]=='/' || sz[i]=='\\')
			LastSlash = i;
	}

	return LastSlash;
}

bool sahara_start() {
    int              retval = 0;
    const char*      id_mapped_file = NULL;
    bool             cmds_executed = false;
    int              num_debug_entries = -1,
                     num_dump_failures = 0;
    int              i = 0, CurrentImageId = 0;
    uint64_t         memory_table_addr = 0,
                     memory_table_length = 0;
    boot_sahara_mode sahara_hello_resp_mode = SAHARA_MODE_LAST;

    // Previous mode before command mode switch
    bool started_raw_image_transfer = false;
    struct timeval time_start, time_end;

    sahara_packet_hello *sahara_hello = (sahara_packet_hello *)sahara_data.rx_buffer;
    sahara_packet_hello_resp *sahara_hello_resp = (sahara_packet_hello_resp *)sahara_data.tx_buffer;
    sahara_packet_end_image_tx *sahara_end_image_tx = (sahara_packet_end_image_tx *)sahara_data.rx_buffer;
    sahara_packet_read_data *sahara_read_data = (sahara_packet_read_data *)sahara_data.rx_buffer;
    //sahara_packet_read_data_64bit *sahara_read_data_64bit = (sahara_packet_read_data_64bit *)sahara_data.rx_buffer;
	//sahara_packet_read_data_64bit_better *sahara_read_data_64bit_better = (sahara_packet_read_data_64bit_better *)sahara_data.rx_buffer;
    sahara_packet_memory_debug *sahara_memory_debug = (sahara_packet_memory_debug *)sahara_data.rx_buffer;
	sahara_packet_memory_debug_64bit *sahara_memory_debug_64bit = (sahara_packet_memory_debug_64bit *)sahara_data.rx_buffer;
	dload_debug_type *sahara_memory_table_rx = (dload_debug_type *)sahara_data.rx_buffer;
	dload_debug_type_64bit *sahara_memory_table = (dload_debug_type_64bit *)sahara_data.misc_buffer;
    sahara_packet_cmd_switch_mode *sahara_cmd_switch_mode = (sahara_packet_cmd_switch_mode *)sahara_data.tx_buffer;
    sahara_packet_done_resp *sahara_done_resp = (sahara_packet_done_resp *)sahara_data.rx_buffer;
    sahara_packet_reset_resp *sahara_reset_resp = (sahara_packet_reset_resp *)sahara_data.rx_buffer;

#ifdef WINDOWSPC
	int dummy_byte = 0;
#endif

    if (sahara_data.ram_dump_image != -1)
        cmds_executed = true;
    sahara_data.state = SAHARA_WAIT_HELLO;

    id_mapped_file = NULL;
    num_debug_entries = -1;
    num_dump_failures = 0;
    sahara_data.state = SAHARA_WAIT_HELLO;

    while (1)
    {
        switch (sahara_data.state)
        {
        case SAHARA_WAIT_HELLO:
          dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_HELLO\n");
          if (false == sahara_rx_data(0))	// size 0 means we don't know what to expect. So we'll just try to read the 8 byte header 
            return false;

          // Reset the raw image transfer started flag to false
          started_raw_image_transfer = false;
          //Check if the received command is a hello command
          if (SAHARA_HELLO_ID != sahara_hello->header.command) {
                dbg(LOG_ERROR, "Received a different command: %x while waiting for hello packet", sahara_hello->header.command);
                // set the state to SAHARA_WAIT_RESET_RESP
                dbg(LOG_EVENT, "SENDING --> SAHARA_RESET");
                if (false == send_reset_command ()) {
                    dbg(LOG_ERROR, "Sending RESET packet failed");
                    return false;
                }
                // set the state to SAHARA_WAIT_RESET_RESP
                dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_RESET_RESP\n");
                sahara_data.state = SAHARA_WAIT_RESET_RESP;
            }
           else {
              dbg(LOG_EVENT, "RECEIVED <-- SAHARA_HELLO");
              dbg(LOG_EVENT, "SENDING --> SAHARA_HELLO_RESPONSE");
              // Recieved hello, send the hello response
              // Create a Hello request

			  UsingSaharaVersion = sahara_hello->version;

              sahara_hello_resp->header.command = SAHARA_HELLO_RESP_ID;
              sahara_hello_resp->header.length = sizeof(sahara_packet_hello_resp);
              sahara_hello_resp->version = UsingSaharaVersion; //SAHARA_VERSION;
              sahara_hello_resp->version_supported = UsingSaharaVersion; //SAHARA_VERSION_SUPPORTED;
/*
              if (!(sahara_hello->version <= SAHARA_VERSION && sahara_hello->version >= SAHARA_VERSION_SUPPORTED)) {
                   dbg(LOG_ERROR, "Invalid packet version %d. Current Sahara version: %d, version supported: %d", sahara_hello->version, SAHARA_VERSION, SAHARA_VERSION_SUPPORTED);
                   sahara_hello_resp->status = SAHARA_NAK_INVALID_TARGET_PROTOCOL;

                  dbg(LOG_ERROR, "Hello packet version: %d", sahara_hello->version);
                  dbg(LOG_ERROR, "Hello packet length: %d", sahara_hello->header.length);
                  dbg(LOG_ERROR, "Hello packet ver supported: %d", sahara_hello->version_supported);
                  dbg(LOG_ERROR, "Hello packet cmd packet length: %d", sahara_hello->cmd_packet_length);
                  dbg(LOG_ERROR, "Hello packet command: %d", sahara_hello->header.command);
                  dbg(LOG_ERROR, "Hello packet mode: %d", sahara_hello->mode);
              }
              else {
*/
                  sahara_hello_resp->status = SAHARA_STATUS_SUCCESS;
/*
              }
*/

              /* Checks whether the mode required by the user has been sent */
              /* if it has not been sent, it is sent out in the first hello response packet from this state */
              /* once that mode has been sent to target, the received mode value is echoed back for future responses */
              if ((sahara_data.mode < SAHARA_MODE_LAST) && (cmds_executed == false)) {
                  sahara_hello_resp->mode  = sahara_data.mode;
                  cmds_executed = true;
              }
              else {
                  sahara_hello_resp->mode = sahara_hello->mode;
              }

              sahara_data.prev_mode = (boot_sahara_mode) sahara_hello->mode;
              sahara_hello_resp_mode = (boot_sahara_mode) sahara_hello_resp->mode;

              dbg(LOG_INFO, "\n\nsahara_mode                         = %u",sahara_data.mode);
              dbg(LOG_INFO, "\nsahara_hello->mode = %u",sahara_hello->mode);
              dbg(LOG_INFO, "\nhelloRx.mode                        = %u\n",sahara_hello_resp->mode);

			  sahara_hello_resp->reserved0 = 1;
			  sahara_hello_resp->reserved1 = 2;
			  sahara_hello_resp->reserved2 = 3;
			  sahara_hello_resp->reserved3 = 4;
			  sahara_hello_resp->reserved4 = 5;
			  sahara_hello_resp->reserved5 = 6;

              /*Send the Hello  Resonse Request*/
              if (false == sahara_tx_data (sizeof(sahara_packet_hello_resp)))
              {
                dbg(LOG_ERROR, "Tx Sahara Data Failed ");
                return false;
              }
              sahara_data.state = SAHARA_WAIT_COMMAND;
              use_wakelock(WAKELOCK_ACQUIRE);
             }
              break;

        case SAHARA_WAIT_COMMAND:
            if (false == sahara_rx_data(0))
               return false;

            dbg(LOG_INFO, "STATE <-- SAHARA_WAIT_COMMAND\n");
            // Check if it is  an end of image Tx
            if (true == is_end_of_image_rx (((sahara_packet_header *)sahara_data.rx_buffer)->command)) {
                dbg(LOG_EVENT, "RECEIVED <-- SAHARA_END_IMAGE_TX");

                if (true == is_ack_successful (sahara_end_image_tx->status)) {
                    if ((int) sahara_end_image_tx->image_id == sahara_data.ram_dump_image)
                        cmds_executed = false;
                    dbg(LOG_EVENT, "SENDING --> SAHARA_DONE");
                    send_done_packet ();
                    sahara_data.state = SAHARA_WAIT_DONE_RESP;
                }
                else {
                    dbg(LOG_EVENT, "SENDING --> SAHARA_RESET");
                    if (false == send_reset_command ()) {
                        dbg(LOG_ERROR, "Sending RESET packet failed");
                        return false;
                    }
                    /*set the state to SAHARA_WAIT_RESET_RESP*/
                    sahara_data.state = SAHARA_WAIT_RESET_RESP;
                }
            }
            else if (SAHARA_READ_DATA_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
                dbg(LOG_EVENT, "RECEIVED <-- SAHARA_READ_DATA");
                if (false == started_raw_image_transfer) {
                    // Get the name of the file the target is requesting
					// Aaron - note that if target wants a different filename mid transfer, this will fail
                    id_mapped_file = get_file_from_sahara_mapping_list(sahara_read_data->image_id);
                    if (NULL == id_mapped_file) 
					{
                        dbg(LOG_ERROR, "Matching input image for ID %d not found", sahara_read_data->image_id);
						return false;
						/*
                        if (false == send_reset_command ()) 
						{
                            dbg(LOG_ERROR, "Sending RESET packet failed");
                        }
                        sahara_data.state = SAHARA_WAIT_COMMAND;
						*/

                    }else {
                        /*Load the image*/
                        dbg(LOG_STATUS, "Requested ID %d, file: \"%s\"", sahara_read_data->image_id, id_mapped_file);
                        CurrentImageId = sahara_read_data->image_id;
                        sahara_data.fd = open_file(id_mapped_file, true);
                        started_raw_image_transfer = true;
                        sahara_data.timed_data_size = 0;
                        gettimeofday(&time_start, NULL);
                    }
                }
                /*start the data transfer */
                if (NULL != id_mapped_file) {
                    if ( false == start_image_transfer() ) {
                        dbg(LOG_ERROR, "start_image_transfer failed");
                        close_file(sahara_data.fd);
                        return false;
                    }
                }
            } // end of SAHARA_READ_DATA_ID
            else if (SAHARA_64_BITS_READ_DATA_ID  == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
                dbg(LOG_EVENT, "RECEIVED <-- SAHARA_READ_DATA");
                if (false == started_raw_image_transfer) {
                    // Get the name of the file the target is requesting
					// Aaron - note that if target wants a different filename mid transfer, this will fail
                    id_mapped_file = get_file_from_sahara_mapping_list(sahara_read_data->image_id);
                    if (NULL == id_mapped_file) 
					{
                        dbg(LOG_ERROR, "2. Matching input image for ID %d not found", sahara_read_data->image_id);
						return false;
						/*
                        if (false == send_reset_command ()) {
                            dbg(LOG_ERROR, "Sending RESET packet failed");
                        }
                        sahara_data.state = SAHARA_WAIT_COMMAND;
						*/
                    }
					else 
					{
                        /*Load the image*/
                        dbg(LOG_STATUS, "Requested ID %d, file: \"%s\"", sahara_read_data->image_id, id_mapped_file);
                        sahara_data.fd = open_file(id_mapped_file, true);
                        started_raw_image_transfer = true;
                        sahara_data.timed_data_size = 0;
                        gettimeofday(&time_start, NULL);
                    }
                }
                /*start the data transfer */
                if (NULL != id_mapped_file) {
                    if ( false == start_image_transfer_64bit() ) {
                        dbg(LOG_ERROR, "start_image_transfer failed");
                        close_file(sahara_data.fd);
                        return false;
                    }
                }
            } // end of SAHARA_64_BITS_READ_DATA_ID
            else if (SAHARA_MEMORY_DEBUG_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command
				  || SAHARA_64_BITS_MEMORY_DEBUG_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
                dbg(LOG_EVENT, "RECEIVED <-- SAHARA_MEMORY_DEBUG");

				if (SAHARA_64_BITS_MEMORY_DEBUG_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
					sahara_data.ram_dump_64bit = true;
					dbg(LOG_EVENT, "Using 64 bit RAM dump mode");
					memory_table_addr = sahara_memory_debug_64bit->memory_table_addr;
					memory_table_length = sahara_memory_debug_64bit->memory_table_length;
				}
				else {
					sahara_data.ram_dump_64bit = false;
					memory_table_addr = sahara_memory_debug->memory_table_addr;
					memory_table_length = sahara_memory_debug->memory_table_length;
				}

                dbg(LOG_INFO, "Memory Table Address: %"PRIu64", Memory Table Length: %"PRIu64, memory_table_addr, memory_table_length);

                if (false == is_valid_memory_table(memory_table_length)) {
                    dbg(LOG_ERROR, "Invalid memory table received");
                    if (false == send_reset_command ()) {
                        dbg(LOG_ERROR, "Sending RESET packet failed");
                        return false;
                    }
                    sahara_data.state = SAHARA_WAIT_RESET_RESP;
                    break;
                }

                if (memory_table_length > 0) {
                    dbg(LOG_EVENT, "SENDING --> SAHARA_MEMORY_READ");
                    if (false == send_memory_read_packet(memory_table_addr, memory_table_length)) {
                        dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
                        return false;
                    }
                    dbg(LOG_EVENT, "Sent SAHARA_MEMORY_READ, address 0x%"PRIX64", length 0x%"PRIX64, memory_table_addr, memory_table_length);
                    if (memory_table_length > SAHARA_RAW_BUFFER_SIZE) {
                        dbg(LOG_ERROR, "Memory table length is greater than size of intermediate buffer");
                        return false;
                    }
                }
                dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_MEMORY_TABLE\n");
                sahara_data.state = SAHARA_WAIT_MEMORY_TABLE;
            }
            else if (SAHARA_CMD_READY_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
                dbg(LOG_EVENT, "RECEIVED <-- SAHARA_CMD_READY");

                if (false == send_cmd_exec_command (sahara_data.command)) {
                    dbg(LOG_ERROR, "Send CMD_EXEC packet failed");
					return false;
				}

                if (false == send_reset_command ()) {
                    dbg(LOG_ERROR, "Sending RESET packet failed");
                    return false;
                }
                // set the state to SAHARA_WAIT_RESET_RESP
                dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_RESET_RESP\n");
                sahara_data.state = SAHARA_WAIT_RESET_RESP;
            }
            else if (SAHARA_HELLO_ID == ((sahara_packet_header *)sahara_data.rx_buffer)->command) {
                dbg(LOG_INFO, "Received HELLO command out of sequence. Discarding.");
                use_wakelock(WAKELOCK_RELEASE);
                sahara_data.state = SAHARA_WAIT_HELLO;
            }
            else {
                dbg(LOG_ERROR, "Received an unknown command: %d ", ((sahara_packet_header *)sahara_data.rx_buffer)->command);
                // set the state to SAHARA_WAIT_RESET_RESP
                dbg(LOG_EVENT, "SENDING --> SAHARA_RESET");
                if (false == send_reset_command ()) {
                    dbg(LOG_ERROR, "Sending RESET packet failed");
                    return false;
                }
                // set the state to SAHARA_WAIT_RESET_RESP
                dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_RESET_RESP\n");
                sahara_data.state = SAHARA_WAIT_RESET_RESP;
            }
            break;

        case SAHARA_WAIT_MEMORY_TABLE:
            if (memory_table_length > 0) {
                if (false == sahara_rx_data((size_t)memory_table_length)) {
                   return false;
                }
                dbg(LOG_INFO, "Memory Debug table received");

				if (true == sahara_data.ram_dump_64bit) {
					memcpy (sahara_data.misc_buffer, sahara_data.rx_buffer, (size_t)memory_table_length);
					num_debug_entries = (int)(memory_table_length/sizeof(dload_debug_type_64bit));
				}
				else {
					num_debug_entries = (int)(memory_table_length/sizeof(dload_debug_type));
					if (num_debug_entries * sizeof(dload_debug_type_64bit) > SAHARA_RAW_BUFFER_SIZE) {
                        dbg(LOG_ERROR, "Length of memory table converted to 64-bit entries is greater than size of intermediate buffer");
                        return false;
                    }
					for (i = 0; i < num_debug_entries; ++i) 
					{
						sahara_memory_table[i].save_pref = (uint64_t) sahara_memory_table_rx[i].save_pref;
						sahara_memory_table[i].mem_base = (uint64_t) sahara_memory_table_rx[i].mem_base;
						sahara_memory_table[i].length = (uint64_t) sahara_memory_table_rx[i].length;
						if (strlcpy(sahara_memory_table[i].filename, sahara_memory_table_rx[i].filename, DLOAD_DEBUG_STRLEN_BYTES) > DLOAD_DEBUG_STRLEN_BYTES
							|| strlcpy(sahara_memory_table[i].desc, sahara_memory_table_rx[i].desc, DLOAD_DEBUG_STRLEN_BYTES) > DLOAD_DEBUG_STRLEN_BYTES) {
							dbg(LOG_ERROR, "String was truncated while copying");
							return false;
						}
						
						while(sahara_memory_table[i].filename[0]=='/' || sahara_memory_table[i].filename[0]=='\\')
						{
							dbg(LOG_ERROR, "Filename from target has SLASHES in it???");
							MoveStringBackwards(sahara_memory_table[i].filename);
						}

						// does the sahara_memory_table[i].filename have a slash in it?, if so we need to clean it
						if( strlen(sahara_memory_table[i].filename)==0 )
						{
							dbg(LOG_ERROR, "Filename from target is corrupt or invalid, please see output, use -v 3");
							return false;
						}


						if(FindLastSlash(sahara_memory_table[i].filename)>0)
						{
							char MyTempPath[1024];
							char MyNewPath[1024];
							unsigned int LastSlash = FindLastSlash(sahara_memory_table[i].filename);
							// need to create folders MyPath + 

							strcpy(MyNewPath,kickstart_options.path_to_save_files);
							strcpy(MyTempPath,sahara_memory_table[i].filename);
							MyTempPath[LastSlash] = '\0';
		
							strcat(MyNewPath,MyTempPath);

							printf("Calling mkdir(%s)",MyNewPath);
#ifdef WINDOWSPC
							retval = mkdir(MyNewPath);
#else
							retval = mkdir(MyNewPath,0700);
#endif
							printf("retval  = 0x%X",retval );
							if (retval != 0) {
								dbg(LOG_ERROR, "Error creating directory '%s'. %s.\n", MyNewPath, strerror(errno));
								return false;
							}

						} // end of if(FindLastSlash(sahara_memory_table[i].filename)>0)

					} // end for (i = 0; i < num_debug_entries; ++i) 
				}

            }
			else {
				num_debug_entries = 0;
			}

            for(i = 0; i < num_debug_entries; i++)
            {
                //dbg(LOG_EVENT, "Base 0x"PRIX64" Len 0x"PRIX64, sahara_memory_table[i].mem_base, sahara_memory_table[i].length);
				//dbg(LOG_EVENT, "'%s', '%s'",sahara_memory_table[i].filename, sahara_memory_table[i].desc);
				dbg(LOG_EVENT, "Base 0x%"PRIX64" Len 0x%"PRIX64", '%s', '%s'", sahara_memory_table[i].mem_base, sahara_memory_table[i].length, sahara_memory_table[i].filename, sahara_memory_table[i].desc);
            }
            num_debug_entries = i-1;
            dbg(LOG_INFO,"\nnum_debug_entries=%i",num_debug_entries);

            /* no break; Simply falling into next state */
            /* no break; Simply falling into next state */
            /* no break; Simply falling into next state */
            dbg(LOG_INFO,"\n\nNOT BREAKING, falling into case SAHARA_WAIT_MEMORY_REGION  *************");

        case SAHARA_WAIT_MEMORY_REGION:
            /* TODO: Add check for invalid raw data or end of image tx ? how ? */

            /* Since previous state case block falls into this block, check whether state
             * is actually SAHARA_WAIT_MEMORY_REGION or not
             */

            dbg(LOG_INFO,"Inside case SAHARA_WAIT_MEMORY_REGION");
            dbg(LOG_INFO,"sahara_data.state=%u, SAHARA_WAIT_MEMORY_REGION=%u, ",sahara_data.state,SAHARA_WAIT_MEMORY_REGION);

            if (sahara_data.state == SAHARA_WAIT_MEMORY_REGION)
            {
                if (false == sahara_rx_data((size_t)memory_table_length)) {
                    num_dump_failures++;
                    if (num_dump_failures == sahara_data.max_ram_dump_retries) {
                        return false;
                    }
                    dbg(LOG_WARN, "Retrying request for RAM dump chunk.");
                }
                else {
                    num_dump_failures = 0;
                    dbg(LOG_EVENT, "Received: %d bytes", memory_table_length);

                    sahara_memory_table[num_debug_entries].mem_base += memory_table_length;
                    sahara_memory_table[num_debug_entries].length -= memory_table_length;

                    dbg(LOG_INFO, "Writing to disk");
                    retval = write(sahara_data.fd, sahara_data.rx_buffer, (unsigned int)memory_table_length);
                    if (retval < 0) {
                        dbg(LOG_ERROR, "file write failed: %s", strerror(errno));
                        return false;
                    }
                    if ((uint32_t) retval != memory_table_length) {
                        dbg(LOG_WARN, "Wrote only %d of %d bytes", retval, memory_table_length);
                    }
                    dbg(LOG_INFO, "Successfully wrote to disk");

                    if (sahara_memory_table[num_debug_entries].length == 0)
                    {
                        dbg(LOG_STATUS, "Received file '%s'", sahara_memory_table[num_debug_entries].filename);
                        num_debug_entries--;
                        close_file(sahara_data.fd);
                        gettimeofday(&time_end, NULL);
                        time_throughput_calculate(&time_start, &time_end, sahara_data.timed_data_size);
                        if (num_debug_entries >= 0)
                        {
							sahara_data.fd = open_file(sahara_memory_table[num_debug_entries].filename, false);
#ifdef WINDOWSPC
							//printf("2 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,sahara_memory_table[num_debug_entries].length - 1);
							_lseek(sahara_data.fd, (long)(sahara_memory_table[num_debug_entries].length - 1), SEEK_SET);
							write(sahara_data.fd, &dummy_byte, 1);
							//printf("3 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,0);
							_lseek(sahara_data.fd, 0, SEEK_SET);
#endif
                            sahara_data.timed_data_size = 0;
                            gettimeofday(&time_start, NULL);
                        }
                    }
                }
            }
            else
            {
                dbg(LOG_INFO,"ELSE sahara_data.state=%u, SAHARA_WAIT_MEMORY_REGION=%u, ",sahara_data.state,SAHARA_WAIT_MEMORY_REGION);
                dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_MEMORY_REGION\n");
                sahara_data.state = SAHARA_WAIT_MEMORY_REGION;

                if (num_debug_entries >= 0)
                {
                    sahara_data.fd = open_file(sahara_memory_table[num_debug_entries].filename, false);
					if(sahara_data.fd==-1)
					{
						printf("ERROR: Your file '%s' does not exist or cannot be created\n\n",sahara_memory_table[num_debug_entries].filename);
						exit(0);
					}
#ifdef WINDOWSPC
					//printf("4 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,sahara_memory_table[num_debug_entries].length - 1);
					_lseek(sahara_data.fd, (long)(sahara_memory_table[num_debug_entries].length - 1), SEEK_SET);
					write(sahara_data.fd, &dummy_byte, 1);
					//printf("5 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,0);
					_lseek(sahara_data.fd, 0, SEEK_SET);
#endif
                    sahara_data.timed_data_size = 0;
                    gettimeofday(&time_start, NULL);
                }
            }

            if (num_debug_entries >= 0)
            {
                memory_table_addr = sahara_memory_table[num_debug_entries].mem_base;
                memory_table_length = sahara_memory_table[num_debug_entries].length < sahara_data.max_ram_dump_read ? sahara_memory_table[num_debug_entries].length : sahara_data.max_ram_dump_read;

                dbg(LOG_EVENT, "SENDING --> SAHARA_MEMORY_READ");
                if (false == send_memory_read_packet(memory_table_addr, memory_table_length))
                {
                    dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
                    return false;
                }
                else
                    dbg(LOG_EVENT, "Sent SAHARA_MEMORY_READ, address 0x%"PRIX64", length 0x%"PRIX64, memory_table_addr, memory_table_length);

                sahara_data.timed_data_size += (size_t)memory_table_length;
            }
            else {
                dbg(LOG_INFO, "num_debug_entries not >=0");
                dbg(LOG_STATUS, "Successfully downloaded files from target ");
                if (sahara_data.mode != SAHARA_MODE_MEMORY_DEBUG) {
                    sahara_cmd_switch_mode->header.command = SAHARA_CMD_SWITCH_MODE_ID;
                    sahara_cmd_switch_mode->header.length = sizeof(sahara_packet_cmd_switch_mode);
                    sahara_cmd_switch_mode->mode = SAHARA_MODE_IMAGE_TX_PENDING;
                    if (false == sahara_tx_data (sizeof(sahara_packet_cmd_switch_mode))) {
                        dbg(LOG_ERROR, "Sending CMD_SWITCH_MODE packet failed");
                        return false;
                    }
                    use_wakelock(WAKELOCK_RELEASE);
                    sahara_data.state = SAHARA_WAIT_HELLO;
                }
                else {
                    dbg(LOG_EVENT, "SENDING --> SAHARA_RESET");
                    if (false == send_reset_command ()) {
                        dbg(LOG_ERROR, "Sending RESET packet failed");
                        return false;
                    }
                    use_wakelock(WAKELOCK_RELEASE);
                    sahara_data.state = SAHARA_WAIT_RESET_RESP;
                }
            }
            break;

        case SAHARA_WAIT_DONE_RESP:
          if (false == sahara_rx_data(0))
            return false;

          gettimeofday(&time_end, NULL);
          time_throughput_calculate(&time_start, &time_end, sahara_data.timed_data_size);
          dbg(LOG_INFO,"SAHARA_WAIT_DONE_RESP recieved with SAHARA_MODE_IMAGE_TX_PENDING=0x%.8X\n",SAHARA_MODE_IMAGE_TX_PENDING);
          dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_DONE_RESP\n");

          use_wakelock(WAKELOCK_RELEASE);
          close_file(sahara_data.fd);

          if(CurrentImageId==13)
          {
            printf("NOTE: Target requested image 13 which is DeviceProgrammer. Forcing QUIT. This is by design, ** All is well ** SUCCESS!!\n\n");
            sahara_done_resp->image_tx_status = SAHARA_MODE_IMAGE_TX_COMPLETE;
          }

          if (SAHARA_MODE_IMAGE_TX_PENDING == sahara_done_resp->image_tx_status) {
               dbg(LOG_INFO, "Still More images to be uploaded, entering Hello wait state");
               sahara_data.state = SAHARA_WAIT_HELLO;
          }
          else if (SAHARA_MODE_IMAGE_TX_COMPLETE == sahara_done_resp->image_tx_status) {
               dbg(LOG_EVENT,"\n\nSuccessfully uploaded all images\n");
               //delete_file_from_list(id_mapped_file); // taken care of in destroy_input_file_list()
               return true;
          }
          else {
           dbg(LOG_ERROR, "Received unrecognized status %d at SAHARA_WAIT_DONE_RESP state",  sahara_done_resp->image_tx_status);
           return false;
          }

          /*delete the file from the file list*/
          //delete_file_from_list(id_mapped_file); // taken care of in destroy_input_file_list()
          break;

        case SAHARA_WAIT_RESET_RESP:
          if (true == sahara_rx_data(0)) {
                if (SAHARA_RESET_RESP_ID != sahara_reset_resp->header.command) {
                    dbg(LOG_INFO,"Waiting for reset response code %i, received %i instead.", SAHARA_RESET_RESP_ID, sahara_reset_resp->header.command);
                    continue;
                }
          }
          else{
              dbg(LOG_ERROR, "read failed: Linux system error: %s", strerror(errno));
              return false;
          }
          dbg(LOG_EVENT, "RECEIVED <-- SAHARA_RESET_RESP");

          if(sahara_data.mode != sahara_hello_resp_mode) // sahara_hello->mode
          {
              // we wanted to load the build, but we were forced to do a memory dump
              dbg(LOG_ERROR, "Exiting abruptly");
              return false;
          }
          else {
              return true;
          }

        default:
          dbg(LOG_ERROR, "Unrecognized state %d",  sahara_data.state);
          return false;
        } /* end switch */
    } /* end while (1) */
}

bool sahara_main(bool is_efs_sync) {
    bool retval;

    sahara_data.rx_buffer = memalign (getpagesize (), SAHARA_RAW_BUFFER_SIZE);
    sahara_data.tx_buffer = memalign (getpagesize (), SAHARA_RAW_BUFFER_SIZE);
    sahara_data.misc_buffer = memalign (getpagesize (), SAHARA_RAW_BUFFER_SIZE);

    if (NULL == sahara_data.rx_buffer || NULL == sahara_data.tx_buffer || NULL == sahara_data.misc_buffer) {
        dbg(LOG_ERROR, "Failed to allocate sahara buffers");
        return false;
    }

    do {
        retval = sahara_start();
        if (false == retval) {
            dbg(LOG_ERROR, "Sahara protocol error");
            break;
        }
        else {
            dbg(LOG_STATUS, "Sahara protocol completed");
        }
    } while (true == is_efs_sync);

    ALIGNED_FREE(sahara_data.rx_buffer);
    ALIGNED_FREE(sahara_data.tx_buffer);
    ALIGNED_FREE(sahara_data.misc_buffer);

    sahara_data.rx_buffer = sahara_data.tx_buffer = sahara_data.misc_buffer = NULL;
    return retval;
}

static bool is_end_of_image_rx (uint32_t command)
{
    if (command == SAHARA_END_IMAGE_TX_ID)
        return true;
    return false;
}

static bool send_reset_command ()
{
    sahara_packet_reset *sahara_reset = (sahara_packet_reset *)sahara_data.tx_buffer;
    sahara_reset->header.command = SAHARA_RESET_ID;
    sahara_reset->header.length = sizeof(sahara_packet_reset);

    /* Send the Reset Request */
    if (false == sahara_tx_data (sizeof(sahara_packet_reset))) {
        fprintf (stderr, "Tx Sahara Data Failed ");
        return false;
    }

    return true;
}

static bool is_ack_successful (int status)
{
    switch (status) {

    /*Success*/
    case SAHARA_STATUS_SUCCESS :
        dbg(LOG_INFO, "SAHARA_STATUS_SUCCESS");
        return true;

    /*Invalid command received in current state*/
    case SAHARA_NAK_INVALID_CMD:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_CMD");
        break;

    /*Protocol mismatch between host and target*/
    case SAHARA_NAK_PROTOCOL_MISMATCH:
        dbg(LOG_ERROR, "SAHARA_NAK_PROTOCOL_MISMATCH");
        break;

    /*Invalid target protocol version*/
    case SAHARA_NAK_INVALID_TARGET_PROTOCOL:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_TARGET_PROTOCOL");
        break;

    /*Invalid host protocol version*/
    case SAHARA_NAK_INVALID_HOST_PROTOCOL:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_HOST_PROTOCOL");
        break;

    /*Invalid packet size received*/
    case SAHARA_NAK_INVALID_PACKET_SIZE:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_PACKET_SIZE");
        break;

    /*Unexpected image ID received*/
    case SAHARA_NAK_UNEXPECTED_IMAGE_ID:
        dbg(LOG_ERROR, "SAHARA_NAK_UNEXPECTED_IMAGE_ID");
        break;

    /*Invalid image header size received*/
    case SAHARA_NAK_INVALID_HEADER_SIZE:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_HEADER_SIZE");
        break;

    /*Invalid image data size received*/
    case SAHARA_NAK_INVALID_DATA_SIZE:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_DATA_SIZE");
        break;

    /*Unsupported image type received*/
    case SAHARA_NAK_INVALID_IMAGE_TYPE:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_IMAGE_TYPE");
        break;

    /*Invalid tranmission length*/
    case SAHARA_NAK_INVALID_TX_LENGTH:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_TX_LENGTH");
        break;

    /*Invalid reception length*/
    case SAHARA_NAK_INVALID_RX_LENGTH :
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_RX_LENGTH");
        break;

    /*General transmission or reception error*/
    case  SAHARA_NAK_GENERAL_TX_RX_ERROR:
        dbg(LOG_ERROR, "SAHARA_NAK_GENERAL_TX_RX_ERROR");
        break;

    /*Error while transmitting READ_DATA packet*/
    case SAHARA_NAK_READ_DATA_ERROR:
        dbg(LOG_ERROR, "SAHARA_NAK_READ_DATA_ERROR");
        break;

    /*Cannot receive specified number of program headers*/
    case SAHARA_NAK_UNSUPPORTED_NUM_PHDRS:
        dbg(LOG_ERROR, "SAHARA_NAK_UNSUPPORTED_NUM_PHDRS");
        break;

    /*Invalid data length received for program headers*/
    case SAHARA_NAK_INVALID_PDHR_SIZE:
        dbg(LOG_ERROR, "SAHARA_NAK_INVALID_PDHR_SIZE");
        break;

    /*Multiple shared segments found in ELF image*/
    case SAHARA_NAK_MULTIPLE_SHARED_SEG:
        dbg(LOG_ERROR, "SAHARA_NAK_MULTIPLE_SHARED_SEG");
        break;

    /*Uninitialized program header location*/
    case SAHARA_NAK_UNINIT_PHDR_LOC:
        dbg(LOG_ERROR, "SAHARA_NAK_UNINIT_PHDR_LOC");
        break;

    /* Invalid destination address*/
    case  SAHARA_NAK_INVALID_DEST_ADDR:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_DEST_ADDR");
       break;

    /* Invalid data size receieved in image header*/
    case SAHARA_NAK_INVALID_IMG_HDR_DATA_SIZE:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_IMG_HDR_DATA_SIZE");
       break;

    /* Invalid ELF header received*/
    case SAHARA_NAK_INVALID_ELF_HDR:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_ELF_HDR");
       break;

    /* Unknown host error received in HELLO_RESP*/
    case SAHARA_NAK_UNKNOWN_HOST_ERROR:
       dbg(LOG_ERROR, "SAHARA_NAK_UNKNOWN_HOST_ERROR");
       break;

    // Timeout while receiving data
    case SAHARA_NAK_TIMEOUT_RX:
       dbg(LOG_ERROR, "SAHARA_NAK_TIMEOUT_RX");
       break;

    // Timeout while transmitting data
    case SAHARA_NAK_TIMEOUT_TX:
       dbg(LOG_ERROR, "SAHARA_NAK_TIMEOUT_TX");
       break;

    // Invalid mode received from host
    case SAHARA_NAK_INVALID_HOST_MODE:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_HOST_MODE");
       break;

    // Invalid memory read access
    case SAHARA_NAK_INVALID_MEMORY_READ:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_MEMORY_READ");
       break;

    // Host cannot handle read data size requested
    case SAHARA_NAK_INVALID_DATA_SIZE_REQUEST:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_DATA_SIZE_REQUEST");
       break;

    // Memory debug not supported
    case SAHARA_NAK_MEMORY_DEBUG_NOT_SUPPORTED:
       dbg(LOG_ERROR, "SAHARA_NAK_MEMORY_DEBUG_NOT_SUPPORTED");
       break;

    // Invalid mode switch
    case SAHARA_NAK_INVALID_MODE_SWITCH:
       dbg(LOG_ERROR, "SAHARA_NAK_INVALID_MODE_SWITCH");
       break;

    // Failed to execute command
    case SAHARA_NAK_CMD_EXEC_FAILURE:
       dbg(LOG_ERROR, "SAHARA_NAK_CMD_EXEC_FAILURE");
       break;

    // Invalid parameter passed to command execution
    case SAHARA_NAK_EXEC_CMD_INVALID_PARAM:
       dbg(LOG_ERROR, "SAHARA_NAK_EXEC_CMD_INVALID_PARAM");
       break;

    // Unsupported client command received
    case SAHARA_NAK_EXEC_CMD_UNSUPPORTED:
       dbg(LOG_ERROR, "SAHARA_NAK_EXEC_CMD_UNSUPPORTED");
       break;

    // Invalid client command received for data response
    case SAHARA_NAK_EXEC_DATA_INVALID_CLIENT_CMD:
       dbg(LOG_ERROR, "SAHARA_NAK_EXEC_DATA_INVALID_CLIENT_CMD");
       break;

    // Failed to authenticate hash table
    case SAHARA_NAK_HASH_TABLE_AUTH_FAILURE:
       dbg(LOG_ERROR, "SAHARA_NAK_HASH_TABLE_AUTH_FAILURE");
       break;

    // Failed to verify hash for a given segment of ELF image
    case SAHARA_NAK_HASH_VERIFICATION_FAILURE:
       dbg(LOG_ERROR, "SAHARA_NAK_HASH_VERIFICATION_FAILURE");
       break;

    // Failed to find hash table in ELF image
    case SAHARA_NAK_HASH_TABLE_NOT_FOUND:
       dbg(LOG_ERROR, "SAHARA_NAK_HASH_TABLE_NOT_FOUND");
       break;

    case SAHARA_NAK_LAST_CODE:
        dbg(LOG_ERROR, "SAHARA_NAK_LAST_CODE");
        break;

    default:
        dbg(LOG_ERROR, "Invalid status field %d", status);
        break;
    }
    return false;
}

static bool start_image_transfer_64bit()
{
    int retval = 0;
    sahara_packet_read_data_64bit *sahara_read_data_64bit = (sahara_packet_read_data_64bit *)sahara_data.rx_buffer;
    int is_reg_file = is_regular_file(sahara_data.fd);
	uint32_t bytes_read = 0, bytes_to_read_next;

	uint64_t DataOffset = 0;
	uint64_t DataLength = 0;

    if (is_reg_file == -1) {
        dbg(LOG_ERROR, "Could not get file info.");
        return false;
    }

	//printf("\nUsing version %d so 64 bit\n",UsingSaharaVersion);
	//printf("\nUsing version %d so 64 bit\n",UsingSaharaVersion);
	//printf("\nUsing version %d so 64 bit\n",UsingSaharaVersion);
	DataOffset	= sahara_read_data_64bit->data_offset;
	DataLength	= sahara_read_data_64bit->data_length;

	dbg(LOG_INFO, "start_image_transfer_64bit: Offset %ld", DataOffset);
	dbg(LOG_INFO, "start_image_transfer_64bit: Offset 0x%X", DataOffset);
	dbg(LOG_INFO, "start_image_transfer_64bit: Length %ld", DataLength);
	dbg(LOG_INFO, "start_image_transfer_64bit: Length 0x%X", DataLength);

    /*check if the input request is valid*/
    if ((0 == DataLength) ||
        //(is_reg_file == 1  &&  (sahara_read_data->data_offset + sahara_read_data->data_length) > get_file_size(sahara_data.fd))) {
		(is_reg_file == 1  &&  (DataOffset + DataLength) > (uint64_t)get_file_size(sahara_data.fd))) {
			
          dbg(LOG_ERROR, "Invalid length %ld bytes request to be transmitted", DataLength);
          dbg(LOG_ERROR, "File offset %ld, file size %d", DataOffset, get_file_size(sahara_data.fd));
          return false;
    }

    /*read data to buffer based on the offset*/
    //lseek(sahara_data.fd, sahara_read_data->data_offset, SEEK_SET);
	//printf("6 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,DataOffset);
	lseek(sahara_data.fd, (long)DataOffset, SEEK_SET); // 2
    while (bytes_read < (uint32_t)DataLength) {
		bytes_to_read_next = MIN((uint32_t)DataLength - bytes_read, SAHARA_RAW_BUFFER_SIZE);
		retval = read(sahara_data.fd, sahara_data.tx_buffer, bytes_to_read_next);
		if (retval < 0) {
			  dbg(LOG_ERROR, "file read failed: %s", strerror(errno));
			  return false;
		}
		if ((uint32_t) retval != bytes_to_read_next) {
			dbg(LOG_ERROR, "Read %d bytes, but was asked for %ld bytes", retval, DataLength);
			return false;
		}

		/*send the image data*/
		if (false == sahara_tx_data (bytes_to_read_next)) {
			dbg(LOG_ERROR, "Tx Sahara Image Failed");
			return false;
		}
		bytes_read += bytes_to_read_next;
	}

    sahara_data.timed_data_size += (uint32_t)DataLength;
    return true;
}


static bool start_image_transfer()
{
    int retval = 0;
    sahara_packet_read_data *sahara_read_data = (sahara_packet_read_data *)sahara_data.rx_buffer;
    int is_reg_file = is_regular_file(sahara_data.fd);
	uint32_t bytes_read = 0, bytes_to_read_next;

	uint64_t DataOffset = 0;
	uint64_t DataLength = 0;

    if (is_reg_file == -1) {
        dbg(LOG_ERROR, "Could not get file info.");
        return false;
    }

	//printf("\nUsing version %d\n",UsingSaharaVersion);
	//printf("\nUsing version %d\n",UsingSaharaVersion);
	//printf("\nUsing version %d\n",UsingSaharaVersion);
	DataOffset = (uint64_t)sahara_read_data->data_offset;
	DataLength = (uint64_t)sahara_read_data->data_length;


	dbg(LOG_INFO, "start_image_transfer: Offset %ld", DataOffset);
	dbg(LOG_INFO, "start_image_transfer: Offset 0x%X", DataOffset);
	dbg(LOG_INFO, "start_image_transfer: Length %ld", DataLength);
	dbg(LOG_INFO, "start_image_transfer: Length 0x%X", DataLength);

    /*check if the input request is valid*/
    if ((0 == DataLength) ||
        //(is_reg_file == 1  &&  (sahara_read_data->data_offset + sahara_read_data->data_length) > get_file_size(sahara_data.fd))) {
		(is_reg_file == 1  &&  (DataOffset + DataLength) > (uint64_t)get_file_size(sahara_data.fd))) {
			
          dbg(LOG_ERROR, "Invalid length %ld bytes request to be transmitted", DataLength);
          dbg(LOG_ERROR, "File offset %ld, file size %d", DataOffset, get_file_size(sahara_data.fd));
          return false;
    }

    /*read data to buffer based on the offset*/
    //lseek(sahara_data.fd, sahara_read_data->data_offset, SEEK_SET);
	//printf("1 lseek(0x%X, %ld,SEEK_SET)",sahara_data.fd,DataOffset);
	lseek(sahara_data.fd, (long)DataOffset, SEEK_SET); // 1
    while (bytes_read < (uint32_t)DataLength) {
		bytes_to_read_next = MIN((uint32_t)DataLength - bytes_read, SAHARA_RAW_BUFFER_SIZE);
		retval = read(sahara_data.fd, sahara_data.tx_buffer, bytes_to_read_next);
		if (retval < 0) {
			  dbg(LOG_ERROR, "file read failed: %s", strerror(errno));
			  return false;
		}
		if ((uint32_t) retval != bytes_to_read_next) {
			dbg(LOG_ERROR, "Read %d bytes, but was asked for %ld bytes", retval, DataLength);
			return false;
		}

		/*send the image data*/
		if (false == sahara_tx_data (bytes_to_read_next)) {
			dbg(LOG_ERROR, "Tx Sahara Image Failed");
			return false;
		}
		bytes_read += bytes_to_read_next;
	}

    sahara_data.timed_data_size += (uint32_t)DataLength;
    return true;
}

static bool send_done_packet ()
{
    sahara_packet_done *sahara_done = (sahara_packet_done *)sahara_data.tx_buffer;

    sahara_done->header.command = SAHARA_DONE_ID;
    sahara_done->header.length = sizeof(sahara_packet_done);
    // Send the image data
    if (false == sahara_tx_data (sizeof(sahara_packet_done))) {
        dbg(LOG_ERROR, "Sending DONE packet failed");
        return false;
    }
    return true;
}

static bool send_memory_read_packet (uint64_t memory_table_address, uint64_t memory_table_length)
{
    sahara_packet_memory_read *sahara_memory_read = (sahara_packet_memory_read *)sahara_data.tx_buffer;
	sahara_packet_memory_read_64bit *sahara_memory_read_64bit = (sahara_packet_memory_read_64bit *)sahara_data.tx_buffer;

	if (true == sahara_data.ram_dump_64bit) {
		sahara_memory_read_64bit->header.command = SAHARA_64_BITS_MEMORY_READ_ID;
		sahara_memory_read_64bit->header.length = sizeof(sahara_packet_memory_read_64bit);
		sahara_memory_read_64bit->memory_addr = memory_table_address;
		sahara_memory_read_64bit->memory_length = memory_table_length;

		/* Send the Memory Read packet */
		if (false == sahara_tx_data (sizeof(sahara_packet_memory_read_64bit))) {
			dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
			return false;
		}
	}
	else {
		sahara_memory_read->header.command	= SAHARA_MEMORY_READ_ID;
		sahara_memory_read->header.length	= sizeof(sahara_packet_memory_read);
		sahara_memory_read->memory_addr		= (uint32_t)memory_table_address;
		sahara_memory_read->memory_length	= (uint32_t)memory_table_length;

		/* Send the Memory Read packet */
		if (false == sahara_tx_data (sizeof(sahara_packet_memory_read))) {
			dbg(LOG_ERROR, "Sending MEMORY_READ packet failed");
			return false;
		}
	}
    return true;
}


static bool is_valid_memory_table(uint64_t memory_table_size)
{
	if (true == sahara_data.ram_dump_64bit && memory_table_size % sizeof(dload_debug_type_64bit) == 0) {
        return true;
    }
	else if (false == sahara_data.ram_dump_64bit && memory_table_size % sizeof(dload_debug_type) == 0) {
		return true;
	}
	else {
		return false;
	}
}

static int send_cmd_exec_command (unsigned int command)
{
    //fd_set rfds;
    //struct timeval tv;
    int retval;
    int print_index = 0;
	int fd;

    int response_length = 0;

    sahara_packet_cmd_exec *sahara_cmd_exec = (sahara_packet_cmd_exec *)sahara_data.tx_buffer;
    sahara_packet_cmd_exec_data *sahara_cmd_exec_data = (sahara_packet_cmd_exec_data *)sahara_data.tx_buffer;
    //sahara_packet_cmd_switch_mode *sahara_cmd_switch_mode = (sahara_packet_cmd_switch_mode *)sahara_data.tx_buffer;
    sahara_packet_cmd_exec_resp *sahara_cmd_exec_resp = (sahara_packet_cmd_exec_resp *)sahara_data.rx_buffer;

    sahara_cmd_exec->header.command = SAHARA_CMD_EXEC_ID;
    sahara_cmd_exec->header.length = sizeof(sahara_packet_cmd_exec);
    sahara_cmd_exec->client_command = command;

    dbg(LOG_EVENT, "SENDING --> SAHARA_CMD_EXEC");

    /*Send the image data*/
    if (false == sahara_tx_data (sizeof(sahara_packet_cmd_exec))) {
        dbg(LOG_ERROR, "Sending CMD_EXEC packet failed");
        return false;
    }

    /*set the state to SAHARA_WAIT_CMD_EXEC_RESP*/
    dbg(LOG_EVENT, "STATE <-- SAHARA_WAIT_CMD_EXEC_RESP\n");
    sahara_data.state = SAHARA_WAIT_CMD_EXEC_RESP;

    /* Retreive CMD_EXEC_RESP */
    if (false == sahara_rx_data(0)) {
        return false;
    }

    if (sahara_cmd_exec_resp->header.command == SAHARA_CMD_EXEC_RESP_ID) {
        dbg(LOG_EVENT, "RECEIVED <-- SAHARA_CMD_EXEC_RESP");
        if (sahara_cmd_exec_resp->resp_length > 0) {

			response_length = sahara_cmd_exec_resp->resp_length;

			sahara_cmd_exec_data->header.command = SAHARA_CMD_EXEC_DATA_ID;
            sahara_cmd_exec_data->header.length = sizeof(sahara_packet_cmd_exec_data);
            sahara_cmd_exec_data->client_command = command;

            dbg(LOG_EVENT, "SENDING --> SAHARA_CMD_EXEC_DATA");

            /*Send the packet data*/
            if (false == sahara_tx_data (sizeof(sahara_packet_cmd_exec_data))) {
                dbg(LOG_ERROR, "Sending CMD_EXEC_DATA packet failed");
                return false;
            }

            // Retreive data
            if (false == sahara_rx_data(response_length)) {
                return false;
            }

            dbg(LOG_EVENT, "RECEIVED <-- RAW_DATA");

            /* Print data */
            for (print_index = 0;
                    print_index < response_length;
                    print_index++)
            {
                dbg(LOG_INFO, "Received Byte: 0x%02x", sahara_data.rx_buffer[print_index]);
            }

			if (response_length > 0) {
				fd = open_file("commandop.bin", false);
				if (-1 == fd) {
					dbg(LOG_ERROR, "Could not open file to write command data");
					return false;
				}
				retval = write(fd, sahara_data.rx_buffer, response_length);
				if (retval < 0) {
					dbg(LOG_ERROR, "file write failed: %s", strerror(errno));
					close_file(fd);
					return false;
				}
				if (retval != response_length) {
					dbg(LOG_WARN, "Wrote only %d of %d bytes", retval, response_length);
				}
				close_file(fd);
			}
        }
    }
    else {
        dbg(LOG_ERROR, "Received an unknown command: %d ", sahara_cmd_exec_resp->header.command);
        return false;
    }

/*
    // Send command switch mode
    sahara_cmd_switch_mode->header.command = SAHARA_CMD_SWITCH_MODE_ID;
    sahara_cmd_switch_mode->header.length = sizeof(sahara_packet_cmd_switch_mode);
    sahara_cmd_switch_mode->mode = sahara_data.prev_mode;

    dbg(LOG_EVENT, "SENDING --> SAHARA_CMD_SWITCH_MODE");

    // Send the packet data
    if (false == sahara_tx_data (sizeof(sahara_packet_cmd_switch_mode))) {
        dbg(LOG_ERROR, "Sending CMD_SWITCH_MODE packet failed");
        return false;
    }
*/

	return true;
}

/* Subtract the `struct timeval' values X and Y,
   storing the result in RESULT.
   Return 1 if the difference is negative, otherwise 0. */
static int timeval_subtract (struct timeval *result, struct timeval *x, struct timeval *y) {
    // Perform the carry for the later subtraction by updating y.
    if (x->tv_usec < y->tv_usec) {
        int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
        y->tv_usec -= 1000000 * nsec;
        y->tv_sec += nsec;
    }
    if (x->tv_usec - y->tv_usec > 1000000) {
        int nsec = (x->tv_usec - y->tv_usec) / 1000000;
        y->tv_usec += 1000000 * nsec;
        y->tv_sec -= nsec;
    }

    // Compute the time remaining to wait. tv_usec is certainly positive. */
    result->tv_sec = x->tv_sec - y->tv_sec;
    result->tv_usec = x->tv_usec - y->tv_usec;

    // Return 1 if result is negative.
    return x->tv_sec < y->tv_sec;
}

void time_throughput_calculate(struct timeval *start_time, struct timeval *end_time, size_t size_bytes)
{
    struct timeval result;
	double TP = 0.0;

    if (size_bytes == 0) {
        dbg(LOG_INFO, "Cannot calculate throughput, size is 0");
        return;
    }
    timeval_subtract(&result, end_time, start_time);

	TP  = (double)result.tv_usec/1000000.0;
	TP += (double)result.tv_sec;

	if(TP>0.0)
	{
		TP = (double)((double)size_bytes/TP)/(1024.0*1024.0);
		dbg(LOG_STATUS, "%d bytes transferred in %ld.%06ld seconds (%.4fMBps)\n\n", size_bytes, result.tv_sec, result.tv_usec,TP);
	}
	else
		dbg(LOG_STATUS, "%d bytes transferred in %ld.%06ld seconds", size_bytes, result.tv_sec, result.tv_usec);

	dbg(LOG_STATUS, "File transferred successfully\n\n");

}

