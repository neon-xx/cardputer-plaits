// Small DSP building blocks: ADSR envelope and a 4-pole (24 dB/oct)
// zero-delay-feedback ladder low-pass filter.
#pragma once

#include <math.h>

namespace ps {

// Cheap tanh approximation, accurate enough for saturation, clamps at +/-3.
static inline float softClip(float x) {
  if (x > 3.0f) return 1.0f;
  if (x < -3.0f) return -1.0f;
  const float x2 = x * x;
  return x * (27.0f + x2) / (27.0f + 9.0f * x2);
}

// ---------------------------------------------------------------------------
// ADSR coefficients, shared by every voice. Segments are exponential
// (analogue-style). The attack aims above 1.0 so it reaches the top in the
// requested time with a slightly convex shape.
struct AdsrCoeffs {
  float attack, decay, release, sustain;

  // Coefficient so that (1-k)^n == ratio after n steps.
  static float coef(float seconds, float steps_per_second, float ratio) {
    float n = seconds * steps_per_second;
    if (n < 1.0f) n = 1.0f;
    return 1.0f - powf(ratio, 1.0f / n);
  }

  void set(float a_s, float d_s, float sus, float r_s, float rate) {
    attack = coef(a_s, rate, 0.2f);     // 1.25 * (1 - 0.2) = 1.0
    decay = coef(d_s, rate, 0.01f);     // -40 dB of the way to sustain
    release = coef(r_s, rate, 0.001f);  // -60 dB
    sustain = sus;
  }
};

struct Adsr {
  enum Stage : unsigned char { IDLE, ATTACK, DECAY, RELEASE };
  Stage stage = IDLE;
  float value = 0.0f;

  void reset() { stage = IDLE; value = 0.0f; }
  void gateOn() { stage = ATTACK; }
  void gateOff() { if (stage != IDLE) stage = RELEASE; }
  bool idle() const { return stage == IDLE; }

  inline float process(const AdsrCoeffs& c) {
    switch (stage) {
      case ATTACK:
        value += (1.25f - value) * c.attack;
        if (value >= 1.0f) { value = 1.0f; stage = DECAY; }
        break;
      case DECAY:  // also acts as sustain once settled
        value += (c.sustain - value) * c.decay;
        break;
      case RELEASE:
        value -= value * c.release;
        if (value < 1.0e-4f) { value = 0.0f; stage = IDLE; }
        break;
      default:
        break;
    }
    return value;
  }
};

// ---------------------------------------------------------------------------
// Zavalishin-style TPT ladder: four one-pole stages with global feedback,
// solved implicitly (no unit delay in the loop), with a tanh-style
// saturator on the input to the ladder. Coefficients are updated once per
// control block; process() is called per sample.
struct LadderFilter {
  float s[4] = {0, 0, 0, 0};
  float G = 0.5f, G2 = 0.25f, G3 = 0.125f, G4 = 0.0625f;
  float inv1g = 0.5f;   // 1 / (1 + g)
  float k = 0.0f;       // feedback 0..4
  float in_gain = 1.0f; // drive + passband compensation
  float out_gain = 1.0f;

  void reset() { s[0] = s[1] = s[2] = s[3] = 0.0f; }

  // fc_norm = cutoff / sample_rate (clamped internally),
  // res 0..1, drive 0..1
  inline void set(float fc_norm, float res, float drive) {
    if (fc_norm < 0.0004f) fc_norm = 0.0004f;
    if (fc_norm > 0.42f) fc_norm = 0.42f;
    const float g = tanf(3.14159265f * fc_norm);
    inv1g = 1.0f / (1.0f + g);
    G = g * inv1g;
    G2 = G * G;
    G3 = G2 * G;
    G4 = G2 * G2;
    k = 4.2f * res;   // self-oscillates above ~95%
    const float dg = 1.0f + 5.0f * drive * drive;
    in_gain = dg * (1.0f + 0.6f * k);     // partial bass-loss compensation
    out_gain = 1.0f / (0.6f + 0.4f * dg); // keep loudness roughly constant
  }

  inline float process(float x) {
    const float S = (G3 * s[0] + G2 * s[1] + G * s[2] + s[3]) * inv1g;
    float u = (x * in_gain - k * S) / (1.0f + k * G4);
    u = softClip(u);
    float v, y;
    v = (u - s[0]) * G; y = v + s[0]; s[0] = y + v;
    v = (y - s[1]) * G; y = v + s[1]; s[1] = y + v;
    v = (y - s[2]) * G; y = v + s[2]; s[2] = y + v;
    v = (y - s[3]) * G; y = v + s[3]; s[3] = y + v;
    return y * out_gain;
  }
};

}  // namespace ps
