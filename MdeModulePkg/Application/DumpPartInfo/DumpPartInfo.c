/** @file

  Copyright (c) 2017, Intel Corporation. All rights reserved.<BR>
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <Uefi.h>
#include <Library/UefiApplicationEntryPoint.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>

#include <Protocol/PartitionInfo.h>

VOID
DumpPartInfoProt (
  IN EFI_PARTITION_INFO_PROTOCOL    *PartInfo
  )
{
  UINT32    Index;
  UINT8     *InfoPtr;

  DEBUG ((DEBUG_INFO, "\tRevision\t is 0x%x\n", PartInfo->Revision));
  DEBUG ((DEBUG_INFO, "\tType\t is 0x%x\n", PartInfo->Type));
  DEBUG ((DEBUG_INFO, "\tSystem\t is 0x%x\n", PartInfo->System));

  DEBUG ((DEBUG_INFO, "\tReserved bytes: "));
  for (Index = 0; Index < sizeof (PartInfo->Reserved); Index++) {
    DEBUG ((DEBUG_INFO, "0x%02x ", PartInfo->Reserved[Index]));
  }
  DEBUG ((DEBUG_INFO, "\n"));

  switch (PartInfo->Type) {
  case PARTITION_TYPE_OTHER:
    DEBUG ((DEBUG_INFO, "Other - Should be all 0s:\n"));
    for (Index = 0, InfoPtr = (UINT8 *)&PartInfo->Info; Index < sizeof (PartInfo->Info); Index++, InfoPtr++) {
      DEBUG ((DEBUG_INFO, "Other - Index:\t %d, Content:\t ", Index));
      DEBUG ((DEBUG_INFO, "0x%02x\n", *InfoPtr));
    }
    break;

  case PARTITION_TYPE_MBR:
    DEBUG ((DEBUG_INFO, "MBR - BootIndicator:\t 0x%x\n", PartInfo->Info.Mbr.BootIndicator));
    DEBUG ((DEBUG_INFO, "MBR - StartHead:\t 0x%x\n", PartInfo->Info.Mbr.StartHead));
    DEBUG ((DEBUG_INFO, "MBR - StartSector:\t 0x%x\n", PartInfo->Info.Mbr.StartSector));
    DEBUG ((DEBUG_INFO, "MBR - StartTrack:\t 0x%x\n", PartInfo->Info.Mbr.StartTrack));
    DEBUG ((DEBUG_INFO, "MBR - OSIndicator:\t 0x%x\n", PartInfo->Info.Mbr.OSIndicator));
    DEBUG ((DEBUG_INFO, "MBR - EndHead:\t 0x%x\n", PartInfo->Info.Mbr.EndHead));
    DEBUG ((DEBUG_INFO, "MBR - EndSector:\t 0x%x\n", PartInfo->Info.Mbr.EndSector));
    DEBUG ((DEBUG_INFO, "MBR - EndTrack:\t 0x%x\n", PartInfo->Info.Mbr.EndTrack));

    DEBUG ((DEBUG_INFO, "MBR - StartingLBA:\t 0x%02x", PartInfo->Info.Mbr.StartingLBA[3]));
    DEBUG ((DEBUG_INFO, "%02x", PartInfo->Info.Mbr.StartingLBA[2]));
    DEBUG ((DEBUG_INFO, "%02x", PartInfo->Info.Mbr.StartingLBA[1]));
    DEBUG ((DEBUG_INFO, "%02x\n", PartInfo->Info.Mbr.StartingLBA[0]));

    DEBUG ((DEBUG_INFO, "MBR - SizeInLBA:\t 0x%02x", PartInfo->Info.Mbr.SizeInLBA[3]));
    DEBUG ((DEBUG_INFO, "%02x", PartInfo->Info.Mbr.SizeInLBA[2]));
    DEBUG ((DEBUG_INFO, "%02x", PartInfo->Info.Mbr.SizeInLBA[1]));
    DEBUG ((DEBUG_INFO, "%02x\n", PartInfo->Info.Mbr.SizeInLBA[0]));
    break;

  case PARTITION_TYPE_GPT:
    DEBUG ((DEBUG_INFO, "GPT - PartitionTypeGUID:\t %g\n", &PartInfo->Info.Gpt.PartitionTypeGUID));
    DEBUG ((DEBUG_INFO, "GPT - UniquePartitionGUID:\t %g\n", &PartInfo->Info.Gpt.UniquePartitionGUID));
    DEBUG ((DEBUG_INFO, "GPT - StartingLBA:\t 0x%p\n", PartInfo->Info.Gpt.StartingLBA));
    DEBUG ((DEBUG_INFO, "GPT - EndingLBA:\t 0x%p\n", PartInfo->Info.Gpt.EndingLBA));
    DEBUG ((DEBUG_INFO, "GPT - Attributes:\t 0x%p\n", PartInfo->Info.Gpt.Attributes));
    DEBUG ((DEBUG_INFO, "GPT - PartitionName:\t %s\n", PartInfo->Info.Gpt.PartitionName));
    break;

  default:
    ASSERT (FALSE);
    break;
  }

  return;
}

/**
  The user Entry Point for Application. The user code starts with this function
  as the real entry point for the application.

  @param[in] ImageHandle    The firmware allocated handle for the EFI image.
  @param[in] SystemTable    A pointer to the EFI System Table.

  @retval EFI_SUCCESS       The entry point is executed successfully.
  @retval other             Some error occurs when executing this entry point.

**/
EFI_STATUS
EFIAPI
UefiMain (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                          Status;
  EFI_HANDLE                          *Handles;
  UINTN                               HandleCount;
  UINTN                               HandleIndex;
  EFI_DEVICE_PATH_PROTOCOL            *DevicePath;
  CHAR16                              *DPText;
  EFI_PARTITION_INFO_PROTOCOL         *PartInfo;

  Status = gBS->LocateHandleBuffer (
                ByProtocol,
                &gEfiPartitionInfoProtocolGuid,
                NULL,
                &HandleCount,
                &Handles
                );
  DEBUG ((DEBUG_INFO, "DumpPartInfo: HandleCount = 0x%p\n", HandleCount));
  for (HandleIndex = 0; HandleIndex < HandleCount; HandleIndex++) {
    DEBUG ((DEBUG_INFO, "DumpPartInfo: HandleIndex = 0x%p\n", HandleIndex));

    DevicePath = DevicePathFromHandle (Handles[HandleIndex]);
    if (NULL == DevicePath) {
      DEBUG ((DEBUG_ERROR, "DumpPartInfo: No DevicePath for this handle!\n"));
      continue;
    } else {
      DPText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
      DEBUG ((DEBUG_INFO, "DumpPartInfo: DevicePath is %s\n", DPText));
      FreePool (DPText);
    }

    Status = gBS->HandleProtocol (Handles[HandleIndex], &gEfiPartitionInfoProtocolGuid, (VOID **) &PartInfo);
    ASSERT_EFI_ERROR (Status);

    //
    // Dump the Partion Information Protocol contents.
    //
    DumpPartInfoProt (PartInfo);
  }

  return EFI_SUCCESS;
}
