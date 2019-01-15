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

#include "HddPasswordPei.h"

EFI_GUID mHddPasswordDeviceInfoGuid = HDD_PASSWORD_DEVICE_INFO_GUID;

/**
  Tell whether the input EDKII_PEI_ATA_PASS_THRU_PPI instance has already been
  handled by the HddPassword PEI driver.

  @param[in] Private             Pointer to the HddPassword PEI driver private data.
  @param[in] PassThruInstance    Pointer to the EDKII_PEI_ATA_PASS_THRU_PPI.

  @retval TRUE     The PPI instance has already been handled.
  @retval FALSE    The PPI instance has not been handled before.

**/
BOOLEAN
IsPassThruInstanceHandled (
  IN HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA    *Private,
  IN EDKII_PEI_ATA_PASS_THRU_PPI             *PassThruInstance
  )
{
  UINTN    Index;

  for (Index = 0; Index < Private->AtaPassThruPpiInstanceNum; Index++) {
    if ((UINTN)PassThruInstance == Private->AtaPassThruPpiInstances[Index]) {
      return TRUE;
    }
  }

  return FALSE;
}

/**
  Send unlock hdd password cmd through ATA PassThru PPI.

  @param[in] AtaPassThru           The pointer to the ATA PassThru PPI.
  @param[in] Port                  The port number of the ATA device.
  @param[in] PortMultiplierPort    The port multiplier port number of the ATA device.
  @param[in] Identifier            The identifier to set user or master password.
  @param[in] Password              The hdd password of attached ATA device.

  @retval EFI_SUCCESS              Successful to send unlock hdd password cmd.
  @retval EFI_INVALID_PARAMETER    The parameter passed-in is invalid.
  @retval EFI_OUT_OF_RESOURCES     Not enough memory to send unlock hdd password cmd.
  @retval EFI_DEVICE_ERROR         Can not send unlock hdd password cmd.

**/
EFI_STATUS
UnlockDevice (
  IN EDKII_PEI_ATA_PASS_THRU_PPI    *AtaPassThru,
  IN UINT16                         Port,
  IN UINT16                         PortMultiplierPort,
  IN CHAR8                          Identifier,
  IN CHAR8                          *Password
  )
{
  EFI_STATUS                          Status;
  EFI_ATA_COMMAND_BLOCK               Acb;
  EFI_ATA_STATUS_BLOCK                *Asb;
  EFI_ATA_PASS_THRU_COMMAND_PACKET    Packet;
  UINT8                               Buffer[HDD_PAYLOAD];

  if ((AtaPassThru == NULL) || (Password == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The 'Asb' field (a pointer to the EFI_ATA_STATUS_BLOCK structure) in
  // EFI_ATA_PASS_THRU_COMMAND_PACKET is required to be aligned specified by
  // the 'IoAlign' field in the EFI_ATA_PASS_THRU_MODE structure. Meanwhile,
  // the structure EFI_ATA_STATUS_BLOCK is composed of only UINT8 fields, so it
  // may not be aligned when allocated on stack for some compilers. Hence, we
  // use the API AllocateAlignedPages to ensure this structure is properly
  // aligned.
  //
  Asb = AllocateAlignedPages (
          EFI_SIZE_TO_PAGES (sizeof (EFI_ATA_STATUS_BLOCK)),
          AtaPassThru->Mode->IoAlign
          );
  if (Asb == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Prepare for ATA command block.
  //
  ZeroMem (&Acb, sizeof (Acb));
  ZeroMem (Asb, sizeof (EFI_ATA_STATUS_BLOCK));
  Acb.AtaCommand    = ATA_SECURITY_UNLOCK_CMD;
  Acb.AtaDeviceHead = (UINT8) (PortMultiplierPort == 0xFFFF ? 0 : (PortMultiplierPort << 4));

  //
  // Prepare for ATA pass through packet.
  //
  ZeroMem (&Packet, sizeof (Packet));
  Packet.Protocol = EFI_ATA_PASS_THRU_PROTOCOL_PIO_DATA_OUT;
  Packet.Length   = EFI_ATA_PASS_THRU_LENGTH_BYTES;
  Packet.Asb      = Asb;
  Packet.Acb      = &Acb;

  ((CHAR16 *) Buffer)[0] = Identifier & BIT0;
  CopyMem (&((CHAR16 *) Buffer)[1], Password, HDD_PASSWORD_MAX_LENGTH);

  Packet.OutDataBuffer     = Buffer;
  Packet.OutTransferLength = sizeof (Buffer);
  Packet.Timeout           = ATA_TIMEOUT;

  Status = AtaPassThru->PassThru (
                          AtaPassThru,
                          Port,
                          PortMultiplierPort,
                          &Packet
                          );
  if (!EFI_ERROR (Status) &&
      ((Asb->AtaStatus & ATA_STSREG_ERR) != 0) &&
      ((Asb->AtaError & ATA_ERRREG_ABRT) != 0)) {
    Status = EFI_DEVICE_ERROR;
  }

  FreeAlignedPages (Asb, EFI_SIZE_TO_PAGES (sizeof (EFI_ATA_STATUS_BLOCK)));

  ZeroMem (Buffer, sizeof (Buffer));

  DEBUG ((DEBUG_INFO, "%a() - %r\n", __FUNCTION__, Status));
  return Status;
}

/**
  Send security freeze lock cmd through ATA PassThru PPI.

  @param[in] AtaPassThru           The pointer to the ATA PassThru PPI.
  @param[in] Port                  The port number of the ATA device.
  @param[in] PortMultiplierPort    The port multiplier port number of the ATA device.

  @retval EFI_SUCCESS              Successful to send security freeze lock cmd.
  @retval EFI_INVALID_PARAMETER    The parameter passed-in is invalid.
  @retval EFI_OUT_OF_RESOURCES     Not enough memory to send unlock hdd password cmd.
  @retval EFI_DEVICE_ERROR         Can not send security freeze lock cmd.

**/
EFI_STATUS
FreezeLockDevice (
  IN EDKII_PEI_ATA_PASS_THRU_PPI    *AtaPassThru,
  IN UINT16                         Port,
  IN UINT16                         PortMultiplierPort
  )
{
  EFI_STATUS                          Status;
  EFI_ATA_COMMAND_BLOCK               Acb;
  EFI_ATA_STATUS_BLOCK                *Asb;
  EFI_ATA_PASS_THRU_COMMAND_PACKET    Packet;

  if (AtaPassThru == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // The 'Asb' field (a pointer to the EFI_ATA_STATUS_BLOCK structure) in
  // EFI_ATA_PASS_THRU_COMMAND_PACKET is required to be aligned specified by
  // the 'IoAlign' field in the EFI_ATA_PASS_THRU_MODE structure. Meanwhile,
  // the structure EFI_ATA_STATUS_BLOCK is composed of only UINT8 fields, so it
  // may not be aligned when allocated on stack for some compilers. Hence, we
  // use the API AllocateAlignedPages to ensure this structure is properly
  // aligned.
  //
  Asb = AllocateAlignedPages (
          EFI_SIZE_TO_PAGES (sizeof (EFI_ATA_STATUS_BLOCK)),
          AtaPassThru->Mode->IoAlign
          );
  if (Asb == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  //
  // Prepare for ATA command block.
  //
  ZeroMem (&Acb, sizeof (Acb));
  ZeroMem (Asb, sizeof (EFI_ATA_STATUS_BLOCK));
  Acb.AtaCommand    = ATA_SECURITY_FREEZE_LOCK_CMD;
  Acb.AtaDeviceHead = (UINT8) (PortMultiplierPort == 0xFFFF ? 0 : (PortMultiplierPort << 4));

  //
  // Prepare for ATA pass through packet.
  //
  ZeroMem (&Packet, sizeof (Packet));
  Packet.Protocol = EFI_ATA_PASS_THRU_PROTOCOL_ATA_NON_DATA;
  Packet.Length   = EFI_ATA_PASS_THRU_LENGTH_NO_DATA_TRANSFER;
  Packet.Asb      = Asb;
  Packet.Acb      = &Acb;
  Packet.Timeout  = ATA_TIMEOUT;

  Status = AtaPassThru->PassThru (
                          AtaPassThru,
                          Port,
                          PortMultiplierPort,
                          &Packet
                          );
  if (!EFI_ERROR (Status) &&
      ((Asb->AtaStatus & ATA_STSREG_ERR) != 0) &&
      ((Asb->AtaError & ATA_ERRREG_ABRT) != 0)) {
    Status = EFI_DEVICE_ERROR;
  }

  FreeAlignedPages (Asb, EFI_SIZE_TO_PAGES (sizeof (EFI_ATA_STATUS_BLOCK)));

  DEBUG ((DEBUG_INFO, "%a() - %r\n", __FUNCTION__, Status));
  return Status;
}

/**
  Unlock HDD password for S3.

  @param[in] Private    Pointer to the HddPassword PEI driver private data.

**/
VOID
UnlockHddPassword (
  IN HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA    *Private
  )
{
  EFI_STATUS                     Status;
  VOID                           *Buffer;
  UINTN                          Length;
  UINT8                          DummyData;
  HDD_PASSWORD_DEVICE_INFO       *DevInfo;
  UINTN                          AtaPassThruInstance;
  EDKII_PEI_ATA_PASS_THRU_PPI    *AtaPassThruPpi;
  UINT16                         Port;
  UINT16                         PortMultiplierPort;
  EFI_DEVICE_PATH_PROTOCOL       *DevicePath;
  UINTN                          DevicePathLength;

  //
  // Get HDD password device info from LockBox.
  //
  Buffer = (VOID *) &DummyData;
  Length = 1;
  Status = RestoreLockBox (&mHddPasswordDeviceInfoGuid, Buffer, &Length);
  if (Status == EFI_BUFFER_TOO_SMALL) {
    Buffer = AllocatePages (EFI_SIZE_TO_PAGES (Length));
    if (Buffer != NULL) {
      Status = RestoreLockBox (&mHddPasswordDeviceInfoGuid, Buffer, &Length);
    }
  }
  if ((Buffer == NULL) || (Buffer == (VOID *) &DummyData)) {
    return;
  } else if (EFI_ERROR (Status)) {
    FreePages (Buffer, EFI_SIZE_TO_PAGES (Length));
    return;
  }

  //
  // Go through the ATA PassThru PPI instances within system.
  //
  for (AtaPassThruInstance = 0;
       AtaPassThruInstance < MAX_ATA_PASSTHRU_PPI;
       AtaPassThruInstance++) {
    Status = PeiServicesLocatePpi (
               &gEdkiiPeiAtaPassThruPpiGuid,
               AtaPassThruInstance,
               NULL,
               (VOID **) &AtaPassThruPpi
               );
    if (EFI_ERROR (Status)) {
      //
      // No more EDKII_PEI_ATA_PASS_THRU_PPI instances.
      //
      break;
    }

    //
    // Check whether this PPI instance has been handled previously.
    //
    if (IsPassThruInstanceHandled (Private, AtaPassThruPpi)) {
      DEBUG ((
        DEBUG_INFO, "%a: ATA PassThru PPI instance (0x%p) already handled.\n",
        __FUNCTION__, (UINTN)AtaPassThruPpi
        ));
      continue;
    } else {
      DEBUG ((
        DEBUG_INFO, "%a: New ATA PassThru PPI instance (0x%p) found.\n",
        __FUNCTION__, (UINTN)AtaPassThruPpi
        ));
      Private->AtaPassThruPpiInstances[Private->AtaPassThruPpiInstanceNum] = (UINTN)AtaPassThruPpi;
      Private->AtaPassThruPpiInstanceNum++;
    }

    Status = AtaPassThruPpi->GetDevicePath (AtaPassThruPpi, &DevicePathLength, &DevicePath);
    if (EFI_ERROR (Status) || (DevicePathLength <= sizeof (EFI_DEVICE_PATH_PROTOCOL))) {
      continue;
    }

    //
    // Go through all the devices managed by this PPI instance.
    //
    Port = 0xFFFF;
    while (TRUE) {
      Status = AtaPassThruPpi->GetNextPort (AtaPassThruPpi, &Port);
      if (EFI_ERROR (Status)) {
        //
        // We cannot find more legal port then we are done.
        //
        break;
      }

      PortMultiplierPort = 0xFFFF;
      while (TRUE) {
        Status = AtaPassThruPpi->GetNextDevice (AtaPassThruPpi, Port, &PortMultiplierPort);
        if (EFI_ERROR (Status)) {
          //
          // We cannot find more legal port multiplier port number for ATA device
          // on the port, then we are done.
          //
          break;
        }

        //
        // Search the device in the restored LockBox.
        //
        DevInfo = (HDD_PASSWORD_DEVICE_INFO *) Buffer;
        while ((UINTN) DevInfo < ((UINTN) Buffer + Length)) {
          //
          // Find the matching device.
          //
          if ((DevInfo->Device.Port == Port) &&
              (DevInfo->Device.PortMultiplierPort == PortMultiplierPort) &&
              (DevInfo->DevicePathLength >= DevicePathLength) &&
              (CompareMem (
                DevInfo->DevicePath,
                DevicePath,
                DevicePathLength - sizeof (EFI_DEVICE_PATH_PROTOCOL)) == 0)) {
            //
            // If device locked, unlock first.
            //
            if (!IsZeroBuffer (DevInfo->Password, HDD_PASSWORD_MAX_LENGTH)) {
              UnlockDevice (AtaPassThruPpi, Port, PortMultiplierPort, 0, DevInfo->Password);
            }
            //
            // Freeze lock the device.
            //
            FreezeLockDevice (AtaPassThruPpi, Port, PortMultiplierPort);
            break;
          }

          DevInfo = (HDD_PASSWORD_DEVICE_INFO *)
                    ((UINTN) DevInfo + sizeof (HDD_PASSWORD_DEVICE_INFO) + DevInfo->DevicePathLength);
        }
      }
    }
  }

  ZeroMem (Buffer, Length);
  FreePages (Buffer, EFI_SIZE_TO_PAGES (Length));

}

/**
  Entry point of the notification callback function itself within the PEIM.
  It is to unlock HDD password for S3.

  @param  PeiServices      Indirect reference to the PEI Services Table.
  @param  NotifyDescriptor Address of the notification descriptor data structure.
  @param  Ppi              Address of the PPI that was installed.

  @return Status of the notification.
          The status code returned from this function is ignored.
**/
EFI_STATUS
EFIAPI
HddPasswordAtaPassThruNotify (
  IN EFI_PEI_SERVICES          **PeiServices,
  IN EFI_PEI_NOTIFY_DESCRIPTOR *NotifyDesc,
  IN VOID                      *Ppi
  )
{
  HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA    *Private;

  DEBUG ((DEBUG_INFO, "%a() - enter at S3 resume\n", __FUNCTION__));

  Private = HDD_PASSWORD_PEI_PRIVATE_DATA_FROM_THIS_NOTIFY (NotifyDesc);
  UnlockHddPassword (Private);

  DEBUG ((DEBUG_INFO, "%a() - exit at S3 resume\n", __FUNCTION__));

  return EFI_SUCCESS;
}

HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA mHddPasswordPeiDriverPrivateTemplate = {
  HDD_PASSWORD_PEI_DRIVER_SIGNATURE,    // Signature
  {                                     // AtaPassThruPpiNotifyList
    (EFI_PEI_PPI_DESCRIPTOR_NOTIFY_CALLBACK | EFI_PEI_PPI_DESCRIPTOR_TERMINATE_LIST),
    &gEdkiiPeiAtaPassThruPpiGuid,
    HddPasswordAtaPassThruNotify
  },
  0,                                    // AtaPassThruPpiInstanceNum
  {0}                                   // AtaPassThruPpiInstances
};

/**
  Main entry for this module.

  @param FileHandle             Handle of the file being invoked.
  @param PeiServices            Pointer to PEI Services table.

  @return Status from PeiServicesNotifyPpi.

**/
EFI_STATUS
EFIAPI
HddPasswordPeiInit (
  IN EFI_PEI_FILE_HANDLE        FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS                              Status;
  EFI_BOOT_MODE                           BootMode;
  HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA    *Private;

  Status = PeiServicesGetBootMode (&BootMode);
  if ((EFI_ERROR (Status)) || (BootMode != BOOT_ON_S3_RESUME)) {
    return EFI_UNSUPPORTED;
  }

  DEBUG ((DEBUG_INFO, "%a: Enters in S3 path.\n", __FUNCTION__));

  Private = (HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA *)
              AllocateCopyPool (
                sizeof (HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA),
                &mHddPasswordPeiDriverPrivateTemplate
                );
  if (Private == NULL) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Failed to allocate memory for HDD_PASSWORD_PEI_DRIVER_PRIVATE_DATA.\n",
      __FUNCTION__
      ));
    return EFI_OUT_OF_RESOURCES;
  }

  Status = PeiServicesNotifyPpi (&Private->AtaPassThruPpiNotifyList);
  ASSERT_EFI_ERROR (Status);
  return Status;
}

