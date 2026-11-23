// Plaitsputer: polyphonic Mutable Instruments Plaits for the M5Stack
// Cardputer ADV.
//
// All synthesis engines are Plaits by Emilie Gillet (Mutable Instruments),
// (c) 2016-2021 Emilie Gillet, MIT licence; see lib/mutable and
// THIRD_PARTY_NOTICES.md. Unofficial port, not affiliated with or endorsed
// by Mutable Instruments.
//
//   MIDI in (BLE-MIDI + M5Stack Unit MIDI) -> event queue -> audio task
//   Audio task (core 1) + worker (core 0) -> per-voice Plaits -> 24 dB ladder
//   -> amp ADSR -> mix -> M5Unified speaker driver (ES8311, 48 kHz)
#include <Arduino.h>
#include <M5Cardputer.h>
#include <esp_heap_caps.h>

#include "config.h"
#include "hw/audio_engine.h"
#include "hw/ble_midi.h"
#include "hw/events.h"
#include "hw/storage.h"
#include "hw/uart_midi.h"
#include "hw/ui.h"
#include "synth/synth.h"

namespace hw {
QueueHandle_t gEventQueue = nullptr;
}

static ps::Synth gSynth;
static hw::DeviceSettings gDev;
static hw::Ui gUi;

static void* internalAlloc(size_t n) {
  return heap_caps_malloc(n, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

static size_t internalFree() {
  return heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
}

void setup() {
  auto cfg = M5.config();
  cfg.internal_mic = false;     // speaker only; the mic shares the I2S bus
  M5Cardputer.begin(cfg, true);
  Serial.begin(115200);

  // M5GFX already rotates the ADV panel to landscape (rotation offset 1),
  // so keep its default. Only fix it up if a library change ever leaves us
  // in portrait.
  if (M5Cardputer.Display.width() < M5Cardputer.Display.height()) {
    M5Cardputer.Display.setRotation(M5Cardputer.Display.getRotation() ^ 1);
  }
  M5Cardputer.Display.setBrightness(80);
  gUi.bootMessage("Starting audio...", "");

  // Speaker: 48 kHz, small DMA buffers for low latency. The driver task
  // runs above the audio task so the I2S DMA is always fed.
  {
    auto spk = M5Cardputer.Speaker.config();
    spk.sample_rate = 48000;
    spk.stereo = false;
    spk.dma_buf_len = PS_DMA_BUF_LEN;
    spk.dma_buf_count = PS_DMA_BUF_COUNT;
    spk.task_priority = 22;
    spk.task_pinned_core = 1;
    M5Cardputer.Speaker.config(spk);
    M5Cardputer.Speaker.begin();
  }

  hw::gEventQueue = xQueueCreate(256, sizeof(hw::Event));

  // BLE first, so its RAM is accounted for before voices are allocated.
  hw::bleMidiBegin();

  const int voices = gSynth.init(internalAlloc, internalFree, PS_HEAP_RESERVE);
  hw::loadAll(gSynth, gDev);
  M5Cardputer.Speaker.setVolume(gDev.speakerVolume);

  hw::uartMidiBegin();

  char l1[48], l2[48];
  snprintf(l1, sizeof(l1), "%d Plaits voices allocated", voices);
  snprintf(l2, sizeof(l2), "free RAM %u KB", unsigned(heap_caps_get_free_size(MALLOC_CAP_INTERNAL) / 1024));
  gUi.bootMessage(l1, l2);
  Serial.printf("[plaitsputer] %s, %s\n", l1, l2);
  delay(800);

  if (voices == 0 || !hw::audioBegin(&gSynth)) {
    gUi.bootMessage("Not enough RAM for audio", "");
    for (;;) delay(1000);
  }
  gUi.begin(&gSynth, &gDev);
}

void loop() {
  gUi.loop();
}
