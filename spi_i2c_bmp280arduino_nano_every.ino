// Два BMP280 на Arduino Nano Every: один по I2C, другий по SPI.
// Тиск фільтрується (медіана з 5 + експоненційне згладжування),
// у разі невалідних даних виводиться попередження.
//
// I2C: SDA -> A4, SCL -> A5, адреса 0x76 (SDO -> GND)
// SPI: SCK -> D13, MISO(SDO) -> D12, MOSI(SDI) -> D11, CS(CSB) -> D10
// Живлення обох модулів: 3.3V, GND спільний
//
// Бібліотека: Adafruit BMP280 Library (+ Adafruit Unified Sensor)

#include <Wire.h>
#include <SPI.h>
#include <Adafruit_BMP280.h>

// ---------- Налаштування ----------
#define I2C_ADDR 0x76
#define BMP_CS   10

const float         P_MIN       = 300.0;   // гПа, нижня межа діапазону датчика
const float         P_MAX       = 1100.0;  // гПа, верхня межа діапазону датчика
const float         MAX_JUMP    = 5.0;     // гПа, максимальний стрибок між сусідніми відліками
const float         MAX_DIFF    = 2.0;     // гПа, допустиме розходження між датчиками
const float         EMA_ALPHA   = 0.3;     // коефіцієнт згладжування (0..1)
const uint8_t       FAIL_LIMIT  = 3;       // скільки поганих відліків підряд до визнання датчика втраченим
const unsigned long PERIOD_MS   = 1000;    // період опитування
const unsigned long RETRY_MS    = 10000;   // період спроб переініціалізації
const uint8_t       MEDIAN_N    = 5;       // розмір вікна медіанного фільтра

enum Status : uint8_t { ST_OK, ST_WARMUP, ST_NODATA, ST_RANGE, ST_JUMP, ST_OFFLINE };

const char* statusText(Status s) {
  switch (s) {
    case ST_OK:      return "OK";
    case ST_WARMUP:  return "ПРОГРІВ: фільтр накопичує дані";
    case ST_NODATA:  return "УВАГА: немає даних від датчика";
    case ST_RANGE:   return "УВАГА: тиск поза діапазоном 300-1100 гПа";
    case ST_JUMP:    return "УВАГА: різкий стрибок тиску, відлік відкинуто";
    case ST_OFFLINE: return "УВАГА: датчик не відповідає";
  }
  return "";
}

// ---------- Канал вимірювання тиску ----------
class PressureChannel {
 public:
  const char* name;
  float  raw = NAN;        // останній прийнятий сирий відлік
  float  filtered = NAN;   // відфільтроване значення
  bool   valid = false;    // true, якщо filtered можна використовувати
  Status status = ST_OFFLINE;

  PressureChannel(const char* n, Adafruit_BMP280& b) : name(n), bmp(b) {}

  void begin() { tryBegin(); }

  void print() const {
    Serial.print(name);
    Serial.print(F(": сире = "));
    if (isnan(raw)) Serial.print(F("--")); else Serial.print(raw, 2);
    Serial.print(F(" гПа, фільтр = "));
    if (isnan(filtered)) Serial.print(F("--")); else Serial.print(filtered, 2);
    Serial.print(F(" гПа ["));
    Serial.print(statusText(status));
    Serial.println(F("]"));
  }

  void update() {
    if (!present) {
      if (millis() - lastTry >= RETRY_MS) tryBegin();
      if (!present) { status = ST_OFFLINE; valid = false; return; }
    }

    float p = bmp.readPressure() / 100.0F;  // гПа

    if (isnan(p) || p == 0.0F) {
      bad(ST_NODATA);
    } else if (p < P_MIN || p > P_MAX) {
      bad(ST_RANGE);
    } else if (hasLast && fabs(p - lastRaw) > MAX_JUMP && jumpCount < 2) {
      jumpCount++;   // два поспіль стрибки відкидаємо, третій приймаємо як новий рівень
      bad(ST_JUMP);
    } else {
      jumpCount = 0;
      accept(p);
    }
  }

 private:
  Adafruit_BMP280& bmp;
  bool present = false;
  unsigned long lastTry = 0;
  float buf[MEDIAN_N];
  uint8_t count = 0, idx = 0;
  bool emaInit = false;
  float lastRaw = 0;
  bool hasLast = false;
  uint8_t failCount = 0, jumpCount = 0;

  void tryBegin() {
    lastTry = millis();
    present = bmp.begin(I2C_ADDR);   // для SPI-екземпляра адреса ігнорується
    if (present) {
      bmp.setSampling(Adafruit_BMP280::MODE_NORMAL,
                      Adafruit_BMP280::SAMPLING_X2,
                      Adafruit_BMP280::SAMPLING_X16,
                      Adafruit_BMP280::FILTER_X16,
                      Adafruit_BMP280::STANDBY_MS_500);
    }
    resetFilter();
  }

  void resetFilter() {
    count = idx = 0;
    emaInit = hasLast = false;
    failCount = jumpCount = 0;
    filtered = raw = NAN;
    valid = false;
  }

  void bad(Status st) {
    status = st;
    valid = false;
    if (++failCount >= FAIL_LIMIT) {   // занадто багато поганих відліків підряд
      present = false;
      lastTry = millis();
      resetFilter();
      status = ST_OFFLINE;
    }
  }

  void accept(float p) {
    raw = lastRaw = p;
    hasLast = true;
    failCount = 0;

    buf[idx] = p;
    idx = (idx + 1) % MEDIAN_N;
    if (count < MEDIAN_N) count++;

    float med = median();
    if (!emaInit) { filtered = med; emaInit = true; }
    else          { filtered = EMA_ALPHA * med + (1.0F - EMA_ALPHA) * filtered; }

    valid  = (count >= MEDIAN_N);
    status = valid ? ST_OK : ST_WARMUP;
  }

  float median() {
    float t[MEDIAN_N];
    for (uint8_t i = 0; i < count; i++) t[i] = buf[i];
    for (uint8_t i = 1; i < count; i++) {          // сортування вставками
      float k = t[i]; int8_t j = i - 1;
      while (j >= 0 && t[j] > k) { t[j + 1] = t[j]; j--; }
      t[j + 1] = k;
    }
    if (count % 2) return t[count / 2];
    return (t[count / 2 - 1] + t[count / 2]) / 2.0F;
  }
};

// ---------- Датчики ----------
Adafruit_BMP280 bmpI2C(&Wire);   // I2C
Adafruit_BMP280 bmpSPI(BMP_CS);  // апаратний SPI

PressureChannel chI2C("I2C", bmpI2C);
PressureChannel chSPI("SPI", bmpSPI);

void setup() {
  Serial.begin(115200);
  while (!Serial) { }
  Wire.begin();
  chI2C.begin();
  chSPI.begin();
  Serial.println(F("Два BMP280 (I2C + SPI), фільтрація тиску"));
}

void loop() {
  static unsigned long last = 0;
  if (millis() - last < PERIOD_MS) return;
  last = millis();

  chI2C.update();
  chSPI.update();

  chI2C.print();
  chSPI.print();

  if (chI2C.valid && chSPI.valid) {
    float diff = fabs(chI2C.filtered - chSPI.filtered);
    Serial.print(F("Різниця між датчиками: "));
    Serial.print(diff, 2);
    Serial.println(F(" гПа"));
    if (diff > MAX_DIFF) {
      Serial.println(F("УВАГА: показання датчиків сильно відрізняються!"));
    }
  } else {
    Serial.println(F("УВАГА: дані одного датчика невалідні, порівняння неможливе"));
  }
  Serial.println();
}