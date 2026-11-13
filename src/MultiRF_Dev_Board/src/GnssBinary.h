/*
  ============================================================================
  File:     GnssBinary.h
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Helpers for the Ebyte EWM108-GN05 GNSS receiver (Allystar HD8040D).

  The receiver sends positions as NMEA text, which a library such as
  TinyGPSPlus decodes. To change what it does (sentences, restarts, sleep)
  you send it frames in the Allystar binary protocol:

      0xF1 0xD9 | group | sub ID | length (2 bytes, little-endian) |
      payload | CK1 CK2

  CK1/CK2 is an 8-bit Fletcher checksum over group, sub ID, length and
  payload. Configuration commands are answered with ACK-ACK (0x05 0x01) or
  ACK-NAK (0x05 0x00); restart commands send no answer.

  Reference: ALLYSTAR GNSS Receiver Binary Protocol Specification V2.3.8.
  ============================================================================
*/
#pragma once
#include <Arduino.h>
#include "MultiRF_Pins.h"

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------

// The UART connected to the EWM108-GN05: a reference to the core's Serial2.
// Read NMEA bytes from it and pass them to TinyGPSPlus.
extern HardwareSerial& gpsSerial;

// Open the UART with a 1024-byte receive buffer. The default 256 bytes
// overflow easily, because a multi-constellation receiver sends a lot of
// text every second. Defaults: IO16/IO15 and 115200 baud.
void gnssBegin(int8_t rxPin = MULTIRF_GNSS_RX, int8_t txPin = MULTIRF_GNSS_TX,
               uint32_t baud = MULTIRF_GNSS_BAUD);

// ---------------------------------------------------------------------------
// Commands to the receiver
// ---------------------------------------------------------------------------

// Build and send one binary frame; the checksum is added automatically.
void gnssSend(uint8_t group, uint8_t id, const uint8_t* pl, uint16_t len);

// NMEA sentence IDs for gnssSetNmeaRate()
enum : uint8_t {
  NMEA_GGA = 0x00,   // fix: time, position, fix quality, satellites, HDOP, altitude
  NMEA_GLL = 0x01,   // position
  NMEA_GSA = 0x02,   // satellites used in the fix, DOP values
  NMEA_GRS = 0x03,   // range residuals
  NMEA_GSV = 0x04,   // satellites in view with signal strength
  NMEA_RMC = 0x05,   // recommended minimum: position, speed, course, date
  NMEA_VTG = 0x06,   // course and speed
  NMEA_ZDA = 0x07    // date and time
};

// Output period of one NMEA sentence: 0 = off, 1 = every fix,
// 2 = every second fix, and so on (CFG-MSG).
void gnssSetNmeaRate(uint8_t sentence, uint8_t period);

// Restart modes for gnssRestart() (CFG-SIMPLERST)
enum : uint8_t {
  GNSS_RESET = 0x00,   // hardware reset
  GNSS_COLD  = 0x01,   // clear all satellite data: about 30 s to a fix in open sky
  GNSS_WARM  = 0x02,   // keep almanac, clear ephemeris
  GNSS_HOT   = 0x03,   // keep everything: about 1 s to a fix
  GNSS_STOP  = 0x10,   // stop navigation
  GNSS_START = 0x11    // start navigation
};
void gnssRestart(uint8_t mode);

// Save settings to flash so they survive power-off (CFG-CFG).
// mask bit 0 = baud rate, bit 1 = NMEA sentence rates, bit 2 = navigation settings.
void gnssSave(uint32_t mask = 0x03);

// Restore all factory settings (CFG-CFG, clear all).
void gnssFactoryReset();

// Sleep for 'ms' milliseconds, then resume by itself (CFG-SLEEP).
// mode 0 = sleep, 1 = deep sleep. Satellite data is kept, so the next
// fix is a hot start. The receiver draws about 47 mA when tracking.
void gnssSleep(uint32_t ms, uint8_t mode = 1);

// ---------------------------------------------------------------------------
// Replies from the receiver
// ---------------------------------------------------------------------------

// Separates binary replies from NMEA text in the incoming byte stream.
// NMEA is plain ASCII, so a 0xF1 byte can only be the start of a binary
// frame. Typical use in loop():
//
//   uint8_t c = gpsSerial.read();
//   if (bin.consume(c)) {                  // byte belongs to a binary frame
//     if (bin.ready) { gnssPrintFrame(bin); bin.ready = false; }
//   } else {
//     gps.encode(c);                       // NMEA text for TinyGPSPlus
//   }
struct GnssBinParser {
  enum State { IDLE, SYNC2, HEADER, PAYLOAD, CK } state = IDLE;
  uint8_t  hdr[4];            // group, sub ID, length low, length high
  uint8_t  idx = 0;
  uint8_t  payload[64];       // longer frames are skipped
  uint16_t len = 0;
  uint16_t got = 0;
  uint8_t  ck[2];
  bool     ready = false;     // true when a complete frame with a valid checksum is waiting

  // Returns true if the byte belonged to a binary frame (do not pass it
  // to the NMEA parser), false if it is NMEA text.
  bool consume(uint8_t c);
  uint8_t group() const { return hdr[0]; }
  uint8_t id() const { return hdr[1]; }
};

// Print an ACK/NAK or any other binary frame in readable form.
void gnssPrintFrame(const GnssBinParser& p);
