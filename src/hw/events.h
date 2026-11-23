// Events posted to the audio task. Everything that changes synth state goes
// through this queue so the synth is only ever mutated from one thread.
#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include "synth/midi_parser.h"

namespace hw {

enum EventType : uint8_t {
  EV_MIDI,         // a=status b=d1 c=d2 (channel-filtered by the synth)
  EV_PARAM_SET,    // a=param, f=value 0..1
  EV_PARAM_NUDGE,  // a=param, b=(int8) steps
  EV_NOTE_ON,      // a=note b=velocity (bypasses channel filter: test note)
  EV_NOTE_OFF,     // a=note
  EV_PANIC,
};

struct Event {
  uint8_t type;
  uint8_t a, b, c;
  float f;
};

extern QueueHandle_t gEventQueue;

inline void postEvent(const Event& e) {
  if (gEventQueue) xQueueSend(gEventQueue, &e, 0);
}

// MidiParser callback that forwards complete messages to the audio task.
inline void postMidiCallback(void*, const ps::MidiMsg& m) {
  if (m.status >= 0xF8) return;   // ignore real-time (clock etc.)
  Event e = {EV_MIDI, m.status, m.d1, m.d2, 0.0f};
  postEvent(e);
}

}  // namespace hw
