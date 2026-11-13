/*
  05 – Industrial Gateway  (non-blocking, interrupt-driven)
  Collects packets from one or more sensor nodes and logs readings.
  GDO0 interrupt fires on packet arrival — MCU is free between packets.

  CHANNEL must match all sensor nodes.
  See Sensor.ino for the packet layout and wiring notes.
*/

#include <cc1101ext.h>
using namespace CC1101;

#define PIN_CS   10
#define PIN_GD0   2

#define CHANNEL      0
#define BASE_FREQ  433.92f

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

static const char *const typeStr[] = { "Temp    ", "Humidity" };

volatile bool pktReady = false;
void onPacket() { pktReady = true; }

void setup() {
  Serial.begin(115200);

  while (radio.begin(MOD_2FSK, BASE_FREQ, 4.8) != STATUS_OK) {
    Serial.println(F("CC1101 not found – check wiring"));
    delay(1000);
  }
  radio.setOutputPower(0);
  radio.setPacketLengthMode(PKT_LEN_MODE_VARIABLE);
  radio.setSyncMode(SYNC_MODE_16_16);
  radio.setCrc(true);
  radio.setChannel(CHANNEL);

  radio.receiveCallback(onPacket);

  Serial.print(F("Industrial Gateway | ch")); Serial.print(CHANNEL);
  Serial.print(F("  ")); Serial.print(BASE_FREQ + CHANNEL * 0.200f, 2); Serial.println(F(" MHz  (waiting…)"));
  Serial.println(F("Node  Channel  Type      Value    #Pkt  RSSI"));
  Serial.println(F("────  ───────  ────      ─────    ────  ────"));
}

void loop() {
  if (!pktReady) {
    /* MCU free — log to SD, update display, etc. */
    return;
  }
  pktReady = false;

  SensorPkt pkt;
  size_t    len = 0;
  Status    s   = radio.readPacket((uint8_t *)&pkt, sizeof(pkt), &len);

  if (s == STATUS_OK && len == sizeof(SensorPkt)) {
    const char *type = (pkt.dataType < 2) ? typeStr[pkt.dataType] : "Unknown ";
    Serial.print(pkt.nodeId);
    Serial.print(F("     ch")); Serial.print(CHANNEL);
    Serial.print(F("      ")); Serial.print(type);
    Serial.print(F("  ")); Serial.print(pkt.value / 10.0, 1);
    if (pkt.dataType == 0) Serial.print(F(" \xC2\xB0" "C"));
    if (pkt.dataType == 1) Serial.print(F(" %"));
    Serial.print(F("   ")); Serial.print(pkt.counter);
    Serial.print(F("    ")); Serial.print(radio.lastRSSI()); Serial.println(F(" dBm"));
  }

  radio.receiveCallback(onPacket);   /* re-arm */
}
