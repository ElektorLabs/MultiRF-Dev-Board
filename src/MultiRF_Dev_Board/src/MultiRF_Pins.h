/*
  ============================================================================
  File:     MultiRF_Pins.h
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop
  ----------------------------------------------------------------------------
  ESP32-S3 pin map of the Elektor MultiRF Dev Board.

  Every pin in this file was traced on the board schematic (250550-001).
  The CC1101 (K3) and nRF24L01P (K2) pins are added together with their
  examples, so nothing here is a guess.
  ============================================================================
*/
#pragma once

// ---------------------------------------------------------------------------
// E77-900M22S LoRaWAN module (MOD1), connected to UART1
// ---------------------------------------------------------------------------
// Note on direction: "RX" and "TX" are always seen from the ESP32.
// The module's TX (PA2) goes to the ESP32's RX pin, and the other way round.
#define MULTIRF_E77_RX      48      // ESP32 RX  <- E77 PA2, UART TX (module pin 9)
#define MULTIRF_E77_TX      47      // ESP32 TX  -> E77 PA3, UART RX (module pin 10)
#define MULTIRF_E77_BAUD    9600    // factory default of Ebyte's AT firmware
                                    // (low-power UART, 9600 baud is its maximum)

// Board notes for the E77-900M22S:
//  - LED2, LED4 and LED5 are driven by the module itself, not by the ESP32:
//      PB3 -> LED2: transmission done
//      PB4 -> LED4: reception done
//      PB5 -> LED5: joined to a LoRaWAN network
//  - The module's NRST pin only has a capacitor to ground and is not wired
//    to the ESP32. Restart the module with the AT+RST command.

// ---------------------------------------------------------------------------
// EWM108-GN05 GNSS receiver (MOD3), connected to UART2
// ---------------------------------------------------------------------------
#define MULTIRF_GNSS_RX     16      // ESP32 RX  <- GNSS TXD (module pin 2, also K8 pin 3)
#define MULTIRF_GNSS_TX     15      // ESP32 TX  -> GNSS RXD (module pin 3, also K8 pin 4)
#define MULTIRF_GNSS_BAUD   115200  // factory default of the EWM108-GN05

// Board notes for the EWM108-GN05:
//  - 1PPS (module pin 4) goes to a test point only, not to the ESP32.
//  - V_BCKP is fed by a 0.1 F supercapacitor (C12) through diode D3, which
//    keeps satellite data for a hot start for a few hours after power-off.
//  - ANT_BIAS powers an active antenna through inductor L1.
//  - The GNSS UART is also on header K8 (GND, 3.3 V, TXD, RXD).

// ---------------------------------------------------------------------------
// USB
// ---------------------------------------------------------------------------
// IO19 = USB D-, IO20 = USB D+ (native USB of the ESP32-S3).
// Enable "USB CDC On Boot" in the Arduino IDE so Serial goes to the USB-C port.
