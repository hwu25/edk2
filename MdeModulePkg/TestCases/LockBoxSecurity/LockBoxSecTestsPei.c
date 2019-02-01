#include <PiPei.h>

#include <Ppi/MasterBootMode.h>

#include <Library/DebugLib.h>
#include <Library/PeiServicesLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/LockBoxLib.h>

#include "LockBoxSecTestsCommon.h"

EFI_GUID mTestLockBox1Guid = TEST_LOCKBOX1_GUID;

/**
  Entry point of the PEIM.

  @param[in] FileHandle     Handle of the file being invoked.
  @param[in] PeiServices    Describes the list of possible PEI Services.

  @retval EFI_SUCCESS             Operation performed successfully.
  @retval EFI_OUT_OF_RESOURCES    Not enough memory to allocate.

**/
EFI_STATUS
EFIAPI
LockBoxSecTestsPeiEntry (
  IN EFI_PEI_FILE_HANDLE        FileHandle,
  IN CONST EFI_PEI_SERVICES     **PeiServices
  )
{
  EFI_STATUS       Status;
  EFI_BOOT_MODE    BootMode;
  UINT8            *DataBuffer;
  UINTN            Length;

  DEBUG ((DEBUG_INFO, "%a() - enter\n", __FUNCTION__));

  Status = PeiServicesGetBootMode (&BootMode);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Fail to get the current boot mode.\n", __FUNCTION__));
    return Status;
  }

  // Test only under S3 path
  if (BootMode != BOOT_ON_S3_RESUME) {
    return EFI_UNSUPPORTED;
  }

  // Restore the LockBox created during normal boot DXE phase, should pass.
  Length     = LOCKBOX_LENGTH;
  DataBuffer = AllocatePool (Length);
  Status = RestoreLockBox (
             &mTestLockBox1Guid,
             DataBuffer,
             &Length
             );
  ASSERT_EFI_ERROR (Status);
  ASSERT (Length == LOCKBOX_LENGTH);

  DEBUG ((DEBUG_INFO, "%a() - exit\n", __FUNCTION__));

  return Status;
}
