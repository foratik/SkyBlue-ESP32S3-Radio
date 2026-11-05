#include "Common.h"
#include "Themes.h"
#include "BootScreen.h"
#include "BootScreenAssets.h"

static uint16_t whiteWithAlpha(uint8_t alpha)
{
  return ((uint16_t)(alpha >> 3) << 11) | ((uint16_t)(alpha >> 2) << 5) | (alpha >> 3);
}

static void drawCheckFrame(uint16_t frame, uint8_t face)
{
  spr.fillSprite(TFT_BLACK);
  for (unsigned y = 0; y < BOOT_LOGO_HEIGHT; ++y) {
    for (unsigned x = 0; x < BOOT_LOGO_WIDTH; ++x) {
      const unsigned index = y * BOOT_LOGO_WIDTH + x;
      const uint8_t order = bootLogoReveal[index];
      if (order == 255 || frame < order) continue;
      uint8_t alpha = bootLogoAlpha[index];
      const unsigned age = frame - order;
      if (age < 2) alpha = (uint16_t)alpha * (age + 1) / 3;
      if (alpha) spr.drawPixel(43 + x, 2 + y, whiteWithAlpha(alpha));
    }
  }
  const unsigned offset = (face % 4) * BOOT_FACE_WIDTH * BOOT_FACE_HEIGHT;
  for (unsigned y = 0; y < BOOT_FACE_HEIGHT; ++y) {
    for (unsigned x = 0; x < BOOT_FACE_WIDTH; ++x) {
      const uint16_t color = bootFacePixels[offset + y * BOOT_FACE_WIDTH + x];
      if (color) spr.drawPixel(129 + x, 85 + y, color);
    }
  }
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(TFT_WHITE, TFT_BLACK);
  spr.drawString("Checking antenna...", 160, 146, 2);
  spr.pushSprite(0, 0);
}

bool refighShowCheckIntro()
{
  constexpr uint16_t FRAME_MS = 40;
  constexpr uint16_t DURATION_MS = (BOOT_WRITE_STEPS + 2) * FRAME_MS;
  const uint32_t started = millis();
  unsigned rendered = 0;
  Serial.println("Refigh screen: handwriting start");
  while ((uint32_t)(millis() - started) < DURATION_MS) {
    const uint16_t frame = (millis() - started) / FRAME_MS;
    drawCheckFrame(frame, (frame / 6) % 4);
    ++rendered;
    if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) {
      spr.setTextColor(TH.text, TH.bg);
      Serial.println("Refigh screen: cancelled");
      return false;
    }
    while ((uint32_t)(millis() - started) < (frame + 1) * FRAME_MS) delay(1);
  }
  drawCheckFrame(BOOT_WRITE_STEPS + 2, 0);
  spr.setTextColor(TH.text, TH.bg);
  Serial.printf("Refigh screen: handwriting ready frames=%u elapsed=%lu ms\n", rendered,
                (unsigned long)(millis() - started));
  return true;
}
