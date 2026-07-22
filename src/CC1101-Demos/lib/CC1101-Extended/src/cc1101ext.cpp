#include "cc1101ext.h"
#include <string.h>

using namespace CC1101;

/* ------------------------------------------------------------------ */
/*  State helpers                                                       */
/* ------------------------------------------------------------------ */

void RadioExt::idle() { this->setState(STATE_IDLE); }

void RadioExt::beginReceive() {
  this->setState(STATE_IDLE);
  this->sendCmd(CC1101_CMD_FRX);   /* flush any stale RX FIFO bytes */
  this->setState(STATE_RX);
}

/* ------------------------------------------------------------------ */
/*  Non-blocking receive                                                */
/* ------------------------------------------------------------------ */

/*
  Read one packet from the RX FIFO if it is ready.

  Returns STATUS_TIMEOUT immediately when no packet is present.
  The radio state is NOT changed on return — the caller must call
  beginReceive() (or receiveCallback()) to re-arm for the next packet.

  Packet format in FIFO (variable-length, APPEND_STATUS=1):
    [len_byte][payload × len][rssi_byte][lqi_crc_byte]
*/
Status RadioExt::readPacket(uint8_t *data, size_t bufSize, size_t *read) {
  if (read) *read = 0;

  uint8_t rxBytes = this->readReg(CC1101_REG_RXBYTES);

  /* RXFIFO overflow — flush and signal the caller */
  if (rxBytes & 0x80) {
    this->setState(STATE_IDLE);
    this->sendCmd(CC1101_CMD_FRX);
    return STATUS_RXFIFO_OVERFLOW;
  }

  /* Nothing in the FIFO yet */
  if ((rxBytes & 0x7F) == 0) return STATUS_TIMEOUT;

  /* Variable-length mode: first byte is the payload length */
  uint8_t dataLen = this->readReg(CC1101_REG_FIFO);
  if (dataLen == 0 || dataLen > bufSize) {
    this->setState(STATE_IDLE);
    this->sendCmd(CC1101_CMD_FRX);
    return STATUS_LENGTH_TOO_SMALL;
  }

  /* Wait for the full payload + 2 appended status bytes to arrive.
     When called from an interrupt context (GDO0 fires at end-of-packet)
     the data is already there and this loop exits immediately. */
  uint32_t t = millis();
  while ((this->readReg(CC1101_REG_RXBYTES) & 0x7F) < (uint8_t)(dataLen + 2)) {
    if (millis() - t > 50) {
      this->setState(STATE_IDLE);
      this->sendCmd(CC1101_CMD_FRX);
      return STATUS_TIMEOUT;
    }
  }

  this->readRegBurst(CC1101_REG_FIFO, data, dataLen);

  /* Appended status: RSSI byte then LQI/CRC_OK byte */
  uint8_t rssiRaw = this->readReg(CC1101_REG_FIFO);
  uint8_t lqiByte = this->readReg(CC1101_REG_FIFO);

  _rssi = (rssiRaw >= 128) ? (int8_t)((rssiRaw - 256) / 2 - 74)
                            : (int8_t)(rssiRaw / 2 - 74);
  _lqi  = lqiByte & 0x7F;

  if (read) *read = dataLen;
  return ((lqiByte >> 7) & 1) ? STATUS_OK : STATUS_CRC_MISMATCH;
}

/* ------------------------------------------------------------------ */
/*  Live RSSI (radio must be in RX mode)                               */
/* ------------------------------------------------------------------ */

int8_t RadioExt::sampleRssi() {
  int16_t r = this->readReg(CC1101_REG_RSSI);
  return (int8_t)((r >= 128 ? r - 256 : r) / 2 - 74);
}

/* ------------------------------------------------------------------ */
/*  Power                                                               */
/* ------------------------------------------------------------------ */

void RadioExt::sleep() {
  this->setState(STATE_IDLE);
  this->sendCmd(CC1101_CMD_PWD);
}

/* ------------------------------------------------------------------ */
/*  Encryption — XTEA-CTR (128-bit key, no delay())                   */
/* ------------------------------------------------------------------ */

void RadioExt::xteaEnc(uint32_t v[2], const uint32_t k[4]) {
  uint32_t s = 0;
  for (int i = 0; i < 32; i++) {
    v[0] += ((v[1] << 4 ^ v[1] >> 5) + v[1]) ^ (s + k[s & 3]);
    s    += 0x9E3779B9U;
    v[1] += ((v[0] << 4 ^ v[0] >> 5) + v[0]) ^ (s + k[s >> 11 & 3]);
  }
}

void RadioExt::encrypt(uint8_t *data, uint8_t len, const uint8_t key[16]) {
  uint32_t k[4], v[2];
  memcpy(k, key, 16);
  for (uint8_t off = 0; off < len; off += 8) {
    v[0] = off >> 3;  v[1] = 0;
    xteaEnc(v, k);
    uint8_t blk = (len - off < 8) ? len - off : 8;
    for (uint8_t j = 0; j < blk; j++)
      data[off + j] ^= ((uint8_t *)v)[j];
  }
}

void RadioExt::decrypt(uint8_t *data, uint8_t len, const uint8_t key[16]) {
  encrypt(data, len, key);
}

/* ------------------------------------------------------------------ */
/*  Reliable transmit (no delay())                                      */
/* ------------------------------------------------------------------ */

Status RadioExt::transmitRetry(uint8_t *data, size_t len,
                                uint8_t retries, uint8_t addr) {
  for (uint8_t i = 0; i <= retries; i++) {
    if (this->transmit(data, len, addr) == STATUS_OK) return STATUS_OK;
    if (i < retries) {
      uint32_t t = millis();
      while (millis() - t < 50) yield();  /* 50 ms back-off, yield-friendly */
    }
  }
  return STATUS_TIMEOUT;
}

/* ------------------------------------------------------------------ */
/*  Channel scan (no delay() — uses millis() busy-wait + yield())     */
/* ------------------------------------------------------------------ */

void RadioExt::scanChannels(uint8_t startCh, uint8_t count,
                             int8_t *rssiOut, uint16_t dwellMs) {
  uint8_t origCh = this->readReg(CC1101_REG_CHANNR);
  for (uint8_t i = 0; i < count; i++) {
    this->setChannel(startCh + i);
    this->setState(STATE_RX);
    uint32_t t = millis();
    while (millis() - t < dwellMs) yield();   /* cooperative yield, no delay() */
    int16_t r = this->readReg(CC1101_REG_RSSI);
    rssiOut[i] = (int8_t)((r >= 128 ? r - 256 : r) / 2 - 74);
    this->setState(STATE_IDLE);
  }
  this->setChannel(origCh);
}
