/*
  04 – Signal Relay Node
  Receives any packet and immediately re-transmits it.

  Deployment:
    [Original TX]  ──433 MHz──►  [This relay node]  ──433 MHz──►  [Final RX]

  The relay extends the effective range when the original TX cannot reach the
  final RX directly.  Place the relay midway between the two endpoints.

  Note: TX and RX must be on DIFFERENT sides of the relay — if the original TX
  can hear its own retransmission it may decode duplicate packets.

  Use the same frequency and radio settings on all three nodes.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS  5
#define PIN_GD0  2

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

void setup() {
  Serial.begin(115200);

  while (radio.begin(MOD_2FSK, 433.92, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(10);          /* max power for maximum relay range */
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);

  Serial.println(F("Relay node active (listening…)"));
}

void loop() {
  uint8_t buf[64];
  size_t  relayed = 0;

  Status s = radio.relay(buf, sizeof(buf), &relayed);

  if (s == STATUS_OK) {
    Serial.print(F("Relayed ")); Serial.print(relayed); Serial.println(F(" bytes"));
  } else if (s != STATUS_TIMEOUT) {
    Serial.print(F("Relay error: ")); Serial.println(s);
  }
}
