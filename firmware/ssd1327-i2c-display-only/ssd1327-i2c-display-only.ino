#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1327.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 128
#define OLED_RESET -1

Adafruit_SSD1327 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
bool displayReady = false;

struct I2cPins {
  int sda;
  int scl;
};

const I2cPins pinCandidates[] = {
#ifdef ESP32_WROOM_TEST
  {34, 35},
  {35, 34},
  {21, 22},
  {22, 21},
  {18, 19},
  {19, 18},
  {16, 17},
  {17, 16}
#else
  {47, 48},
  {48, 47},
  {8, 3},
  {3, 8},
  {9, 10},
  {10, 9},
  {14, 21},
  {21, 14},
  {41, 42},
  {42, 41}
#endif
};

const uint8_t displayAddresses[] = {0x3C, 0x3D};

bool isValidEsp32S3Gpio(int pin) {
#ifdef ESP32_WROOM_TEST
  if (pin < 0 || pin > 39) return false;
  if (pin >= 6 && pin <= 11) return false;
  if (pin >= 34 && pin <= 39) return false;
  return true;
#else
  if (pin < 0 || pin > 48) return false;
  if (pin >= 22 && pin <= 25) return false;
  return true;
#endif
}

bool scanAddress(uint8_t address) {
  Wire.beginTransmission(address);
  return Wire.endTransmission() == 0;
}

void scanBus() {
  Serial.println("Scanning I2C bus...");
  bool foundAny = false;

  for (uint8_t address = 1; address < 127; address++) {
    if (scanAddress(address)) {
      Serial.print("I2C device found at 0x");
      if (address < 16) Serial.print("0");
      Serial.println(address, HEX);
      foundAny = true;
    }
    delay(2);
  }

  if (!foundAny) {
    Serial.println("No I2C devices found on this pin pair.");
  }
}

bool tryDisplayOnPins(int sda, int scl) {
  Serial.println();
  Serial.print("Trying SDA=");
  Serial.print(sda);
  Serial.print(" SCL=");
  Serial.println(scl);

  if (!isValidEsp32S3Gpio(sda) || !isValidEsp32S3Gpio(scl)) {
    Serial.println("Skipping invalid ESP32 GPIO pair. On ESP32-WROOM, GPIO34-39 are input-only and cannot be I2C SDA/SCL.");
    return false;
  }

  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  Wire.end();
  Wire.begin(sda, scl);
  Wire.setClock(100000);
  Wire.setTimeOut(20);

  scanBus();

  for (uint8_t i = 0; i < sizeof(displayAddresses); i++) {
    uint8_t address = displayAddresses[i];
    if (!scanAddress(address)) continue;

    Serial.print("Trying SSD1327 init at 0x");
    if (address < 16) Serial.print("0");
    Serial.println(address, HEX);

    if (display.begin(address)) {
      displayReady = true;
      Serial.println("SSD1327 initialized successfully.");

      display.clearDisplay();
      display.setTextColor(SSD1327_WHITE);
      display.setTextSize(1);
      display.setCursor(0, 0);
      display.println("SSD1327 OK");
      display.print("SDA ");
      display.print(sda);
      display.print(" SCL ");
      display.println(scl);
      display.print("ADDR 0x");
      if (address < 16) display.print("0");
      display.println(address, HEX);
      display.drawRect(0, 32, 128, 64, SSD1327_WHITE);
      display.drawCircle(64, 64, 22, SSD1327_WHITE);
      display.fillCircle(64, 64, 8, SSD1327_WHITE);
      display.display();

      return true;
    }

    Serial.println("SSD1327 begin failed even though address ACKed.");
  }

  return false;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("SSD1327 I2C display-only test");

  for (uint8_t i = 0; i < sizeof(pinCandidates) / sizeof(pinCandidates[0]); i++) {
    if (tryDisplayOnPins(pinCandidates[i].sda, pinCandidates[i].scl)) {
      Serial.println("Display test running. You should see text/shapes on OLED.");
      return;
    }
  }

  Serial.println();
  Serial.println("SSD1327 was not found.");
  Serial.println("Check VCC, GND, SDA, SCL, and whether the module uses 0x3C or 0x3D.");
}

void loop() {
  if (!displayReady) {
    delay(1000);
    return;
  }

  static unsigned long lastBlinkMs = 0;
  static bool inverted = false;

  if (millis() - lastBlinkMs >= 1000) {
    lastBlinkMs = millis();
    inverted = !inverted;
    display.invertDisplay(inverted);
    Serial.println(inverted ? "Display invert ON" : "Display invert OFF");
  }
}
