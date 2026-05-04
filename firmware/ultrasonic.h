#pragma once
#include <Arduino.h>

void ultrasonic_init();
bool ultrasonic_read(int& distance_mm);
