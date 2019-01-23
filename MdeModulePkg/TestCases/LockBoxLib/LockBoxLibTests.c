#include <PiDxe.h>

#include <Guid/EventGroup.h>

#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/LockBoxLib.h>

#define  TEST_LOCKBOX_GUID  { 0x591a08c6, 0x4f89, 0x402d, { 0xb6, 0xf4, 0xd1, 0x71, 0xd8, 0x9b, 0x1e, 0x57 } }
EFI_GUID mTestLockBoxGuid = TEST_LOCKBOX_GUID;

typedef struct {
  UINTN    Offset;
  UINTN    Length;
} UPDATE_LOCKBOX_PARAM;

#define  ORI_LOCKBOX_LENGTH  64

UPDATE_LOCKBOX_PARAM  mNotEnlargeInputs[] = {
  {0, 1},
  {32, 4},
  {0, 64}
};

UPDATE_LOCKBOX_PARAM  mEnlargeInputs[] = {
  {0, 65},                                    // #0 Not cross page
  {32, 34},                                   // #1 Not cross page
  {0, SIZE_1KB},                              // #2 Not cross page
  {47, SIZE_1KB},                             // #3 Not cross page
  {SIZE_2KB, SIZE_1KB},                       // #4 Not cross page
  {0, EFI_PAGE_SIZE + 1},                     // #5 Cross page
  {SIZE_2KB, EFI_PAGE_SIZE},                  // #6 Not cross page
  {2 * EFI_PAGE_SIZE, 1},                     // #7 Cross page
  {2 * EFI_PAGE_SIZE + 1, EFI_PAGE_SIZE - 1}  // #8 Not cross page
};

UPDATE_LOCKBOX_PARAM  mOutOfResourceInputs[] = {
  {0, SIZE_16MB},
  {SIZE_1KB, SIZE_16MB}
};

UPDATE_LOCKBOX_PARAM  mNotEnlarge2Inputs[] = {
  {0, 1},
  {2 * EFI_PAGE_SIZE + 1, EFI_PAGE_SIZE - 1}
};


/**
  Internal helper function for data display.

**/
VOID
InternalDumpData (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN  Index;
  for (Index = 0; Index < Size; Index++) {
    DEBUG ((DEBUG_ERROR, "%02x ", (UINTN)Data[Index]));
  }
}

/**
  Internal helper function for data display.

**/
VOID
InternalDumpHex (
  IN UINT8  *Data,
  IN UINTN  Size
  )
{
  UINTN   Index;
  UINTN   Count;
  UINTN   Left;

#define COLUME_SIZE  (16)

  Count = Size / COLUME_SIZE;
  Left  = Size % COLUME_SIZE;
  for (Index = 0; Index < Count; Index++) {
    DEBUG ((DEBUG_ERROR, "%04x: ", Index * COLUME_SIZE));
    InternalDumpData (Data + Index * COLUME_SIZE, COLUME_SIZE);
    DEBUG ((DEBUG_ERROR, "\n"));
  }

  if (Left != 0) {
    DEBUG ((DEBUG_ERROR, "%04x: ", Index * COLUME_SIZE));
    InternalDumpData (Data + Index * COLUME_SIZE, Left);
    DEBUG ((DEBUG_ERROR, "\n"));
  }
}

/**
  Test cases for the UpdateLockBox() API.

**/
VOID
UpdateLockBoxTests (
  )
{
  EFI_STATUS    Status;
  UINT8         *LockBoxData;
  UINTN         LockBoxDataLen;
  UINT8         *UpdateData;
  UINTN         UpdateDataLen;
  UINT8         *RestoreData;
  UINTN         RestoreDataLength;
  UINTN         Index;

  LockBoxDataLen = ORI_LOCKBOX_LENGTH;
  LockBoxData    = AllocateZeroPool (LockBoxDataLen);
  ASSERT (LockBoxData != NULL);

  UpdateDataLen  = SIZE_16MB;
  UpdateData     = AllocatePool (UpdateDataLen);
  ASSERT (UpdateData != NULL);
  SetMem (UpdateData, UpdateDataLen, 0xA5);

  RestoreData    = AllocatePool (UpdateDataLen);
  ASSERT (RestoreData != NULL);

  // Create the origin LockBox without setting the attribute first
  Status = SaveLockBox (
             &mTestLockBoxGuid,
             LockBoxData,
             LockBoxDataLen
             );
  ASSERT_EFI_ERROR (Status);

  //
  // *****[Test 1]*****
  // Any UpdateLockBox() calls to enlarge the LockBox here will fail, since
  // LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY attribute is not set.
  //
  for (Index = 0; Index < ARRAY_SIZE (mEnlargeInputs); Index++) {
    Status = UpdateLockBox (
               &mTestLockBoxGuid,
               mEnlargeInputs[Index].Offset,
               UpdateData,
               mEnlargeInputs[Index].Length
               );
    if (Status != EFI_BUFFER_TOO_SMALL) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Test 1-(%d) failed!\n", __FUNCTION__, __LINE__, Index));
      ASSERT (FALSE);
    } else {
      DEBUG ((DEBUG_INFO, "[%a:%d] Test 1-(%d) pass!\n", __FUNCTION__, __LINE__, Index));
    }
  }

  //
  // *****[Test 2]*****
  // Any UpdateLockBox() calls that WONT enlarge the LockBox here will succeed.
  //
  for (Index = 0; Index < ARRAY_SIZE (mNotEnlargeInputs); Index++) {
    Status = UpdateLockBox (
               &mTestLockBoxGuid,
               mNotEnlargeInputs[Index].Offset,
               UpdateData,
               mNotEnlargeInputs[Index].Length
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Test 2-(%d) failed!\n", __FUNCTION__, __LINE__, Index));
      ASSERT (FALSE);
    } else {
      DEBUG ((DEBUG_INFO, "[%a:%d] Test 2-(%d) pass!\n", __FUNCTION__, __LINE__, Index));
    }

    // Dump the LockBox content
    RestoreDataLength = ORI_LOCKBOX_LENGTH;
    Status = RestoreLockBox (
               &mTestLockBoxGuid,
               RestoreData,
               &RestoreDataLength
               );
    ASSERT_EFI_ERROR (Status);
    ASSERT (RestoreDataLength == ORI_LOCKBOX_LENGTH);
    InternalDumpHex (RestoreData, RestoreDataLength);
  }

  Status = SetLockBoxAttributes (
             &mTestLockBoxGuid,
             LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY
             );
  ASSERT_EFI_ERROR (Status);

  //
  // *****[Test 3]*****
  // UpdateLockBox() calls to enlarge the LockBox here will succeed, since
  // LOCK_BOX_ATTRIBUTE_RESTORE_IN_S3_ONLY attribute is set.
  //
  for (Index = 0; Index < ARRAY_SIZE (mEnlargeInputs); Index++) {
    Status = UpdateLockBox (
               &mTestLockBoxGuid,
               mEnlargeInputs[Index].Offset,
               UpdateData,
               mEnlargeInputs[Index].Length
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Test 3-(%d) failed!\n", __FUNCTION__, __LINE__, Index));
      ASSERT (FALSE);
    } else {
      DEBUG ((DEBUG_INFO, "[%a:%d] Test 3-(%d) pass!\n", __FUNCTION__, __LINE__, Index));
    }

    // Dump the LockBox content
    RestoreDataLength = mEnlargeInputs[Index].Offset + mEnlargeInputs[Index].Length;
    Status = RestoreLockBox (
               &mTestLockBoxGuid,
               RestoreData,
               &RestoreDataLength
               );
    ASSERT_EFI_ERROR (Status);
    ASSERT (RestoreDataLength == mEnlargeInputs[Index].Offset + mEnlargeInputs[Index].Length);
    InternalDumpHex (RestoreData, RestoreDataLength);
  }

  //
  // *****[Test 4]*****
  // UpdateLockBox() calls to enlarge the LockBox by a size larger than the SMRAM
  // size. EFI_OUT_OF_RESOURCES is expected here.
  //
  for (Index = 0; Index < ARRAY_SIZE (mOutOfResourceInputs); Index++) {
    Status = UpdateLockBox (
               &mTestLockBoxGuid,
               mOutOfResourceInputs[Index].Offset,
               UpdateData,
               mOutOfResourceInputs[Index].Length
               );
    if (Status != EFI_OUT_OF_RESOURCES) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Test 4-(%d) failed!\n", __FUNCTION__, __LINE__, Index));
      ASSERT (FALSE);
    } else {
      DEBUG ((DEBUG_INFO, "[%a:%d] Test 4-(%d) pass!\n", __FUNCTION__, __LINE__, Index));
    }
  }

  //
  // *****[Test 5]*****
  // Any UpdateLockBox() calls that WONT enlarge the LockBox again, here will succeed.
  //
  SetMem (UpdateData, UpdateDataLen, 0xFF);
  for (Index = 0; Index < ARRAY_SIZE (mNotEnlarge2Inputs); Index++) {
    Status = UpdateLockBox (
               &mTestLockBoxGuid,
               mNotEnlarge2Inputs[Index].Offset,
               UpdateData,
               mNotEnlarge2Inputs[Index].Length
               );
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "[%a:%d] Test 5-(%d) failed!\n", __FUNCTION__, __LINE__, Index));
      ASSERT (FALSE);
    } else {
      DEBUG ((DEBUG_INFO, "[%a:%d] Test 5-(%d) pass!\n", __FUNCTION__, __LINE__, Index));
    }

    // Dump the LockBox content
    // By now, the test LockBox should be (3*EFI_PAGE_SIZE)Kb in size
    RestoreDataLength = 3 * EFI_PAGE_SIZE;
    Status = RestoreLockBox (
               &mTestLockBoxGuid,
               RestoreData,
               &RestoreDataLength
               );
    ASSERT_EFI_ERROR (Status);
    ASSERT (RestoreDataLength == 3 * EFI_PAGE_SIZE);
    InternalDumpHex (RestoreData, RestoreDataLength);
  }

}

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
  DEBUG ((DEBUG_INFO, "%a() - enter\n", __FUNCTION__));

  UpdateLockBoxTests ();

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
LockBoxLibTestsEntry (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE*  SystemTable
  )
{
  EFI_STATUS    Status;
  EFI_EVENT     EndOfDxeEvent;

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

  return Status;
}