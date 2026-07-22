/*
  05 – Industrial Sensor Node  (non-blocking)
  Sends structured sensor data every SEND_INTERVAL ms using a millis() timer.
  transmitRetry() uses no delay() — back-off between retries is millis()-based.

  Packet layout (6 bytes, packed):
    [nodeId:1][dataType:1][value:2 (int16 ×10)][counter:2]

  Run alongside Gateway.ino.  Change NODE_ID for each additional sensor.
  CHANNEL must match the gateway.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2

#define NODE_ID        1     /* change to 2, 3 … for each additional sensor */
#define CHANNEL        0
#define BASE_FREQ    433.92f
#define SEND_INTERVAL  5000  // ms between readings

struct __attribute__((packed)) SensorPkt {
  uint8_t  nodeId;
  uint8_t  dataType;   /* 0=temperature  1=humidity */
  int16_t  value;      /* ×10, e.g. 223 = 22.3 °C */
  uint16_t counter;
};

#ifdef ESP32
CC1101::RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
CC1101::RadioExt radio(PIN_CS, PIN_GD0);
#endif

static uint16_t cnt    = 0;
static uint32_t lastTx = 0;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  while (radio.begin(CC1101::MOD_2FSK, BASE_FREQ, 4.8) != CC1101::STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(CC1101::PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(CC1101::SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setChannel(CHANNEL);

  Serial.print(F("Sensor node ")); Serial.print(NODE_ID);
  Serial.print(F(" | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz"));
}

void loop() {
  /* ---- Send on interval ---- */
  if (millis() - lastTx >= SEND_INTERVAL) {
    lastTx = millis();

    SensorPkt pkt;
    pkt.nodeId   = NODE_ID;
    pkt.dataType = 0;
    pkt.value    = 220 + random(-20, 20);   /* simulated temperature ×10 */
    pkt.counter  = cnt++;

    CC1101::Status s = radio.transmitRetry((uint8_t *)&pkt, sizeof(pkt));

    Serial.print(F("TX ch")); Serial.print(CHANNEL);
    Serial.print(F("  node ")); Serial.print(NODE_ID);
    Serial.print(F("  temp ")); Serial.print(pkt.value / 10.0, 1); Serial.print(F(" \xC2\xB0" "C"));
    Serial.print(F("  #")); Serial.print(pkt.counter);
    Serial.println(s == CC1101::STATUS_OK ? F("  [sent]") : F("  [FAILED]"));
  }

  /* ---- Other tasks here ---- */
}
