// BMP280 -> Arduino Nano Every по I2C
// Підключення:
//   VCC -> 5V
//   GND -> GND
//   SDA -> A4
//   SCL -> A5
//
// Библиотека: "Adafruit BMP280 Library" (+ "Adafruit Unified Sensor")

#include <Wire.h>
#include <Adafruit_BMP280.h>

Adafruit_BMP280 bmp;  // I2C

void setup() {
  Serial.begin(115200);
  while (!Serial) { }

  // Адрес 0x76 (SDO -> GND) или 0x77 (SDO -> VCC)
  if (!bmp.begin(0x76)) {
    Serial.println(F("BMP280 не знайдено! Перевірте дроти або адресу (0x76/0x77)."));
    while (1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,      // режим роботи
                  Adafruit_BMP280::SAMPLING_X2,      // температура
                  Adafruit_BMP280::SAMPLING_X16,     // тиск
                  Adafruit_BMP280::FILTER_X16,       // фільтр
                  Adafruit_BMP280::STANDBY_MS_500);  // пауза 
}

void loop() {
  float temperature = bmp.readTemperature();        // °C
  float pressure    = bmp.readPressure() / 100.0F;  // гПа
  float pressureMM  = bmp.readPressure() * 0.00750062F; // мм рт. ст.
  float altitude    = bmp.readAltitude(1013.25);    // м (тиск на рівні моря, гПа)

  Serial.print(F("Температура: "));
  Serial.print(temperature);
  Serial.println(F(" °C"));

  Serial.print(F("Тиск: "));
  Serial.print(pressure);
  Serial.print(F(" гПа ("));
  Serial.print(pressureMM);
  Serial.println(F(" мм рт. ст.)"));

  Serial.print(F("Висота: "));
  Serial.print(altitude);
  Serial.println(F(" м"));

  Serial.println();
  delay(2000);
}
