/** @file
  Implementation of loading microcode on processors.

  Copyright (c) 2015 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include "MpLib.h"

/**
  Get microcode update signature of currently loaded microcode update.

  @return  Microcode signature.
**/
UINT32
GetCurrentMicrocodeSignature (
  VOID
  )
{
  MSR_IA32_BIOS_SIGN_ID_REGISTER   BiosSignIdMsr;

  AsmWriteMsr64 (MSR_IA32_BIOS_SIGN_ID, 0);
  AsmCpuid (CPUID_VERSION_INFO, NULL, NULL, NULL, NULL);
  BiosSignIdMsr.Uint64 = AsmReadMsr64 (MSR_IA32_BIOS_SIGN_ID);
  return BiosSignIdMsr.Bits.MicrocodeUpdateSignature;
}

/**
  Detect whether specified processor can find matching microcode patch and load it.

  Microcode Payload as the following format:
  +----------------------------------------+------------------+
  |          CPU_MICROCODE_HEADER          |                  |
  +----------------------------------------+  CheckSum Part1  |
  |            Microcode Binary            |                  |
  +----------------------------------------+------------------+
  |  CPU_MICROCODE_EXTENDED_TABLE_HEADER   |                  |
  +----------------------------------------+  CheckSum Part2  |
  |      CPU_MICROCODE_EXTENDED_TABLE      |                  |
  |                   ...                  |                  |
  +----------------------------------------+------------------+

  There may by multiple CPU_MICROCODE_EXTENDED_TABLE in this format.
  The count of CPU_MICROCODE_EXTENDED_TABLE is indicated by ExtendedSignatureCount
  of CPU_MICROCODE_EXTENDED_TABLE_HEADER structure.

  When we are trying to verify the CheckSum32 with extended table.
  We should use the fields of exnteded table to replace the corresponding
  fields in CPU_MICROCODE_HEADER structure, and recalculate the
  CheckSum32 with CPU_MICROCODE_HEADER + Microcode Binary. We named
  it as CheckSum Part3.

  The CheckSum Part2 is used to verify the CPU_MICROCODE_EXTENDED_TABLE_HEADER
  and CPU_MICROCODE_EXTENDED_TABLE parts. We should make sure CheckSum Part2
  is correct before we are going to verify each CPU_MICROCODE_EXTENDED_TABLE.

  Only ProcessorSignature, ProcessorFlag and CheckSum are different between
  CheckSum Part1 and CheckSum Part3. To avoid multiple computing CheckSum Part3.
  Save an in-complete CheckSum32 from CheckSum Part1 for common parts.
  When we are going to calculate CheckSum32, just should use the corresponding part
  of the ProcessorSignature, ProcessorFlag and CheckSum with in-complete CheckSum32.

  Notes: CheckSum32 is not a strong verification.
         It does not guarantee that the data has not been modified.
         CPU has its own mechanism to verify Microcode Binary part.

  @param[in]  CpuMpData        The pointer to CPU MP Data structure.
  @param[in]  ProcessorNumber  The handle number of the processor. The range is
                               from 0 to the total number of logical processors
                               minus 1.
**/
VOID
MicrocodeDetect (
  IN CPU_MP_DATA             *CpuMpData,
  IN UINTN                   ProcessorNumber
  )
{
  UINT32                                  ExtendedTableLength;
  UINT32                                  ExtendedTableCount;
  CPU_MICROCODE_EXTENDED_TABLE            *ExtendedTable;
  CPU_MICROCODE_EXTENDED_TABLE_HEADER     *ExtendedTableHeader;
  CPU_MICROCODE_HEADER                    *MicrocodeEntryPoint;
  UINTN                                   MicrocodeEnd;
  UINTN                                   Index;
  UINT8                                   PlatformId;
  CPUID_VERSION_INFO_EAX                  Eax;
  UINT32                                  CurrentRevision;
  UINT32                                  LatestRevision;
  UINTN                                   TotalSize;
  UINT32                                  CheckSum32;
  UINT32                                  InCompleteCheckSum32;
  BOOLEAN                                 CorrectMicrocode;
  VOID                                    *MicrocodeData;
  MSR_IA32_PLATFORM_ID_REGISTER           PlatformIdMsr;
  UINT32                                  ProcessorFlags;
  UINT32                                  ThreadId;
  BOOLEAN                                 IsBspCallIn;

  //
  // set ProcessorFlags to suppress incorrect compiler/analyzer warnings
  //
  ProcessorFlags = 0;

  if (CpuMpData->MicrocodePatchRegionSize == 0) {
    //
    // There is no microcode patches
    //
    return;
  }

  CurrentRevision = GetCurrentMicrocodeSignature ();
  IsBspCallIn     = (ProcessorNumber == (UINTN)CpuMpData->BspNumber) ? TRUE : FALSE;
  if (CurrentRevision != 0 && !IsBspCallIn) {
    //
    // Skip loading microcode if it has been loaded successfully
    //
    return;
  }

  GetProcessorLocationByApicId (GetInitialApicId (), NULL, NULL, &ThreadId);
  if (ThreadId != 0) {
    //
    // Skip loading microcode if it is not the first thread in one core.
    //
    return;
  }

  ExtendedTableLength = 0;
  //
  // Here data of CPUID leafs have not been collected into context buffer, so
  // GetProcessorCpuid() cannot be used here to retrieve CPUID data.
  //
  AsmCpuid (CPUID_VERSION_INFO, &Eax.Uint32, NULL, NULL, NULL);

  //
  // The index of platform information resides in bits 50:52 of MSR IA32_PLATFORM_ID
  //
  PlatformIdMsr.Uint64 = AsmReadMsr64 (MSR_IA32_PLATFORM_ID);
  PlatformId = (UINT8) PlatformIdMsr.Bits.PlatformId;

  //
  // Check whether AP has same processor with BSP.
  // If yes, direct use microcode info saved by BSP.
  //
  if (!IsBspCallIn) {
    if ((CpuMpData->ProcessorSignature == Eax.Uint32) &&
        (CpuMpData->ProcessorFlags & (1 << PlatformId)) != 0) {
        MicrocodeData = (VOID *)(UINTN) CpuMpData->MicrocodeDataAddress;
        LatestRevision = CpuMpData->MicrocodeRevision;
        goto Done;
    }
  }

  LatestRevision = 0;
  MicrocodeData  = NULL;
  MicrocodeEnd = (UINTN) (CpuMpData->MicrocodePatchAddress + CpuMpData->MicrocodePatchRegionSize);
  MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (UINTN) CpuMpData->MicrocodePatchAddress;

  do {
    //
    // Check if the microcode is for the Cpu and the version is newer
    // and the update can be processed on the platform
    //
    CorrectMicrocode = FALSE;

    if (MicrocodeEntryPoint->DataSize == 0) {
      TotalSize = sizeof (CPU_MICROCODE_HEADER) + 2000;
    } else {
      TotalSize = sizeof (CPU_MICROCODE_HEADER) + MicrocodeEntryPoint->DataSize;
    }

    ///
    /// 0x0       MicrocodeBegin  MicrocodeEntry  MicrocodeEnd      0xffffffff
    /// |--------------|---------------|---------------|---------------|
    ///                                 valid TotalSize
    /// TotalSize is only valid between 0 and (MicrocodeEnd - MicrocodeEntry).
    /// And it should be aligned with 4 bytes.
    /// If the TotalSize is invalid, skip 1KB to check next entry.
    ///
    if ( (UINTN)MicrocodeEntryPoint > (MAX_ADDRESS - TotalSize) ||
         ((UINTN)MicrocodeEntryPoint + TotalSize) > MicrocodeEnd ||
         (TotalSize & 0x3) != 0
       ) {
      MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + SIZE_1KB);
      continue;
    }

    //
    // Save an in-complete CheckSum32 from CheckSum Part1 for common parts.
    //
    InCompleteCheckSum32 = CalculateSum32 (
                             (UINT32 *) MicrocodeEntryPoint,
                             TotalSize
                             );
    InCompleteCheckSum32 -= MicrocodeEntryPoint->ProcessorSignature.Uint32;
    InCompleteCheckSum32 -= MicrocodeEntryPoint->ProcessorFlags;
    InCompleteCheckSum32 -= MicrocodeEntryPoint->Checksum;

    if (MicrocodeEntryPoint->HeaderVersion == 0x1) {
      //
      // It is the microcode header. It is not the padding data between microcode patches
      // because the padding data should not include 0x00000001 and it should be the repeated
      // byte format (like 0xXYXYXYXY....).
      //
      if (MicrocodeEntryPoint->ProcessorSignature.Uint32 == Eax.Uint32 &&
          MicrocodeEntryPoint->UpdateRevision > LatestRevision &&
          (MicrocodeEntryPoint->ProcessorFlags & (1 << PlatformId))
          ) {
        //
        // Calculate CheckSum Part1.
        //
        CheckSum32 = InCompleteCheckSum32;
        CheckSum32 += MicrocodeEntryPoint->ProcessorSignature.Uint32;
        CheckSum32 += MicrocodeEntryPoint->ProcessorFlags;
        CheckSum32 += MicrocodeEntryPoint->Checksum;
        if (CheckSum32 == 0) {
          CorrectMicrocode = TRUE;
          ProcessorFlags = MicrocodeEntryPoint->ProcessorFlags;
        }
      } else if ((MicrocodeEntryPoint->DataSize != 0) &&
                 (MicrocodeEntryPoint->UpdateRevision > LatestRevision)) {
        ExtendedTableLength = MicrocodeEntryPoint->TotalSize - (MicrocodeEntryPoint->DataSize +
                                sizeof (CPU_MICROCODE_HEADER));
        if (ExtendedTableLength != 0) {
          //
          // Extended Table exist, check if the CPU in support list
          //
          ExtendedTableHeader = (CPU_MICROCODE_EXTENDED_TABLE_HEADER *) ((UINT8 *) (MicrocodeEntryPoint)
                                  + MicrocodeEntryPoint->DataSize + sizeof (CPU_MICROCODE_HEADER));
          //
          // Calculate Extended Checksum
          //
          if ((ExtendedTableLength % 4) == 0) {
            //
            // Calculate CheckSum Part2.
            //
            CheckSum32 = CalculateSum32 ((UINT32 *) ExtendedTableHeader, ExtendedTableLength);
            if (CheckSum32 == 0) {
              //
              // Checksum correct
              //
              ExtendedTableCount = ExtendedTableHeader->ExtendedSignatureCount;
              ExtendedTable      = (CPU_MICROCODE_EXTENDED_TABLE *) (ExtendedTableHeader + 1);
              for (Index = 0; Index < ExtendedTableCount; Index ++) {
                //
                // Calculate CheckSum Part3.
                //
                CheckSum32 = InCompleteCheckSum32;
                CheckSum32 += ExtendedTable->ProcessorSignature.Uint32;
                CheckSum32 += ExtendedTable->ProcessorFlag;
                CheckSum32 += ExtendedTable->Checksum;
                if (CheckSum32 == 0) {
                  //
                  // Verify Header
                  //
                  if ((ExtendedTable->ProcessorSignature.Uint32 == Eax.Uint32) &&
                      (ExtendedTable->ProcessorFlag & (1 << PlatformId)) ) {
                    //
                    // Find one
                    //
                    CorrectMicrocode = TRUE;
                    ProcessorFlags = ExtendedTable->ProcessorFlag;
                    break;
                  }
                }
                ExtendedTable ++;
              }
            }
          }
        }
      }
    } else {
      //
      // It is the padding data between the microcode patches for microcode patches alignment.
      // Because the microcode patch is the multiple of 1-KByte, the padding data should not
      // exist if the microcode patch alignment value is not larger than 1-KByte. So, the microcode
      // alignment value should be larger than 1-KByte. We could skip SIZE_1KB padding data to
      // find the next possible microcode patch header.
      //
      MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + SIZE_1KB);
      continue;
    }
    //
    // Get the next patch.
    //
    if (MicrocodeEntryPoint->DataSize == 0) {
      TotalSize = 2048;
    } else {
      TotalSize = MicrocodeEntryPoint->TotalSize;
    }

    if (CorrectMicrocode) {
      LatestRevision = MicrocodeEntryPoint->UpdateRevision;
      MicrocodeData = (VOID *) ((UINTN) MicrocodeEntryPoint + sizeof (CPU_MICROCODE_HEADER));
    }

    MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + TotalSize);
  } while (((UINTN) MicrocodeEntryPoint < MicrocodeEnd));

Done:
  if (LatestRevision > CurrentRevision) {
    //
    // BIOS only authenticate updates that contain a numerically larger revision
    // than the currently loaded revision, where Current Signature < New Update
    // Revision. A processor with no loaded update is considered to have a
    // revision equal to zero.
    //
    ASSERT (MicrocodeData != NULL);
    AsmWriteMsr64 (
        MSR_IA32_BIOS_UPDT_TRIG,
        (UINT64) (UINTN) MicrocodeData
        );
    //
    // Get and check new microcode signature
    //
    CurrentRevision = GetCurrentMicrocodeSignature ();
    if (CurrentRevision != LatestRevision) {
      AcquireSpinLock(&CpuMpData->MpLock);
      DEBUG ((EFI_D_ERROR, "Updated microcode signature [0x%08x] does not match \
                loaded microcode signature [0x%08x]\n", CurrentRevision, LatestRevision));
      ReleaseSpinLock(&CpuMpData->MpLock);
    } else {
      //
      // Save the detected microcode patch address for each processor.
      // It will be used when building the microcode patch cache HOB.
      //
      CpuMpData->CpuData[ProcessorNumber].MicrocodeData = (UINTN) MicrocodeData;
    }
  }

  if (IsBspCallIn && (LatestRevision != 0)) {
    //
    // Save BSP processor info and microcode info for later AP use.
    //
    CpuMpData->ProcessorSignature   = Eax.Uint32;
    CpuMpData->ProcessorFlags       = ProcessorFlags;
    CpuMpData->MicrocodeDataAddress = (UINTN) MicrocodeData;
    CpuMpData->MicrocodeRevision    = LatestRevision;
    DEBUG ((DEBUG_INFO, "BSP Microcode:: signature [0x%08x], ProcessorFlags [0x%08x], \
       MicroData [0x%08x], Revision [0x%08x]\n", Eax.Uint32, ProcessorFlags, (UINTN) MicrocodeData, LatestRevision));
  }
}

/**
  Actual worker function that loads the required microcode patches into memory.

  @param[in, out]  CpuMpData          The pointer to CPU MP Data structure.
  @param[in]       PatchInfoBuffer    The pointer to an array of information on
                                      the microcode patches that will be loaded
                                      into memory.
  @param[in]       PatchNumber        The number of microcode patches that will
                                      be loaded into memory.
  @param[in]       TotalLoadSize      The total size of all the microcode
                                      patches to be loaded.
**/
VOID
LoadMicrocodePatchWorker (
  IN OUT CPU_MP_DATA             *CpuMpData,
  IN     MICROCODE_PATCH_INFO    *PatchInfoBuffer,
  IN     UINTN                   PatchNumber,
  IN     UINTN                   TotalLoadSize
  )
{
  UINTN    Index;
  VOID     *MicrocodePatchInRam;
  UINT8    *Walker;

  ASSERT ((PatchInfoBuffer != NULL) && (PatchNumber != 0));

  MicrocodePatchInRam = AllocatePages (EFI_SIZE_TO_PAGES (TotalLoadSize));
  if (MicrocodePatchInRam == NULL) {
    return;
  }

  //
  // Load all the required microcode patches into memory
  //
  for (Walker = MicrocodePatchInRam, Index = 0; Index < PatchNumber; Index++) {
    CopyMem (
      Walker,
      (VOID *) PatchInfoBuffer[Index].Address,
      PatchInfoBuffer[Index].Size
      );

    if (PatchInfoBuffer[Index].AlignedSize > PatchInfoBuffer[Index].Size) {
      //
      // Zero-fill the padding area
      //
      ZeroMem (
        Walker + PatchInfoBuffer[Index].Size,
        PatchInfoBuffer[Index].AlignedSize - PatchInfoBuffer[Index].Size
        );
    }

    Walker += PatchInfoBuffer[Index].AlignedSize;
  }

  //
  // Update the microcode patch related fields in CpuMpData
  //
  CpuMpData->MicrocodePatchAddress    = (UINTN) MicrocodePatchInRam;
  CpuMpData->MicrocodePatchRegionSize = TotalLoadSize;

  DEBUG ((
    DEBUG_INFO,
    "%a: Required microcode patches have been loaded at 0x%lx, with size 0x%lx.\n",
    __FUNCTION__, CpuMpData->MicrocodePatchAddress, CpuMpData->MicrocodePatchRegionSize
    ));

  return;
}

/**
  Load the required microcode patches data into memory.

  @param[in, out]  CpuMpData    The pointer to CPU MP Data structure.
**/
VOID
LoadMicrocodePatch (
  IN OUT CPU_MP_DATA             *CpuMpData
  )
{
  CPU_MICROCODE_HEADER    *MicrocodeEntryPoint;
  UINTN                   MicrocodeEnd;
  UINTN                   DataSize;
  UINTN                   TotalSize;
  MICROCODE_PATCH_INFO    *PatchInfoBuffer;
  UINTN                   MaxPatchNumber;
  UINTN                   PatchNumber;
  UINTN                   TotalLoadSize;
  UINT32                  ProcessorSignature;
  UINT32                  ProcessorFlags;
  UINTN                   Index;
  CPU_AP_DATA             *CpuData;
  BOOLEAN                 NeedLoad;

  //
  // Initialize the microcode patch related fields in CpuMpData as the values
  // specified by the PCD pair. If the microcode patches are loaded into memory,
  // these fields will be updated.
  //
  CpuMpData->MicrocodePatchAddress    = PcdGet64 (PcdCpuMicrocodePatchAddress);
  CpuMpData->MicrocodePatchRegionSize = PcdGet64 (PcdCpuMicrocodePatchRegionSize);

  MicrocodeEntryPoint    = (CPU_MICROCODE_HEADER *) (UINTN) CpuMpData->MicrocodePatchAddress;
  MicrocodeEnd           = (UINTN) MicrocodeEntryPoint +
                           (UINTN) CpuMpData->MicrocodePatchRegionSize;
  if ((MicrocodeEntryPoint == NULL) || ((UINTN) MicrocodeEntryPoint == MicrocodeEnd)) {
    //
    // There is no microcode patches
    //
    return;
  }

  PatchNumber     = 0;
  MaxPatchNumber  = DEFAULT_MAX_MICROCODE_PATCH_NUM;
  TotalLoadSize   = 0;
  PatchInfoBuffer = AllocatePool (MaxPatchNumber * sizeof (MICROCODE_PATCH_INFO));
  if (PatchInfoBuffer == NULL) {
    return;
  }

  //
  // Process the header of each microcode patch within the region.
  // The purpose is to decide which microcode patch(es) will be loaded into memory.
  //
  do {
    if (MicrocodeEntryPoint->HeaderVersion != 0x1) {
      //
      // Padding data between the microcode patches, skip 1KB to check next entry.
      //
      MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + SIZE_1KB);
      continue;
    }

    DataSize = MicrocodeEntryPoint->DataSize;
    if (DataSize == 0) {
      TotalSize = sizeof (CPU_MICROCODE_HEADER) + 2000;
    } else {
      TotalSize = sizeof (CPU_MICROCODE_HEADER) + DataSize;
    }

    if ( (UINTN)MicrocodeEntryPoint > (MAX_ADDRESS - TotalSize) ||
         ((UINTN)MicrocodeEntryPoint + TotalSize) > MicrocodeEnd ||
         (TotalSize & 0x3) != 0
       ) {
      //
      // Not a valid microcode header, skip 1KB to check next entry.
      //
      MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + SIZE_1KB);
      continue;
    }

    TotalSize = (DataSize == 0) ? 2048 : MicrocodeEntryPoint->TotalSize;

    //
    // Check the 'ProcessorSignature' & 'ProcessorFlags' of this microcode patch
    // with the processors' CPUID & PlatformID to decide if it will be copied
    // into memory
    //
    ProcessorSignature = MicrocodeEntryPoint->ProcessorSignature.Uint32;
    ProcessorFlags     = MicrocodeEntryPoint->ProcessorFlags;
    NeedLoad           = FALSE;
    for (Index = 0; Index < CpuMpData->CpuCount; Index++) {
      CpuData = &CpuMpData->CpuData[Index];
      if ((ProcessorSignature == CpuData->ProcessorSignature) &&
          (ProcessorFlags & (1 << CpuData->PlatformId)) != 0) {
        NeedLoad = TRUE;
        break;
      }
    }

    if (NeedLoad) {
      PatchNumber++;
      if (PatchNumber >= MaxPatchNumber) {
        //
        // Current 'PatchInfoBuffer' cannot hold the information, double the size
        // and allocate a new buffer.
        //
        if (MaxPatchNumber > MAX_UINTN / 2 / sizeof (MICROCODE_PATCH_INFO)) {
          //
          // Overflow check for MaxPatchNumber
          //
          goto OnExit;
        }

        PatchInfoBuffer = ReallocatePool (
                            MaxPatchNumber * sizeof (MICROCODE_PATCH_INFO),
                            2 * MaxPatchNumber * sizeof (MICROCODE_PATCH_INFO),
                            PatchInfoBuffer
                            );
        if (PatchInfoBuffer == NULL) {
          goto OnExit;
        }
        MaxPatchNumber = MaxPatchNumber * 2;
      }

      //
      // Store the information of this microcode patch
      //
      if (TotalSize > MAX_UINTN - TotalLoadSize ||
          ALIGN_VALUE (TotalSize, SIZE_1KB) > MAX_UINTN - TotalLoadSize) {
        goto OnExit;
      }
      PatchInfoBuffer[PatchNumber - 1].Address     = (UINTN) MicrocodeEntryPoint;
      PatchInfoBuffer[PatchNumber - 1].Size        = TotalSize;
      PatchInfoBuffer[PatchNumber - 1].AlignedSize = ALIGN_VALUE (TotalSize, SIZE_1KB);
      TotalLoadSize += PatchInfoBuffer[PatchNumber - 1].AlignedSize;
    }

    //
    // Process the next microcode patch
    //
    MicrocodeEntryPoint = (CPU_MICROCODE_HEADER *) (((UINTN) MicrocodeEntryPoint) + TotalSize);
  } while (((UINTN) MicrocodeEntryPoint < MicrocodeEnd));

  if (PatchNumber == 0) {
    //
    // No patch needs to be loaded
    //
    goto OnExit;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: 0x%x microcode patches will be loaded into memory, with size 0x%x.\n",
    __FUNCTION__, PatchNumber, TotalLoadSize
    ));

  LoadMicrocodePatchWorker (CpuMpData, PatchInfoBuffer, PatchNumber, TotalLoadSize);

OnExit:
  if (PatchInfoBuffer != NULL) {
    FreePool (PatchInfoBuffer);
  }
  return;
}
