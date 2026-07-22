/*
  03 – Channel Scanner
  Sweeps 20 consecutive CC1101 channels and displays a live RSSI bar graph
  on the Serial Monitor (115200 baud).

  Each CC1101 channel is offset from the base frequency by:
    channel_freq = base_freq + channel * channel_spacing
  Default channel spacing is ~200 kHz, so channels 0-19 cover ~4 MHz.

  Use this to find a quiet channel before deploying your radio link.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS  5
#define PIN_GD0  2

#define NUM_CHANNELS  20
#define DWELL_MS      20     /* listen time per channel (ms) */

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
  radio.setRxBandwidth(200);   /* wider BW captures more activity */

  Serial.println(F("Channel Scanner ready"));
  Serial.println(F("CH  RSSI(dBm)  Activity"));
}

void loop() {
  int8_t rssi[NUM_CHANNELS];
  radio.scanChannels(0, NUM_CHANNELS, rssi, DWELL_MS);

  Serial.println(F("---"));
  for (uint8_t i = 0; i < NUM_CHANNELS; i++) {
    Serial.print(i < 10 ? F(" ") : F("")); Serial.print(i);
    Serial.print(F("  "));
    if (rssi[i] > -50) Serial.print(F(" "));
    Serial.print(rssi[i]); Serial.print(F("        "));

    int bars = constrain((rssi[i] + 110) / 4, 0, 15);  /* scale −110..−50 → 0..15 */
    for (int b = 0; b < bars; b++) Serial.print('|');
    Serial.println();
  }

  delay(500);
}
