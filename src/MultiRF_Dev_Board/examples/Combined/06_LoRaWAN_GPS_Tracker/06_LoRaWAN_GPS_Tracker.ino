/*
  ============================================================================
  MultiRF Dev Board - Combined Example 6: LoRaWAN GPS Tracker
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Every REPORT_INTERVAL_MS the tracker waits for a good GNSS fix, packs it
  into 11 bytes and sends it as an unconfirmed LoRaWAN uplink:

      bytes 0-3   latitude,  int32, units of 1e-7 degrees, big-endian
      bytes 4-7   longitude, int32, units of 1e-7 degrees, big-endian
      bytes 8-9   altitude,  int16, metres
      byte  10    satellites in use

  Binary instead of text: "ALT=504.2,..." style text would need about 40
  bytes; at SF12 the 11 bytes go out in 1.48 s instead of 2.47 s.

  Paste ttn_payload_formatter.js into the TTN application, so the console
  and map integrations show latitude, longitude and altitude.

  Power: after a successful fix the GNSS receiver is put into deep sleep
  (CFG-SLEEP). It keeps its satellite data and wakes by itself
  GPS_WAKE_LEAD_MS before the next report, so each fix is a hot start.
  If no fix was found, the receiver stays on, so it can finish collecting
  satellite data instead of starting over. The E77-900M22S sleeps on its
  own between AT commands.

  Flow (one cycle):
    join if needed -> wait for a good fix -> send 11 bytes -> listen 4 s
    -> GNSS deep sleep (only after a fix) -> wait for the next cycle

  Library: TinyGPSPlus. Serial monitor: 115200 baud.
  ============================================================================
*/
#include <TinyGPSPlus.h>
#include "config.h"
#include <MultiRFDevBoard.h>

TinyGPSPlus   gps;             // NMEA decoder
GnssBinParser bin;             // picks binary replies (ACK/NAK) out of the stream
bool          joined = false;

// Read both UARTs. Every wait in this sketch goes through service(), so the
// GNSS receive buffer never overflows while the sketch is busy elsewhere.
void service() {
  while (gpsSerial.available()) {
    uint8_t c = gpsSerial.read();
    if (bin.consume(c)) {
      if (bin.ready) { gnssPrintFrame(bin); bin.ready = false; }
      continue;
    }
    gps.encode((char)c);
  }
  String line;
  if (e77ReadLine(line)) { Serial.print("[E77] "); Serial.println(line); }
}

// A delay() that keeps servicing both UARTs.
void waitMs(uint32_t ms) {
  uint32_t t0 = millis();
  while (millis() - t0 < ms) { service(); delay(2); }
}

// Wait for a fresh fix that passes the quality filter:
// new position, less than 1.5 s old, HDOP <= MAX_HDOP, >= MIN_SATS satellites.
bool waitForFix(uint32_t timeoutMs) {
  uint32_t t0 = millis();
  while (millis() - t0 < timeoutMs) {
    service();
    if (gps.location.isUpdated() && gps.location.isValid() && gps.location.age() < 1500 &&
        gps.hdop.isValid() && gps.hdop.hdop() <= MAX_HDOP &&
        gps.satellites.value() >= MIN_SATS) {
      Serial.printf("[GNSS] Fix after %.1f s: %.6f, %.6f, %lu sats, HDOP %.2f\n",
                    (millis() - t0) / 1000.0, gps.location.lat(), gps.location.lng(),
                    (unsigned long)gps.satellites.value(), gps.hdop.hdop());
      return true;
    }
  }
  Serial.println("[GNSS] No usable fix within the timeout");
  return false;
}

// Pack the current fix into 11 bytes (big-endian) and send it.
void sendFix() {
  int32_t lat  = lround(gps.location.lat() * 1e7);
  int32_t lon  = lround(gps.location.lng() * 1e7);
  int16_t alt  = (int16_t)lround(gps.altitude.meters());
  uint8_t sats = (uint8_t)min((uint32_t)255, (uint32_t)gps.satellites.value());

  uint8_t b[11];
  b[0] = lat >> 24; b[1] = lat >> 16; b[2] = lat >> 8; b[3] = lat;   // latitude
  b[4] = lon >> 24; b[5] = lon >> 16; b[6] = lon >> 8; b[7] = lon;   // longitude
  b[8] = alt >> 8;  b[9] = alt;                                      // altitude
  b[10] = sats;                                                      // satellites

  switch (e77Send(TRACKER_PORT, b, sizeof b)) {    // unconfirmed: no downlink needed
    case E77_SENT:       Serial.println("[App] Position sent"); break;
    case E77_DUTYCYCLE:  Serial.println("[App] Duty cycle limit, skipped"); break;
    case E77_NOT_JOINED: Serial.println("[App] Not joined, rejoining"); joined = e77Join(); break;
    default:             Serial.println("[App] Send failed"); break;
  }
  waitMs(4000);                                   // let RX1/RX2 reports arrive and print
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nMultiRF Dev Board - Example 6: LoRaWAN GPS tracker");

  gnssBegin();                                   // UART2, 115200 baud, IO16/IO15
  e77Begin();                                    // UART1, 9600 baud, IO48/IO47
  delay(500);

  // LoRaWAN: region, keys, join.
  atCmd("AT");
  e77EnsureRegion(LORAWAN_REGION);
  e77SetKeys(LORAWAN_DEVEUI, LORAWAN_APPEUI, LORAWAN_APPKEY);
  joined = e77Join();

  // A moving node: switch ADR off and fix the data rate.
  atCmd("AT+CADR=0");
  char cmd[24];
  snprintf(cmd, sizeof cmd, "AT+CDATARATE=%d", TRACKER_DATARATE);
  atCmd(cmd);
  atCmd("AT+CTXP=0");                            // maximum power allowed in the region

  // The tracker only needs GGA and RMC; switching GSV/GSA off cuts UART traffic.
  gnssSetNmeaRate(NMEA_GSV, 0);
  gnssSetNmeaRate(NMEA_GSA, 0);
}

void loop() {
  uint32_t cycleStart = millis();

  if (!joined) joined = e77Join();

  bool haveFix = waitForFix(FIX_TIMEOUT_MS);
  if (haveFix && joined) sendFix();

  // Sleep the receiver until GPS_WAKE_LEAD_MS before the next report, but
  // only after a fix: without one it must stay on to collect satellite data.
  uint32_t elapsed = millis() - cycleStart;
  if (haveFix && REPORT_INTERVAL_MS > elapsed + GPS_WAKE_LEAD_MS + 10000UL) {
    uint32_t sleepMs = REPORT_INTERVAL_MS - elapsed - GPS_WAKE_LEAD_MS;
    Serial.printf("[GNSS] Deep sleep for %lu s\n", (unsigned long)(sleepMs / 1000));
    gnssSleep(sleepMs, 1);
  }

  // Wait for the rest of the interval, still servicing both UARTs.
  elapsed = millis() - cycleStart;
  if (elapsed < REPORT_INTERVAL_MS) waitMs(REPORT_INTERVAL_MS - elapsed);
}
