/*
  ============================================================================
  MultiRF Dev Board - EWM108-GN05 Example 5: Talking Back to the GPS
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Sends Allystar binary configuration commands to the EWM108-GN05 from a
  menu in the serial monitor, shows the ACK/NAK replies, and measures the
  time to first fix (TTFF) after a cold, warm or hot start.

  Library:        TinyGPSPlus
  Serial monitor: 115200 baud, any line ending.

  Menu:
    1 / 2   GSV sentences off / on
    3 / 4   GSA sentences off / on
    c w h   cold / warm / hot start, then measure TTFF
    z       deep sleep for 10 s (the receiver wakes by itself)
    s       save sentence settings to flash
    f       factory reset of all settings (asks for confirmation)
    r       toggle raw NMEA output
    ?       show this menu

  How the TTFF stopwatch works: after the restart command it waits until
  the GGA sentence reports fix quality 0 (fix lost), then times how long it
  takes until the quality is above 0 again.
  ============================================================================
*/
#include <TinyGPSPlus.h>
#include <MultiRFDevBoard.h>

TinyGPSPlus   gps;                               // NMEA decoder
GnssBinParser bin;                               // picks binary replies out of the stream

// Field 6 of the GGA sentence is the fix quality (0 = no fix). The talker
// is GN when several systems are combined, GP for GPS only.
TinyGPSCustom ggaQualGN(gps, "GNGGA", 6);
TinyGPSCustom ggaQualGP(gps, "GPGGA", 6);

bool raw          = false;                       // show raw NMEA
bool confirmReset = false;                       // waiting for 'y' after 'f'

// TTFF stopwatch states
enum { TTFF_IDLE, TTFF_WAIT_LOSS, TTFF_WAIT_FIX } ttffState = TTFF_IDLE;
uint32_t ttffStart = 0;

// Fix quality from the latest GGA sentence, or -1 if no new GGA arrived.
int fixQuality() {
  if (ggaQualGN.isUpdated()) return atoi(ggaQualGN.value());
  if (ggaQualGP.isUpdated()) return atoi(ggaQualGP.value());
  return -1;
}

// Send a restart command and start the stopwatch.
void startTtff(uint8_t mode, const char* name) {
  Serial.printf("[GNSS] %s start\n", name);
  gnssRestart(mode);
  ttffStart = millis();
  ttffState = TTFF_WAIT_LOSS;
}

void printMenu() {
  Serial.println("\n1/2 GSV off/on   3/4 GSA off/on   c/w/h cold/warm/hot start");
  Serial.println("z sleep 10 s   s save   f factory reset   r raw NMEA   ? menu");
}

void handleKey(char k) {
  // Second key press after 'f': only 'y' confirms the factory reset.
  if (confirmReset) {
    confirmReset = false;
    if (k == 'y') { gnssFactoryReset(); Serial.println("[GNSS] Factory reset sent"); }
    else          Serial.println("[GNSS] Factory reset cancelled");
    return;
  }

  switch (k) {
    case '1': gnssSetNmeaRate(NMEA_GSV, 0); Serial.println("[GNSS] GSV off"); break;
    case '2': gnssSetNmeaRate(NMEA_GSV, 1); Serial.println("[GNSS] GSV on");  break;
    case '3': gnssSetNmeaRate(NMEA_GSA, 0); Serial.println("[GNSS] GSA off"); break;
    case '4': gnssSetNmeaRate(NMEA_GSA, 1); Serial.println("[GNSS] GSA on");  break;
    case 'c': startTtff(GNSS_COLD, "Cold"); break;
    case 'w': startTtff(GNSS_WARM, "Warm"); break;
    case 'h': startTtff(GNSS_HOT,  "Hot");  break;
    case 'z': gnssSleep(10000, 1); Serial.println("[GNSS] Deep sleep for 10 s"); break;
    case 's': gnssSave(0x03); Serial.println("[GNSS] Saving baud rate and sentence settings"); break;
    case 'f': confirmReset = true;
              Serial.println("Factory reset all GNSS settings? Press 'y' to confirm.");
              break;
    case 'r': raw = !raw; break;
    case '?': printMenu(); break;
    default:  break;
  }
}

void setup() {
  Serial.begin(115200);
  gnssBegin();                                   // UART2, 115200 baud, IO16/IO15
  delay(1500);
  Serial.println("\nMultiRF Dev Board - Example 5: GNSS configuration");
  printMenu();
}

void loop() {
  // 1. Read the receiver: binary replies go to the parser, NMEA to TinyGPSPlus.
  while (gpsSerial.available()) {
    uint8_t c = gpsSerial.read();
    if (bin.consume(c)) {
      if (bin.ready) { gnssPrintFrame(bin); bin.ready = false; }
      continue;
    }
    gps.encode((char)c);
    if (raw) Serial.write(c);
  }

  // 2. Menu keys from the serial monitor.
  while (Serial.available()) {
    char k = Serial.read();
    if (k != '\r' && k != '\n') handleKey(k);
  }

  // 3. TTFF stopwatch: wait for the fix to drop, then time its return.
  int q = fixQuality();
  if (q >= 0 && ttffState != TTFF_IDLE) {
    uint32_t t = millis() - ttffStart;
    if (ttffState == TTFF_WAIT_LOSS) {
      if (q == 0) ttffState = TTFF_WAIT_FIX;
      else if (t > 5000) {                      // a hot start may never lose the fix
        Serial.println("[GNSS] Fix never dropped: position kept through the restart");
        ttffState = TTFF_IDLE;
      }
    } else if (ttffState == TTFF_WAIT_FIX && q > 0) {
      Serial.printf("[GNSS] Fix after %.1f s, %lu satellites\n", t / 1000.0,
                    (unsigned long)gps.satellites.value());
      ttffState = TTFF_IDLE;
    }
  }

  // 4. Every 5 s, show the current fix (not while the stopwatch runs).
  static uint32_t last = 0;
  if (!raw && ttffState == TTFF_IDLE && millis() - last > 5000) {
    last = millis();
    if (gps.location.isValid() && gps.location.age() < 2000)
      Serial.printf("[Fix] %.6f, %.6f  sats %lu  HDOP %.2f\n", gps.location.lat(),
                    gps.location.lng(), (unsigned long)gps.satellites.value(), gps.hdop.hdop());
    else
      Serial.println("[Fix] none");
  }
}
