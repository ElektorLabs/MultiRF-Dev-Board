/*
  02 – Encrypted Receiver
  Receives an encrypted packet, decrypts it, and prints the plaintext.
  See Transmitter.ino for wiring notes.
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

/* Must be identical to the transmitter's KEY */
static const uint8_t KEY[16] = {
  0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

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

  Serial.println(F("Encrypted Receiver ready (waiting…)"));
}

void loop() {
  uint8_t buf[64];
  size_t  len = 0;

  if (radio.receive(buf, sizeof(buf) - 1, &len) == STATUS_OK) {
    RadioExt::decrypt(buf, len, KEY);        /* decrypt in-place */
    buf[len] = '\0';
    Serial.print(F("Decrypted: ")); Serial.println((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
  }
}
