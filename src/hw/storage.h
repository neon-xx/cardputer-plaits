#pragma once

#include "synth/synth.h"

namespace hw {

struct DeviceSettings {
  uint8_t speakerVolume = 128;   // M5 speaker master volume 0..255
};

// Load / save synth settings (channel, encoder modes) and the patch from NVS.
// Saving writes flash, which briefly stalls both cores: only call while the
// synth is silent.
void loadAll(ps::Synth& synth, DeviceSettings& dev);  // call before audio starts
void saveSettings(const ps::Synth& synth, const DeviceSettings& dev);
void savePatch(const ps::Synth& synth);
bool loadPatchInto(float* params);  // returns false if none stored

}  // namespace hw
