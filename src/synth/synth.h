// Polyphonic Plaits synth core: voice allocation, MIDI handling, per-voice
// Plaits engine -> 24 dB ladder filter -> amp ADSR.
//
// Threading contract (enforced by the platform layer):
//  * handleMidi / setParam / nudgeParam / prepare / mixdown are only called
//    from the audio task, between buffers.
//  * renderVoice may be called concurrently for DIFFERENT voices (one set on
//    each CPU core) after prepare() and before mixdown().
//  * Getters used by the UI only read plain 32-bit values (benign races).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include "plaits/dsp/voice.h"
#include "dsp_blocks.h"
#include "midi_parser.h"
#include "params.h"

namespace ps {

constexpr float kSampleRate = 48000.0f;
constexpr size_t kPlaitsBlock = plaits::kBlockSize;   // 12 samples
constexpr size_t kMaxBufferBlocks = 32;               // up to 384 samples/buffer
constexpr size_t kPlaitsSharedRam = 16384;            // per voice, as on hardware

struct SynthVoice {
  plaits::Voice* plaits = nullptr;
  plaits::Patch patch;
  plaits::Modulations mods;
  LadderFilter filter;
  Adsr amp, fenv;

  int note = -1;
  float velocity = 0.0f;
  bool gate = false;        // key held
  bool sustained = false;   // key released while pedal down
  uint32_t age = 0;         // allocation order stamp
  int trig_low_blocks = 0;  // hold Plaits trigger low to force a new edge
  int silent_blocks = 0;    // consecutive near-silent Plaits blocks

  // Click-free voice stealing / shedding: fade out, then (optionally)
  // start the pending note.
  bool fading = false;
  int fade = 0;             // samples remaining in fade
  int pending_note = -1;    // note to start after the fade (-1: just stop)
  float pending_velocity = 0.0f;
  bool pending_gate = false;  // false if the key was already released

  bool active() const { return !amp.idle() || pending_note >= 0 || fading; }
};

class Synth {
 public:
  // alloc must return 4-byte aligned internal RAM (or nullptr on failure).
  // max_voices is capped by how many Plaits voices could be allocated while
  // keeping `reserve` bytes free; returns the number of voices created.
  typedef void* (*Allocator)(size_t bytes);
  typedef size_t (*FreeHeapFn)();
  int init(Allocator alloc, FreeHeapFn free_heap, size_t reserve);

  // ---- control (audio task only) ----
  void handleMidi(const MidiMsg& m);
  void setParam(ParamId id, float x);
  void nudgeParam(ParamId id, int steps);   // relative change (+/- steps)
  void panic();                              // all sound off, reset state
  void noteOn(int note, int velocity);
  void noteOff(int note);
  void shedVoice();                          // CPU overload protection

  // ---- rendering ----
  // prepare(): compute per-block smoothed globals for an n-sample buffer and
  // return the list of voices that need rendering.
  int prepare(size_t n, int* active_list);
  // Adds the voice's output into acc[0..n).
  void renderVoice(int v, float* acc, size_t n);
  // Sum of accumulators -> int16 output with volume and soft clipping.
  void mixdown(const float* const* accs, int num_accs, int16_t* out, size_t n);

  // ---- state for UI / settings ----
  float param(ParamId id) const { return params_[id]; }
  int numVoices() const { return num_voices_; }
  int polyphony() const;
  int activeVoiceCount() const { return active_count_; }
  bool silent() const { return active_count_ == 0; }

  uint8_t midiChannel = 0;                    // 0 = omni, 1..16
  EncoderMode encoderMode = ENC_ABSOLUTE;     // global default
  uint8_t ccModeOverride[128] = {0};          // 0 = use global, else mode+1
  volatile int lastCC = -1;                   // last parameter CC received
  volatile int lastCCValue = 0;
  volatile uint32_t paramChangeCounter = 0;   // UI redraw hint

 private:
  void handleCC(uint8_t cc, uint8_t value);
  void startNote(SynthVoice& v, int note, float vel);
  int allocateVoice(int note);
  void releaseVoice(SynthVoice& v);
  void updateEnvCoeffs();
  void enforcePolyphony();
  static bool engineHasOwnEnvelope(int engine);

  SynthVoice voices_[kMaxVoices];
  int num_voices_ = 0;
  uint32_t age_counter_ = 0;
  int active_count_ = 0;

  float params_[P_COUNT];
  int8_t cc_to_param_[128];

  // Controller state
  float pitch_bend_ = 0.0f;   // -1..+1
  float mod_wheel_ = 0.0f;    // 0..1
  bool sustain_pedal_ = false;

  // Derived (updated in setParam / prepare)
  AdsrCoeffs amp_coeffs_;
  AdsrCoeffs fenv_coeffs_;   // block-rate coefficients
  bool env_dirty_ = true;

  // Smoothed per-buffer, per-block values (index = block in buffer)
  size_t blocks_ = 0;
  float cutoff_blk_[kMaxBufferBlocks];   // normalised 0..1
  float res_blk_[kMaxBufferBlocks];
  float drive_blk_[kMaxBufferBlocks];
  float mix_blk_[kMaxBufferBlocks];
  float volume_blk_[kMaxBufferBlocks + 1];
  float cutoff_s_, res_s_, drive_s_, mix_s_, volume_s_;
};

}  // namespace ps
