#include "DisplayManager.h"
#include <Wire.h>
#include "../../core/AppConfig.h"

DisplayManager::DisplayManager()
  : _oled(U8G2_R0, U8X8_PIN_NONE) {
}

void DisplayManager::begin() {
  Wire.begin(AppConfig::Display::SDA_PIN, AppConfig::Display::SCL_PIN);
  _oled.setI2CAddress(AppConfig::Display::I2C_ADDRESS << 1);
  _oled.begin();
  _oled.setFont(u8g2_font_6x12_tf);
}

void DisplayManager::showBoot() {
  _oled.clearBuffer();
  _oled.drawStr(28, 28, "AdPet");
  _oled.drawStr(18, 45, "Booting...");
  _oled.sendBuffer();
}

void DisplayManager::drawEye(int x, int y, int w, int h) {
  _oled.drawRBox(x, y, w, h, 4);
}

void DisplayManager::drawClosedEye(int x, int y, int w) {
  _oled.drawLine(x, y, x + w, y);
  _oled.drawLine(x + 2, y + 1, x + w - 2, y + 1);
}

void DisplayManager::drawMouthSmile() {
  _oled.drawLine(52, 44, 58, 50);
  _oled.drawLine(58, 50, 66, 52);
  _oled.drawLine(66, 52, 74, 50);
  _oled.drawLine(74, 50, 80, 44);
}

void DisplayManager::drawMouthFlat() {
  _oled.drawLine(52, 48, 76, 48);
}

void DisplayManager::drawMouthOpen() {
  _oled.drawEllipse(64, 47, 8, 10);
}

void DisplayManager::drawFace(Emotion emotion) {
  _oled.clearBuffer();

  switch (emotion) {
    case EMOTION_IDLE:
      drawEye(34, 22, 18, 18);
      drawEye(76, 22, 18, 18);
      drawMouthFlat();
      break;

    case EMOTION_HAPPY:
      _oled.drawLine(33, 31, 40, 25);
      _oled.drawLine(40, 25, 50, 31);
      _oled.drawLine(75, 31, 84, 25);
      _oled.drawLine(84, 25, 95, 31);
      drawMouthSmile();
      break;

    case EMOTION_SLEEPY:
      drawClosedEye(32, 31, 22);
      drawClosedEye(74, 31, 22);
      _oled.drawStr(96, 18, "Z");
      _oled.drawStr(106, 11, "z");
      drawMouthFlat();
      break;

    case EMOTION_BLINK:
      drawClosedEye(32, 31, 22);
      drawClosedEye(74, 31, 22);
      drawMouthFlat();
      break;

    case EMOTION_SURPRISED:
      _oled.drawCircle(43, 31, 10);
      _oled.drawCircle(85, 31, 10);
      drawMouthOpen();
      break;

    case EMOTION_ANGRY:
      _oled.drawLine(31, 22, 54, 30);
      _oled.drawLine(74, 30, 97, 22);
      drawEye(36, 31, 14, 12);
      drawEye(78, 31, 14, 12);
      _oled.drawLine(54, 50, 74, 45);
      break;

    case EMOTION_THINKING:
      drawEye(34, 24, 16, 16);
      drawEye(78, 24, 16, 16);
      _oled.drawCircle(62, 49, 2);
      _oled.drawCircle(70, 49, 2);
      _oled.drawCircle(78, 49, 2);
      break;

    case EMOTION_TALKING:
      drawEye(34, 22, 18, 18);
      drawEye(76, 22, 18, 18);
      _oled.drawRFrame(54, 46, 20, 8, 2);
      break;
  }

  _oled.sendBuffer();
}
