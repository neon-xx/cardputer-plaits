#include "params.h"

#include <math.h>
#include <stdio.h>

namespace ps {

// Plaits 1.2 engine order (see plaits/dsp/voice.cc, Voice::Init).
const char* const kEngineNames[kNumEngines] = {
  "VA + VCF",        // 0  green bank
  "Phase Dist",      // 1
  "6-Op FM A",       // 2
  "6-Op FM B",       // 3
  "6-Op FM C",       // 4
  "Wave Terrain",    // 5
  "String Machine",  // 6
  "Chiptune",        // 7
  "Virtual Analog",  // 8  original bank
  "Waveshaping",     // 9
  "2-Op FM",         // 10
  "Grain Formant",   // 11
  "Additive",        // 12
  "Wavetable",       // 13
  "Chords",          // 14
  "Speech",          // 15
  "Swarm",           // 16
  "Filtered Noise",  // 17
  "Particle",        // 18
  "Inharm String",   // 19
  "Modal Reson",     // 20
  "Bass Drum",       // 21
  "Snare Drum",      // 22
  "Hi-Hat",          // 23
};

#define IDX(i, n) ((float(i) + 0.5f) / float(n))

const ParamDef kParams[P_COUNT] = {
  // name           cc   default              kind
  { "Engine",       14,  IDX(8, kNumEngines), K_ENGINE },
  { "Harmonics",    15,  0.50f,               K_PERCENT },
  { "Timbre",       16,  0.50f,               K_PERCENT },
  { "Morph",        17,  0.50f,               K_PERCENT },
  { "Out/Aux mix",  18,  0.00f,               K_PERCENT },
  { "Vel>Timbre",   19,  0.00f,               K_PERCENT },

  { "Cutoff",       74,  0.75f,               K_CUTOFF },
  { "Resonance",    71,  0.15f,               K_PERCENT },
  { "F.Env amt",    20,  0.65f,               K_BIPOLAR },
  { "Key track",    21,  0.50f,               K_PERCENT },
  { "Drive",        22,  0.20f,               K_PERCENT },
  { "Vel>Cutoff",   23,  0.30f,               K_PERCENT },

  { "Amp Attack",   73,  0.05f,               K_TIME },
  { "Amp Decay",    75,  0.55f,               K_TIME },
  { "Amp Sustain",  79,  0.80f,               K_PERCENT },
  { "Amp Release",  72,  0.50f,               K_TIME },

  { "Flt Attack",  102,  0.10f,               K_TIME },
  { "Flt Decay",   103,  0.60f,               K_TIME },
  { "Flt Sustain", 104,  0.30f,               K_PERCENT },
  { "Flt Release", 105,  0.50f,               K_TIME },

  { "Vel>Amp",      87,  0.70f,               K_PERCENT },
  { "Volume",        7,  0.80f,               K_PERCENT },
  { "Polyphony",    85,  IDX(4, kMaxVoices),  K_COUNT_POLY },
  { "Bend range",   86,  IDX(2, 25),          K_SEMITONES },
};

const char* const kEncoderModeNames[ENC_MODE_COUNT] = {
  "Absolute", "Rel 64+/-", "Rel 2s comp", "Rel signbit",
};

int paramSteps(ParamId id) {
  switch (kParams[id].kind) {
    case K_ENGINE: return kNumEngines;
    case K_COUNT_POLY: return kMaxVoices;
    case K_SEMITONES: return 25;
    default: return 0;
  }
}

int paramIndex(ParamId id, float x) {
  int n = paramSteps(id);
  if (n == 0) return 0;
  int i = int(x * float(n));
  if (i < 0) i = 0;
  if (i > n - 1) i = n - 1;
  return i;
}

float paramFromIndex(ParamId id, int index) {
  int n = paramSteps(id);
  if (n == 0) return 0.0f;
  if (index < 0) index = 0;
  if (index > n - 1) index = n - 1;
  return IDX(index, n);
}

float envTimeSeconds(float x) {
  return 0.001f * powf(10.0f, 4.0f * x);
}

float cutoffHz(float x) {
  return 20.0f * powf(1000.0f, x);
}

void formatParam(ParamId id, float x, char* buf, size_t len) {
  switch (kParams[id].kind) {
    case K_PERCENT:
      snprintf(buf, len, "%d%%", int(x * 100.0f + 0.5f));
      break;
    case K_BIPOLAR:
      snprintf(buf, len, "%+d%%", int(roundf((x - 0.5f) * 200.0f)));
      break;
    case K_ENGINE: {
      int i = paramIndex(id, x);
      snprintf(buf, len, "%d %s", i + 1, kEngineNames[i]);
      break;
    }
    case K_TIME: {
      float t = envTimeSeconds(x);
      if (t < 1.0f) snprintf(buf, len, "%d ms", int(t * 1000.0f + 0.5f));
      else snprintf(buf, len, "%.2f s", t);
      break;
    }
    case K_CUTOFF: {
      float f = cutoffHz(x);
      if (f < 1000.0f) snprintf(buf, len, "%d Hz", int(f + 0.5f));
      else snprintf(buf, len, "%.1f kHz", f / 1000.0f);
      break;
    }
    case K_COUNT_POLY:
      snprintf(buf, len, "%d voices", paramIndex(id, x) + 1);
      break;
    case K_SEMITONES:
      snprintf(buf, len, "%d st", paramIndex(id, x));
      break;
  }
}

int decodeRelative(EncoderMode mode, uint8_t v) {
  v &= 0x7F;
  switch (mode) {
    case ENC_REL_OFFSET64: return int(v) - 64;
    case ENC_REL_TWOS:     return v < 64 ? int(v) : int(v) - 128;
    case ENC_REL_SIGNBIT:  return (v & 0x40) ? -int(v & 0x3F) : int(v & 0x3F);
    default: return 0;
  }
}

}  // namespace ps
