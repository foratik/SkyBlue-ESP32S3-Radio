#include "Common.h"
#include "Refigh.h"
#include "RefighDecision.h"
#include "RefighSignature.h"
#include "RefighAssets.h"
#include "BootScreen.h"
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
  char message[256];
  const int length = snprintf(message, sizeof(message), "Refigh: enabled=%u antenna=%s samples=%u stations=%u RSSI=%u SNR=%u method=%s distance-present=%.2f distance-absent=%.2f\n",
                refighEnabled, estimate, validSamples, stableStations, bestRssi, bestSnr,
                usedSignature ? "signature" : "stations", presentDistance, absentDistance);
  if (length > 0 && length < (int)sizeof(message) && Serial.availableForWrite() >= length)
    Serial.write((const uint8_t *)message, length);
}

static bool outputJpeg(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t *pixels)
{
  if (x < 0 || y < 0 || x + w > spr.width() || y + h > spr.height()) return false;
  spr.pushImage(x, y, w, h, pixels);
  return true;
}

void refighPlay()
{
  // Compose text AND frame zero off-screen. No USB writes may sit between
  // presenting the composite and starting playback: CDC backpressure can wait 2s.
  const uint32_t preparing = millis();
  const bool oldSpriteSwap = spr.getSwapBytes();
  const bool oldDecoderSwap = TJpgDec._swap;
  const auto oldCallback = TJpgDec.tft_output;
  const auto restoreDecoder = [&]() {
    spr.setSwapBytes(oldSpriteSwap);
    TJpgDec.setSwapBytes(oldDecoderSwap);
    TJpgDec.setCallback(oldCallback);
  };
  spr.setSwapBytes(false);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setJpgScale(1);
  TJpgDec.setCallback(outputJpeg);
  spr.fillSprite(TFT_BLACK);
  if (TJpgDec.drawJpg(96, 0, refighPanel, sizeof(refighPanel)) != JDR_OK) {
    Serial.println("Refigh: text panel decode failed");
    restoreDecoder();
    return;
  }
  const RefighFrame &first = refighFrames[0];
  if (TJpgDec.drawJpg(0, 0, refighVideo + first.offset, first.length) != JDR_OK) {
    restoreDecoder();
    Serial.println("Refigh: first frame decode failed");
    return;
  }
  const uint32_t preparedMs = millis() - preparing;
  spr.pushSprite(0, 0);
  const uint32_t started = millis();
  unsigned played = 1;
  uint32_t previousFrame = 0;
  bool decodeFailed = false;
  while (true) {
    const uint32_t frame = (millis() - started) / REFIGH_FRAME_MS;
    if (frame >= REFIGH_FRAME_COUNT) break;
    if (digitalRead(ENCODER_PUSH_BUTTON) == LOW) break;
    if (frame == previousFrame) { delay(1); continue; }
    const RefighFrame &entry = refighFrames[frame];
    if (TJpgDec.drawJpg(0, 0, refighVideo + entry.offset, entry.length) != JDR_OK) {
      decodeFailed = true;
      break;
    }
    spr.pushSprite(0, 0, 0, 0, 96, 170);
    previousFrame = frame;
    ++played;
  }
  const uint32_t elapsed = millis() - started;
  restoreDecoder();
  // Keep diagnostics outside the timed/visible playback path.
  char message[192];
  const int length = snprintf(message, sizeof(message), "Refigh: playback finished frames=%u elapsed=%lu ms prepare=%lu ms first-frame=paired error=%u\n",
                played, (unsigned long)elapsed, (unsigned long)preparedMs, decodeFailed);
  if (length > 0 && length < (int)sizeof(message) && Serial.availableForWrite() >= length)
    Serial.write((const uint8_t *)message, length);
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
  if (!refighShowCheckIntro()) return RefighAntenna::Unknown;
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
    tft.fillRect(30, 162, width, 3, TFT_ORANGE);
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
