/*
  01 – Basic Receiver
  Waits for a packet and prints payload, RSSI and LQI.
  See Transmitter.ino for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS  2
#define PIN_GD0  1

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/7, /*miso*/8, /*mosi*/9, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

void setup() {
  Serial.begin(115200);

  while (radio.begin(MOD_2FSK, 433.92, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);

  Serial.println(F("CC1101 ready — Receiver (waiting…)"));
}

void loop() {
  uint8_t buf[62];
  size_t  len = 0;

  Status s = radio.receive(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    buf[len] = '\0';
    Serial.print(F("RX: ")); Serial.print((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.getRSSI()); Serial.print(F(" dBm"));
    Serial.print(F("  LQI "));  Serial.println(radio.getLQI());
  } else if (s == STATUS_CRC_MISMATCH) {
    Serial.println(F("CRC error"));
  }
}
