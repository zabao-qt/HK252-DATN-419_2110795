#pragma once
#include <Arduino.h>

void pressure_init();
bool pressure_read(
  float& kPa,
  float& MPa,
  float& Vadc
);
float pressure_get_zero();
void pressure_calibrate_zero(float current_v);
