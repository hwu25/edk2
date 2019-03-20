/** @file
  Definitions and function declarations for 8259 Programmable Interrupt
  Controller.

Copyright (c) 2019, Intel Corporation. All rights reserved.<BR>
This program and the accompanying materials
are licensed and made available under the terms and conditions of the BSD License
which accompanies this distribution.  The full text of the license may be found at
http://opensource.org/licenses/bsd-license.php

THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef _8259_SERVICE_H_
#define _8259_SERVICE_H_

//
// 8259 Hardware definitions
//
#define LEGACY_8259_BASE_VECTOR_MASTER                    0x68
#define LEGACY_8259_BASE_VECTOR_SLAVE                     0x70

#define LEGACY_8259_CONTROL_REGISTER_MASTER               0x20
#define LEGACY_8259_MASK_REGISTER_MASTER                  0x21
#define LEGACY_8259_CONTROL_REGISTER_SLAVE                0xA0
#define LEGACY_8259_MASK_REGISTER_SLAVE                   0xA1
#define LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_MASTER  0x4D0
#define LEGACY_8259_EDGE_LEVEL_TRIGGERED_REGISTER_SLAVE   0x4D1

#define LEGACY_8259_EOI                                   0x20

#define LEGACY_8259_IRQ0                                  0
#define LEGACY_8259_IRQ8                                  8
#define LEGACY_8259_IRQ15                                 15


/**
  Gets the current IRQ interrupt masks.

  @param[out] IntMask      Interrupt mask for IRQ0-IRQ15.

  @retval EFI_SUCCESS      The IRQ marks were returned successfully.

**/
EFI_STATUS
Legacy8259GetMask (
  OUT UINT16    *IntMask   OPTIONAL
  );

/**
  Translates the IRQ0 into vector.

  @param[out] Vector     The vector that is assigned to the IRQ0.

  @retval EFI_SUCCESS    The Vector that matches Irq was returned.

**/
EFI_STATUS
Legacy8259GetIrq0Vector (
  OUT UINT8    *Vector
  );

/**
  Enables the IRQ0 in edge-triggered mode.

  @retval EFI_SUCCESS          The Irq0 was enabled on the 8259 PIC.

**/
EFI_STATUS
Legacy8259EnableIrq0 (
  VOID
  );

/**
  Disables the IRQ0.

  @retval EFI_SUCCESS    The Irq0 was disabled on the 8259 PIC.

**/
EFI_STATUS
Legacy8259DisableIrq0 (
  VOID
  );

/**
  Issues the End of Interrupt (EOI) commands to 8259 PIC.

  @param[in] Irq    The interrupt for which to issue the EOI command.

**/
VOID
Legacy8259EndOfInterrupt (
  IN UINT8    Irq
  );

/**
  Intialize the 8259 PIC.

  @retval EFI_SUCCESS    The 8259 PIC was initialized successfully.

**/
EFI_STATUS
Initialize8259 (
  VOID
  );

#endif  // _8259_SERVICE_H_
