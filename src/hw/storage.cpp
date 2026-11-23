#include "storage.h"

#include <Preferences.h>

namespace hw {

namespace {
constexpr const char* kNs = "plaitsputer";
constexpr uint32_t kPatchVersion = 1;
}  // namespace

void loadAll(ps::Synth& synth, DeviceSettings& dev) {
  Preferences p;
  if (!p.begin(kNs, true)) return;
  synth.midiChannel = p.getUChar("ch", 0);
  if (synth.midiChannel > 16) synth.midiChannel = 0;
  uint8_t enc = p.getUChar("enc", ps::ENC_ABSOLUTE);
  synth.encoderMode = enc < ps::ENC_MODE_COUNT ? ps::EncoderMode(enc) : ps::ENC_ABSOLUTE;
  if (p.getBytesLength("ovr") == sizeof(synth.ccModeOverride)) {
    p.getBytes("ovr", synth.ccModeOverride, sizeof(synth.ccModeOverride));
    for (auto& o : synth.ccModeOverride) if (o > ps::ENC_MODE_COUNT) o = 0;
  }
  dev.speakerVolume = p.getUChar("spkvol", dev.speakerVolume);
  p.end();

  float params[ps::P_COUNT];
  if (loadPatchInto(params)) {
    for (int i = 0; i < ps::P_COUNT; ++i) synth.setParam(ps::ParamId(i), params[i]);
  }
}

void saveSettings(const ps::Synth& synth, const DeviceSettings& dev) {
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUChar("ch", synth.midiChannel);
  p.putUChar("enc", synth.encoderMode);
  p.putBytes("ovr", synth.ccModeOverride, sizeof(synth.ccModeOverride));
  p.putUChar("spkvol", dev.speakerVolume);
  p.end();
}

void savePatch(const ps::Synth& synth) {
  float params[ps::P_COUNT];
  for (int i = 0; i < ps::P_COUNT; ++i) params[i] = synth.param(ps::ParamId(i));
  Preferences p;
  if (!p.begin(kNs, false)) return;
  p.putUInt("pver", kPatchVersion);
  p.putUInt("pcount", ps::P_COUNT);
  p.putBytes("patch", params, sizeof(params));
  p.end();
}

bool loadPatchInto(float* params) {
  Preferences p;
  if (!p.begin(kNs, true)) return false;
  bool ok = p.getUInt("pver", 0) == kPatchVersion &&
            p.getUInt("pcount", 0) == ps::P_COUNT &&
            p.getBytesLength("patch") == sizeof(float) * ps::P_COUNT;
  if (ok) p.getBytes("patch", params, sizeof(float) * ps::P_COUNT);
  p.end();
  return ok;
}

}  // namespace hw
