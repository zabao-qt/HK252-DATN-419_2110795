#include "pressure.h"
#include "config.h"
#include <Preferences.h>

static Preferences prefs;
static float V_zero_sensor = 0.5f;
static float slope_kPa_per_V = 125.0f;

static const int NUM_SAMPLES = 100;
static float voltageDividerFactor = 1.0f;

void pressure_init() {
  analogReadResolution(12);
  analogSetPinAttenuation(PRESSURE_SENSOR_PIN, ADC_11db);
  prefs.begin("calibration", true); 
  V_zero_sensor = prefs.getFloat("v_zero", 0.5f);
  prefs.end();
  Serial.print("[PRESS] Loaded V_zero: ");
  Serial.println(V_zero_sensor);
}

void pressure_calibrate_zero(float current_v) {
  V_zero_sensor = current_v;
  prefs.begin("calibration", false);
  prefs.putFloat("v_zero", V_zero_sensor);
  prefs.end();
}

float pressure_get_zero() {
  return V_zero_sensor;
}

bool pressure_read(float& kPa, float& MPa, float& Vadc) {
  float raw = 0.0f;

  for (int i = 0; i < NUM_SAMPLES; i++) {
    raw += analogRead(PRESSURE_SENSOR_PIN);
    delay(2);
  }
  raw /= NUM_SAMPLES;

  Vadc = (raw / ADC_MAX) * VREF;
  float Vsensor = Vadc * voltageDividerFactor;
  Vadc = Vsensor; // update after voltage divider or compensate for calibration

  kPa = (Vsensor - V_zero_sensor) * slope_kPa_per_V;
  if (kPa < 0) kPa = 0;

  MPa = kPa / 1000.0f;

  return true;
}
