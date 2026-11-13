/*
  ============================================================================
  File:     E77AT.cpp
  Library:  MultiRF Dev Board
  Author:   Saad-Imtiaz
  Series:   Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Implementation of the E77-900M22S AT command helpers. See E77AT.h for the
  description of each function.
  ============================================================================
*/
#include "E77AT.h"

// The E77-900M22S is on UART1. Using a reference to the core's Serial1
// avoids a second driver object for the same hardware port.
HardwareSerial& lora = Serial1;

// ---------------------------------------------------------------------------
// Module-wide settings and state
// ---------------------------------------------------------------------------
static bool    s_echo       = true;   // print commands and replies on Serial
static uint8_t s_sendFields = 4;      // AT+SEND format: 3 or 4 fields
static String  s_lineAcc;             // partial line collected by e77ReadLine()

// Constants used below
static const uint32_t JOIN_EVENT_TIMEOUT_MS = 20000;   // join result arrives after
                                                       // RX1/RX2 (5 s / 6 s in EU868)
static const uint32_t JOIN_BACKOFF_START_MS = 15000;   // first wait after a failed join
static const uint32_t JOIN_BACKOFF_MAX_MS   = 120000;  // never wait longer than 2 min
static const size_t   MAX_PAYLOAD           = 51;      // EU868 limit at DR0 to DR2
static const size_t   MAX_LINE              = 200;     // longest line we keep

// ---------------------------------------------------------------------------
// UART and settings
// ---------------------------------------------------------------------------
void e77Begin(int8_t rxPin, int8_t txPin, uint32_t baud) {
  lora.begin(baud, SERIAL_8N1, rxPin, txPin);
}

void e77SetEcho(bool on) { s_echo = on; }

void e77SetSendFields(uint8_t fields) { s_sendFields = (fields == 3) ? 3 : 4; }

// ---------------------------------------------------------------------------
// Commands and events
// ---------------------------------------------------------------------------
String atCmd(const char* cmd, uint32_t timeoutMs) {
  // Throw away anything left in the UART, and any half-collected line,
  // so that the reply we gather belongs to this command only.
  while (lora.available()) lora.read();
  s_lineAcc = "";

  lora.print(cmd);
  lora.print("\r\n");                              // every command ends in CR LF

  // Collect bytes until a status line arrives or the time runs out.
  String resp;
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (lora.available()) resp += (char)lora.read();
    if (resp.indexOf("OK\r") >= 0 || resp.indexOf("AT_") >= 0) {
      delay(20);                                   // let the rest of the line arrive
      while (lora.available()) resp += (char)lora.read();
      break;
    }
  }

  if (s_echo) {
    Serial.print("> "); Serial.println(cmd);
    Serial.print(resp);
    if (!resp.endsWith("\n")) Serial.println();
  }
  return resp;
}

bool atOk(const String& resp) {
  // Error replies all start with "AT_", e.g. AT_PARAM_ERROR.
  return resp.indexOf("OK") >= 0 && resp.indexOf("AT_") < 0;
}

bool waitFor(const char* token, uint32_t timeoutMs) {
  String buf;
  s_lineAcc = "";                                  // we consume raw bytes here
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    while (lora.available()) {
      char c = (char)lora.read();
      buf += c;
      if (s_echo) Serial.write(c);
    }
    if (buf.indexOf(token) >= 0) return true;
    if (buf.indexOf("JOIN FAILED") >= 0) return false;   // no point waiting longer
  }
  return false;
}

bool e77ReadLine(String& line) {
  while (lora.available()) {
    char c = (char)lora.read();
    if (c == '\r') continue;                       // lines end in CR LF, keep LF only
    if (c == '\n') {
      if (s_lineAcc.length() == 0) continue;       // skip empty lines
      line = s_lineAcc;
      s_lineAcc = "";
      return true;
    }
    if (s_lineAcc.length() < MAX_LINE) s_lineAcc += c;
  }
  return false;
}

// ---------------------------------------------------------------------------
// Network setup
// ---------------------------------------------------------------------------

// Find the region number in the reply to AT+REGION=?. Accepts replies such
// as "5:EU868", "REGION=5" or "+REGION: 5". Returns -1 if no valid region
// number (0 to 9) is found, so that the caller never restores the module
// because of a reply it could not read.
static int parseRegion(const String& resp) {
  static const char* const names[] = { "AS923", "AU915", "CN470", "CN779", "EU433",
                                       "EU868", "KR920", "IN865", "US915", "RU864" };
  int start = 0;
  while (start < (int)resp.length()) {
    int end = resp.indexOf('\n', start);
    if (end < 0) end = resp.length();
    String l = resp.substring(start, end);
    start = end + 1;
    l.trim();
    if (l.length() == 0 || l == "OK" || l.startsWith("AT")) continue;

    // Only trust lines that look like a region report.
    bool looksLikeRegion = l.indexOf("REGION") >= 0;
    for (const char* n : names) if (l.indexOf(n) >= 0) looksLikeRegion = true;
    if (!looksLikeRegion) continue;

    // The region number is the first number on the line ("5:EU868" -> 5).
    for (int i = 0; i < (int)l.length(); i++) {
      if (!isDigit(l[i])) continue;
      int j = i;
      while (j < (int)l.length() && isDigit(l[j])) j++;
      int v = l.substring(i, j).toInt();
      return (v >= 0 && v <= 9) ? v : -1;          // "EU868" alone gives 868: not a region number
    }
  }
  return -1;
}

bool e77EnsureRegion(int region) {
  int current = parseRegion(atCmd("AT+REGION=?"));

  if (current == region) return true;              // nothing to do
  if (current < 0) {
    // Could not read the region: do NOT restore, that would reset the
    // join counter for nothing. Check the wiring and the firmware type.
    Serial.println("[E77] Could not read the region (AT+REGION=?). Settings left unchanged.");
    Serial.println("[E77] Check wiring, 9600 baud, and that the module has Ebyte's AT firmware.");
    return false;
  }

  Serial.printf("[E77] Region is %d, changing to %d (restores factory defaults first)\n",
                current, region);
  atCmd("AT+RESTORE", 3000);
  delay(500);                                      // give the module time to restart
  char cmd[24];
  snprintf(cmd, sizeof cmd, "AT+REGION=%d", region);
  return atOk(atCmd(cmd));
}

// True if s has exactly 'len' hex characters.
static bool isHex(const char* s, size_t len) {
  if (!s || strlen(s) != len) return false;
  for (size_t i = 0; i < len; i++) if (!isxdigit((unsigned char)s[i])) return false;
  return true;
}

// True if s consists of '0' characters only (an unfilled placeholder).
static bool allZeros(const char* s) {
  for (; *s; s++) if (*s != '0') return false;
  return true;
}

void e77SetKeys(const char* devEui, const char* appEui, const char* appKey) {
  char cmd[80];

  // DevEUI is optional: an empty string keeps the module's own identity.
  if (devEui && strlen(devEui) > 0) {
    if (isHex(devEui, 16)) {
      snprintf(cmd, sizeof cmd, "AT+CDEVEUI=%s", devEui); atCmd(cmd);
    } else {
      Serial.println("[E77] DevEUI must be 16 hex characters, keeping the module's own.");
    }
  }

  if (isHex(appEui, 16)) {
    snprintf(cmd, sizeof cmd, "AT+CAPPEUI=%s", appEui); atCmd(cmd);
  } else {
    Serial.println("[E77] JoinEUI/AppEUI must be 16 hex characters, not written.");
  }

  if (isHex(appKey, 32)) {
    if (allZeros(appKey))
      Serial.println("[E77] Warning: the AppKey is all zeros. Copy the real key from TTN into config.h.");
    snprintf(cmd, sizeof cmd, "AT+CAPPKEY=%s", appKey); atCmd(cmd);
  } else {
    Serial.println("[E77] AppKey must be 32 hex characters, not written.");
  }
}

bool e77Join(int maxAttempts) {
  uint32_t backoff = JOIN_BACKOFF_START_MS;
  for (int i = 1; i <= maxAttempts; i++) {
    Serial.printf("[E77] Join attempt %d of %d\n", i, maxAttempts);

    // "OK" only means the request was accepted. The result comes seconds
    // later as an event, after the module has listened in both receive windows.
    String r = atCmd("AT+CJOIN=1:0");              // 1 = OTAA, 0 = no auto-join at power-up
    if (atOk(r) && waitFor("+EVT:JOINED", JOIN_EVENT_TIMEOUT_MS)) {
      Serial.println("\n[E77] Joined. LED5 (LINK) on the board should be lit.");
      return true;
    }

    if (i == maxAttempts) break;                   // no point waiting after the last try
    Serial.printf("\n[E77] Join failed, retrying in %lu s\n", (unsigned long)(backoff / 1000));
    delay(backoff);
    backoff = min(backoff * 2, JOIN_BACKOFF_MAX_MS);
  }
  Serial.println("\n[E77] Join failed. Check region, keys and gateway coverage.");
  return false;
}

// ---------------------------------------------------------------------------
// Sending and receiving
// ---------------------------------------------------------------------------
E77SendResult e77Send(uint8_t port, const uint8_t* data, size_t len, bool confirmed) {
  if (port < 1 || port > 199) {
    Serial.println("[E77] Port must be between 1 and 199.");
    return E77_ERROR;
  }
  if (len > MAX_PAYLOAD) {
    Serial.printf("[E77] Payload of %u bytes is too long (max %u).\n",
                  (unsigned)len, (unsigned)MAX_PAYLOAD);
    return E77_ERROR;
  }

  // Build "AT+SEND=port:retries:ack:HEX" (4 fields) or "AT+SEND=port:ack:HEX".
  // A confirmed uplink allows one retransmission; unconfirmed uses none.
  char cmd[24 + 2 * MAX_PAYLOAD + 1];
  int n;
  if (s_sendFields == 4)
    n = snprintf(cmd, sizeof cmd, "AT+SEND=%u:%u:%u:", port, confirmed ? 1 : 0, confirmed ? 1 : 0);
  else
    n = snprintf(cmd, sizeof cmd, "AT+SEND=%u:%u:", port, confirmed ? 1 : 0);
  for (size_t i = 0; i < len; i++) n += snprintf(cmd + n, sizeof cmd - n, "%02X", data[i]);

  // Map the module's reply onto a result code.
  String r = atCmd(cmd, 5000);
  if (r.indexOf("DUTYC") >= 0) return E77_DUTYCYCLE;           // AT_DUTYCYCLE_RESTRICTED
  if (r.indexOf("NO_NETWORK_JOINED") >= 0 || r.indexOf("NO_NET_JOINED") >= 0) return E77_NOT_JOINED;
  if (r.indexOf("BUSY") >= 0) return E77_BUSY;
  if (atOk(r)) return E77_SENT;
  return E77_ERROR;
}

// Read the number that follows 'key' in 's' ("RSSI -49" -> -49).
static bool numberAfter(const String& s, const char* key, int& out) {
  int i = s.indexOf(key);
  if (i < 0) return false;
  i += strlen(key);
  while (i < (int)s.length() && (s[i] == ' ' || s[i] == ':' || s[i] == '=')) i++;
  int j = i;
  if (j < (int)s.length() && (s[j] == '-' || s[j] == '+')) j++;
  while (j < (int)s.length() && isDigit(s[j])) j++;
  if (j == i) return false;
  out = s.substring(i, j).toInt();
  return true;
}

bool e77ParseRx(const String& line, E77RxInfo& info) {
  int p = line.indexOf("+EVT:RX_");
  if (p < 0) return false;

  // The digit after "RX_" is the receive window (1 or 2).
  char w = line[p + 8];
  info.window = isDigit(w) ? (w - '0') : 0;

  // Ebyte format: keywords followed by values.
  if (line.indexOf("RSSI") >= 0) {
    numberAfter(line, "DR", info.dr);
    numberAfter(line, "RSSI", info.rssi);
    numberAfter(line, "SNR", info.snr);
    numberAfter(line, "DMODM", info.margin);       // link check answer, if reported
    numberAfter(line, "GWN", info.gateways);
    return true;
  }

  // ST format: values separated by colons, "+EVT:RX_1:dr:rssi:snr[:margin:gw]".
  int vals[6];
  int count = 0;
  int i = line.indexOf(':', p + 5);                // first colon after "RX_1"
  while (i >= 0 && count < 6) {
    int j = line.indexOf(':', i + 1);
    String f = (j < 0) ? line.substring(i + 1) : line.substring(i + 1, j);
    f.trim();
    vals[count++] = f.toInt();
    i = j;
  }
  if (count >= 3) { info.dr = vals[0]; info.rssi = vals[1]; info.snr = vals[2]; }
  if (count >= 5) { info.margin = vals[3]; info.gateways = vals[4]; }
  return count >= 3;
}
