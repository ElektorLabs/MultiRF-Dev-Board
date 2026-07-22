/*
  01 – Basic Receiver  (non-blocking, interrupt-driven)
  GDO0 fires when a packet arrives; the ISR sets a flag.
  The main loop reads the packet only when the flag is set —
  no blocking wait, the MCU is free between packets.

  CHANNEL must match the transmitter.
  GDO0 must be on an interrupt-capable pin (Uno/Nano: 2 or 3).
  See Transmitter.ino for full wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2   /* interrupt-capable pin */

#define CHANNEL      0
#define BASE_FREQ  433.92f   // MHz

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

volatile bool pktReady = false;
void onPacket() { pktReady = true; }   /* ISR — keep minimal */

void setup() {
  Serial.begin(115200);

  while (radio.begin(MOD_2FSK, BASE_FREQ, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setChannel(CHANNEL);

  radio.receiveCallback(onPacket);   /* flush FIFO, enter RX, attach interrupt */

  Serial.print(F("CC1101 ready — Receiver | ch"));
  Serial.print(CHANNEL); Serial.print(F("  "));
  Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz  (waiting…)"));
}

void loop() {
  if (!pktReady) {
    /* MCU free — read sensors, update display, etc. */
    return;
  }
  pktReady = false;

  uint8_t buf[62];
  size_t  len = 0;
  Status  s   = radio.readPacket(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    buf[len] = '\0';
    Serial.print(F("RX ch")); Serial.print(CHANNEL);
    Serial.print(F(": ")); Serial.print((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.lastRSSI()); Serial.print(F(" dBm"));
    Serial.print(F("  LQI ")); Serial.println(radio.lastLQI());
  } else if (s == STATUS_CRC_MISMATCH) {
    Serial.println(F("CRC error – packet corrupted"));
  }

  radio.receiveCallback(onPacket);   /* re-arm for next packet */
}
