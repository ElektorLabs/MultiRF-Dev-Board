/*
  02 – Encrypted Transmitter
  Encrypts the payload with a 128-bit key using XTEA-CTR before transmitting.
  The receiver must have the identical key to decrypt.

  Both devices MUST share the same KEY[], frequency and radio settings.
  See Transmitter.ino in 01_BasicRxTx for wiring notes.
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

/* 128-bit shared secret — must match the receiver exactly */
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

  Serial.println(F("Encrypted Transmitter ready"));
}

static uint8_t count = 0;

void loop() {
  uint8_t buf[32];
  snprintf((char *)buf, sizeof(buf), "Secret msg #%u", count++);
  uint8_t len = strlen((char *)buf);

  RadioExt::encrypt(buf, len, KEY);          /* encrypt in-place */
  radio.transmit(buf, len);

  Serial.println(F("Encrypted packet sent"));
  delay(2000);
}
