/*
  01 – Basic Transmitter  (non-blocking)
  Sends a numbered string every TX_INTERVAL ms using a millis() timer.
  The MCU is free to do other work between transmissions.

  Wiring (same for all examples):
    Arduino Uno/Nano  CS=10  GDO0=2   (SPI: SCK=13 MISO=12 MOSI=11)
    Arduino Mega      CS=53  GDO0=2   (SPI: SCK=52 MISO=50 MOSI=51)
    ESP32             CS=5   GDO0=4   (SPI: SCK=18 MISO=19 MOSI=23)
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   2
#define PIN_GD0   1

#define CHANNEL      12
#define BASE_FREQ  433.92f   // MHz

#define TX_INTERVAL  3000    // ms between transmissions

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/7, /*miso*/8, /*mosi*/9, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

static uint8_t  counter = 0;
static uint32_t lastTx  = 0;

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

  Serial.print(F("CC1101 ready — Transmitter | ch"));
  Serial.print(CHANNEL); Serial.print(F("  "));
  Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz"));
}

void loop() {
  /* ---- Transmit on interval ---- */
  if (millis() - lastTx >= TX_INTERVAL) {
    lastTx = millis();

    char msg[32];
    snprintf(msg, sizeof(msg), "Hello #%u", counter++);
    Status s = radio.transmit((uint8_t *)msg, strlen(msg));

    Serial.print(F("TX ch")); Serial.print(CHANNEL);
    Serial.print(F(": ")); Serial.print(msg);
    Serial.println(s == STATUS_OK ? F("  [OK]") : F("  [FAIL]"));
  }

  /* ---- Other tasks can go here ---- */
}
