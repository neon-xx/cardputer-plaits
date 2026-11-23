#include "synth.h"

#include <math.h>
#include <new>
#include <string.h>

#include "stmlib/utils/buffer_allocator.h"

namespace ps {

namespace {
constexpr int kFadeSamples = 96;                 // 2 ms steal / shed fade
constexpr float kInvFade = 1.0f / float(kFadeSamples);
constexpr float kSmooth = 0.08f;                 // per 12-sample block (~3 ms)
constexpr float kMasterHeadroom = 0.4f;
constexpr float kInt16Scale = 1.0f / 32768.0f;

inline float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
}  // namespace

int Synth::init(Allocator alloc, FreeHeapFn free_heap, size_t reserve) {
  for (int i = 0; i < P_COUNT; ++i) params_[i] = kParams[i].def;
  memset(cc_to_param_, -1, sizeof(cc_to_param_));
  for (int i = 0; i < P_COUNT; ++i) cc_to_param_[kParams[i].cc] = int8_t(i);

  const size_t per_voice = sizeof(plaits::Voice) + kPlaitsSharedRam + 512;
  num_voices_ = 0;
  for (int i = 0; i < kMaxVoices; ++i) {
    if (free_heap && free_heap() < reserve + per_voice) break;
    void* vm = alloc(sizeof(plaits::Voice));
    void* buf = alloc(kPlaitsSharedRam);
    if (!vm || !buf) break;
    SynthVoice& v = voices_[i];
    v.plaits = new (vm) plaits::Voice();
    stmlib::BufferAllocator allocator(buf, kPlaitsSharedRam);
    v.plaits->Init(&allocator);

    memset(&v.patch, 0, sizeof(v.patch));
    v.patch.note = 60.0f;
    v.patch.harmonics = v.patch.timbre = v.patch.morph = 0.5f;
    v.patch.decay = 0.5f;
    v.patch.lpg_colour = 1.0f;   // with level=1 the internal LPG is transparent

    memset(&v.mods, 0, sizeof(v.mods));
    // Mark everything "patched" with zero modulation: this disables Plaits'
    // internal decay-envelope modulation routings, so our ADSRs are in charge.
    v.mods.frequency_patched = true;
    v.mods.timbre_patched = true;
    v.mods.morph_patched = true;
    v.mods.trigger_patched = true;   // gates drive the engines' triggers
    v.mods.level_patched = true;     // keep Plaits' LPG open (level = 1)
    v.mods.level = 1.0f;
    ++num_voices_;
  }

  cutoff_s_ = params_[P_CUTOFF];
  res_s_ = params_[P_RESONANCE];
  drive_s_ = params_[P_DRIVE];
  mix_s_ = params_[P_AUX_MIX];
  volume_s_ = params_[P_VOLUME] * params_[P_VOLUME];
  env_dirty_ = true;
  return num_voices_;
}

int Synth::polyphony() const {
  int p = paramIndex(P_POLYPHONY, params_[P_POLYPHONY]) + 1;
  return p < num_voices_ ? p : num_voices_;
}

bool Synth::engineHasOwnEnvelope(int e) {
  // Engines registered with already_enveloped = true in Voice::Init.
  return (e >= 2 && e <= 4) || e >= 19;
}

// ---------------------------------------------------------------------------
// MIDI

void Synth::handleMidi(const MidiMsg& m) {
  if (m.status >= 0xF0) return;
  const uint8_t ch = (m.status & 0x0F) + 1;
  if (midiChannel != 0 && ch != midiChannel) return;
  switch (m.status & 0xF0) {
    case 0x90:
      if (m.d2 == 0) noteOff(m.d1); else noteOn(m.d1, m.d2);
      break;
    case 0x80:
      noteOff(m.d1);
      break;
    case 0xB0:
      handleCC(m.d1, m.d2);
      break;
    case 0xE0: {
      int v = (int(m.d2) << 7 | m.d1) - 8192;
      pitch_bend_ = float(v) / 8192.0f;
      break;
    }
    default:
      break;
  }
}

void Synth::handleCC(uint8_t cc, uint8_t value) {
  cc &= 0x7F;
  switch (cc) {
    case 1:
      mod_wheel_ = value / 127.0f;
      return;
    case 64: {
      bool down = value >= 64;
      if (sustain_pedal_ && !down) {
        for (int i = 0; i < num_voices_; ++i)
          if (voices_[i].sustained) releaseVoice(voices_[i]);
      }
      sustain_pedal_ = down;
      return;
    }
    case 120:  // all sound off
      panic();
      return;
    case 121:  // reset all controllers
      pitch_bend_ = 0.0f;
      mod_wheel_ = 0.0f;
      handleCC(64, 0);
      return;
    case 123:  // all notes off
      for (int i = 0; i < num_voices_; ++i) {
        voices_[i].pending_note = -1;
        if (voices_[i].gate || voices_[i].sustained) releaseVoice(voices_[i]);
      }
      return;
    default:
      break;
  }

  const int pid = cc_to_param_[cc];
  if (pid < 0) return;
  const ParamId id = ParamId(pid);
  const EncoderMode mode = ccModeOverride[cc]
      ? EncoderMode(ccModeOverride[cc] - 1) : encoderMode;
  if (mode == ENC_ABSOLUTE) {
    const int steps = paramSteps(id);
    if (steps) setParam(id, paramFromIndex(id, int(value) * steps / 128));
    else setParam(id, value / 127.0f);
  } else {
    int d = decodeRelative(mode, value);
    if (d) nudgeParam(id, d);
  }
  lastCCValue = value;
  lastCC = cc;
}

void Synth::setParam(ParamId id, float x) {
  x = clamp01(x);
  params_[id] = x;
  switch (id) {
    case P_AMP_A: case P_AMP_D: case P_AMP_S: case P_AMP_R:
    case P_FLT_A: case P_FLT_D: case P_FLT_S: case P_FLT_R:
      env_dirty_ = true;
      break;
    case P_POLYPHONY:
      enforcePolyphony();
      break;
    default:
      break;
  }
  ++paramChangeCounter;
}

void Synth::nudgeParam(ParamId id, int steps) {
  if (steps == 0) return;
  if (paramSteps(id)) {
    // Discrete parameters move one slot per detent, regardless of
    // encoder acceleration, so engines don't skip.
    int idx = paramIndex(id, params_[id]) + (steps > 0 ? 1 : -1);
    setParam(id, paramFromIndex(id, idx));
  } else {
    setParam(id, params_[id] + float(steps) / 127.0f);
  }
}

// ---------------------------------------------------------------------------
// Voices

void Synth::startNote(SynthVoice& v, int note, float vel) {
  v.note = note;
  v.velocity = vel;
  v.gate = true;
  v.sustained = false;
  v.amp.reset();
  v.fenv.reset();
  v.filter.reset();
  v.amp.gateOn();
  v.fenv.gateOn();
  v.trig_low_blocks = 1;
  v.silent_blocks = 0;
  v.age = ++age_counter_;
  v.fade = 0;
  v.fading = false;
  v.pending_note = -1;
}

void Synth::releaseVoice(SynthVoice& v) {
  v.gate = false;
  v.sustained = false;
  v.amp.gateOff();
  v.fenv.gateOff();
}

int Synth::allocateVoice(int note) {
  const int poly = polyphony();
  if (poly <= 0) return -1;
  // 1. Same note still sounding: retrigger it (avoids doubled notes).
  for (int i = 0; i < poly; ++i) {
    const SynthVoice& v = voices_[i];
    if (v.active() && !v.fading && v.note == note) return i;
  }
  // 2. Oldest idle voice.
  int best = -1;
  for (int i = 0; i < poly; ++i) {
    const SynthVoice& v = voices_[i];
    if (!v.active() && (best < 0 || v.age < voices_[best].age)) best = i;
  }
  if (best >= 0) return best;
  // 3. Quietest released voice.
  float lowest = 2.0f;
  for (int i = 0; i < poly; ++i) {
    const SynthVoice& v = voices_[i];
    if (!v.gate && !v.sustained && v.amp.value < lowest) {
      lowest = v.amp.value;
      best = i;
    }
  }
  if (best >= 0) return best;
  // 4. Oldest voice.
  for (int i = 0; i < poly; ++i) {
    if (best < 0 || voices_[i].age < voices_[best].age) best = i;
  }
  return best;
}

void Synth::noteOn(int note, int velocity) {
  const int i = allocateVoice(note);
  if (i < 0) return;
  SynthVoice& v = voices_[i];
  const float vel = velocity / 127.0f;
  if (!v.active()) {
    startNote(v, note, vel);
  } else if (v.note == note && !v.fading) {
    // Retrigger in place: envelopes restart from their current level.
    v.velocity = vel;
    v.gate = true;
    v.sustained = false;
    v.amp.gateOn();
    v.fenv.gateOn();
    v.trig_low_blocks = 1;
    v.age = ++age_counter_;
  } else {
    // Steal: quick fade, then start the new note.
    v.pending_note = note;
    v.pending_velocity = vel;
    v.pending_gate = true;
    v.age = ++age_counter_;
    if (!v.fading) {
      v.fading = true;
      v.fade = kFadeSamples;
    }
  }
}

void Synth::noteOff(int note) {
  for (int i = 0; i < num_voices_; ++i) {
    SynthVoice& v = voices_[i];
    if (v.pending_note == note) v.pending_gate = false;
    if (v.note == note && v.gate && !v.fading) {
      if (sustain_pedal_) {
        v.gate = false;
        v.sustained = true;
      } else {
        releaseVoice(v);
      }
    }
  }
}

void Synth::panic() {
  for (int i = 0; i < num_voices_; ++i) {
    SynthVoice& v = voices_[i];
    v.amp.reset();
    v.fenv.reset();
    v.filter.reset();
    v.gate = v.sustained = false;
    v.pending_note = -1;
    v.fading = false;
    v.fade = 0;
    v.note = -1;
  }
  sustain_pedal_ = false;
  pitch_bend_ = 0.0f;
}

void Synth::shedVoice() {
  int best = -1;
  float lowest = 2.0f;
  for (int i = 0; i < num_voices_; ++i) {
    const SynthVoice& v = voices_[i];
    if (!v.active() || v.fading) continue;
    // Prefer released voices, quietest first; then the oldest held one.
    float score = (v.gate || v.sustained)
        ? 1.0f + 1.0f / (1.0f + float(age_counter_ - v.age))   // older -> lower
        : v.amp.value;
    if (best < 0 || score < lowest) { best = i; lowest = score; }
  }
  if (best < 0) return;
  SynthVoice& v = voices_[best];
  v.pending_note = -1;
  v.fading = true;
  v.fade = kFadeSamples;
}

void Synth::enforcePolyphony() {
  const int poly = polyphony();
  for (int i = poly; i < num_voices_; ++i) {
    SynthVoice& v = voices_[i];
    if (v.active() && !v.fading) {
      v.pending_note = -1;
      v.fading = true;
      v.fade = kFadeSamples;
    }
  }
}

void Synth::updateEnvCoeffs() {
  amp_coeffs_.set(envTimeSeconds(params_[P_AMP_A]), envTimeSeconds(params_[P_AMP_D]),
                  params_[P_AMP_S], envTimeSeconds(params_[P_AMP_R]), kSampleRate);
  fenv_coeffs_.set(envTimeSeconds(params_[P_FLT_A]), envTimeSeconds(params_[P_FLT_D]),
                   params_[P_FLT_S], envTimeSeconds(params_[P_FLT_R]),
                   kSampleRate / float(kPlaitsBlock));
  env_dirty_ = false;
}

// ---------------------------------------------------------------------------
// Rendering

int Synth::prepare(size_t n, int* active_list) {
  blocks_ = n / kPlaitsBlock;
  if (blocks_ > kMaxBufferBlocks) blocks_ = kMaxBufferBlocks;
  if (env_dirty_) updateEnvCoeffs();

  const float vol_t = params_[P_VOLUME] * params_[P_VOLUME];
  volume_blk_[0] = volume_s_;
  for (size_t b = 0; b < blocks_; ++b) {
    cutoff_s_ += (params_[P_CUTOFF] - cutoff_s_) * kSmooth;
    res_s_ += (params_[P_RESONANCE] - res_s_) * kSmooth;
    drive_s_ += (params_[P_DRIVE] - drive_s_) * kSmooth;
    mix_s_ += (params_[P_AUX_MIX] - mix_s_) * kSmooth;
    volume_s_ += (vol_t - volume_s_) * kSmooth;
    cutoff_blk_[b] = cutoff_s_;
    res_blk_[b] = res_s_;
    drive_blk_[b] = drive_s_;
    mix_blk_[b] = mix_s_;
    volume_blk_[b + 1] = volume_s_;
  }

  int count = 0;
  for (int i = 0; i < num_voices_; ++i)
    if (voices_[i].active()) active_list[count++] = i;
  active_count_ = count;
  return count;
}

void Synth::renderVoice(int vi, float* acc, size_t n) {
  SynthVoice& v = voices_[vi];
  plaits::Voice::Frame frames[kPlaitsBlock];

  const int engine = paramIndex(P_ENGINE, params_[P_ENGINE]);
  const bool own_env = engineHasOwnEnvelope(engine);
  const float bend_st = pitch_bend_ * float(paramIndex(P_BEND_RANGE, params_[P_BEND_RANGE]));
  const float fenv_oct = (params_[P_FENV_AMT] - 0.5f) * 2.0f * 7.0f;  // +/- 7 octaves
  const float keytrack = params_[P_KEYTRACK];
  const float vel_cut = params_[P_VEL_CUTOFF];
  const float vel_amp = params_[P_VEL_AMP];
  const float vel_tim = params_[P_VEL_TIMBRE];
  const float harmonics = params_[P_HARMONICS];
  const float morph = params_[P_MORPH];
  const float base_timbre = params_[P_TIMBRE];
  const float wheel = mod_wheel_;
  const float inv_sr = 1.0f / kSampleRate;

  const size_t blocks = n / kPlaitsBlock;
  for (size_t b = 0; b < blocks && b < blocks_; ++b) {
    float* out = acc + b * kPlaitsBlock;

    // --- Plaits engine ---
    float timbre = base_timbre + wheel * (1.0f - base_timbre);
    timbre += vel_tim * (v.velocity - 0.5f);
    v.patch.engine = engine;
    v.patch.harmonics = harmonics;
    v.patch.timbre = clamp01(timbre);
    v.patch.morph = morph;
    v.patch.note = float(v.note) + bend_st;
    if (v.trig_low_blocks > 0) {
      --v.trig_low_blocks;
      v.mods.trigger = 0.0f;
    } else {
      v.mods.trigger = (v.gate || v.sustained) ? 1.0f : 0.0f;
    }
    v.mods.level = own_env ? v.velocity : 1.0f;   // accent for drums/strings
    v.plaits->Render(v.patch, v.mods, frames, kPlaitsBlock);

    // Self-enveloped engines (drums, strings, modal, 6-op FM) decay on their
    // own: once they've been silent for ~50 ms, free the voice even if the
    // key is still held. Saves CPU (decaying resonators also drift into
    // slow denormal floats) and frees the voice for new notes.
    if (own_env) {
      int pk = 0;
      for (size_t i = 0; i < kPlaitsBlock; ++i) {
        int a = frames[i].out < 0 ? -frames[i].out : frames[i].out;
        if (a > pk) pk = a;
      }
      if (pk < 8) {
        if (++v.silent_blocks > 200 && !v.fading) {
          v.amp.reset();
          v.fenv.reset();
          v.gate = v.sustained = false;
          v.note = -1;
          v.silent_blocks = 0;
          break;
        }
      } else {
        v.silent_blocks = 0;
      }
    }

    // --- Filter (coefficients at block rate) ---
    const float fe = v.fenv.process(fenv_coeffs_);
    float oct = fenv_oct * fe
        + keytrack * float(v.note - 60) * (1.0f / 12.0f)
        + vel_cut * (v.velocity - 1.0f) * 3.0f;
    const float fc = cutoffHz(cutoff_blk_[b]) * exp2f(oct);
    v.filter.set(fc * inv_sr, res_blk_[b], drive_blk_[b]);

    const float mix = mix_blk_[b];
    const float vel_gain = (1.0f - vel_amp) + vel_amp * v.velocity * v.velocity;
    const float out_w = (1.0f - mix) * kInt16Scale;
    const float aux_w = mix * kInt16Scale;

    for (size_t i = 0; i < kPlaitsBlock; ++i) {
      const float x = float(frames[i].out) * out_w + float(frames[i].aux) * aux_w;
      const float y = v.filter.process(x);
      float g = v.amp.process(amp_coeffs_) * vel_gain;
      if (v.fading) {
        if (v.fade > 0) { g *= float(v.fade) * kInvFade; --v.fade; }
        else g = 0.0f;
      }
      out[i] += y * g;
    }

    // --- End of fade: start the pending note or free the voice ---
    if (v.fading && v.fade == 0) {
      v.fading = false;
      if (v.pending_note >= 0) {
        const bool g = v.pending_gate;
        startNote(v, v.pending_note, v.pending_velocity);
        if (!g) {
          if (sustain_pedal_) { v.gate = false; v.sustained = true; }
          else releaseVoice(v);
        }
      } else {
        v.amp.reset();
        v.fenv.reset();
        v.gate = v.sustained = false;
        v.note = -1;
      }
    }
    if (!v.active()) break;   // rest of the buffer is silence for this voice
  }
}

void Synth::mixdown(const float* const* accs, int num_accs, int16_t* out, size_t n) {
  size_t i = 0;
  for (size_t b = 0; b < blocks_; ++b) {
    const float v0 = volume_blk_[b] * kMasterHeadroom;
    const float dv = (volume_blk_[b + 1] * kMasterHeadroom - v0) / float(kPlaitsBlock);
    for (size_t j = 0; j < kPlaitsBlock && i < n; ++j, ++i) {
      float s = 0.0f;
      for (int k = 0; k < num_accs; ++k) s += accs[k][i];
      s *= v0 + dv * float(j);
      out[i] = int16_t(softClip(s) * 32000.0f);
    }
  }
  for (; i < n; ++i) out[i] = 0;
}

}  // namespace ps
