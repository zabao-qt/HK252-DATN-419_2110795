#include "sd_logger.h"
#include <SPI.h>
#include <SD.h>
#include "config.h"

SPIClass spiSD(HSPI);

static bool sd_ok = false;

bool sd_is_ok() {
  return sd_ok;
}

void sd_init() {
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);

  spiSD.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);

  if (!SD.begin(SD_CS, spiSD, 1000000)) {
    Serial.println("[SD] init failed");
    sd_ok = false;
  } else {
    Serial.println("[SD] ready");
    sd_ok = true;
  }
}

bool sd_log_line(const char* line) {
  File f = SD.open("/data.txt", FILE_APPEND);
  if (!f) {
    Serial.println("[SD] open failed");
    return false;
  }

  f.println(line);
  f.close();

  Serial.print("[SD] write: ");
  Serial.println(line);
  return true;
}
