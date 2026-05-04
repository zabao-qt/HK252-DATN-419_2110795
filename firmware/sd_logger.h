#pragma once
#include <Arduino.h>

void sd_init();
bool sd_log_line(const char* line);
bool sd_is_ok();
