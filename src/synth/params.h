// Parameter table shared by MIDI CC handling, the on-device UI and patch
// storage. Every parameter is stored as a normalised float in [0, 1].
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {

constexpr int kMaxVoices = 8;      // upper bound; actual count set by RAM at boot
constexpr int kNumEngines = 24;    // Plaits 1.2 engine count

enum ParamId : uint8_t {
  // Plaits
  P_ENGINE,
  P_HARMONICS,
  P_TIMBRE,
  P_MORPH,
  P_AUX_MIX,
  P_VEL_TIMBRE,
  // 24 dB ladder filter
  P_CUTOFF,
  P_RESONANCE,
  P_FENV_AMT,
  P_KEYTRACK,
  P_DRIVE,
  P_VEL_CUTOFF,
  // Amp ADSR
  P_AMP_A,
  P_AMP_D,
  P_AMP_S,
  P_AMP_R,
  // Filter ADSR
  P_FLT_A,
  P_FLT_D,
  P_FLT_S,
  P_FLT_R,
  // Global
  P_VEL_AMP,
  P_VOLUME,
  P_POLYPHONY,
  P_BEND_RANGE,
  P_COUNT
};

enum ParamKind : uint8_t {
  K_PERCENT,    // 0..100 %
  K_BIPOLAR,    // -100..+100 %
  K_ENGINE,     // discrete engine list
  K_TIME,       // envelope time, 1 ms .. 10 s
  K_CUTOFF,     // 20 Hz .. 20 kHz
  K_COUNT_POLY, // 1 .. kMaxVoices
  K_SEMITONES,  // 0 .. 24
};

struct ParamDef {
  const char* name;   // short label for the 240x135 display
  uint8_t cc;         // MIDI CC number
  float def;          // default normalised value
  ParamKind kind;
};

extern const ParamDef kParams[P_COUNT];
extern const char* const kEngineNames[kNumEngines];

// Number of discrete steps for a parameter (0 = continuous).
int paramSteps(ParamId id);
// Discrete index from a normalised value.
int paramIndex(ParamId id, float x);
// Normalised value at the centre of a discrete step.
float paramFromIndex(ParamId id, int index);
// Human readable value, e.g. "1.25 s", "880 Hz", "Chiptune".
void formatParam(ParamId id, float x, char* buf, size_t len);

// Value mappings used by the DSP.
float envTimeSeconds(float x);   // 1 ms .. 10 s, exponential
float cutoffHz(float x);         // 20 Hz .. 20 kHz, exponential

// ---- Encoder handling --------------------------------------------------
enum EncoderMode : uint8_t {
  ENC_ABSOLUTE = 0,
  ENC_REL_OFFSET64,   // 64 = no change, 65.. = +1.., 63.. = -1..  ("Binary offset")
  ENC_REL_TWOS,       // 1..63 = +1..+63, 127..65 = -1..-63      ("2's complement")
  ENC_REL_SIGNBIT,    // 1..63 = +1..+63, 65..127 = -1..-63      ("Signed bit")
  ENC_MODE_COUNT
};
extern const char* const kEncoderModeNames[ENC_MODE_COUNT];
// Decode a relative encoder value to a signed step count (0 if none).
int decodeRelative(EncoderMode mode, uint8_t value);

}  // namespace ps
