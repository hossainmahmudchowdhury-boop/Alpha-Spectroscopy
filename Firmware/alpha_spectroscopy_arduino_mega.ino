#include <Wire.h>
#include <SPI.h>
#include <Adafruit_AS7341.h>
#include <Arduino_GFX_Library.h>

// Confirm the display controller and pin labels against your module before use.
// These pins are examples for a SPI TFT connected to an Arduino Mega 2560.
#define TFT_CS   53
#define TFT_DC   49
#define TFT_RST  48
#define TFT_BL   47

Arduino_DataBus *bus = new Arduino_HWSPI(TFT_DC, TFT_CS);
Arduino_GFX *gfx = new Arduino_ILI9488_18bit(bus, TFT_RST, 0, false);

Adafruit_AS7341 as7341;

const uint16_t BG = 0x0000;
const uint16_t WHITE = 0xFFFF;
const uint16_t CYAN = 0x07FF;
const uint16_t GREEN = 0x07E0;
const uint16_t YELLOW = 0xFFE0;
const uint16_t RED = 0xF800;
const uint16_t GRID = 0x4208;

const uint16_t channelWavelengths[8] = {415, 445, 480, 515, 555, 590, 630, 680};
uint16_t channelValues[12];
uint16_t previousPlotY[8];
bool previousPlotValid = false;

const int SCREEN_W = 320;
const int SCREEN_H = 480;
const int GRAPH_X = 42;
const int GRAPH_Y = 118;
const int GRAPH_W = 260;
const int GRAPH_H = 230;

void drawHeader() {
  gfx->fillScreen(BG);
  gfx->setTextColor(CYAN);
  gfx->setTextSize(2);
  gfx->setCursor(12, 12);
  gfx->println("ALPHA SPECTRAL VIEW");

  gfx->drawFastHLine(10, 40, 300, CYAN);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(12, 50);
  gfx->println("AS7341 multispectral sensor");
  gfx->setCursor(12, 64);
  gfx->println("Discrete channels - not Raman shift");

  gfx->drawRect(GRAPH_X, GRAPH_Y, GRAPH_W, GRAPH_H, WHITE);
  for (int i = 1; i < 5; i++) {
    int y = GRAPH_Y + (GRAPH_H * i) / 5;
    gfx->drawFastHLine(GRAPH_X + 1, y, GRAPH_W - 2, GRID);
  }
  for (int i = 1; i < 8; i++) {
    int x = GRAPH_X + (GRAPH_W * i) / 8;
    gfx->drawFastVLine(x, GRAPH_Y + 1, GRAPH_H - 2, GRID);
  }

  gfx->setTextColor(WHITE);
  gfx->setCursor(12, 100);
  gfx->print("Signal");
  gfx->setCursor(116, 358);
  gfx->print("Wavelength (nm)");

  gfx->setCursor(12, 390);
  gfx->print("F1 415  F2 445  F3 480  F4 515");
  gfx->setCursor(12, 404);
  gfx->print("F5 555  F6 590  F7 630  F8 680");
  gfx->setCursor(12, 428);
  gfx->print("CLEAR:");
  gfx->setCursor(12, 443);
  gfx->print("NIR:");
}

void drawWavelengthLabels() {
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  for (int i = 0; i < 8; i++) {
    int x = GRAPH_X + (i * (GRAPH_W - 1)) / 7;
    gfx->setCursor(x - 8, GRAPH_Y + GRAPH_H + 5);
    gfx->print(channelWavelengths[i]);
  }
}

void showSensorError() {
  gfx->fillScreen(BG);
  gfx->setTextColor(RED);
  gfx->setTextSize(2);
  gfx->setCursor(12, 20);
  gfx->println("AS7341 NOT FOUND");
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(12, 60);
  gfx->println("Check sensor power, GND, SDA and SCL.");
  gfx->setCursor(12, 78);
  gfx->println("Mega SDA = pin 20; SCL = pin 21.");
}

void drawSpectrum() {
  uint16_t maxValue = 1;
  for (int i = 0; i < 8; i++) {
    if (channelValues[i] > maxValue) maxValue = channelValues[i];
  }

  gfx->fillRect(GRAPH_X + 1, GRAPH_Y + 1, GRAPH_W - 2, GRAPH_H - 2, BG);
  for (int i = 1; i < 5; i++) {
    int y = GRAPH_Y + (GRAPH_H * i) / 5;
    gfx->drawFastHLine(GRAPH_X + 1, y, GRAPH_W - 2, GRID);
  }
  for (int i = 1; i < 8; i++) {
    int x = GRAPH_X + (GRAPH_W * i) / 8;
    gfx->drawFastVLine(x, GRAPH_Y + 1, GRAPH_H - 2, GRID);
  }

  for (int i = 0; i < 8; i++) {
    int x = GRAPH_X + 2 + (i * (GRAPH_W - 5)) / 7;
    int y = GRAPH_Y + GRAPH_H - 3 -
            ((uint32_t)channelValues[i] * (GRAPH_H - 8)) / maxValue;
    if (i > 0) {
      int prevX = GRAPH_X + 2 + ((i - 1) * (GRAPH_W - 5)) / 7;
      int prevY = GRAPH_Y + GRAPH_H - 3 -
                  ((uint32_t)channelValues[i - 1] * (GRAPH_H - 8)) / maxValue;
      gfx->drawLine(prevX, prevY, x, y, GREEN);
    }
    gfx->fillCircle(x, y, 3, YELLOW);
  }

  gfx->fillRect(54, 370, 250, 14, BG);
  gfx->setTextColor(WHITE);
  gfx->setTextSize(1);
  gfx->setCursor(54, 372);
  gfx->print("Max channel count: ");
  gfx->print(maxValue);

  gfx->fillRect(55, 428, 250, 12, BG);
  gfx->setCursor(55, 428);
  gfx->print(channelValues[8]);

  gfx->fillRect(55, 443, 250, 12, BG);
  gfx->setCursor(55, 443);
  gfx->print(channelValues[9]);
}

void setup() {
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  Serial.begin(115200);
  Wire.begin();

  gfx->begin();
  gfx->setRotation(0);
  drawHeader();
  drawWavelengthLabels();

  if (!as7341.begin()) {
    showSensorError();
    while (true) delay(100);
  }

  as7341.setATIME(100);
  as7341.setASTEP(999);
  as7341.setGain(AS7341_GAIN_4X);

  gfx->setTextColor(GREEN);
  gfx->setCursor(12, 86);
  gfx->print("Sensor connected");
}

void loop() {
  if (!as7341.readAllChannels(channelValues)) {
    gfx->setTextColor(RED);
    gfx->setCursor(12, 462);
    gfx->print("Read failed      ");
    delay(500);
    return;
  }

  drawSpectrum();

  Serial.print("F1, F2, F3, F4, F5, F6, F7, F8, CLEAR, NIR: ");
  for (int i = 0; i < 10; i++) {
    Serial.print(channelValues[i]);
    if (i < 9) Serial.print(", ");
  }
  Serial.println();

  delay(500);
}
