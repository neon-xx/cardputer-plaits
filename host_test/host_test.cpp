// Native (PC) test harness for the synth core. Not part of the firmware.
// Links Emilie Gillet's Plaits DSP (Mutable Instruments, MIT licence).
// Builds Plaits + the synth core with g++, renders WAV files, checks the
// MIDI / BLE-MIDI parsers and the encoder decoding, and times each engine.
//
//   make -C host_test && ./host_test/host_test
#include <assert.h>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "synth/midi_parser.h"
#include "synth/params.h"
#include "synth/synth.h"

using namespace ps;

static void* hostAlloc(size_t n) { return aligned_alloc(16, (n + 15) & ~size_t(15)); }

static const size_t kBuf = 192;

struct Renderer {
  Synth& s;
  std::vector<int16_t> pcm;
  float accA[kBuf], accB[kBuf];
  void run(size_t samples) {
    int list[kMaxVoices];
    int16_t out[kBuf];
    for (size_t done = 0; done < samples; done += kBuf) {
      memset(accA, 0, sizeof(accA));
      memset(accB, 0, sizeof(accB));
      int n = s.prepare(kBuf, list);
      for (int i = 0; i < n; ++i) s.renderVoice(list[i], (i & 1) ? accB : accA, kBuf);
      const float* accs[2] = {accA, accB};
      s.mixdown(accs, 2, out, kBuf);
      pcm.insert(pcm.end(), out, out + kBuf);
    }
  }
  void seconds(float t) { run(size_t(t * kSampleRate)); }
};

static void writeWav(const char* path, const std::vector<int16_t>& pcm) {
  FILE* f = fopen(path, "wb");
  uint32_t data = pcm.size() * 2, rate = 48000, byterate = rate * 2;
  uint32_t riff = 36 + data, fmtlen = 16;
  uint16_t fmt = 1, ch = 1, align = 2, bits = 16;
  fwrite("RIFF", 1, 4, f); fwrite(&riff, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
  fwrite(&fmtlen, 4, 1, f); fwrite(&fmt, 2, 1, f); fwrite(&ch, 2, 1, f);
  fwrite(&rate, 4, 1, f); fwrite(&byterate, 4, 1, f); fwrite(&align, 2, 1, f);
  fwrite(&bits, 2, 1, f); fwrite("data", 1, 4, f); fwrite(&data, 4, 1, f);
  fwrite(pcm.data(), 2, pcm.size(), f);
  fclose(f);
}

static float peak(const std::vector<int16_t>& p, size_t from, size_t to) {
  int m = 0;
  for (size_t i = from; i < to && i < p.size(); ++i) m = std::max(m, abs(int(p[i])));
  return m / 32768.0f;
}

// ---- parser tests ----
static std::vector<MidiMsg> got;
static void collect(void*, const MidiMsg& m) { got.push_back(m); }

static void testParsers() {
  MidiParser p;
  p.setCallback(collect, nullptr);
  // Running status + realtime interleaved + sysex skipped
  const uint8_t stream[] = {0x90, 60, 100, 62, 100, 0xF8, 64, 0, 0xF0, 1, 2, 3, 0xF7,
                            0xB0, 74, 10, 0xE0, 0x00, 0x40};
  got.clear();
  for (uint8_t b : stream) p.feed(b);
  assert(got.size() == 6);
  assert(got[0].status == 0x90 && got[0].d1 == 60 && got[0].d2 == 100);
  assert(got[1].d1 == 62);
  assert(got[2].status == 0xF8);
  assert(got[3].status == 0x90 && got[3].d1 == 64 && got[3].d2 == 0);
  assert(got[4].status == 0xB0 && got[4].d1 == 74 && got[4].d2 == 10);
  assert(got[5].status == 0xE0 && got[5].d2 == 0x40);

  // BLE-MIDI: header, ts, note on, running status w/o ts, ts + running status,
  // ts + new status (CC)
  MidiParser b;
  b.setCallback(collect, nullptr);
  got.clear();
  const uint8_t pkt[] = {0x80, 0x81, 0x90, 60, 100, 61, 90, 0x82, 62, 80, 0x83, 0xB0, 16, 5};
  decodeBleMidi(pkt, sizeof(pkt), b);
  assert(got.size() == 4);
  assert(got[0].status == 0x90 && got[0].d1 == 60);
  assert(got[1].status == 0x90 && got[1].d1 == 61 && got[1].d2 == 90);
  assert(got[2].status == 0x90 && got[2].d1 == 62);
  assert(got[3].status == 0xB0 && got[3].d1 == 16 && got[3].d2 == 5);

  // BLE-MIDI SysEx across two packets, then a note; timestamp == 0xF7 edge
  got.clear();
  const uint8_t s1[] = {0x80, 0x80, 0xF0, 1, 2, 3};
  const uint8_t s2[] = {0x80, 4, 5, 0xF7, 0xF7, 0xF7, 0x90, 70, 1};
  decodeBleMidi(s1, sizeof(s1), b);
  decodeBleMidi(s2, sizeof(s2), b);
  assert(got.size() == 1 && got[0].d1 == 70);

  // Relative encoder decoding
  assert(decodeRelative(ENC_REL_OFFSET64, 65) == 1);
  assert(decodeRelative(ENC_REL_OFFSET64, 63) == -1);
  assert(decodeRelative(ENC_REL_OFFSET64, 64) == 0);
  assert(decodeRelative(ENC_REL_TWOS, 1) == 1);
  assert(decodeRelative(ENC_REL_TWOS, 127) == -1);
  assert(decodeRelative(ENC_REL_TWOS, 120) == -8);
  assert(decodeRelative(ENC_REL_SIGNBIT, 3) == 3);
  assert(decodeRelative(ENC_REL_SIGNBIT, 65) == -1);
  assert(decodeRelative(ENC_REL_SIGNBIT, 72) == -8);
  printf("parsers + encoder decoding: OK\n");
}

static void cc(Synth& s, int n, int v) { s.handleMidi({0xB0, uint8_t(n), uint8_t(v)}); }
static void on(Synth& s, int n, int v = 100) { s.handleMidi({0x90, uint8_t(n), uint8_t(v)}); }
static void off(Synth& s, int n) { s.handleMidi({0x80, uint8_t(n), 0}); }

static void testControl(Synth& s) {
  // Absolute CC
  cc(s, 74, 127);
  assert(fabsf(s.param(P_CUTOFF) - 1.0f) < 1e-6f);
  cc(s, 74, 0);
  assert(s.param(P_CUTOFF) == 0.0f);
  // Engine absolute: 0..127 spread over 24 engines
  cc(s, 14, 127);
  assert(paramIndex(P_ENGINE, s.param(P_ENGINE)) == 23);
  cc(s, 14, 0);
  assert(paramIndex(P_ENGINE, s.param(P_ENGINE)) == 0);
  // Relative modes
  s.encoderMode = ENC_REL_TWOS;
  cc(s, 74, 10);
  assert(fabsf(s.param(P_CUTOFF) - 10.0f / 127.0f) < 1e-5f);
  cc(s, 74, 126);   // -2
  assert(fabsf(s.param(P_CUTOFF) - 8.0f / 127.0f) < 1e-5f);
  cc(s, 14, 5);     // engine: one slot per detent even with acceleration
  assert(paramIndex(P_ENGINE, s.param(P_ENGINE)) == 1);
  cc(s, 14, 127);
  assert(paramIndex(P_ENGINE, s.param(P_ENGINE)) == 0);
  // Per-CC override: CC 71 absolute while the rest are relative
  s.ccModeOverride[71] = ENC_ABSOLUTE + 1;
  cc(s, 71, 64);
  assert(fabsf(s.param(P_RESONANCE) - 64 / 127.0f) < 1e-5f);
  s.encoderMode = ENC_REL_OFFSET64;
  cc(s, 74, 64 + 5);
  assert(fabsf(s.param(P_CUTOFF) - 13.0f / 127.0f) < 1e-5f);
  s.encoderMode = ENC_ABSOLUTE;
  s.ccModeOverride[71] = 0;
  assert(s.lastCC == 74);
  // Channel filter
  s.midiChannel = 2;
  s.handleMidi({0xB0, 74, 100});     // channel 1: ignored
  assert(fabsf(s.param(P_CUTOFF) - 13.0f / 127.0f) < 1e-5f);
  s.handleMidi({0xB1, 74, 100});     // channel 2: accepted
  assert(fabsf(s.param(P_CUTOFF) - 100.0f / 127.0f) < 1e-5f);
  s.midiChannel = 0;
  printf("CC / encoder / channel handling: OK\n");
}

static void resetPatch(Synth& s) {
  for (int i = 0; i < P_COUNT; ++i) s.setParam(ParamId(i), kParams[i].def);
}

#include <xmmintrin.h>
#include <pmmintrin.h>
int main() {
  if (getenv("FTZ")) { _MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON); _MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON); }
  testParsers();

  Synth s;
  int nv = s.init(hostAlloc, nullptr, 0);
  printf("voices allocated: %d (sizeof plaits::Voice = %zu)\n", nv, sizeof(plaits::Voice));
  testControl(s);
  resetPatch(s);

  // ---- Polyphony / voice allocation ----
  {
    Renderer r{s};
    for (int n : {48, 52, 55, 59, 62}) on(s, n);
    r.seconds(0.05f);
    assert(s.activeVoiceCount() == 5);
    on(s, 65);   // 6th note with polyphony 5 -> steal
    r.seconds(0.05f);
    assert(s.activeVoiceCount() == 5);
    for (int n : {48, 52, 55, 59, 62, 65}) off(s, n);
    r.seconds(1.5f);
    assert(s.silent());
    // Same note twice -> one voice
    on(s, 60); on(s, 60);
    r.seconds(0.02f);
    assert(s.activeVoiceCount() == 1);
    off(s, 60);
    // Sustain pedal holds released notes
    cc(s, 64, 127); on(s, 60); off(s, 60);
    r.seconds(1.0f);
    assert(s.activeVoiceCount() == 1);
    cc(s, 64, 0);
    r.seconds(1.5f);
    assert(s.silent());
    // Polyphony CC
    cc(s, 85, 127);
    assert(s.polyphony() == std::min(nv, kMaxVoices));
    resetPatch(s);
    printf("voice allocation / sustain / polyphony: OK\n");
  }

  // ---- Demo render: chord through each engine family + filter sweep ----
  {
    Renderer r{s};
    const int demo_engines[] = {8, 0, 1, 2, 5, 6, 7, 10, 11, 13, 14, 15, 16, 18, 19, 20, 21};
    for (int e : demo_engines) {
      s.setParam(P_ENGINE, paramFromIndex(P_ENGINE, e));
      const bool perc = e >= 19 || (e >= 2 && e <= 4);
      if (perc) {
        for (int k = 0; k < 4; ++k) {
          on(s, 48 + k * 5, 70 + k * 15);
          r.seconds(0.18f);
          off(s, 48 + k * 5);
        }
      } else {
        for (int n : {48, 55, 60, 64, 67}) on(s, n, 100);
        for (int k = 0; k <= 30; ++k) {   // sweep cutoff with CC 74
          cc(s, 74, 40 + k * 2);
          r.seconds(0.025f);
        }
        for (int n : {48, 55, 60, 64, 67}) off(s, n);
      }
      r.seconds(0.4f);
    }
    // Resonant sweep, self-oscillation check, high drive
    s.setParam(P_ENGINE, paramFromIndex(P_ENGINE, 8));
    cc(s, 71, 120);
    cc(s, 22, 100);
    on(s, 36, 127);
    for (int k = 0; k <= 60; ++k) { cc(s, 74, 20 + k * 1.5); r.seconds(0.02f); }
    off(s, 36);
    r.seconds(0.6f);
    writeWav("host_test/demo.wav", r.pcm);
    float pk = peak(r.pcm, 0, r.pcm.size());
    printf("demo.wav written: %.1f s, peak %.2f\n", r.pcm.size() / 48000.0f, pk);
    assert(pk > 0.05f && pk <= 1.0f);
    for (int16_t x : r.pcm) assert(x > -32768);  // no overflow
    resetPatch(s);
  }

  // ---- Click check: steal fade must not produce a big step ----
  {
    Renderer r{s};
    cc(s, 85, 0);   // mono
    s.setParam(P_AMP_S, 1.0f);
    on(s, 48, 127);
    r.seconds(0.2f);
    size_t before = r.pcm.size();
    on(s, 60, 127);  // steal the only voice
    r.seconds(0.2f);
    int maxStep = 0;
    for (size_t i = before - 200; i < before + 400; ++i)
      maxStep = std::max(maxStep, abs(r.pcm[i] - r.pcm[i - 1]));
    printf("max sample step around steal: %d\n", maxStep);
    off(s, 60);
    r.seconds(1.0f);
    resetPatch(s);
  }

  // ---- Benchmark: 5 voices per engine (host CPU, relative only) ----
  {
    printf("\nengine timing, 5 held voices, host CPU (relative cost only):\n");
    double base = 0;
    for (int e = 0; e < kNumEngines; ++e) {
      s.panic();
      s.setParam(P_ENGINE, paramFromIndex(P_ENGINE, e));
      for (int n : {48, 55, 60, 64, 67}) on(s, n);
      Renderer r{s};
      auto t0 = std::chrono::steady_clock::now();
      r.seconds(2.0f);
      auto t1 = std::chrono::steady_clock::now();
      double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      if (e == 8) base = ms;
      printf("  %2d %-15s %7.1f ms per 2 s of audio\n", e + 1, kEngineNames[e], ms);
      for (int n : {48, 55, 60, 64, 67}) off(s, n);
    }
    (void)base;
  }
  printf("\nall host tests passed\n");
  return 0;
}
