#pragma once
#include <cc1101.h>

namespace CC1101 {

/*
  CC1101-Extended — higher-level helpers built on top of CC1101::Radio.

  Non-blocking receive pattern:
    1. radio.beginReceive()         — flush FIFO, enter RX
    2. radio.readPacket(buf, size)  — returns STATUS_TIMEOUT instantly if nothing
                                      arrived; STATUS_OK when a full packet is read
    3. radio.beginReceive()         — re-arm for the next packet

  Interrupt-driven receive pattern (lower MCU load):
    1. radio.receiveCallback(onPkt) — configure GDO0 interrupt, enter RX
    2. ISR sets volatile bool flag
    3. loop() calls readPacket() then receiveCallback(onPkt) to re-arm
*/
class RadioExt : public Radio {
 public:
  using Radio::Radio;

  /* --- State control --- */
  void idle();          /* put radio in IDLE (stop TX/RX)             */
  void beginReceive();  /* flush FIFO and enter RX (non-blocking)     */

  /* --- Non-blocking receive --- */
  /* Returns STATUS_TIMEOUT immediately if no packet has arrived yet.
     Returns STATUS_OK when a complete packet has been read.
     Call beginReceive() or receiveCallback() after each successful read. */
  Status readPacket(uint8_t *data, size_t bufSize, size_t *read = nullptr);

  /* RSSI / LQI from the last successful readPacket() call */
  int8_t  lastRSSI() const { return _rssi; }
  uint8_t lastLQI()  const { return _lqi;  }

  /* Live RSSI register — only valid while radio is in RX (beginReceive first) */
  int8_t sampleRssi();

  /* --- Power --- */
  void sleep();  /* SPWD power-down (~0.5 µA); call begin() to wake */

  /* --- Encryption (XTEA-CTR, 128-bit key, in-place) --- */
  static void encrypt(uint8_t *data, uint8_t len, const uint8_t key[16]);
  static void decrypt(uint8_t *data, uint8_t len, const uint8_t key[16]);

  /* --- Reliable transmit --- */
  /* Retries up to `retries` extra times with 50 ms back-off. No delay(). */
  Status transmitRetry(uint8_t *data, size_t len,
                       uint8_t retries = 3, uint8_t addr = 0);

  /* --- Channel scanner (blocking per-channel dwell, no delay()) --- */
  void scanChannels(uint8_t startCh, uint8_t count,
                    int8_t *rssiOut, uint16_t dwellMs = 15);

 private:
  int8_t  _rssi = 0;
  uint8_t _lqi  = 0;

  static void xteaEnc(uint32_t v[2], const uint32_t k[4]);
};

}  /* namespace CC1101 */
