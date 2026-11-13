/*
  02 – Encrypted Receiver  (non-blocking, interrupt-driven)
  Receives ciphertext, decrypts it with XTEA-CTR, prints the plaintext.
  KEY[] and CHANNEL must match the transmitter exactly.
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

static const uint8_t KEY[16] = {
  0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6,
  0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C
};

volatile bool pktReady = false;
void onPacket() { pktReady = true; }

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

  radio.receiveCallback(onPacket);

  Serial.print(F("Encrypted RX | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz  (waiting…)"));
}

void loop() {
  if (!pktReady) return;
  pktReady = false;

  uint8_t buf[64];
  size_t  len = 0;
  Status  s   = radio.readPacket(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    RadioExt::decrypt(buf, len, KEY);
    buf[len] = '\0';
    Serial.print(F("RX ch")); Serial.print(CHANNEL);
    Serial.print(F(": ")); Serial.print((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.lastRSSI()); Serial.println(F(" dBm"));
  }

  radio.receiveCallback(onPacket);
}
