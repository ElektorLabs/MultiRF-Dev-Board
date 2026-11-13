/*
  ============================================================================
  MultiRF Dev Board - E77-900M22S Example 3: How Good Is the Link?
  Author:  Saad-Imtiaz
  Series:  Elektor Wireless Workshop (3)
  ----------------------------------------------------------------------------
  A walk test. The sketch joins TTN, switches adaptive data rate (ADR) off,
  fixes the data rate, and then runs TEST_CYCLES test cycles. In each cycle
  it requests a link check (AT+LINKC), sends a 2-byte uplink, and listens
  through both receive windows for the network's answer.

  For each answer it prints the downlink RSSI and SNR and, when the firmware
  reports them, the demodulation margin (how many dB above the decoding limit
  the uplink arrived) and the number of gateways that heard it.

  A margin of 3 dB or less means the next step back will start losing
  messages. Walk away from the gateway between cycles to see it fall.

  The format of the link check answer can differ between firmware versions,
  so every raw line from the module is printed as well.
  ============================================================================
*/
#include "config.h"
#include <MultiRFDevBoard.h>

int      cycle    = 0;         // test cycles done
uint32_t lastTest = 0;         // millis() of the last cycle
bool     joined   = false;

// One test cycle: request a link check, send, then listen for 8 s.
void runTest() {
  cycle++;
  Serial.printf("\n[Test] Cycle %d of %d at DR%d\n", cycle, TEST_CYCLES, TEST_DATARATE);

  // AT+LINKC adds a LinkCheckReq to the next uplink.
  atCmd("AT+LINKC");
  uint8_t payload[2] = { (uint8_t)cycle, (uint8_t)TEST_DATARATE };
  E77SendResult res = e77Send(TEST_PORT, payload, sizeof payload);
  if (res == E77_NOT_JOINED) { joined = e77Join(); return; }
  if (res != E77_SENT)       { Serial.println("[Test] Uplink not sent"); return; }

  // Class A: the answer comes in RX1 or RX2, a few seconds after the uplink.
  uint32_t t0 = millis();
  bool gotRx = false;
  while (millis() - t0 < 8000) {
    String line;
    if (!e77ReadLine(line)) continue;
    Serial.print("[E77] "); Serial.println(line);

    E77RxInfo rx;
    if (e77ParseRx(line, rx)) {
      gotRx = true;
      Serial.printf("[Test] RX%d  DR%d  RSSI %d dBm  SNR %d dB",
                    rx.window, rx.dr, rx.rssi, rx.snr);
      if (rx.margin >= 0) Serial.printf("  margin %d dB  gateways %d", rx.margin, rx.gateways);
      Serial.println();
    }
  }
  if (!gotRx) Serial.println("[Test] No downlink received in this cycle");
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println("\nMultiRF Dev Board - Example 3: Link check");

  e77Begin();
  delay(500);
  atCmd("AT");
  e77EnsureRegion(LORAWAN_REGION);
  e77SetKeys(LORAWAN_DEVEUI, LORAWAN_APPEUI, LORAWAN_APPKEY);
  joined = e77Join();

  // Fixed data rate: ADR must be off before AT+CDATARATE is accepted.
  // ADR suits a fixed sensor; for a walk test it would keep changing the
  // data rate under us.
  atCmd("AT+CADR=0");
  char cmd[24];
  snprintf(cmd, sizeof cmd, "AT+CDATARATE=%d", TEST_DATARATE);
  atCmd(cmd);
  atCmd("AT+CTXP=0");                            // maximum power allowed in the region

  if (joined) runTest();
  lastTest = millis();
}

void loop() {
  if (cycle >= TEST_CYCLES) return;              // test finished, reset to run again

  String line;
  if (e77ReadLine(line)) { Serial.print("[E77] "); Serial.println(line); }

  if (millis() - lastTest >= TEST_INTERVAL_MS) {
    lastTest = millis();
    if (!joined) joined = e77Join();
    if (joined) runTest();
    if (cycle >= TEST_CYCLES) Serial.println("\n[Test] Done. Reset the board to run again.");
  }
}
