/*
  06 – Low-Power Transmitter
  Transmits a sensor reading, powers down the CC1101, then sleeps.
  After the sleep period the radio is fully re-initialised and the cycle repeats.

  Measured power states (E07-M1101D-SMA @ 3.3 V):
    TX (0 dBm) ≈ 30 mA
    RX          ≈ 14 mA
    Sleep       ≈  0.5 µA  ← CC1101 SPWD mode

  For production: replace delay(SLEEP_MS) with MCU deep-sleep (AVR power-down,
  ESP32 deep sleep, STM32 STOP2, etc.) to achieve true µA system current.

  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS    10
#define PIN_GD0    2
#define SLEEP_MS  30000UL   /* sleep 30 s between transmissions */

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

static uint16_t seq = 0;

/* Centralise radio setup so we can call it after every wake */
void radioInit() {
  while (radio.begin(MOD_2FSK, 433.92, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
}

void setup() {
  Serial.begin(115200);
  radioInit();
  Serial.println(F("Low-Power Transmitter ready"));
}

void loop() {
  /* --- Transmit --- */
  char msg[24];
  snprintf(msg, sizeof(msg), "LP:%u", seq++);
  Status s = radio.transmit((uint8_t *)msg, strlen(msg));
  Serial.print(F("TX ")); Serial.print(msg);
  Serial.println(s == STATUS_OK ? F(" [OK]") : F(" [FAIL]"));

  /* --- Sleep radio --- */
  radio.sleep();
  Serial.print(F("Radio sleeping for ")); Serial.print(SLEEP_MS / 1000);
  Serial.println(F(" s…"));
  Serial.flush();

  /* Replace this with MCU deep-sleep for maximum power saving */
  delay(SLEEP_MS);

  /* --- Wake radio --- */
  radioInit();   /* full re-init after power-down */
}
