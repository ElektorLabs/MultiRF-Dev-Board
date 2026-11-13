/*
  ============================================================================
  MultiRF Dev Board - E77-900M22S Example 1: Serial Bridge
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Turns the ESP32-S3 into a transparent bridge between the USB console and
  the Ebyte E77-900M22S LoRaWAN module, so you can type AT commands by hand.
  This is the first thing to run: it shows that the module answers, which
  firmware it carries, and lets you set the region.

  Serial monitor: 115200 baud, line ending "Both NL & CR".

  Try these first:
    AT              link test, answers OK
    AT+DEVTYPE=?    module model
    AT+VER=?        firmware and LoRaWAN version
    AT+REGION=?     current region (5 = EU868, 8 = US915, 7 = IN865)
    AT+CDEVEUI=?    the module's DevEUI, to register on The Things Network
    AT+SEND?        shows the exact AT+SEND format of your firmware

  If AT+REGION is rejected but AT+BAND=? answers, the module carries ST's
  reference AT firmware instead of Ebyte's; see ST application note AN5481.
  ============================================================================
*/
#include <MultiRFDevBoard.h>

void setup() {
  Serial.begin(115200);            // USB console
  e77Begin();                      // UART1 to the module: 9600 baud, RX = IO48, TX = IO47
  delay(1500);                     // time to open the serial monitor
  Serial.println();
  Serial.println("E77-900M22S serial bridge ready. Type AT and press Enter.");
}

void loop() {
  // Pass every byte straight through, in both directions.
  while (Serial.available()) lora.write(Serial.read());   // console -> module
  while (lora.available())   Serial.write(lora.read());   // module -> console
}
