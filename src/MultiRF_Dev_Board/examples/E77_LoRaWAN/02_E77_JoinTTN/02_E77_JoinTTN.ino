/*
  ============================================================================
  MultiRF Dev Board - E77-900M22S Example 2: Join The Things Network
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  Joins The Things Network (TTN) by OTAA and sends a small uplink every
  5 minutes: a 16-bit counter and the module's supply voltage in mV.

  Steps:
    1. On TTN, add an end device manually: your frequency plan, LoRaWAN 1.0.4.
    2. Copy DevEUI (optional), JoinEUI and AppKey into config.h.
    3. Upload, open the serial monitor at 115200 baud.
    4. Paste ttn_payload_formatter.js into the TTN application to see the
       counter and voltage decoded.

  Flow:
    setup(): open UART -> check module -> set region -> write keys -> join
    loop():  print every line from the module (and decode downlink reports),
             send an uplink every UPLINK_INTERVAL_MS

  On the board, LED2 flashes on transmit, LED4 on receive, and LED5 stays on
  once the module has joined.
  ============================================================================
*/
#include "config.h"
#include <MultiRFDevBoard.h>

uint16_t counter    = 0;       // number of uplinks sent so far
uint32_t lastUplink = 0;       // millis() of the last uplink attempt
bool     joined     = false;   // true once the module is on the network

// Read the supply voltage with AT+BAT=?. The reply contains the value in mV;
// we take the first number in a plausible range. Returns 0 if not found.
uint16_t readSupplyMv() {
  String r = atCmd("AT+BAT=?");
  for (int i = 0; i < (int)r.length(); i++) {
    if (!isDigit(r[i])) continue;
    int j = i;
    while (j < (int)r.length() && isDigit(r[j])) j++;
    int v = r.substring(i, j).toInt();
    if (v > 1000 && v < 4000) return v;      // 1.0 V to 4.0 V
    i = j;
  }
  return 0;
}

// Build the 4-byte payload (big-endian) and send it.
void sendUplink() {
  uint16_t mv = readSupplyMv();
  uint8_t payload[4] = {
    (uint8_t)(counter >> 8), (uint8_t)counter,   // bytes 0-1: counter
    (uint8_t)(mv >> 8),      (uint8_t)mv         // bytes 2-3: supply in mV
  };

  switch (e77Send(UPLINK_PORT, payload, sizeof payload)) {
    case E77_SENT:
      Serial.printf("[App] Uplink %u sent (%u mV)\n", counter, mv);
      counter++;
      break;
    case E77_DUTYCYCLE:
      Serial.println("[App] Duty cycle limit, will retry next interval");
      break;
    case E77_NOT_JOINED:
      Serial.println("[App] Not joined, rejoining");
      joined = e77Join();
      break;
    case E77_BUSY:
      Serial.println("[App] Module busy");
      break;
    default:
      Serial.println("[App] Send error: check the AT+SEND? format, see e77SetSendFields()");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nMultiRF Dev Board - Example 2: Join TTN");

  e77Begin();                                    // UART1, 9600 baud, IO48/IO47
  delay(500);

  // Is the module there, and which firmware does it run?
  if (!atOk(atCmd("AT"))) Serial.println("[E77] No answer. Check wiring and 9600 baud.");
  atCmd("AT+DEVTYPE=?");
  atCmd("AT+VER=?");

  // Region first, then keys. The region is only changed if it differs.
  e77EnsureRegion(LORAWAN_REGION);
  e77SetKeys(LORAWAN_DEVEUI, LORAWAN_APPEUI, LORAWAN_APPKEY);
  atCmd("AT+CDEVEUI=?");                         // this is the DevEUI to register on TTN

  joined = e77Join();
  if (joined) sendUplink();
  lastUplink = millis();
}

void loop() {
  // Print everything the module says. Downlink reports are also decoded.
  String line;
  if (e77ReadLine(line)) {
    Serial.print("[E77] "); Serial.println(line);
    E77RxInfo rx;
    if (e77ParseRx(line, rx)) {
      Serial.printf("[App] Downlink in RX%d, DR%d, RSSI %d dBm, SNR %d dB\n",
                    rx.window, rx.dr, rx.rssi, rx.snr);
    }
  }

  // Time for the next uplink? Rejoin first if the last join failed.
  if (millis() - lastUplink >= UPLINK_INTERVAL_MS) {
    lastUplink = millis();
    if (!joined) joined = e77Join();
    if (joined) sendUplink();
  }
}
