#include "oled.h"
#include "gps.h"
#include "sd_logger.h"
#include "config.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static TwoWire I2C_BUS = TwoWire(0);
static Adafruit_SSD1306 display(
  SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_BUS, OLED_RESET
);

#define P_X     18
#define P_Y      0

#define VADC_X  86
#define VADC_Y   0

#define WLEVEL_X  54
#define WLEVEL_Y   8

#define WDEPTH_X  54
#define WDEPTH_Y  16

#define GPS_L1_Y  24
#define GPS_L2_Y  32
#define GPS_L3_Y  40
#define GPS_MID_Y  28

static void clear_field(int x, int y, int w, int h) {
  display.fillRect(x, y, w, h, SSD1306_BLACK);
}

void oled_begin_update() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
}

void oled_draw_footer() {
  const int y = SCREEN_HEIGHT - 8;
  clear_field(0, y, SCREEN_WIDTH, 8);
  display.setCursor(10, y);
  display.setTextSize(1);
  if (!sd_is_ok()) {
    display.print("SD error - reset");
  } else {
    display.print("Press MODE to save");
  }
}

void oled_render() {
  oled_draw_footer();
  display.display();
}

void oled_flash_message(const char* msg, uint16_t ms) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);

  const int maxCharsPerLine = 10;

  char line1[16] = {0};
  char line2[16] = {0};

  int len = strlen(msg);

  if (len <= maxCharsPerLine) {
    strcpy(line1, msg);
  } else {
    int split = maxCharsPerLine;
    for (int i = maxCharsPerLine; i > 0; i--) {
      if (msg[i] == ' ') {
        split = i;
        break;
      }
    }

    strncpy(line1, msg, split);
    line1[split] = '\0';

    strncpy(line2, msg + split + 1, sizeof(line2) - 1);
  }

  int16_t x1, y1;
  uint16_t w, h;
  display.getTextBounds(line1, 0, 0, &x1, &y1, &w, &h);
  display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT / 2) - h);
  display.print(line1);

  if (line2[0] != '\0') {
    display.getTextBounds(line2, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((SCREEN_WIDTH - w) / 2, (SCREEN_HEIGHT / 2) + 2);
    display.print(line2);
  }

  display.setTextSize(1);
  display.display();
  delay(ms);
}


void oled_init() {
  I2C_BUS.begin(I2C_SDA, I2C_SCL, 400000);
  display.begin(SSD1306_SWITCHCAPVCC, OLED_I2C_ADDR);

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void oled_update_pressure(float kPa, float Vadc) {
  clear_field(P_X, P_Y, 36, 8);
  display.setCursor(0, 0);   display.print("P:");
  display.setCursor(60, 0);  display.print("V:");
  display.setCursor(P_X, P_Y);

  if (isnan(kPa)) {
    display.print("--");
  } else {
    display.printf("%.0f", kPa);
  }
  display.print("kPa");

  clear_field(VADC_X, VADC_Y, 40, 8);
  display.setCursor(VADC_X, VADC_Y);

  if (isnan(Vadc)) {
    display.print("--");
  } else {
    display.printf("%.2f", Vadc);
  }
  display.print("V");
}

void oled_update_wlevel(int wLevel_cm) {
  clear_field(WLEVEL_X, WLEVEL_Y, 70, 8);
  display.setCursor(0, 8);   display.print("wLevel:");
  display.setCursor(WLEVEL_X, WLEVEL_Y);

  if (wLevel_cm == -2) {
    display.print("EXCEED");
  } else if (wLevel_cm < 0) {
    display.print("0cm");
  } else {
    display.printf("%dcm", wLevel_cm);
  }
}

void oled_update_wdepth(int wDepth_cm) {
  clear_field(WDEPTH_X, WDEPTH_Y, 70, 8);
  display.setCursor(0, 16);  display.print("wDepth:");
  display.setCursor(WDEPTH_X, WDEPTH_Y);

  if (wDepth_cm < 0) {
    display.print("--cm");
  } else {
    display.printf("%dcm", wDepth_cm);
  }
}

void oled_update_gps(GPSState state, double lat, double lon, int sats, double hdop, double offset) {
  clear_field(0, GPS_L1_Y, SCREEN_WIDTH, 8);
  clear_field(0, GPS_L2_Y, SCREEN_WIDTH, 8);
  clear_field(0, GPS_L3_Y, SCREEN_WIDTH, 8);

  switch (state) {
    case GPSState::NO_UART:
      display.setCursor(0, GPS_L1_Y);
      display.print("No UART data from GPS");
      break;

    case GPSState::WAITING_FIX:
      display.setCursor(0, GPS_L1_Y);
      display.print("Receiving NMEA");
      display.setCursor(0, GPS_L2_Y);
      display.print("Waiting for fix...");
      display.setCursor(0, GPS_L3_Y);  display.print("Sats:");
      display.setCursor(30, GPS_L3_Y);
      display.print(sats);
      break;

    case GPSState::FIX_LOST:
      display.setCursor(0, GPS_L1_Y);
      display.print("Fix lost");
      display.setCursor(0, GPS_L2_Y);
      display.print("Searching...");
      display.setCursor(0, GPS_L3_Y);  display.print("Sats:");
      display.setCursor(30, GPS_L3_Y);
      display.print(sats);
      break;

    case GPSState::FIX_OK:
      display.setCursor(0, GPS_L1_Y);  display.print("Lat:");
      display.setCursor(0, GPS_L2_Y);  display.print("Lon:");
      display.setCursor(0, GPS_L3_Y);  display.print("Sats:");
      display.setCursor(30, GPS_L1_Y);
      display.printf("%.5f", lat);
      display.setCursor(30, GPS_L2_Y);
      display.printf("%.5f", lon);
      display.setCursor(30, GPS_L3_Y);
      display.print(sats);
      display.setCursor(60, GPS_L3_Y);
      if (hdop < 0) {
        display.print("DOP:--");
      } else {
        display.printf("D:%.1f", hdop);
      }
      if (offset >= 0) {
        char note[12];
        snprintf(note, sizeof(note), "(%.1fm)", offset);

        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(note, 0, 0, &x1, &y1, &w, &h);

        display.setCursor(SCREEN_WIDTH - w - 2, GPS_MID_Y);
        display.print(note);
      }
      break;

    default:
      break;
  }
}

void oled_web_screen(int clients) {
  display.clearDisplay();
 
  display.setTextSize(2);
  display.setTextColor(SSD1306_WHITE);
  const char* title = "WEB MODE";
  int16_t x1, y1; uint16_t tw, th;
  display.getTextBounds(title, 0, 0, &x1, &y1, &tw, &th);
  display.setCursor((SCREEN_WIDTH - tw) / 2, 0);
  display.print(title);
 
  display.setTextSize(1);
 
  display.setCursor(0, 20);
  display.print("SSID: DepthMapper");
 
  display.setCursor(0, 30);
  display.print("IP:   192.168.4.1");
 
  display.setCursor(0, 40);
  char cbuf[22];
  snprintf(cbuf, sizeof(cbuf), "Clients: %d", clients);
  display.print(cbuf);
 
  display.setCursor(0, 56);
  display.print("UP btn = exit");
 
  display.display();
}