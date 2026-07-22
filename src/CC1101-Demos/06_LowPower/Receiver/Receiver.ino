/*
  06 – Low-Power Receiver  (non-blocking, interrupt-driven)
  GDO0 fires when a packet arrives; the ISR sets a flag.
  Between packets the MCU loop() returns immediately — no spinning, no delay().
  Add MCU light-sleep in the "MCU free" section for maximum power saving.

  GDO0 must be on an interrupt-capable pin (Uno/Nano: 2 or 3).
  CHANNEL must match the transmitter.
  See 01_BasicRxTx/Transmitter for full wiring notes.
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

volatile bool pktReady = false;
void onPacket() { pktReady = true; }   /* ISR — sets flag only */

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

  radio.receiveCallback(onPacket);   /* configure GDO0, flush FIFO, enter RX */

  Serial.print(F("Low-Power RX | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz  (interrupt-driven)"));
}

void loop() {
  if (!pktReady) {
    /* MCU free — add light-sleep here (e.g. set_sleep_mode(SLEEP_MODE_IDLE)) */
    return;
  }
  pktReady = false;

  uint8_t buf[62];
  size_t  len = 0;
  Status  s   = radio.readPacket(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    buf[len] = '\0';
    Serial.print(F("RX ch")); Serial.print(CHANNEL);
    Serial.print(F(": ")); Serial.print((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.lastRSSI()); Serial.println(F(" dBm"));
  }

  radio.receiveCallback(onPacket);   /* re-arm interrupt + re-enter RX */
}
