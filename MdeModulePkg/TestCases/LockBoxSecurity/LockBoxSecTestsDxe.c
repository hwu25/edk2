#include <PiDxe.h>

#include <Guid/EventGroup.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/LockBoxLib.h>

#include "LockBoxSecTestsCommon.h"

EFI_GUID mTestLockBox1Guid = TEST_LOCKBOX1_GUID;
EFI_GUID mTestLockBox2Guid = TEST_LOCKBOX2_GUID;

UINT8    mDataBuffer[LOCKBOX_LENGTH];

/**
  Notification function of EFI_END_OF_DXE_EVENT_GROUP_GUID event group.

  This is a notification function registered on EFI_END_OF_DXE_EVENT_GROUP_GUID event group.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
LockBoxTestsEndOfDxeNotify (
  EFI_EVENT    Event,
  VOID         *Context
  )
{
  EFI_STATUS    Status;

  DEBUG ((DEBUG_INFO, "%a() - enter\n", __FUNCTION__));

  // Create the origin LockBox with 'RESTORE_IN_S3_ONLY' attr.
  Status = SaveLockBox (
             &mTestLockBox1Guid,
             mDataBuffer,
             LOCKBOX_LENGTH
             );
  ASSERT_EFI_ERROR (Status);
  Status = SetLockBoxAttributes (
             &mTestLockBox1Guid,
             LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY
             );
  ASSERT_EFI_ERROR (Status);

  // Update the above LockBox, should pass.
  Status = UpdateLockBox (
             &mTestLockBox1Guid,
             0,
             mDataBuffer,
             LOCKBOX_LENGTH
             );
  ASSERT_EFI_ERROR (Status);

  // Create the origin LockBox with both 'RESTORE_IN_S3_ONLY' and
  // 'RESTORE_IN_PLACE' attr. Should fail.
  Status = SaveLockBox (
             &mTestLockBox2Guid,
             mDataBuffer,
             LOCKBOX_LENGTH
             );
  ASSERT_EFI_ERROR (Status);
  Status = SetLockBoxAttributes (
             &mTestLockBox2Guid,
             (LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE |
              LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY)
             );
  ASSERT (Status == EFI_INVALID_PARAMETER);

  // Create the origin LockBox with 'RESTORE_IN_PLACE' attr.
  Status = SetLockBoxAttributes (
             &mTestLockBox2Guid,
             LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE
             );
  ASSERT_EFI_ERROR (Status);

  // Update the above LockBox, should pass.
  Status = UpdateLockBox (
             &mTestLockBox2Guid,
             0,
             mDataBuffer,
             LOCKBOX_LENGTH
             );
  ASSERT_EFI_ERROR (Status);

  DEBUG ((DEBUG_INFO, "%a() - exit\n", __FUNCTION__));

  gBS->CloseEvent (Event);
}

/**
  Notification function of EFI_EVENT_GROUP_READY_TO_BOOT event group.

  This is a notification function registered on EFI_EVENT_GROUP_READY_TO_BOOT event group.

  @param  Event        Event whose notification function is being invoked.
  @param  Context      Pointer to the notification function's context.

**/
VOID
EFIAPI
LockBoxTestsReadyToBootNotify (
  EFI_EVENT    Event,
  VOID         *Context
  )
{
  EFI_STATUS    Status;
  UINTN         Length;

  DEBUG ((DEBUG_INFO, "%a() - enter\n", __FUNCTION__));

  // The save/update/setattr of the LockBox with attribute 'RESTORE_IN_S3_ONLY'
  // is NOT allowed after SmmReadyToLock.
  // Here, ReadyToBoot is after SmmReadyToLock.
  Status = SaveLockBox (
             &mTestLockBox1Guid,
             mDataBuffer,
             LOCKBOX_LENGTH / 2
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);
  Status = SetLockBoxAttributes (
             &mTestLockBox1Guid,
             LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);
  Status = UpdateLockBox (
             &mTestLockBox1Guid,
             0,
             mDataBuffer,
             LOCKBOX_LENGTH / 2
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);

  // The restore of the LockBox with attribute 'RESTORE_IN_S3_ONLY' is NOT
  // allowed after SmmReadyToLock.
  // Here, ReadyToBoot is after SmmReadyToLock.
  Length = LOCKBOX_LENGTH;
  Status = RestoreLockBox (
             &mTestLockBox1Guid,
             mDataBuffer,
             &Length
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);

  // The save/update/setattr of the LockBox with attribute 'RESTORE_IN_PLACE'
  // is NOT allowed after SmmReadyToLock.
  // Here, ReadyToBoot is after SmmReadyToLock.
  Status = SaveLockBox (
             &mTestLockBox2Guid,
             mDataBuffer,
             LOCKBOX_LENGTH / 2
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);
  Status = SetLockBoxAttributes (
             &mTestLockBox2Guid,
             LOCK_BOX_ATTRIBUTE_RESTORE_IN_PLACE
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);
  Status = UpdateLockBox (
             &mTestLockBox2Guid,
             0,
             mDataBuffer,
             LOCKBOX_LENGTH / 2
             );
  ASSERT (Status == RETURN_ACCESS_DENIED);

  // The restore of the LockBox with attribute 'RESTORE_IN_PLACE' is
  // allowed after SmmReadyToLock.

  DEBUG ((DEBUG_INFO, "%a() - exit\n", __FUNCTION__));

  gBS->CloseEvent (Event);
}

/**
  Main entry for this driver.

  @param ImageHandle     Image Handle this driver.
  @param SystemTable     Pointer to SystemTable.

  @retval EFI_SUCESS     This function always complete successfully.
**/
EFI_STATUS
EFIAPI
LockBoxSecTestsDxeEntry (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE*  SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_EVENT     EndOfDxeEvent;
  EFI_EVENT     ReadyToBootEvent;

  //
  // Register a EndOfDxe event callback.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  LockBoxTestsEndOfDxeNotify,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );
  ASSERT_EFI_ERROR (Status);

  //
  // Register a ReadyToBoot event callback.
  //
  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  LockBoxTestsReadyToBootNotify,
                  NULL,
                  &gEfiEventReadyToBootGuid,
                  &ReadyToBootEvent
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}