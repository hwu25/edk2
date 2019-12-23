/** @file
  The microcode patch HOB is used to store the information of:
    A. Base address and size of the loaded microcode patches data;
    B. Detected microcode patch for each processor within system.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php.

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _MICROCODE_PATCH_HOB_H_
#define _MICROCODE_PATCH_HOB_H_

extern EFI_GUID gEdkiiMicrocodePatchHobGuid;

//
// The EDKII microcode patch HOB will be produced by MpInitLib and it can be
// consumed by modules that want to detect/apply microcode patches.
//
typedef struct {
  //
  // The base address of the microcode patches data after being loaded into
  // memory.
  //
  UINT64    MicrocodePatchAddress;
  //
  // The total size of the loaded microcode patches.
  //
  UINT64    MicrocodePatchRegionSize;
  //
  // The number of processors within the system.
  //
  UINT32    ProcessorCount;
  //
  // An array with 'ProcessorCount' elements that stores the offset (with
  // regard to 'MicrocodePatchAddress') of the detected microcode patch
  // (including the CPU_MICROCODE_HEADER data structure) for each processor.
  // If no microcode patch is detected for certain processor, the relating
  // element will be set to MAX_UINT64.
  //
  UINT64    ProcessorSpecificPatchOffset[0];
} EDKII_MICROCODE_PATCH_HOB;

#endif
