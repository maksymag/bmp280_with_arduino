// BMP280 -> Arduino Nano Every по I2C
// Подключение:
//   VCC -> 3.3V (или 5V, если у модуля есть стабилизатор)
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
    Serial.println(F("BMP280 не найден! Проверьте подключение и адрес (0x76/0x77)."));
    while (1) delay(10);
  }

  bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,      // режим работы
                  Adafruit_BMP280::SAMPLING_X2,      // температура
                  Adafruit_BMP280::SAMPLING_X16,     // давление
                  Adafruit_BMP280::FILTER_X16,       // фильтр
                  Adafruit_BMP280::STANDBY_MS_500);  // пауза между измерениями
}

void loop() {
  float temperature = bmp.readTemperature();        // °C
  float pressure    = bmp.readPressure() / 100.0F;  // гПа
  float pressureMM  = bmp.readPressure() * 0.00750062F; // мм рт. ст.
  float altitude    = bmp.readAltitude(1013.25);    // м (давление на уровне моря, гПа)

  Serial.print(F("Температура: "));
  Serial.print(temperature);
  Serial.println(F(" °C"));

  Serial.print(F("Давление: "));
  Serial.print(pressure);
  Serial.print(F(" гПа ("));
  Serial.print(pressureMM);
  Serial.println(F(" мм рт. ст.)"));

  Serial.print(F("Высота: "));
  Serial.print(altitude);
  Serial.println(F(" м"));

  Serial.println();
  delay(2000);
}
