/*
  ============================================================================
  File:     GnssBinary.cpp
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Implementation of the EWM108-GN05 binary protocol helpers. See GnssBinary.h
  for the description of each function.
  ============================================================================
*/
#include "GnssBinary.h"

// The EWM108-GN05 is on UART2. Using a reference to the core's Serial2
// avoids a second driver object for the same hardware port.
HardwareSerial& gpsSerial = Serial2;

// Group and sub IDs used below (Allystar protocol specification)
static const uint8_t GRP_CFG        = 0x06;
static const uint8_t ID_CFG_MSG     = 0x01;   // NMEA sentence output rate
static const uint8_t ID_CFG_CFG     = 0x09;   // save, load or clear settings
static const uint8_t ID_CFG_RST     = 0x40;   // CFG-SIMPLERST: restart
static const uint8_t ID_CFG_SLEEP   = 0x41;   // timed sleep
static const uint8_t GRP_ACK        = 0x05;   // ACK-NAK = 0x00, ACK-ACK = 0x01
static const uint8_t NMEA_CLASS     = 0xF0;   // class of standard NMEA sentences

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------
void gnssBegin(int8_t rxPin, int8_t txPin, uint32_t baud) {
  gpsSerial.setRxBufferSize(1024);             // must be called before begin()
  gpsSerial.begin(baud, SERIAL_8N1, rxPin, txPin);
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
void gnssSend(uint8_t group, uint8_t id, const uint8_t* pl, uint16_t len) {
  // Header: group, sub ID and the payload length, little-endian.
  uint8_t hdr[4] = { group, id, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8) };

  // 8-bit Fletcher checksum over the header and the payload.
  uint8_t ck1 = 0, ck2 = 0;
  for (int i = 0; i < 4; i++)   { ck1 += hdr[i]; ck2 += ck1; }
  for (int i = 0; i < len; i++) { ck1 += pl[i];  ck2 += ck1; }

  const uint8_t sync[2] = { 0xF1, 0xD9 };      // every frame starts with these
  gpsSerial.write(sync, 2);
  gpsSerial.write(hdr, 4);
  if (len) gpsSerial.write(pl, len);
  gpsSerial.write(ck1);
  gpsSerial.write(ck2);
}

void gnssSetNmeaRate(uint8_t sentence, uint8_t period) {
  const uint8_t pl[3] = { NMEA_CLASS, sentence, period };
  gnssSend(GRP_CFG, ID_CFG_MSG, pl, 3);
}

void gnssRestart(uint8_t mode) {
  gnssSend(GRP_CFG, ID_CFG_RST, &mode, 1);
}

void gnssSave(uint32_t mask) {
  // Payload: action (U4, 0 = save) followed by the mask (U4), little-endian.
  const uint8_t pl[8] = { 0, 0, 0, 0,
                          (uint8_t)mask, (uint8_t)(mask >> 8),
                          (uint8_t)(mask >> 16), (uint8_t)(mask >> 24) };
  gnssSend(GRP_CFG, ID_CFG_CFG, pl, 8);
}

void gnssFactoryReset() {
  // Action 2 = clear, mask 0xFFFFFFFF = all settings.
  const uint8_t pl[8] = { 2, 0, 0, 0, 0xFF, 0xFF, 0xFF, 0xFF };
  gnssSend(GRP_CFG, ID_CFG_CFG, pl, 8);
}

void gnssSleep(uint32_t ms, uint8_t mode) {
  // Payload: sleep time in ms (U4, little-endian) and the sleep mode (U1).
  const uint8_t pl[5] = { (uint8_t)ms, (uint8_t)(ms >> 8),
                          (uint8_t)(ms >> 16), (uint8_t)(ms >> 24), mode };
  gnssSend(GRP_CFG, ID_CFG_SLEEP, pl, 5);
}

// ---------------------------------------------------------------------------
// Binary frame parser
// ---------------------------------------------------------------------------
bool GnssBinParser::consume(uint8_t c) {
  switch (state) {
    case IDLE:                                  // waiting for the first sync byte
      if (c == 0xF1) { state = SYNC2; return true; }
      return false;                             // NMEA text

    case SYNC2:                                 // second sync byte must follow
      if (c == 0xD9) { state = HEADER; idx = 0; return true; }
      state = IDLE;
      return false;

    case HEADER:                                // group, sub ID, length (2 bytes)
      hdr[idx++] = c;
      if (idx == 4) {
        len = hdr[2] | (hdr[3] << 8);
        got = 0;
        idx = 0;
        if (len > sizeof payload) state = IDLE; // too long for this parser, skip it
        else state = (len == 0) ? CK : PAYLOAD;
      }
      return true;

    case PAYLOAD:
      payload[got++] = c;
      if (got == len) { state = CK; idx = 0; }
      return true;

    case CK:                                    // two checksum bytes
      ck[idx++] = c;
      if (idx == 2) {
        uint8_t a = 0, b = 0;
        for (int i = 0; i < 4; i++)   { a += hdr[i];     b += a; }
        for (int i = 0; i < len; i++) { a += payload[i]; b += a; }
        ready = (a == ck[0] && b == ck[1]);     // only report frames that check out
        state = IDLE;
      }
      return true;
  }
  return false;
}

void gnssPrintFrame(const GnssBinParser& p) {
  if (p.group() == GRP_ACK && p.len >= 2) {
    // The payload of an ACK/NAK holds the group and sub ID of the command.
    Serial.printf("[GNSS] %s for command 0x%02X 0x%02X\n",
                  p.id() == 0x01 ? "ACK" : "NAK", p.payload[0], p.payload[1]);
  } else {
    Serial.printf("[GNSS] Binary frame 0x%02X 0x%02X, %u bytes\n",
                  p.group(), p.id(), (unsigned)p.len);
  }
}
