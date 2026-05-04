#pragma once
#include <Arduino.h>

void oled_init();
void oled_flash_message(const char* msg, uint16_t ms);
void oled_update_pressure(float kPa, float Vadc);
void oled_update_wlevel(int wLevel_cm);
void oled_update_wdepth(int wDepth_cm);
void oled_begin_update();
void oled_render();
void oled_web_screen(int clients);

enum class GPSState;
void oled_update_gps(GPSState state,double lat,double lon,int sats, double hdop, double offset);