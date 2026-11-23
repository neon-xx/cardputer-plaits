// Audio scheduling: one task per CPU core renders half of the active voices
// into its own accumulator; core 1 then mixes and streams to the speaker.
#include "audio_engine.h"

#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_timer.h>
#include <string.h>

#include "config.h"
#include "events.h"

namespace hw {

namespace {

constexpr size_t kBuf = PS_AUDIO_BUFFER;
static_assert(kBuf % ps::kPlaitsBlock == 0, "buffer must be a multiple of 12");
static_assert(kBuf / ps::kPlaitsBlock <= ps::kMaxBufferBlocks, "buffer too long");

ps::Synth* gSynth = nullptr;
TaskHandle_t gAudioTask = nullptr;
TaskHandle_t gWorkerTask = nullptr;

int16_t gOut[3][kBuf] __attribute__((aligned(4)));
float gAccA[kBuf] __attribute__((aligned(4)));
float gAccB[kBuf] __attribute__((aligned(4)));

int gWorkerList[ps::kMaxVoices];
volatile int gWorkerCount = 0;

volatile float gLoad = 0.0f;
volatile float gPeakLoad = 0.0f;
volatile uint32_t gShed = 0;
volatile uint32_t gLastActivity = 0;

void applyEvent(const Event& e) {
  switch (e.type) {
    case EV_MIDI:
      gSynth->handleMidi({e.a, e.b, e.c});
      break;
    case EV_PARAM_SET:
      if (e.a < ps::P_COUNT) gSynth->setParam(ps::ParamId(e.a), e.f);
      break;
    case EV_PARAM_NUDGE:
      if (e.a < ps::P_COUNT) gSynth->nudgeParam(ps::ParamId(e.a), int8_t(e.b));
      break;
    case EV_NOTE_ON:
      gSynth->noteOn(e.a, e.b);
      break;
    case EV_NOTE_OFF:
      gSynth->noteOff(e.a);
      break;
    case EV_PANIC:
      gSynth->panic();
      break;
  }
}

// Core 0: renders its share of voices when poked by the audio task.
void workerTask(void*) {
  for (;;) {
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    memset(gAccB, 0, sizeof(gAccB));
    const int n = gWorkerCount;
    for (int i = 0; i < n; ++i) gSynth->renderVoice(gWorkerList[i], gAccB, kBuf);
    xTaskNotifyGive(gAudioTask);
  }
}

// Core 1: control, render, mix, stream.
void audioTask(void*) {
  const float budget_us = 1.0e6f * float(kBuf) / ps::kSampleRate;
  int list[ps::kMaxVoices];
  int mine[ps::kMaxVoices];
  int write = 0;
  int overload_runs = 0;

  for (;;) {
    // 1. Apply queued MIDI / UI events (bounded so a flood can't stall audio).
    Event e;
    for (int k = 0; k < 64 && xQueueReceive(gEventQueue, &e, 0) == pdTRUE; ++k) applyEvent(e);

    // 2. Render.
    const int64_t t0 = esp_timer_get_time();
    const int n = gSynth->prepare(kBuf, list);
    int n_mine = 0, n_worker = 0;
    for (int i = 0; i < n; ++i) {
      if (i & 1) gWorkerList[n_worker++] = list[i];
      else mine[n_mine++] = list[i];
    }
    gWorkerCount = n_worker;
    if (n_worker) xTaskNotifyGive(gWorkerTask);

    memset(gAccA, 0, sizeof(gAccA));
    for (int i = 0; i < n_mine; ++i) gSynth->renderVoice(mine[i], gAccA, kBuf);
    if (n_worker) ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

    const float* accs[2] = {gAccA, gAccB};
    gSynth->mixdown(accs, n_worker ? 2 : 1, gOut[write], kBuf);
    const int64_t t1 = esp_timer_get_time();

    // 3. Load metering + overload protection.
    const float load = float(t1 - t0) / budget_us;
    gLoad = gLoad + (load - gLoad) * 0.05f;
    if (load > gPeakLoad) gPeakLoad = load;
    if (n > 0) gLastActivity = millis();
    if (load > PS_OVERLOAD_THRESHOLD) {
      if (++overload_runs >= 2) {
        gSynth->shedVoice();
        ++gShed;
        overload_runs = 0;
      }
    } else {
      overload_runs = 0;
    }

    // 4. Hand the buffer to the speaker driver. It holds at most two
    //    buffers (one playing, one queued); we fill the third meanwhile.
    while (M5Cardputer.Speaker.isPlaying(0) >= 2) vTaskDelay(1);
    M5Cardputer.Speaker.playRaw(gOut[write], kBuf, uint32_t(ps::kSampleRate),
                                false, 1, 0, false);
    write = (write + 1) % 3;
  }
}

}  // namespace

bool audioBegin(ps::Synth* synth) {
  gSynth = synth;
  gLastActivity = millis();
  // Worker on core 0 (shares it with BLE), audio + speaker driver on core 1.
  if (xTaskCreatePinnedToCore(workerTask, "ps_worker", 12288, nullptr, 18,
                              &gWorkerTask, 0) != pdPASS) return false;
  if (xTaskCreatePinnedToCore(audioTask, "ps_audio", 12288, nullptr, 19,
                              &gAudioTask, 1) != pdPASS) return false;
  return true;
}

float audioLoad() { return gLoad; }
float audioPeakLoad() { return gPeakLoad; }
void audioResetPeak() { gPeakLoad = 0.0f; }
uint32_t audioShedCount() { return gShed; }
uint32_t audioLastActivityMs() { return gLastActivity; }

}  // namespace hw
