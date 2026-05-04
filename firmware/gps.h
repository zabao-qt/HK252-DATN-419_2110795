#pragma once
#include <Arduino.h>

enum class GPSState {
  NO_UART,
  WAITING_FIX,
  FIX_LOST,
  FIX_OK
};

void gps_init();
// void gps_enable_multi_gnss();
void gps_update();

GPSState gps_get_state();
bool gps_get_location(double& lat, double& lon);
int  gps_get_sats();
bool gps_get_timestamp(uint32_t& unix_ts);
double gps_get_hdop();
double gps_get_avg_offset();
