#pragma once

#include <stdint.h>

#include "synth/synth.h"

namespace hw {

// Starts the audio task (core 1) and the render worker (core 0).
// The speaker must already be configured and started.
bool audioBegin(ps::Synth* synth);

float audioLoad();        // smoothed render time / real time (0..1+)
float audioPeakLoad();    // peak since last reset
void audioResetPeak();
uint32_t audioShedCount();  // voices shed due to overload
uint32_t audioLastActivityMs();  // millis() when a voice was last active

}  // namespace hw
