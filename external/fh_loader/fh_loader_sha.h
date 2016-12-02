/**************************************************************************
 *
 * This module contains the software implementation for sha256.
 *
 * Copyright (c) 2016 Qualcomm Technologies, Inc.
 * All Rights Reserved.
 *
 *************************************************************************/

/*===========================================================================

                        EDIT HISTORY FOR MODULE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header: //components/rel/boot.xf/1.0/QcomPkg/Tools/storage/fh_loader/fh_loader/fh_loader_sha.h#1 $
  $DateTime: 2016/05/11 00:15:02 $
  $Author: pwbldsvc $

when         who   what, where, why
----------   ---   ---------------------------------------------------------
2016-01-14   wek   Create. Move SHA functions from security to a new file.

===========================================================================*/


#ifndef __DEVICEPROGRAMMER_SHA_H__
#define __DEVICEPROGRAMMER_SHA_H__

#include "fh_comdef.h"

#define CONTEXT_LEFTOVER_FIELD_SIZE 64

struct __sechsh_ctx_s
{
    uint32  counter[2];
    uint32  iv[16];  // is 64 byte for SHA2-512
    uint8   leftover[CONTEXT_LEFTOVER_FIELD_SIZE];
    uint32  leftover_size;
};

void sechsharm_sha256_init(   struct __sechsh_ctx_s* );
void sechsharm_sha256_update( struct __sechsh_ctx_s*, uint8*, uint32*, uint8*, uint32 );
void sechsharm_sha256_final(  struct __sechsh_ctx_s*, uint8*, uint32*, uint8* );

#endif /* __DEVICEPROGRAMMER_SHA_H__ */
