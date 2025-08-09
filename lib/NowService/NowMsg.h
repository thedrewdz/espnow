// NowMsg.h
#pragma once
#include <stdint.h>
#include <string.h>

static const size_t ESPNOW_MAX_DATA = 250;  // conservative app-layer limit

enum : uint16_t {
  NOW_DT_ADVERTISE  = 0,
  NOW_DT_CONNECT    = 1,
  NOW_DT_HANDSHAKE  = 2,
  NOW_DT_ACK        = 3,
  NOW_DT_HEARTBEAT  = 4,
  NOW_DT_DATA       = 5
};

struct __attribute__((packed)) NowMsg {
  uint32_t timestamp;   // millis()
  uint16_t datatype;    // values above
  uint8_t  fromMac[6];
  uint8_t  toMac[6];
  uint16_t length;      // bytes valid in payload
  uint8_t  payload[230];// 250 - 20-byte header = 230
};

static_assert(sizeof(NowMsg) == 250, "NowMsg must be exactly 250 bytes");

inline void copyMac(uint8_t dst[6], const uint8_t src[6]) { memcpy(dst, src, 6); }

inline bool buildMsg(NowMsg& m,
                     uint16_t datatype,
                     const uint8_t fromMac[6],
                     const uint8_t toMac[6],
                     const void* buf,
                     uint16_t len,
                     uint32_t nowMillis) {
  if (len > sizeof(m.payload)) return false;
  m.timestamp = nowMillis;
  m.datatype  = datatype;
  copyMac(m.fromMac, fromMac);
  copyMac(m.toMac,   toMac);
  m.length = len;
  if (len) memcpy(m.payload, buf, len);
  return true;
}

inline bool validateMsg(const uint8_t* data, int rxLen) {
  if (rxLen != (int)sizeof(NowMsg)) return false;
  const NowMsg* m = reinterpret_cast<const NowMsg*>(data);
  return (m->length <= sizeof(m->payload));
}
