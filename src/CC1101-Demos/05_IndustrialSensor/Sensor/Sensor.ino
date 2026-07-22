/*
  05 – Industrial Sensor Node
  Reads simulated sensor data every 5 s and transmits a structured packet to
  the gateway with automatic retry.  Sensor ID is embedded in the payload.

  Packet layout (6 bytes):
    [nodeId:1][dataType:1][value:2 (int16, scaled x10)][counter:2]

  Run one or more sensor nodes (change NODE_ID) alongside Gateway.ino.
  See 01_BasicRxTx/Transmitter for wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2
#define NODE_ID   1        /* change to 2, 3 … for additional sensors */
#define GW_ADDR  0x01      /* gateway hardware address */

struct __attribute__((packed)) SensorPkt {
  uint8_t  nodeId;
  uint8_t  dataType;   /* 0=temperature  1=humidity */
  int16_t  value;      /* scaled ×10, e.g. 223 = 22.3 °C */
  uint16_t counter;
};

#ifdef ESP32
CC1101::RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
CC1101::RadioExt radio(PIN_CS, PIN_GD0);
#endif

static uint16_t cnt = 0;

void setup() {
  Serial.begin(115200);
  randomSeed(analogRead(A0));

  while (radio.begin(CC1101::MOD_2FSK, 433.92, 4.8) != CC1101::STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(CC1101::PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(CC1101::SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setAddressFilteringMode(CC1101::ADDR_FILTER_MODE_CHECK_BC_0_255);

  Serial.print(F("Sensor node ")); Serial.print(NODE_ID); Serial.println(F(" ready"));
}

void loop() {
  SensorPkt pkt;
  pkt.nodeId   = NODE_ID;
  pkt.dataType = 0;
  pkt.value    = 220 + random(-20, 20);   /* simulated temperature ×10 */
  pkt.counter  = cnt++;

  CC1101::Status s = radio.transmitRetry(
    (uint8_t *)&pkt, sizeof(pkt), /*retries*/3, GW_ADDR);

  Serial.print(F("Node ")); Serial.print(NODE_ID);
  Serial.print(F("  temp ")); Serial.print(pkt.value / 10.0, 1); Serial.print(F(" °C"));
  Serial.println(s == CC1101::STATUS_OK ? F("  [sent]") : F("  [FAILED]"));

  delay(5000);
}
