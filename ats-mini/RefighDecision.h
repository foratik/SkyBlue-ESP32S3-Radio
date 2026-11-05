#pragma once
#include <stdint.h>

enum class RefighAntenna : uint8_t { Unknown, LikelyAbsent, LikelyPresent };
constexpr uint8_t REFIGH_MIN_RSSI = 18;
constexpr uint8_t REFIGH_MIN_SNR = 7;

// RF reception is a proxy, never proof of a physical antenna connection.
inline RefighAntenna refighDecision(unsigned validSamples, unsigned stableStations,
                                   uint8_t strongestRssi, uint8_t strongestSnr)
{
  if (!validSamples) return RefighAntenna::Unknown;
  // A station is counted only after three consistent RSSI/SNR measurements.
  if (stableStations >= 1 && strongestRssi >= REFIGH_MIN_RSSI && strongestSnr >= REFIGH_MIN_SNR)
    return RefighAntenna::LikelyPresent;
  return RefighAntenna::LikelyAbsent;
}
