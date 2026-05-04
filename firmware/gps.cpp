#include "gps.h"
#include "config.h"
#include <TinyGPSPlus.h>
#include <TimeLib.h>
#include <vector>
#include <math.h>

#define GPS_AVG_N 6
static double lat_buf[GPS_AVG_N];
static double lon_buf[GPS_AVG_N];
static int buf_idx = 0;
static int buf_count = 0;

static HardwareSerial GPS_Serial(GPS_UART_ID);
static TinyGPSPlus gps;

static GPSState state = GPSState::NO_UART;
void gps_init() {
  GPS_Serial.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, GPS_TX);
  delay(50);
}

// static void gps_query_rate() {
//   const uint8_t msg[] = {
//     0xB5, 0x62,       // sync
//     0x06, 0x08,       // class, id
//     0x00, 0x00,       // length = 0
//     0x0E, 0x30        // checksum
//   };

//   while (GPS_Serial.available()) GPS_Serial.read();

//   GPS_Serial.write(msg, sizeof(msg));
//   GPS_Serial.flush();

//   unsigned long start = millis();

//   enum { SYNC1, SYNC2, CLASS, ID, LEN1, LEN2, PAYLOAD, CK_A, CK_B } state = SYNC1;
//   uint8_t ckA = 0, ckB = 0;
//   uint8_t len = 0;
//   uint8_t payload[6];
//   uint8_t idx = 0;

//   while (millis() - start < 1000) {
//     if (!GPS_Serial.available()) continue;
//     uint8_t b = GPS_Serial.read();

//     switch (state) {
//       case SYNC1:
//         if (b == 0xB5) state = SYNC2;
//         break;

//       case SYNC2:
//         if (b == 0x62) state = CLASS;
//         else state = SYNC1;
//         break;

//       case CLASS:
//         if (b == 0x06) {
//           ckA = 0; ckB = 0;
//           ckA += b; ckB += ckA;
//           state = ID;
//         } else state = SYNC1;
//         break;

//       case ID:
//         if (b == 0x08) {
//           ckA += b; ckB += ckA;
//           state = LEN1;
//         } else state = SYNC1;
//         break;

//       case LEN1:
//         len = b;
//         ckA += b; ckB += ckA;
//         state = LEN2;
//         break;

//       case LEN2:
//         ckA += b; ckB += ckA;
//         if (len == 6) {
//           idx = 0;
//           state = PAYLOAD;
//         } else {
//           state = SYNC1;
//         }
//         break;

//       case PAYLOAD:
//         payload[idx++] = b;
//         ckA += b; ckB += ckA;
//         if (idx >= len) state = CK_A;
//         break;

//       case CK_A:
//         if (b != ckA) return;
//         state = CK_B;
//         break;

//       case CK_B:
//         if (b != ckB) return;

//         // parse payload
//         uint16_t measRate = payload[0] | (payload[1] << 8);
//         uint16_t navRate  = payload[2] | (payload[3] << 8);
//         uint16_t timeRef  = payload[4] | (payload[5] << 8);

//         float hz = 1000.0f / measRate;

//         Serial.print("[GPS CFG] measRate: ");
//         Serial.print(measRate);
//         Serial.print(" ms | navRate: ");
//         Serial.print(navRate);
//         Serial.print(" | freq: ");
//         Serial.print(hz, 2);
//         Serial.println(" Hz");

//         return;
//     }
//   }

//   Serial.println("[GPS CFG] query rate timeout");
// }

// static void ubx_send(const uint8_t *cls, size_t len) {
//   GPS_Serial.write(cls, len);
//   GPS_Serial.flush();
// }

// static void ubx_compute_checksum(const uint8_t *buf, size_t len, uint8_t &ckA, uint8_t &ckB) {
//   ckA = 0; ckB = 0;
//   for (size_t i = 0; i < len; ++i) {
//     ckA = ckA + buf[i];
//     ckB = ckB + ckA;
//   }
// }

// static bool ubx_wait_ack(uint8_t ackClass, uint8_t ackID, unsigned long timeout_ms = 1000) {
//   // Wait for UBX-ACK-ACK (class 0x05 id 0x01) matching ackClass/ackID
//   unsigned long start = millis();
//   enum {SYNC1, SYNC2, CLASS, ID, LEN1, LEN2, PAYLOAD, CK_A } state = SYNC1;
//   uint8_t ckA=0, ckB=0;
//   uint8_t len = 0;
//   uint8_t payload[4] = {0};
//   uint8_t idx = 0;

//   while (millis() - start < timeout_ms) {
//     if (!GPS_Serial.available()) continue;
//     uint8_t b = GPS_Serial.read();
//     switch (state) {
//       case SYNC1: if (b == 0xB5) state = SYNC2; break;
//       case SYNC2: if (b == 0x62) state = CLASS; else state = SYNC1; break;
//       case CLASS:
//         if (b == 0x05) { state = ID; } else state = SYNC1;
//         break;
//       case ID:
//         if (b == 0x01 || b == 0x00) { /* ACK-ACK or ACK-NAK */ state = LEN1; ckA = 0; ckB = 0; ckA += 0x05; ckB += ckA; ckA += b; ckB += ckA; }
//         else state = SYNC1;
//         break;
//       case LEN1: len = b; ckA += b; ckB += ckA; state = LEN2; break;
//       case LEN2: ckA += b; ckB += ckA; if (len <= sizeof(payload)) { idx = 0; state = PAYLOAD; } else state = SYNC1; break;
//       case PAYLOAD:
//         payload[idx++] = b;
//         ckA += b; ckB += ckA;
//         if (idx >= len) state = CK_A;
//         break;
//       case CK_A:
//         if (b != ckA) return false;
//         // next byte should be CK_B
//         while (!GPS_Serial.available()) { if (millis()-start>timeout_ms) return false; }
//         if (GPS_Serial.read() != ckB) return false;
//         // payload[0] == ackClass, payload[1] == ackID for ACK messages
//         if (payload[0] == ackClass && payload[1] == ackID) return true;
//         return false;
//     }
//   }
//   return false;
// }

void gps_update() {
  while (GPS_Serial.available()) {
    gps.encode(GPS_Serial.read());
  }

  if (gps.charsProcessed() < 10) {
    state = GPSState::NO_UART;
  }
  else if (gps.sentencesWithFix() == 0) {
    state = GPSState::WAITING_FIX;
  }
  else if (gps.satellites.value() == 0) {
    state = GPSState::FIX_LOST;
  }
  else {
    state = GPSState::FIX_OK;
    if (gps.location.isValid()) {
      lat_buf[buf_idx] = gps.location.lat();
      lon_buf[buf_idx] = gps.location.lng();

      buf_idx = (buf_idx + 1) % GPS_AVG_N;
      if (buf_count < GPS_AVG_N) buf_count++;
    }
  }
}

GPSState gps_get_state() {
  return state;
}

bool gps_get_location(double& lat, double& lon) {
  if (buf_count == 0) return false;

  double sum_lat = 0;
  double sum_lon = 0;

  for (int i = 0; i < buf_count; i++) {
    sum_lat += lat_buf[i];
    sum_lon += lon_buf[i];
  }

  lat = sum_lat / buf_count;
  lon = sum_lon / buf_count;
  return true;
}

int gps_get_sats() {
  return gps.satellites.isValid() ? gps.satellites.value() : 0;
}

bool gps_get_timestamp(uint32_t& unix_ts) {
  if (!gps.time.isValid() || !gps.date.isValid()) return false;

  tmElements_t tm;
  tm.Year  = gps.date.year() - 1970;
  tm.Month = gps.date.month();
  tm.Day   = gps.date.day();
  tm.Hour  = gps.time.hour();
  tm.Minute= gps.time.minute();
  tm.Second= gps.time.second();

  time_t t = makeTime(tm);
  unix_ts = (uint32_t)t;
  return true;
}

double gps_get_hdop() {
  if (!gps.hdop.isValid()) return -1.0;
  return gps.hdop.hdop();
}

// static void gps_set_rate_5hz() {
//   // measRate = 200 ms (5 Hz), navRate = 1, timeRef = 0 (UTC)
//   uint8_t payload[] = {
//     0xC8, 0x00,   // 200 ms
//     0x01, 0x00,   // navRate = 1
//     0x00, 0x00    // timeRef = UTC
//   };

//   const uint8_t UBX_CLASS = 0x06;
//   const uint8_t UBX_ID    = 0x08;

//   uint8_t ckbuf[2 + 2 + sizeof(payload)];
//   ckbuf[0] = UBX_CLASS;
//   ckbuf[1] = UBX_ID;
//   ckbuf[2] = sizeof(payload) & 0xFF;
//   ckbuf[3] = (sizeof(payload) >> 8) & 0xFF;
//   memcpy(&ckbuf[4], payload, sizeof(payload));

//   uint8_t ckA, ckB;
//   ubx_compute_checksum(ckbuf, sizeof(ckbuf), ckA, ckB);

//   uint8_t msg[2 + sizeof(ckbuf) + 2];
//   msg[0] = 0xB5;
//   msg[1] = 0x62;
//   memcpy(&msg[2], ckbuf, sizeof(ckbuf));
//   msg[2 + sizeof(ckbuf)] = ckA;
//   msg[3 + sizeof(ckbuf)] = ckB;

//   ubx_send(msg, sizeof(msg));

//   if (ubx_wait_ack(UBX_CLASS, UBX_ID, 1000)) {
//     Serial.println("[GPS CFG] Set rate 5Hz OK");
//   } else {
//     Serial.println("[GPS CFG] Set rate 5Hz FAIL");
//   }
// }

static double haversine_m(double lat1, double lon1, double lat2, double lon2) {
  const double R = 6371000.0;
  const double DEG_TO_RAD_D = 0.017453292519943295;

  double dLat = (lat2 - lat1) * DEG_TO_RAD_D;
  double dLon = (lon2 - lon1) * DEG_TO_RAD_D;
  double a =
      sin(dLat * 0.5) * sin(dLat * 0.5) +
      cos(lat1 * DEG_TO_RAD_D) * cos(lat2 * DEG_TO_RAD_D) *
      sin(dLon * 0.5) * sin(dLon * 0.5);

  double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
  return R * c;
}

double gps_get_avg_offset() {
  if (!gps.location.isValid()) return -1;
  static double last_lat = 0, last_lon = 0;
  static bool init = false;
  double lat = gps.location.lat();
  double lon = gps.location.lng();
  if (!init) {
    last_lat = lat;
    last_lon = lon;
    init = true;
    return 0;
  }
  double d = haversine_m(last_lat, last_lon, lat, lon);
  last_lat = lat;
  last_lon = lon;
  Serial.println(d);
  return round(d * 10.0) / 10.0;
}