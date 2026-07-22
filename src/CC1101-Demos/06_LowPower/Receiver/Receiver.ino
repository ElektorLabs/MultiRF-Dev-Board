/*
  06 – Low-Power Receiver
  Uses interrupt-driven (GDO0) packet detection so the MCU does not spin in a
  tight polling loop.  The MCU can execute other tasks or enter a light sleep
  between interrupts; only the CC1101 stays active in RX mode.

  GDO0 is configured by receiveCallback() to assert when the RX FIFO has data.
  The ISR sets a flag; the main loop reads and prints the packet.

  PIN_GD0 must be connected to a hardware-interrupt-capable pin:
    Arduino Uno/Nano  pin 2 or 3
    Arduino Mega      pin 2, 3, 18, 19, 20, 21
    ESP32             any GPIO

  See 01_BasicRxTx/Transmitter for full wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2    /* interrupt-capable pin */

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

volatile bool pktReady = false;

void onPacket() { pktReady = true; }   /* called from ISR */

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

  /* Attach interrupt — also flushes RX FIFO and enters RX mode */
  radio.receiveCallback(onPacket);

  Serial.println(F("Low-Power Receiver ready (interrupt-driven)"));
}

void loop() {
  if (!pktReady) {
    /* MCU could enter light sleep here */
    return;
  }
  pktReady = false;

  uint8_t buf[62];
  size_t  len = 0;
  Status  s   = radio.receive(buf, sizeof(buf) - 1, &len);

  if (s == STATUS_OK) {
    buf[len] = '\0';
    Serial.print(F("RX: ")); Serial.print((char *)buf);
    Serial.print(F("  RSSI ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
  }

  /* Re-arm interrupt for next packet */
  radio.receiveCallback(onPacket);
}
