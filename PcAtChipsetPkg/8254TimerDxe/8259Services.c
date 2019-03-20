/** @file
  Services used by the driver to program the 8259 Programmable Interrupt
  Controller.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include "Timer.h"

//
// Global variables to track the IRQ masks of the 8259 PIC.
//
UINT16 mIntMask   = 0xffff;
UINT16 mEdgeLevel = 0x0000;


/**
  Write to mask and edge/level triggered registers of master and slave PICs.

  @param[in] Mask         Low byte for master PIC mask register,
                          High byte for slave PIC mask register.
  @param[in] EdgeLevel    Low byte for master PIC edge/level triggered register,
                          High byte for slave PIC edge/level triggered register.

**/
VOID
Legacy8259WriteMask (
  IN UINT16    Mask,
  IN UINT16    EdgeLevel
  )
{
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, (UINT8) Mask);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE, (UINT8) (Mask >> 8));
  IoWrite8 (LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_MASTER, (UINT8) EdgeLevel);
  IoWrite8 (LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_SLAVE, (UINT8) (EdgeLevel >> 8));
}

/**
  Sets the base address for the 8259 master and slave PICs.

  @param[in] MasterBase     Interrupt vectors for IRQ0-IRQ7.
  @param[in] SlaveBase      Interrupt vectors for IRQ8-IRQ15.

**/
VOID
Legacy8259SetBaseAddress (
  IN UINT8    MasterBase,
  IN UINT8    SlaveBase
  )
{
  UINT8   Mask;
  EFI_TPL OriginalTpl;

  OriginalTpl = gBS->RaiseTPL (TPL_HIGH_LEVEL);

  //
  // Preserve interrtup mask register
  //
  Mask = IoRead8 (LEGACY_8259_MASK_REGISTER_SLAVE);

  //
  // ICW1: cascade mode
  // ICW2: new vector base (must be multiple of 8)
  // ICW3: slave indentification code must be 2
  // ICW4: fully nested mode, non-buffered mode, normal EOI, IA processor
  //
  IoWrite8 (LEGACY_8259_CONTROL_REGISTER_SLAVE, 0x11);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE, SlaveBase);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE, 0x02);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE, 0x01);

  //
  // Restore interrupt mask register
  //
  IoWrite8 (LEGACY_8259_MASK_REGISTER_SLAVE, Mask);

  //
  // Preserve interrtup mask register
  //
  Mask = IoRead8 (LEGACY_8259_MASK_REGISTER_MASTER);

  //
  // ICW1: cascade mode
  // ICW2: new vector base (must be multiple of 8)
  // ICW3: slave PIC is cascaded on IRQ2
  // ICW4: fully nested mode, non-buffered mode, normal EOI, IA processor
  //
  IoWrite8 (LEGACY_8259_CONTROL_REGISTER_MASTER, 0x11);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, MasterBase);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, 0x04);
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, 0x01);

  //
  // Restore interrupt mask register
  //
  IoWrite8 (LEGACY_8259_MASK_REGISTER_MASTER, Mask);

  IoWrite8 (LEGACY_8259_CONTROL_REGISTER_SLAVE, LEGACY_8259_EOI);
  IoWrite8 (LEGACY_8259_CONTROL_REGISTER_MASTER, LEGACY_8259_EOI);

  gBS->RestoreTPL (OriginalTpl);
}

/**
  Gets the current IRQ interrupt masks.

  @param[out] IntMask      Interrupt mask for IRQ0-IRQ15.

  @retval EFI_SUCCESS      The IRQ marks were returned successfully.

**/
EFI_STATUS
Legacy8259GetMask (
  OUT UINT16    *IntMask   OPTIONAL
  )
{
  if (IntMask != NULL) {
    *IntMask   = mIntMask;
  }

  return EFI_SUCCESS;
}

/**
  Translates the IRQ0 into vector.

  @param[out] Vector     The vector that is assigned to the IRQ0.

  @retval EFI_SUCCESS    The Vector that matches Irq was returned.

**/
EFI_STATUS
Legacy8259GetIrq0Vector (
  OUT UINT8    *Vector
  )
{
  *Vector = (UINT8) (LEGACY_8259_BASE_VECTOR_MASTER + LEGACY_8259_IRQ0);

  return EFI_SUCCESS;
}

/**
  Enables the IRQ0 in edge-triggered mode.

  @retval EFI_SUCCESS          The Irq0 was enabled on the 8259 PIC.

**/
EFI_STATUS
Legacy8259EnableIrq0 (
  VOID
  )
{
  mIntMask = (UINT16) (mIntMask & ~(1 << LEGACY_8259_IRQ0));
  mEdgeLevel = (UINT16) (mEdgeLevel & ~(1 << LEGACY_8259_IRQ0));

  Legacy8259WriteMask (mIntMask, mEdgeLevel);

  return EFI_SUCCESS;
}

/**
  Disables the IRQ0.

  @retval EFI_SUCCESS    The Irq0 was disabled on the 8259 PIC.

**/
EFI_STATUS
Legacy8259DisableIrq0 (
  VOID
  )
{
  mIntMask = (UINT16) (mIntMask | (1 << LEGACY_8259_IRQ0));

  mEdgeLevel = (UINT16) (mEdgeLevel & ~(1 << LEGACY_8259_IRQ0));

  Legacy8259WriteMask (mIntMask, mEdgeLevel);

  return EFI_SUCCESS;
}

/**
  Issues the End of Interrupt (EOI) commands to 8259 PIC.

  @param[in] Irq    The interrupt for which to issue the EOI command.

**/
VOID
Legacy8259EndOfInterrupt (
  IN UINT8    Irq
  )
{
  if (Irq >= LEGACY_8259_IRQ8) {
    IoWrite8 (LEGACY_8259_CONTROL_REGISTER_SLAVE, LEGACY_8259_EOI);
  }

  IoWrite8 (LEGACY_8259_CONTROL_REGISTER_MASTER, LEGACY_8259_EOI);
}

/**
  Intialize the 8259 PIC.

  @retval EFI_SUCCESS    The 8259 PIC was initialized successfully.

**/
EFI_STATUS
Initialize8259 (
  VOID
  )
{
  UINT8    Irq;

  //
  // Clear all pending interrupt
  //
  for (Irq = LEGACY_8259_IRQ0; Irq <= LEGACY_8259_IRQ15; Irq++) {
    Legacy8259EndOfInterrupt (Irq);
  }

  //
  // Set the 8259 Master base to 0x68 and the 8259 Slave base to 0x70
  //
  Legacy8259SetBaseAddress (
    LEGACY_8259_BASE_VECTOR_MASTER,
    LEGACY_8259_BASE_VECTOR_SLAVE
    );

  //
  // Set all 8259 interrupts to edge triggered and disabled
  //
  Legacy8259WriteMask (mIntMask, mEdgeLevel);

  return EFI_SUCCESS;
}
