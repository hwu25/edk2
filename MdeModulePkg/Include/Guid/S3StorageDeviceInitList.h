/** @file
  Define the LockBox GUID for list of storage devices need to be initialized in
  S3.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef __S3_STORAGE_DEVICE_INIT_LIST_H__
#define __S3_STORAGE_DEVICE_INIT_LIST_H__

#define S3_STORAGE_DEVICE_INIT_LIST \
  { \
    0x310e9b8c, 0xcf90, 0x421e, { 0x8e, 0x9b, 0x9e, 0xef, 0xb6, 0x17, 0xc8, 0xef } \
  }

//
// The LockBox will store a DevicePath structure that contains zero or more
// DevicePath instances. Each instance denotes a storage device that needs to
// get initialized during the S3 resume.
//
// The attribute of the LockBox should be set to
// 'LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY'.
//
extern EFI_GUID  gS3StorageDeviceInitListGuid;

#endif  // __S3_STORAGE_DEVICE_INIT_LIST_H__
