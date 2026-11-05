#include "Common.h"
#include "Refigh.h"
#include "RefighDecision.h"
#include "RefighSignature.h"
#include "RefighAssets.h"
#include <Preferences.h>
#include <TJpg_Decoder.h>
#include <Wire.h>

bool refighEnabled = true;
static RefighAntenna lastAntenna = RefighAntenna::Unknown;
static unsigned validSamples = 0, stableStations = 0;
static uint8_t bestRssi = 0, bestSnr = 0;
struct RefighSample { uint16_t frequency; uint8_t rssi; uint8_t snr; };
static RefighSample samples[206];
static unsigned sampleCount = 0;
static bool usedSignature = false;
static float presentDistance = 0, absentDistance = 0;

void refighPrintSamples()
{
  for (unsigned i = 0; i < sampleCount; ++i)
    Serial.printf("Refigh sample: %u,%u,%u\n", samples[i].frequency, samples[i].rssi, samples[i].snr);
}

void refighLoad()
{
  Preferences prefs;
  if (prefs.begin("refigh", false)) {
    refighEnabled = prefs.getBool("enabled", true);
    prefs.end();
  }
}

void refighSetEnabled(bool enabled)
{
  Preferences prefs;
  if (!prefs.begin("refigh", false)) {
    Serial.println("Refigh: preference storage unavailable");
    return;
  }
  if (prefs.putBool("enabled", enabled) == sizeof(bool)) refighEnabled = enabled;
  else Serial.println("Refigh: preference save failed");
  prefs.end();
  refighPrintStatus();
}

void refighPrintStatus()
{
  const char *estimate = lastAntenna == RefighAntenna::LikelyPresent ? "likely-present" :
                         lastAntenna == RefighAntenna::LikelyAbsent ? "likely-absent" : "unknown";
  Serial.printf("Refigh: enabled=%u antenna=%s samples=%u stations=%u RSSI=%u SNR=%u method=%s distance-present=%.2f distance-absent=%.2f\n",
                refighEnabled, estimate, validSamples, stableStations, bestRssi, bestSnr,
                usedSignature ? "signature" : "stations", presentDistance, absentDistance);
}

static bool outputJpeg(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *pixels)
{
  if (x < 0 || y < 0 || x + w > tft.width() || y + h > tft.height()) return false;
  tft.pushImage(x, y, w, h, pixels);
  return true;
}

void refighPlay()
{
  // No rotation change: video left, properly shaped Persian text right.
  const bool oldSwap = tft.getSwapBytes();
  const bool oldDecoderSwap = TJpgDec._swap;
  tft.setSwapBytes(false);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(outputJpeg);
  tft.fillScreen(TFT_BLACK);
  if (TJpgDec.drawJpg(96, 0, refighPanel, sizeof(refighPanel)) != JDR_OK) {
    Serial.println("Refigh: text panel decode failed");
    tft.setSwapBytes(oldSwap);
    TJpgDec.setSwapBytes(oldDecoderSwap);
    return;
  }
  Serial.printf("Refigh: playing %u silent frames\n", REFIGH_FRAME_COUNT);
  const uint32_t started = millis();
  unsigned played = 0;
  while (true) {
    const uint32_t frame = (millis() - started) / REFIGH_FRAME_MS;
    if (frame >= REFIGH_FRAME_COUNT) break;
    const RefighFrame &entry = refighFrames[frame];
    if (TJpgDec.drawJpg(0, 0, refighVideo + entry.offset, entry.length) != JDR_OK) {
      Serial.printf("Refigh: video decode failed at frame %lu\n", (unsigned long)frame);
      break;
    }
    ++played;
    // Skip with the physical encoder button; never alter tuning/settings.
    if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) break;
    while ((uint32_t)(millis() - started) < (frame + 1) * REFIGH_FRAME_MS) delay(1);
  }
  Serial.printf("Refigh: playback finished frames=%u elapsed=%lu ms\n", played,
                (unsigned long)(millis() - started));
  tft.setSwapBytes(oldSwap);
  TJpgDec.setSwapBytes(oldDecoderSwap);
}

static RefighAntenna estimateAntenna()
{
  // A silent FM sweep tests reception, not DC continuity. It can misclassify
  // strong nearby stations without an antenna, or a connected HF-only antenna.
  validSamples = stableStations = 0;
  sampleCount = 0;
  usedSignature = false;
  presentDistance = absentDistance = 0;
  bestRssi = bestSnr = 0;
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Refigh", 160, 53, 4);
  tft.drawString("Checking antenna...", 160, 88, 2);
  bool radioPresent = false;
  for (uint8_t address : {uint8_t(0x11), uint8_t(0x63)}) {
    Wire.beginTransmission(address);
    radioPresent |= Wire.endTransmission() == 0;
  }
  if (!radioPresent) return RefighAntenna::Unknown;
  rx.setFM(6400, 10800, 8750, 20);
  // Match the normal FM path, including the Si4732 GPIO-controlled RF switch.
  rx.setGpioCtl(1, 0, 0);
  rx.setGpio(0, 0, 0);
  rx.setAutomaticGainControl(0, 0);
  rx.setVolume(0);
  rx.setAudioMute(true);
  delay(100);
  uint16_t previousStation = 0;
  for (uint16_t frequency = 8750; frequency <= 10800; frequency += 10) {
    if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) return RefighAntenna::Unknown;
    rx.setFrequency(frequency);
    rx.getFrequency();
    if (rx.getStatusError()) continue;
    rx.getCurrentReceivedSignalQuality();
    ++validSamples;
    const uint8_t rssi = rx.getCurrentRSSI(), snr = rx.getCurrentSNR();
    if (sampleCount < 206) samples[sampleCount++] = {frequency, rssi, snr};
    if (rssi >= 16 && snr >= 4 && (!previousStation || frequency - previousStation >= 40)) {
      bool stable = true;
      // Let AGC and SNR settle at candidate channels before deciding.
      delay(80);
      uint8_t lowRssi = 255, lowSnr = 255;
      for (unsigned repeat = 0; repeat < 3; ++repeat) {
        delay(20);
        rx.getCurrentReceivedSignalQuality();
        lowRssi = min(lowRssi, rx.getCurrentRSSI());
        lowSnr = min(lowSnr, rx.getCurrentSNR());
        stable &= lowRssi >= REFIGH_MIN_RSSI && lowSnr >= REFIGH_MIN_SNR;
      }
      if (stable) {
        ++stableStations;
        previousStation = frequency;
        if (lowRssi > bestRssi) { bestRssi = lowRssi; bestSnr = lowSnr; }
      }
    }
    if (refighDecision(validSamples, stableStations, bestRssi, bestSnr) == RefighAntenna::LikelyPresent)
      return RefighAntenna::LikelyPresent;
    const unsigned width = (frequency - 8750) * 260UL / (10800 - 8750);
    tft.fillRect(30, 120, width, 3, TFT_ORANGE);
    delay(1);
  }
  if (!validSamples) return RefighAntenna::Unknown;
  float signature[8] = {};
  unsigned counts[8] = {};
  for (unsigned i = 0; i < sampleCount; ++i) {
    const unsigned bin = min(unsigned((samples[i].frequency - 8750) / 250), 7u);
    signature[bin] += samples[i].rssi;
    ++counts[bin];
  }
  for (unsigned i = 0; i < 8; ++i) {
    if (counts[i] < 15) return RefighAntenna::Unknown;
    signature[i] /= counts[i];
  }
  usedSignature = true;
  return refighSignatureDecision(signature, &presentDistance, &absentDistance);
}

void refighBoot()
{
  if (!refighEnabled) { refighPrintStatus(); return; }
  lastAntenna = estimateAntenna();
  refighPrintStatus();
  if (lastAntenna == RefighAntenna::LikelyAbsent) refighPlay();
  // setup() restores the saved band/mode/frequency/volume after this returns.
}
