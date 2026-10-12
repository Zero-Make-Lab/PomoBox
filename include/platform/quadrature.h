// Half-quadrature counting, as the ESP32 pulse counter does it for ESP32Encoder::attachHalfQuad(): one count per
// edge of A, +1 when A and B are equal after the edge, -1 when they differ (2 counts per detent on the EN11 and
// the KY-040). No Arduino dependencies, so the host unit tests (pio test -e native) cover it.
#pragma once
#include <stdint.h>

// state = (A << 1) | B. Returns the count change for the move from prev to cur.
inline int halfQuadStep(uint8_t prev, uint8_t cur) {
  // B alone moving does not count, and A and B both moving means an edge was missed: direction unknown
  if ((prev ^ cur) != 2) return 0;
  return ((cur >> 1) == (cur & 1)) ? 1 : -1;
}
