/*
  ============================================================================
  File:     E77AT.h
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Helpers for the Ebyte E77-900M22S LoRaWAN module and its AT firmware.

  How the module talks:
    - Every command is a line of text ending in CR LF, e.g. "AT+REGION=?".
    - Every command is answered with a status line: "OK", or an error that
      starts with "AT_" (AT_ERROR, AT_PARAM_ERROR, AT_BUSY_ERROR, ...).
    - Some results arrive later on their own, as events starting with
      "+EVT:", for example "+EVT:JOINED" a few seconds after AT+CJOIN.
    - Keys and payloads are written as hex text: bytes 0x11 0x22 -> "1122".

  Reference: Ebyte E77-M series user manual V1.6 (Ebyte AT command set).
  ============================================================================
*/
#pragma once
#include <Arduino.h>
#include "MultiRF_Pins.h"

// ---------------------------------------------------------------------------
// UART
// ---------------------------------------------------------------------------

// The UART connected to the E77-900M22S. This is a reference to the core's
// Serial1 (UART1), so no second driver object is created for the same port.
// Use it directly for raw access, e.g. lora.write(), lora.read().
extern HardwareSerial& lora;

// Open the UART. The defaults are the MultiRF Dev Board pins (IO48/IO47)
// and the module's factory baud rate (9600).
void e77Begin(int8_t rxPin = MULTIRF_E77_RX, int8_t txPin = MULTIRF_E77_TX,
              uint32_t baud = MULTIRF_E77_BAUD);

// Print every command and reply on Serial. On by default, which is handy
// while learning; switch it off in finished applications.
void e77SetEcho(bool on);

// Select the AT+SEND format. Ebyte's v1.6 manual shows four fields
// (port:retries:ack:payload) in its example and three (port:ack:payload)
// in its help text. The default is 4. Type "AT+SEND?" to check your
// firmware, and call e77SetSendFields(3) after e77Begin() if needed.
void e77SetSendFields(uint8_t fields);

// ---------------------------------------------------------------------------
// Basic command and event handling
// ---------------------------------------------------------------------------

// Send one command and return everything received up to the status line
// ("OK" or an "AT_" error), or up to the timeout. Unread bytes waiting in
// the UART are discarded first, so the reply belongs to this command.
String atCmd(const char* cmd, uint32_t timeoutMs = 2000);

// True if a reply from atCmd() contains "OK" and no "AT_" error.
bool atOk(const String& resp);

// Keep reading until 'token' appears (e.g. "+EVT:JOINED") or the timeout
// expires. Returns false on timeout, and early on "JOIN FAILED".
bool waitFor(const char* token, uint32_t timeoutMs);

// Non-blocking line reader for use in loop(). Returns true when 'line'
// holds a new complete line from the module (without CR/LF).
bool e77ReadLine(String& line);

// ---------------------------------------------------------------------------
// Network setup
// ---------------------------------------------------------------------------

// Make sure the module uses 'region'
// (0 = AS923, 1 = AU915, 5 = EU868, 6 = KR920, 7 = IN865, 8 = US915, 9 = RU864).
// Changing region needs AT+RESTORE first, and a restore also resets the
// LoRaWAN 1.0.4 join counter (DevNonce), after which TTN refuses joins until
// the DevNonces are reset in the console. So the restore only happens when
// the current region was read successfully AND differs. If the reply cannot
// be read, nothing is changed and a warning is printed.
// Returns true if the module is (now) on the requested region.
bool e77EnsureRegion(int region);

// Write the OTAA credentials as plain hex (no spaces or colons).
//   devEui: 16 hex characters, or "" to keep the module's own DevEUI
//   appEui: 16 hex characters (JoinEUI), all zeros is fine for TTN
//   appKey: 32 hex characters from the TTN console
// Prints a warning for wrong lengths or an all-zero placeholder AppKey.
void e77SetKeys(const char* devEui, const char* appEui, const char* appKey);

// Join the network by OTAA. Waits for the "+EVT:JOINED" event, not just for
// "OK", and backs off 15 s, 30 s, 60 s, up to 2 min between attempts,
// because every attempt is a transmission that counts against the duty cycle.
bool e77Join(int maxAttempts = 6);

// ---------------------------------------------------------------------------
// Sending and receiving
// ---------------------------------------------------------------------------

enum E77SendResult {
  E77_SENT,         // accepted by the module and sent
  E77_DUTYCYCLE,    // AT_DUTYCYCLE_RESTRICTED: sending too often, wait
  E77_NOT_JOINED,   // module is not on a network, join again
  E77_BUSY,         // module still busy with a previous transmission
  E77_ERROR         // any other error, e.g. wrong AT+SEND format
};

// Send 'len' bytes as an uplink on 'port' (1 to 199). The bytes are converted
// to hex text. Payloads are limited to 51 bytes, the EU868 maximum at the
// slowest data rates; longer payloads are refused with E77_ERROR.
E77SendResult e77Send(uint8_t port, const uint8_t* data, size_t len, bool confirmed = false);

// Downlink report as printed by the module, e.g.
//   "+EVT:RX_1, PORT 0, DR 3, RSSI -49, SNR 10"   (Ebyte format)
//   "+EVT:RX_1:3:-49:10"                          (ST format)
// margin (dB above the decoding limit) and gateways are filled in only when
// the firmware reports them after AT+LINKC; otherwise they stay at -1.
struct E77RxInfo {
  int window   = 0;    // receive window, 1 or 2 (0 if unknown)
  int dr       = -1;   // data rate of the downlink
  int rssi     = 0;    // received signal strength in dBm
  int snr      = 0;    // signal-to-noise ratio in dB
  int margin   = -1;   // link check: demodulation margin in dB
  int gateways = -1;   // link check: number of gateways that heard the uplink
};

// Parse a "+EVT:RX_..." line into 'info'. Returns false for other lines.
bool e77ParseRx(const String& line, E77RxInfo& info);
