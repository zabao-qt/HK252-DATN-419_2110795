#include "ultrasonic.h"
#include "config.h"

HardwareSerial sensorSerial(SENSOR_UART_ID);
uint8_t buf[4];

void ultrasonic_init() {
  sensorSerial.begin(
    115200, SERIAL_8N1,
    SENSOR_UART_RX_PIN,
    SENSOR_UART_TX_PIN
  );
}

bool ultrasonic_read(int& dist) {
  static uint32_t lastReq = 0;

  // send request every 100 ms
  if (millis() - lastReq >= 50) {
    sensorSerial.write(ULTRASONIC_CMD);
    lastReq = millis();
  }

  // need full frame
  if (sensorSerial.available() < 4) return false;

  // sync to header
  while (sensorSerial.available() > 0) {
    if (sensorSerial.peek() == 0xFF) break;
    sensorSerial.read(); // discard garbage
  }

  if (sensorSerial.available() < 4) return false;

  buf[0] = sensorSerial.read(); // 0xFF
  buf[1] = sensorSerial.read();
  buf[2] = sensorSerial.read();
  buf[3] = sensorSerial.read();

  uint8_t checksum = buf[0] + buf[1] + buf[2];
  if (buf[3] != checksum) return false;

  dist = (buf[1] << 8) | buf[2];
  return true;
}
