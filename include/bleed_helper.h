#pragma once

#include <stdint.h>
#include <vector>
#include "config.h"

// Applies bleed margin per side with rounded corners and black border to raw 24-bit RGB buffer.
// Canvas is scaled proportionally so final card corresponds to finalCardWidthMm x finalCardHeightMm.
bool ApplyCardBleed(const uint8_t* srcRgb, int srcW, int srcH,
                    std::vector<uint8_t>& outRgb, int& outW, int& outH,
                    const BleedConfig& cfg);

// Applies bleed margin and rounded corners to 8-bit grayscale hologram mask (outer bleed set to 0).
bool ApplyHoloMaskBleed(const uint8_t* srcGray, int srcW, int srcH,
                        std::vector<uint8_t>& outGray, int& outW, int& outH,
                        const BleedConfig& cfg);
