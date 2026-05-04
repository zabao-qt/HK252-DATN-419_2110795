#include "config.h"
#include "oled.h"
#include "ultrasonic.h"
#include "pressure.h"
#include "gps.h"
#include "sd_logger.h"
#include "wifi_server.h"

enum class DeviceMode { SURVEY, WEB };
static DeviceMode mode = DeviceMode::SURVEY;

static bool     lastUpBtn    = HIGH;
static bool     lastDownBtn  = HIGH;
static uint32_t upPressTime  = 0;
static uint32_t downPressTime = 0;

void setup() {
  Serial.begin(115200);

  oled_init();
  ultrasonic_init();
  pressure_init();
  gps_init();
  sd_init();

  pinMode(BUTTON_PIN,      INPUT_PULLUP);
  pinMode(BUTTON_UP_PIN,   INPUT_PULLUP);
  pinMode(BUTTON_DOWN_PIN, INPUT_PULLUP);

  oled_flash_message("DEPTH MAPPER", 1500);
}

static void oled_show_web_mode(int clients) {
  oled_web_screen(clients);
}

void loop() {
  bool logNow  = digitalRead(BUTTON_PIN);
  bool upNow   = digitalRead(BUTTON_UP_PIN);
  bool downNow = digitalRead(BUTTON_DOWN_PIN);

  if (lastDownBtn == HIGH && downNow == LOW) {
    downPressTime = millis();
  }
  if (lastDownBtn == LOW && downNow == HIGH) {
    if (mode == DeviceMode::SURVEY) {
      mode = DeviceMode::WEB;
      wifi_server_start();
      oled_flash_message("WEB MODE ON", 1200);
    }
  }

  if (lastUpBtn == HIGH && upNow == LOW) {
    upPressTime = millis();
  }
  if (lastUpBtn == LOW && upNow == HIGH) {
    uint32_t held = millis() - upPressTime;

    if (mode == DeviceMode::WEB) {
      wifi_server_stop();
      mode = DeviceMode::SURVEY;
      oled_flash_message("SURVEY MODE", 1200);

    } else if (mode == DeviceMode::SURVEY && held > 1000) {
      float kPa_dummy, MPa_dummy, Vadc;
      pressure_read(kPa_dummy, MPa_dummy, Vadc);
      pressure_calibrate_zero(Vadc);
      char msg[32];
      snprintf(msg, sizeof(msg), "CALIB %.2fV", Vadc);
      oled_flash_message(msg, 2000);
    }
  }

  lastUpBtn   = upNow;
  lastDownBtn = downNow;

  if (mode == DeviceMode::WEB) {
    wifi_server_handle();
    oled_show_web_mode(wifi_server_client_count());
    delay(50);
    return;
  }


  int   depth_mm   = -1;
  float kPa = NAN, MPa = NAN, Vadc = NAN;
  double lat = 0, lon = 0;
  uint32_t ts = 0;

  bool depth_ok    = ultrasonic_read(depth_mm);
  bool pressure_ok = pressure_read(kPa, MPa, Vadc);

  gps_update();
  GPSState gpsState    = gps_get_state();
  bool     gps_loc_ok  = gps_get_location(lat, lon);
  bool     gps_time_ok = gps_get_timestamp(ts);
  int      sats        = gps_get_sats();
  double   hdop        = gps_get_hdop();
  double   gps_offset  = gps_get_avg_offset();

  oled_begin_update();

  if (depth_ok) {
    oled_update_wdepth(depth_mm / 10);
  } else {
    oled_update_wdepth(-1);
  }

  if (pressure_ok) {
    float vZero    = pressure_get_zero();
    int   wLevel_cm;
    if      (Vadc < vZero) wLevel_cm = 0;
    else if (Vadc > 3.0f)  wLevel_cm = -2;   // EXCEED
    else                   wLevel_cm = (int)(kPa * 10.2f);

    oled_update_pressure(kPa, Vadc);
    oled_update_wlevel(wLevel_cm);
  } else {
    oled_update_pressure(NAN, NAN);
    oled_update_wlevel(-1);
  }

  oled_update_gps(gpsState, lat, lon, sats, hdop, gps_offset);
  oled_render();

  static bool prevLogBtn = HIGH;
  if (prevLogBtn == HIGH && logNow == LOW) {
    Serial.println("[BTN] LOG pressed");

    if (gpsState != GPSState::FIX_OK || !gps_time_ok) {
      oled_flash_message("GPS NOT READY", 1000);
    } else {
      char line[128];
      snprintf(line, sizeof(line),
               "%.6f, %.6f, %.2f, %d, %lu",
               lat, lon,
               pressure_ok ? kPa   : -1.0f,
               depth_ok    ? depth_mm : -1,
               ts);

      if (sd_log_line(line)) {
        oled_flash_message("SAVED TO SD", 1000);
      } else {
        oled_flash_message("SD ERROR", 1000);
      }
    }
  }
  prevLogBtn = logNow;

  delay(20);
}
