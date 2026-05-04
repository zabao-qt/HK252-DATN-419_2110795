#pragma once

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define I2C_SDA 8
#define I2C_SCL 9
#define OLED_I2C_ADDR 0x3C

#define SENSOR_UART_ID 1
#define SENSOR_UART_TX_PIN 17
#define SENSOR_UART_RX_PIN 18
#define ULTRASONIC_CMD 0x55

#define PRESSURE_SENSOR_PIN 7
#define VREF 3.300f
#define ADC_MAX 4095

#define GPS_UART_ID 2
#define GPS_RX 15
#define GPS_TX 16
#define GPS_BAUD 9600

#define SD_CS   10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK  12

#define BUTTON_PIN 4
#define BUTTON_DOWN_PIN 40
#define BUTTON_UP_PIN 41