/*
  05 – Industrial Gateway
  Collects packets from one or more sensor nodes, parses the structured payload,
  and logs sensor readings to the Serial Monitor.

  The gateway listens on hardware address GW_ADDR.  The CC1101 discards any
  packet not addressed to GW_ADDR (hardware address filtering).

  See Sensor.ino for the packet layout and wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2
#define GW_ADDR  0x01

struct __attribute__((packed)) SensorPkt {
  uint8_t  nodeId;
  uint8_t  dataType;
  int16_t  value;
  uint16_t counter;
};

#ifdef ESP32
RadioExt radio(PIN_CS, /*clk*/18, /*miso*/19, /*mosi*/23, PIN_GD0);
#else
RadioExt radio(PIN_CS, PIN_GD0);
#endif

static const char *const dataTypeStr[] = { "Temp", "Humidity" };

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
  radio.setAddressFilteringMode(ADDR_FILTER_MODE_CHECK_BC_0_255);

  Serial.println(F("Industrial Gateway ready (listening…)"));
  Serial.println(F("Node  Type      Value   Cnt  RSSI"));
}

void loop() {
  SensorPkt pkt;
  size_t    len = 0;

  Status s = radio.receive((uint8_t *)&pkt, sizeof(pkt), &len, GW_ADDR);

  if (s == STATUS_OK && len == sizeof(SensorPkt)) {
    const char *type = (pkt.dataType < 2) ? dataTypeStr[pkt.dataType] : "Unknown";
    Serial.print(pkt.nodeId);   Serial.print(F("     "));
    Serial.print(type);         Serial.print(F("    "));
    Serial.print(pkt.value / 10.0, 1);
    if (pkt.dataType == 0) Serial.print(F(" °C"));
    if (pkt.dataType == 1) Serial.print(F(" %"));
    Serial.print(F("   ")); Serial.print(pkt.counter);
    Serial.print(F("   ")); Serial.print(radio.getRSSI()); Serial.println(F(" dBm"));
  }
}
