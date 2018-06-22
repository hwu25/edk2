/** @file
  The device path help function.

  Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>

  This program and the accompanying materials
  are licensed and made available under the terms and conditions
  of the BSD License which accompanies this distribution.  The
  full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "AhciPei.h"

//
// Template for a SATA Device Path node
//
SATA_DEVICE_PATH  mAhciSataDevicePathNodeTemplate = {
  {        // Header
    MESSAGING_DEVICE_PATH,
    MSG_SATA_DP,
    {
      (UINT8) (sizeof (SATA_DEVICE_PATH)),
      (UINT8) ((sizeof (SATA_DEVICE_PATH)) >> 8)
    }
  },
  0x0,     // HBAPortNumber
  0xFFFF,  // PortMultiplierPortNumber
  0x0      // Lun
};

//
// Template for an End of entire Device Path node
//
EFI_DEVICE_PATH_PROTOCOL  mAhciEndDevicePathNodeTemplate = {
  END_DEVICE_PATH_TYPE,
  END_ENTIRE_DEVICE_PATH_SUBTYPE,
  {
    (UINT8) (sizeof (EFI_DEVICE_PATH_PROTOCOL)),
    (UINT8) ((sizeof (EFI_DEVICE_PATH_PROTOCOL)) >> 8)
  }
};

/**
  Returns the 16-bit Length field of a device path node.

  Returns the 16-bit Length field of the device path node specified by Node.
  Node is not required to be aligned on a 16-bit boundary, so it is recommended
  that a function such as ReadUnaligned16() be used to extract the contents of
  the Length field.

  If Node is NULL, then ASSERT().

  @param  Node      A pointer to a device path node data structure.

  @return The 16-bit Length field of the device path node specified by Node.

**/
UINTN
DevicePathNodeLength (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return ReadUnaligned16 ((UINT16 *)&((EFI_DEVICE_PATH_PROTOCOL *)(Node))->Length[0]);
}

/**
  Returns a pointer to the next node in a device path.

  If Node is NULL, then ASSERT().

  @param  Node    A pointer to a device path node data structure.

  @return a pointer to the device path node that follows the device path node
  specified by Node.

**/
EFI_DEVICE_PATH_PROTOCOL *
NextDevicePathNode (
  IN CONST VOID  *Node
  )
{
  ASSERT (Node != NULL);
  return (EFI_DEVICE_PATH_PROTOCOL *)((UINT8 *)(Node) + DevicePathNodeLength(Node));
}

/**
  Returns the size of a device path in bytes.

  This function returns the size, in bytes, of the device path data structure
  specified by DevicePath including the end of device path node.
  If DevicePath is NULL or invalid, then 0 is returned.

  @param  DevicePath  A pointer to a device path data structure.

  @retval 0           If DevicePath is NULL or invalid.
  @retval Others      The size of a device path in bytes.

**/
UINTN
GetDevicePathSize (
  IN CONST EFI_DEVICE_PATH_PROTOCOL  *DevicePath
  )
{
  CONST EFI_DEVICE_PATH_PROTOCOL  *Start;

  if (DevicePath == NULL) {
    return 0;
  }

  //
  // Search for the end of the device path structure
  //
  Start = DevicePath;
  while (!(DevicePath->Type == END_DEVICE_PATH_TYPE &&
           DevicePath->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE)) {
    DevicePath = NextDevicePathNode (DevicePath);
  }

  //
  // Compute the size and add back in the size of the end device path structure
  //
  return ((UINTN) DevicePath - (UINTN) Start) + DevicePathNodeLength (DevicePath);
}

/**
  Creates a copy of the current device path instance and returns a pointer to the
  next device path instance.

  @param  DevicePath    On input, this holds the pointer to the current
                        device path instance. On output, this holds
                        the pointer to the next device path instance
                        or NULL if there are no more device path
                        instances in the device path pointer to a
                        device path data structure.
  @param  Size          On output, this holds the size of the device
                        path instance, in bytes or zero, if DevicePath
                        is NULL.

  @return A pointer to the current device path instance.

**/
EFI_DEVICE_PATH_PROTOCOL *
GetNextDevicePathInstance (
  IN OUT EFI_DEVICE_PATH_PROTOCOL    **DevicePath,
  OUT UINTN                          *Size
  )
{
  EFI_DEVICE_PATH_PROTOCOL  *DevPath;
  EFI_DEVICE_PATH_PROTOCOL  *ReturnValue;
  UINTN                     DevicePathSize;
  UINT8                     Temp;

  ASSERT (Size != NULL);

  if (DevicePath == NULL || *DevicePath == NULL) {
    *Size = 0;
    return NULL;
  }

  //
  // Find the end of the device path instance
  //
  DevPath = *DevicePath;
  while (DevPath->Type != END_DEVICE_PATH_TYPE) {
    DevPath = NextDevicePathNode (DevPath);
  }

  //
  // Compute the size of the device path instance
  //
  *Size = ((UINTN) DevPath - (UINTN) (*DevicePath)) + sizeof (EFI_DEVICE_PATH_PROTOCOL);

  //
  // Make a copy and return the device path instance
  //
  Temp              = DevPath->SubType;
  DevPath->SubType  = END_ENTIRE_DEVICE_PATH_SUBTYPE;
  DevicePathSize    = GetDevicePathSize (*DevicePath);
  ReturnValue       = (DevicePathSize == 0) ? NULL : AllocateCopyPool (DevicePathSize, *DevicePath);
  DevPath->SubType  = Temp;

  //
  // If DevPath is the end of an entire device path, then another instance
  // does not follow, so *DevicePath is set to NULL.
  //
  if (DevPath->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE) {
    *DevicePath = NULL;
  } else {
    *DevicePath = NextDevicePathNode (DevPath);
  }

  return ReturnValue;
}

/**
  Check the validity of the device path of a ATA AHCI host controller.

  @param[in] DevicePath          A pointer to the EFI_DEVICE_PATH_PROTOCOL
                                 structure.
  @param[in] DevicePathLength    The length of the device path.

  @retval EFI_SUCCESS              The device path is valid.
  @retval EFI_INVALID_PARAMETER    The device path is invalid.

**/
EFI_STATUS
AhciCheckHcDevicePath (
  IN EFI_DEVICE_PATH_PROTOCOL    *DevicePath,
  IN UINTN                       DevicePathLength
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *Start;
  UINTN                       Size;

  if (DevicePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Start = DevicePath;
  while (!(DevicePath->Type == END_DEVICE_PATH_TYPE &&
           DevicePath->SubType == END_ENTIRE_DEVICE_PATH_SUBTYPE)) {
    DevicePath = NextDevicePathNode (DevicePath);
  }

  //
  // Check if the device path and its size match each other.
  //
  Size = ((UINTN) DevicePath - (UINTN) Start) + sizeof (EFI_DEVICE_PATH_PROTOCOL);
  if (Size != DevicePathLength) {
    return EFI_INVALID_PARAMETER;
  }

  return EFI_SUCCESS;
}

/**
  Build the device path for an ATA device with given port and port multiplier number.

  @param[in]  Private               A pointer to the PEI_AHCI_CONTROLLER_PRIVATE_DATA
                                    data structure.
  @param[in]  Port                  The given port number.
  @param[in]  PortMultiplierPort    The given port multiplier number.
  @param[out] DevicePathLength      The length of the device path in bytes specified
                                    by DevicePath.
  @param[out] DevicePath            The device path of ATA device.

  @retval EFI_SUCCESS               The operation succeeds.
  @retval EFI_INVALID_PARAMETER     The parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES      The operation fails due to lack of resources.

**/
EFI_STATUS
AhciBuildDevicePath (
  IN  PEI_AHCI_CONTROLLER_PRIVATE_DATA    *Private,
  IN  UINT16                              Port,
  IN  UINT16                              PortMultiplierPort,
  OUT UINTN                               *DevicePathLength,
  OUT EFI_DEVICE_PATH_PROTOCOL            **DevicePath
  )
{
  EFI_DEVICE_PATH_PROTOCOL    *DevicePathWalker;
  SATA_DEVICE_PATH            *SataDeviceNode;

  if (DevicePathLength == NULL || DevicePath == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *DevicePathLength = Private->DevicePathLength + sizeof (SATA_DEVICE_PATH);
  *DevicePath       = AllocatePool (*DevicePathLength);
  if (*DevicePath == NULL) {
    *DevicePathLength = 0;
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Construct the host controller part device nodes
  //
  DevicePathWalker = *DevicePath;
  CopyMem (
    DevicePathWalker,
    Private->DevicePath,
    Private->DevicePathLength - sizeof (EFI_DEVICE_PATH_PROTOCOL)
    );

  //
  // Construct the SATA device node
  //
  DevicePathWalker = (EFI_DEVICE_PATH_PROTOCOL *) ((UINT8 *)DevicePathWalker +
                     (Private->DevicePathLength - sizeof (EFI_DEVICE_PATH_PROTOCOL)));
  CopyMem (
    DevicePathWalker,
    &mAhciSataDevicePathNodeTemplate,
    sizeof (mAhciSataDevicePathNodeTemplate)
    );
  SataDeviceNode                           = (SATA_DEVICE_PATH *)DevicePathWalker;
  SataDeviceNode->HBAPortNumber            = Port;
  SataDeviceNode->PortMultiplierPortNumber = PortMultiplierPort;

  //
  // Construct the end device node
  //
  DevicePathWalker = (EFI_DEVICE_PATH_PROTOCOL *) ((UINT8 *)DevicePathWalker +
                     sizeof (SATA_DEVICE_PATH));
  CopyMem (
    DevicePathWalker,
    &mAhciEndDevicePathNodeTemplate,
    sizeof (mAhciEndDevicePathNodeTemplate)
    );

  return EFI_SUCCESS;
}
