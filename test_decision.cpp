#include "ats-mini/RefighDecision.h"
#include "ats-mini/RefighSignature.h"
#include <cassert>

int main()
{
  // No valid receiver measurements must never be called a disconnected antenna.
  assert(refighDecision(0, 0, 0, 0) == RefighAntenna::Unknown);
  // A quiet sweep and high noise without usable stations both take the warning path.
  assert(refighDecision(103, 0, 7, 0) == RefighAntenna::LikelyAbsent);
  assert(refighDecision(103, 0, 65, 0) == RefighAntenna::LikelyAbsent);
  // Stable stations suppress the video, including the actual connected-antenna case.
  assert(refighDecision(27, 2, 28, 12) == RefighAntenna::LikelyPresent);
  assert(refighDecision(12, 1, 40, 14) == RefighAntenna::LikelyPresent);
  assert(refighDecision(103, 1, 23, 12) == RefighAntenna::LikelyPresent);
  // A transient hit (not stable) and insufficient signal quality still warn.
  assert(refighDecision(103, 0, 23, 12) == RefighAntenna::LikelyAbsent);
  assert(refighDecision(206, 1, 18, 7) == RefighAntenna::LikelyPresent);
  assert(refighDecision(206, 1, 23, 6) == RefighAntenna::LikelyAbsent);
  const float present[8] = {7.24f,3.40f,1.92f,2.24f,4.48f,3.96f,5.96f,8.16f};
  const float absent[8] = {2.64f,2.40f,3.24f,6.04f,6.12f,6.52f,6.04f,4.45f};
  assert(refighSignatureDecision(present) == RefighAntenna::LikelyPresent);
  assert(refighSignatureDecision(absent) == RefighAntenna::LikelyAbsent);
  float shifted[8], midpoint[8], unfamiliar[8];
  for (unsigned i=0; i<8; ++i) {
    shifted[i] = present[i] + 15;
    midpoint[i] = (present[i] + absent[i]) / 2;
    unfamiliar[i] = i % 2 ? 30 : 0;
  }
  assert(refighSignatureDecision(shifted) == RefighAntenna::LikelyPresent);
  assert(refighSignatureDecision(midpoint) == RefighAntenna::Unknown);
  assert(refighSignatureDecision(unfamiliar) == RefighAntenna::Unknown);
}
