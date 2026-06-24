#pragma once

#include <Arduino.h>
#include <U8g2lib.h>
#include "../../core/Emotion.h"

class DisplayManager {
public:
  DisplayManager();

  void begin();
  void showBoot();
  void drawFace(Emotion emotion);
  void showStatus(const String& status);

private:
  U8G2_SSD1306_128X64_NONAME_F_HW_I2C _oled;

  void drawEye(int x, int y, int w, int h);
  void drawClosedEye(int x, int y, int w);
  void drawMouthSmile();
  void drawMouthFlat();
  void drawMouthOpen();
};
