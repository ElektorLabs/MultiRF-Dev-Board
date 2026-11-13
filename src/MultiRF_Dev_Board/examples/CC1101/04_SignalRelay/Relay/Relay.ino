/*
  04 – Signal Relay Node  (non-blocking)
  Receives any packet and immediately re-transmits it.
  readPacket() returns STATUS_TIMEOUT instantly when nothing has arrived —
  the MCU is never blocked waiting for a packet.

  Deployment:
    [Original TX]  ──433 MHz──►  [This relay node]  ──433 MHz──►  [Final RX]

  Use the same CHANNEL on all three nodes.
  Test with 01_BasicRxTx/Transmitter as source and 01_BasicRxTx/Receiver as sink.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2

#define CHANNEL      0
#define BASE_FREQ  433.92f

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

static uint32_t totalRelayed = 0;

void setup() {
  Serial.begin(115200);

  while (radio.begin(MOD_2FSK, BASE_FREQ, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(10);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setChannel(CHANNEL);

  radio.beginReceive();   /* enter RX once; readPacket() will re-enter after each packet */

  Serial.print(F("Relay active | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz"));
  Serial.println(F("#    Bytes  RSSI       Hex (raw bytes)                    |ASCII|"));
  Serial.println(F("──   ─────  ────       ───────────────                    ───────"));
}

void loop() {
  uint8_t buf[64];
  size_t  len = 0;

  /* readPacket() returns STATUS_TIMEOUT immediately if nothing arrived */
  Status s = radio.readPacket(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    buf[len] = '\0';
    radio.transmit(buf, len);          /* re-transmit the packet */
    radio.beginReceive();              /* re-enter RX for next packet */

    totalRelayed++;
    Serial.print(totalRelayed); Serial.print(F("    "));
    if (len < 10) Serial.print(' ');
    Serial.print(len);          Serial.print(F("     "));
    Serial.print(radio.lastRSSI()); Serial.print(F(" dBm  ch"));
    Serial.print(CHANNEL); Serial.print(F(": "));
    for (size_t i = 0; i < len; i++) {        // hex — works for any data
      if (buf[i] < 0x10) Serial.print('0');
      Serial.print(buf[i], HEX); Serial.print(' ');
    }
    Serial.print(F(" |"));
    for (size_t i = 0; i < len; i++)           // ASCII best-effort
      Serial.print(buf[i] >= 0x20 && buf[i] < 0x7F ? (char)buf[i] : '.');
    Serial.println('|');

  } else if (s == STATUS_TIMEOUT) {
    /* Nothing arrived — MCU free for other tasks */
  } else {
    Serial.print(F("RX error: ")); Serial.println(s);
    radio.beginReceive();
  }
}
