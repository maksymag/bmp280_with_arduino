#include <SPI.h>
#include <Adafruit_BMP280.h>

#define BMP_CS 10
Adafruit_BMP280 bmp_spi(BMP_CS);

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("SPI BMP280 Test");
  if (!bmp_spi.begin()) {
    Serial.println("SPI BMP280 not found!");
    while (1);
  }
  Serial.println("SPI BMP280 found!");
}

void loop() {
  Serial.print("Pressure = ");
  Serial.print(bmp_spi.readPressure() / 100.0F);
  Serial.println(" hPa");
  delay(2000);
}