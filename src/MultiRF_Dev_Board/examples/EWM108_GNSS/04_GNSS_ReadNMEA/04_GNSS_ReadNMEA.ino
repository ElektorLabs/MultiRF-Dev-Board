/*
  ============================================================================
  MultiRF Dev Board - EWM108-GN05 Example 4: Reading the GPS
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Reads the NMEA sentences of the Ebyte EWM108-GN05 and prints UTC time,
  position, altitude, satellites in use, HDOP and speed once per second.

  Library:        TinyGPSPlus by Mikal Hart (Library Manager: "TinyGPSPlus")
  Serial monitor: 115200 baud. Type 'r' to toggle the raw NMEA stream.

  Without a fix the module sends sentences with empty fields, for example
  $GNRMC,,V,...  ('V' = void, no position yet). Take the antenna outside or
  to a window: a cold start needs about 30 s under open sky, longer indoors.
  ============================================================================
*/
#include <TinyGPSPlus.h>
#include <MultiRFDevBoard.h>

TinyGPSPlus gps;               // NMEA decoder
bool raw = false;              // true: show the raw NMEA text instead

void setup() {
  Serial.begin(115200);
  gnssBegin();                 // UART2, 115200 baud, IO16/IO15, 1 KB receive buffer
  delay(1500);
  Serial.println("\nMultiRF Dev Board - Example 4: Reading the GPS");
  Serial.println("Type 'r' to toggle raw NMEA output.");
}

void loop() {
  // Feed every received character to the decoder (and echo it in raw mode).
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
    if (raw) Serial.write(c);
  }
  if (Serial.available() && Serial.read() == 'r') raw = !raw;

  // Print a summary once per second.
  static uint32_t last = 0;
  if (raw || millis() - last < 1000) return;
  last = millis();

  // TinyGPSPlus keeps the last position after the fix is lost, so check its
  // age as well: a stale position is worse than no position.
  if (gps.location.isValid() && gps.location.age() < 2000) {
    Serial.printf("%02d:%02d:%02d UTC  %.6f, %.6f  alt %.1f m  sats %lu  HDOP %.2f  %.1f km/h\n",
                  gps.time.hour(), gps.time.minute(), gps.time.second(),
                  gps.location.lat(), gps.location.lng(),
                  gps.altitude.meters(), (unsigned long)gps.satellites.value(),
                  gps.hdop.hdop(), gps.speed.kmph());
  } else {
    // A rising bad-checksum count means the UART buffer is overflowing.
    Serial.printf("No fix yet. Sentences OK %lu, bad checksums %lu, sats in use %lu\n",
                  (unsigned long)gps.passedChecksum(), (unsigned long)gps.failedChecksum(),
                  (unsigned long)gps.satellites.value());
  }
}
