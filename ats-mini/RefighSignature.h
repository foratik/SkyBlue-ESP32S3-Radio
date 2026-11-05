#pragma once
#include "RefighDecision.h"
#include <math.h>

// Eight 2.5 MHz RSSI averages measured on this receiver with the user's antenna
// attached/detached. Centering rejects an overall gain/noise-floor offset.
// This is a calibrated RF fingerprint, not a connector continuity measurement.
inline RefighAntenna refighSignatureDecision(const float values[8],
                                            float *presentDistance = nullptr,
                                            float *absentDistance = nullptr)
{
  const float present[8] = {7.24f,3.40f,1.92f,2.24f,4.48f,3.96f,5.96f,8.16f};
  const float absent[8] = {2.64f,2.40f,3.24f,6.04f,6.12f,6.52f,6.04f,4.45f};
  float mean = 0, presentMean = 0, absentMean = 0;
  for (unsigned i = 0; i < 8; ++i) {
    mean += values[i] / 8;
    presentMean += present[i] / 8;
    absentMean += absent[i] / 8;
  }
  float presentError = 0, absentError = 0;
  for (unsigned i = 0; i < 8; ++i) {
    const float centered = values[i] - mean;
    const float p = centered - (present[i] - presentMean);
    const float a = centered - (absent[i] - absentMean);
    presentError += p*p / 8;
    absentError += a*a / 8;
  }
  const float pd = sqrtf(presentError), ad = sqrtf(absentError);
  if (presentDistance) *presentDistance = pd;
  if (absentDistance) *absentDistance = ad;
  // Reject unfamiliar/ambiguous conditions instead of calling them disconnected.
  if (pd <= 2.5f && pd + 1.0f < ad) return RefighAntenna::LikelyPresent;
  if (ad <= 2.5f && ad + 1.0f < pd) return RefighAntenna::LikelyAbsent;
  return RefighAntenna::Unknown;
}
