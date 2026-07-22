/*
  06 – Low-Power Transmitter  (non-blocking)
  Transmits once, powers down the CC1101, then idles until the next interval.
  No delay() — timing is millis()-based so other tasks keep running.

  The comment "replace with MCU deep-sleep" marks where you would add:
    • AVR: LowPower.powerDown(SLEEP_8S, ADC_OFF, BOD_OFF)  (LowPower library)
    • ESP32: esp_sleep_enable_timer_wakeup(30e6); esp_deep_sleep_start()
    • STM32: HAL_PWR_EnterSTOPMode(...)
  That drops full-system current from ~20 mA idle to < 10 µA.

  CHANNEL must match the receiver.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   2
#define PIN_GD0   1

#define CHANNEL        0
#define BASE_FREQ    433.92f
#define TX_INTERVAL  3000UL   /* ms between transmissions */

RadioExt radio(PIN_CS, /*clk*/7, /*miso*/8, /*mosi*/9, PIN_GD0);

static uint16_t seq    = 0;
static uint32_t lastTx = 0;
static bool     asleep = false;

void radioInit() {
  while (radio.begin(MOD_2FSK, BASE_FREQ, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setChannel(CHANNEL);
}

void setup() {
  Serial.begin(115200);
  radioInit();

  Serial.print(F("Low-Power TX | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz"));
}

void loop() {
  uint32_t now = millis();

  /* ---- Wake: re-init radio after sleep period ---- */
  if (asleep && now - lastTx >= TX_INTERVAL) {
    radioInit();               /* full re-init required after SPWD */
    asleep = false;
  }

  /* ---- Transmit + sleep ---- */
  if (!asleep && now - lastTx >= TX_INTERVAL) {
    lastTx = now;

    char msg[24];
    snprintf(msg, sizeof(msg), "LP:%u", seq++);
    Status s = radio.transmit((uint8_t *)msg, strlen(msg));

    Serial.print(F("TX ch")); Serial.print(CHANNEL);
    Serial.print(F(": ")); Serial.print(msg);
    Serial.println(s == STATUS_OK ? F("  [OK]") : F("  [FAIL]"));

    radio.sleep();             /* CC1101 → SPWD (~0.5 µA) */
    Serial.print(F("Radio sleeping for ")); Serial.print(TX_INTERVAL / 1000); Serial.println(F(" s"));

    /* ← Replace with MCU deep-sleep here for µA system current */

    asleep = true;
  }

  /* ---- Other tasks run here while waiting ---- */
}
