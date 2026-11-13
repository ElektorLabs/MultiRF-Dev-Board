/*
  02 – Encrypted Transmitter  (non-blocking)
  Encrypts payload with XTEA-CTR (128-bit key) then transmits every 2 s.
  KEY[] and CHANNEL must match the receiver.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2

#define CHANNEL      0
#define BASE_FREQ  433.92f
#define TX_INTERVAL  2000   // ms

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

static const uint8_t KEY[16] = {
  0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

static uint8_t  count  = 0;
static uint32_t lastTx = 0;

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

  Serial.print(F("Encrypted TX | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz"));
}

void loop() {
  if (millis() - lastTx >= TX_INTERVAL) {
    lastTx = millis();

    uint8_t buf[32];
    snprintf((char *)buf, sizeof(buf), "Secret msg #%u", count++);
    uint8_t len = strlen((char *)buf);

    RadioExt::encrypt(buf, len, KEY);
    Status s = radio.transmit(buf, len);

    Serial.print(F("TX ch")); Serial.print(CHANNEL);
    Serial.print(F(": [encrypted ")); Serial.print(len); Serial.print(F(" bytes]"));
    Serial.println(s == STATUS_OK ? F("  [OK]") : F("  [FAIL]"));
  }

  /* Other tasks here */
}
