/*
  03 — Channel Scanner  (non-blocking | Flipper Zero-style)
  ─────────────────────────────────────────────────────────
  SCAN    sweeps ch0–ch(NUM_CHANNELS-1), live RSSI bar graph.
          Samples RSSI multiple times per dwell to catch short bursts.
          Auto-saves any complete packet detected during the sweep.
  CAPTURE locks on one channel (interrupt-driven), saves every packet.
  REPLAY  re-transmits any saved packet on its original channel.

  Serial commands (115200 baud):
    s      toggle SCAN ↔ CAPTURE (auto-selects most-active channel)
    l      list all saved packets (hex + ASCII)
    p0–p7  replay saved packet 0–7
    c      clear all saved packets
    0–9    set capture channel (CAPTURE mode only)

  No delay() anywhere — timing is millis()-based throughout.
  Wiring: see 01_BasicRxTx/Transmitter for pin assignments.
*/

#include <cc1101ext.h>
using namespace CC1101;

// ── Pins ─────────────────────────────────────────────────────────────────────
#define PIN_CS         5
#define PIN_GD0         2

// ── Scan config ──────────────────────────────────────────────────────────────
#define BASE_FREQ     433.92f   // ch0 centre frequency (MHz)
#define CH_SPACING      0.200f  // MHz between adjacent channels
#define NUM_CHANNELS       10   // ch0 – ch9
#define DWELL_MS           80   // ms per channel  (longer = catches more bursts)
#define POLL_MS            10   // ms between RSSI/packet polls during dwell

// Bar graph scale — signals below RSSI_FLOOR show 0 bars, above RSSI_CEIL show full
#define RSSI_FLOOR      (-100)  // dBm (noise floor)
#define RSSI_CEIL        (-40)  // dBm (strong signal)
#define BAR_WIDTH           16  // bar characters

// ── RAM budget ────────────────────────────────────────────────────────────────
#ifdef __AVR__
  #define MAX_RECORDS   4
  #define PKT_MAX      32
#else
  #define MAX_RECORDS   8
  #define PKT_MAX      48
#endif

// ── Saved packet record ───────────────────────────────────────────────────────
struct Record {
  uint32_t ts;           // millis() at capture
  uint8_t  ch;
  int8_t   rssi;
  uint8_t  len;
  uint8_t  data[PKT_MAX];
};

// ── Radio ─────────────────────────────────────────────────────────────────────
#ifdef ESP32
RadioExt radio(PIN_CS, 18, 19, 23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

// ── Mode ──────────────────────────────────────────────────────────────────────
enum Mode { SCAN, CAPTURE };
static Mode mode = SCAN;

// Scan state
static uint8_t  scanCh   = 0;
static uint32_t dwellEnd = 0;
static bool     dwelling = false;
static uint32_t sweepNum = 0;
static int8_t   maxRssi[NUM_CHANNELS];   // peak RSSI seen per channel this sweep
static uint8_t  pktsSeen[NUM_CHANNELS];  // packets decoded per channel

// Capture state
static uint8_t capCh    = 0;
volatile bool  pktReady = false;

// Record store
static Record  recs[MAX_RECORDS];
static uint8_t recCount = 0;

void onPkt() { pktReady = true; }

// ── Print helpers ─────────────────────────────────────────────────────────────

void printRow(uint8_t ch, int8_t rssi, uint8_t pkts) {
  if (ch < 10) Serial.print(' ');
  Serial.print(F("ch")); Serial.print(ch);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + ch * CH_SPACING, 2); Serial.print(F(" MHz  ["));

  // Bar: map RSSI_FLOOR..RSSI_CEIL → 0..BAR_WIDTH
  int f = (int)((long)(rssi - RSSI_FLOOR) * BAR_WIDTH / (RSSI_CEIL - RSSI_FLOOR));
  if (f < 0) f = 0;
  if (f > BAR_WIDTH) f = BAR_WIDTH;
  for (int i = 0; i < f; i++) Serial.print('#');
  for (int i = f; i < BAR_WIDTH; i++) Serial.print('.');
  Serial.print(F("]  "));

  if (rssi > -10) Serial.print(' ');
  Serial.print(rssi); Serial.print(F(" dBm"));
  if (pkts) { Serial.print(F("  *** PKT x")); Serial.print(pkts); }
  Serial.println();
}

void printRecord(uint8_t i) {
  const Record &r = recs[i];
  Serial.print(F("#")); Serial.print(i);
  // Timestamp as  s.mmm
  Serial.print(F("  ")); Serial.print(r.ts / 1000); Serial.print('.');
  uint16_t ms = r.ts % 1000;
  if (ms < 100) Serial.print('0');
  if (ms < 10)  Serial.print('0');
  Serial.print(ms); Serial.print(F("s"));
  Serial.print(F("  ch")); Serial.print(r.ch);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + r.ch * CH_SPACING, 2); Serial.print(F(" MHz"));
  Serial.print(F("  ")); Serial.print(r.rssi); Serial.print(F(" dBm  "));
  Serial.print(r.len); Serial.print(F("B: "));
  for (uint8_t j = 0; j < r.len; j++) {
    if (r.data[j] < 0x10) Serial.print('0');
    Serial.print(r.data[j], HEX); Serial.print(' ');
  }
  Serial.print(F("|"));
  for (uint8_t j = 0; j < r.len; j++)
    Serial.print(r.data[j] >= 0x20 && r.data[j] < 0x7F ? (char)r.data[j] : '.');
  Serial.println('|');
}

void saveRecord(uint8_t ch, int8_t rssi, const uint8_t *data, uint8_t len) {
  if (recCount >= MAX_RECORDS) {
    Serial.println(F("[Buffer full — 'c' to clear]")); return;
  }
  Record &r = recs[recCount];
  r.ts   = millis(); r.ch = ch; r.rssi = rssi;
  r.len  = len < PKT_MAX ? len : PKT_MAX;
  memcpy(r.data, data, r.len);
  Serial.print(F("[Saved #")); Serial.print(recCount++);
  Serial.print(F("  ch")); Serial.print(ch); Serial.println(F("]"));
}

void replayRecord(uint8_t idx) {
  if (idx >= recCount) { Serial.print(F("No record #")); Serial.println(idx); return; }
  const Record &r = recs[idx];
  Serial.print(F("[REPLAY #")); Serial.print(idx);
  Serial.print(F("  ch")); Serial.print(r.ch); Serial.print(F("  "));
  Serial.print(BASE_FREQ + r.ch * CH_SPACING, 2); Serial.print(F(" MHz  "));
  Serial.print(r.len); Serial.print(F("B]  "));
  radio.idle(); radio.setChannel(r.ch);
  Serial.println(radio.transmitRetry((uint8_t*)r.data, r.len) == STATUS_OK ? F("OK") : F("FAIL"));
  radio.idle();
  // Return to current mode
  if (mode == CAPTURE) {
    radio.setChannel(capCh); pktReady = false; radio.receiveCallback(onPkt);
  } else if (dwelling) {
    radio.setChannel(scanCh); radio.beginReceive();  // resume scan RX
  }
}

// ── Serial command handler ────────────────────────────────────────────────────
void handleSerial() {
  if (!Serial.available()) return;
  char c = (char)Serial.read();

  if (c == 'p') {
    uint32_t t = millis();
    while (!Serial.available() && millis() - t < 100) {}
    if (!Serial.available()) return;
    char d = (char)Serial.read();
    while (Serial.available()) Serial.read();
    if (d >= '0' && d <= '7') replayRecord(d - '0');
    return;
  }
  while (Serial.available()) Serial.read();

  switch (c) {
    case 's':
      if (mode == SCAN) {
        // Switch to CAPTURE on channel with strongest signal
        capCh = 0;
        for (uint8_t i = 1; i < NUM_CHANNELS; i++)
          if (maxRssi[i] > maxRssi[capCh]) capCh = i;
        mode = CAPTURE;
        radio.idle(); radio.setChannel(capCh);
        pktReady = false; radio.receiveCallback(onPkt);
        Serial.print(F("\n[CAPTURE ch")); Serial.print(capCh);
        Serial.print(F("  ")); Serial.print(BASE_FREQ + capCh * CH_SPACING, 2);
        Serial.println(F(" MHz | 's'=scan  '0'-'9'=ch  'l'=list  'p0-7'=replay  'c'=clear]"));
      } else {
        mode = SCAN; scanCh = 0; dwelling = false; radio.idle();
        for (uint8_t i = 0; i < NUM_CHANNELS; i++) { maxRssi[i] = RSSI_FLOOR; pktsSeen[i] = 0; }
        Serial.println(F("\n[SCAN MODE]"));
      }
      break;

    case 'l':
      if (!recCount) Serial.println(F("No records."));
      else {
        Serial.print(recCount); Serial.println(F(" record(s):"));
        for (uint8_t i = 0; i < recCount; i++) printRecord(i);
      }
      break;

    case 'c':
      recCount = 0; Serial.println(F("Records cleared."));
      break;

    default:
      if (mode == CAPTURE && c >= '0' && c <= '9') {
        capCh = c - '0';
        if (capCh < NUM_CHANNELS) {
          radio.idle(); radio.setChannel(capCh); pktReady = false; radio.receiveCallback(onPkt);
          Serial.print(F("[ch")); Serial.print(capCh); Serial.print(F("  "));
          Serial.print(BASE_FREQ + capCh * CH_SPACING, 2); Serial.println(F(" MHz]"));
        }
      }
      break;
  }
}

// ── Scan mode loop ────────────────────────────────────────────────────────────
void loopScan() {
  static uint32_t nextPoll = 0;

  if (!dwelling) {
    radio.setChannel(scanCh);
    radio.beginReceive();         // flush FIFO, enter RX
    dwellEnd = millis() + DWELL_MS;
    nextPoll = millis();          // begin polling immediately
    dwelling = true;
    return;
  }

  if (millis() < dwellEnd) {
    if (millis() >= nextPoll) {
      nextPoll = millis() + POLL_MS;

      // Peak RSSI — catches any RF energy, not just packets matching our format
      int8_t r = radio.sampleRssi();
      if (r > maxRssi[scanCh]) maxRssi[scanCh] = r;

      // Packet check — returns instantly (STATUS_TIMEOUT) when FIFO is empty
      uint8_t buf[PKT_MAX]; size_t len = 0;
      if (radio.readPacket(buf, sizeof(buf), &len) == STATUS_OK) {
        pktsSeen[scanCh]++;
        int8_t pr = radio.lastRSSI();
        if (pr > maxRssi[scanCh]) maxRssi[scanCh] = pr;
        saveRecord(scanCh, pr, buf, (uint8_t)len);
        radio.beginReceive();     // re-arm for rest of dwell
      }
    }
    return;                       // MCU free between polls
  }

  // Dwell done — advance
  radio.idle();
  dwelling = false;

  if (++scanCh >= NUM_CHANNELS) {
    Serial.print(F("\n── Sweep #")); Serial.print(++sweepNum);
    Serial.println(F(" ────────────────────────────────────────────────────────────"));
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) printRow(i, maxRssi[i], pktsSeen[i]);
    Serial.println(F("'s'=capture  'l'=list  'p0-7'=replay  'c'=clear\n"));
    scanCh = 0;
    for (uint8_t i = 0; i < NUM_CHANNELS; i++) { maxRssi[i] = RSSI_FLOOR; pktsSeen[i] = 0; }
  }
}

// ── Capture mode loop ─────────────────────────────────────────────────────────
void loopCapture() {
  if (!pktReady) return;           // interrupt hasn't fired — MCU is free
  pktReady = false;
  uint8_t buf[PKT_MAX]; size_t len = 0;
  if (radio.readPacket(buf, sizeof(buf), &len) == STATUS_OK) {
    saveRecord(capCh, radio.lastRSSI(), buf, (uint8_t)len);
    printRecord(recCount - 1);
  }
  radio.receiveCallback(onPkt);   // re-arm for next packet
}

// ── Setup / main loop ─────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  while (radio.begin(MOD_2FSK, BASE_FREQ, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring")); delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setRxBandwidth(200);      // 200 kHz filter — wide enough to catch nearby signals

  for (uint8_t i = 0; i < NUM_CHANNELS; i++) { maxRssi[i] = RSSI_FLOOR; pktsSeen[i] = 0; }

  Serial.println(F("=== CC1101 Channel Scanner (Flipper Zero-style) ==="));
  Serial.print(F("ch0–ch")); Serial.print(NUM_CHANNELS - 1);
  Serial.print(F("  (")); Serial.print(BASE_FREQ, 2);
  Serial.print(F(" – ")); Serial.print(BASE_FREQ + (NUM_CHANNELS - 1) * CH_SPACING, 2); Serial.println(F(" MHz)"));
  Serial.println(F("'s'=capture  'l'=list  'p0-7'=replay  'c'=clear\n"));
}

void loop() {
  handleSerial();
  if (mode == SCAN) loopScan();
  else              loopCapture();
}
