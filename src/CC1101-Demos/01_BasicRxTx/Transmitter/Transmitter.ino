/*
  01 – Basic Transmitter
  Sends a numbered string every second and prints the result.

  Wiring (same for all examples):
    Arduino Uno/Nano  CS=10  GDO0=2   (SPI: SCK=13 MISO=12 MOSI=11)
    Arduino Mega      CS=53  GDO0=2   (SPI: SCK=52 MISO=50 MOSI=51)
    ESP32             CS=5   GDO0=4   (SPI: SCK=18 MISO=19 MOSI=23)
*/

#include <cc1101ext.h>
using namespace CC1101;

// --- Pin Configuration ---
#define PIN_CS  5   // change to 53 for Mega, 5 for ESP32
#define PIN_GD0  4   // change to 4 for ESP32

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
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);

  Serial.println(F("CC1101 ready — Transmitter"));
}

static uint8_t counter = 0;

void loop() {
  char msg[32];
  snprintf(msg, sizeof(msg), "Hello #%u", counter++);

  Status s = radio.transmit((uint8_t *)msg, strlen(msg));

  Serial.print(F("TX: ")); Serial.print(msg);
  Serial.println(s == STATUS_OK ? F(" [OK]") : F(" [FAIL]"));

  delay(1000);
}
