/** @file
  HddPassword PEI module which is used to unlock HDD password for S3.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _HDD_PASSWORD_PEI_H_
#define _HDD_PASSWORD_PEI_H_

#include <PiPei.h>
#include <IndustryStandard/Atapi.h>
//#include <IndustryStandard/Pci.h>

#include <Library/DebugLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PeimEntryPoint.h>
#include <Library/PeiServicesLib.h>
#include <Library/PciLib.h>
#include <Library/LockBoxLib.h>

#include <Ppi/AtaPassThru.h>

#include "HddPasswordCommon.h"

//
// The maximum number of ATA PassThru PPI instances supported by the driver
//
#define MAX_ATA_PASSTHRU_PPI                 32

//
// Time out value for ATA PassThru PPI
//
#define ATA_TIMEOUT                          30000000

//
// Private data structure for HddPassword PEI driver
//
#define HDD_PASSWORD_PEI_DRIVER_SIGNATURE    SIGNATURE_32 ('h', 'd', 'r', 'i')

typedef struct {
  UINTN                        Signature;
  EFI_PEI_NOTIFY_DESCRIPTOR    AtaPassThruPpiNotifyList;

  UINTN                        AtaPassThruPpiInstanceNum;
  UINTN                        AtaPassThruPpiInstances[MAX_ATA_PASSTHRU_PPI];
} HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA;

#define HDD_PASSWORD_PEI_PRIVATE_DATA_FROM_THIS_NOTIFY(a)    \
  CR (a, HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA, AtaPassThruPpiNotifyList, HDD_PASSWORD_PEI_DRIVER_SIGNATURE)

#endif

