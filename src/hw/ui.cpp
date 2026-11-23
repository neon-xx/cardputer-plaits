#include "ui.h"

#include <M5Cardputer.h>
#include <stdio.h>
#include <string.h>

#include "audio_engine.h"
#include "ble_midi.h"
#include "config.h"
#include "events.h"
#include "synth/keymap.h"
#include "uart_midi.h"

namespace hw {

namespace {
constexpr int kRows = 8;
constexpr int kRowH = 10;
constexpr int kListY = 36;
constexpr uint16_t kBarBg = 0x18E3;     // dark grey-blue
constexpr uint16_t kSelBg = 0x2945;
constexpr int kTestNote = 60;

void postNudge(ps::ParamId id, int steps) {
  Event e = {EV_PARAM_NUDGE, uint8_t(id), uint8_t(int8_t(steps)), 0, 0.0f};
  postEvent(e);
}
void postSet(ps::ParamId id, float v) {
  Event e = {EV_PARAM_SET, uint8_t(id), 0, 0, v};
  postEvent(e);
}
}  // namespace

void Ui::begin(ps::Synth* synth, DeviceSettings* dev) {
  synth_ = synth;
  dev_ = dev;
  last_param_counter_ = synth->paramChangeCounter;
  memset(held_note_, -1, sizeof(held_note_));
  dirty_ = true;
}

// ---------------------------------------------------------------------------
// Keyboard: edge detection on the set of held keys, so we see releases too.

void Ui::scanKeyboard() {
  const auto& st = M5Cardputer.Keyboard.keysState();
  bool now[128] = {};
  char raw[128] = {};
  for (char c : st.word) {
    const uint8_t k = uint8_t(ps::unshiftKey(c)) & 0x7F;
    now[k] = true;
    raw[k] = c;
  }
  if (st.tab && !prev_tab_) setPlayMode(!play_mode_);
  prev_tab_ = st.tab;
  for (int k = 0; k < 128; ++k) {
    if (now[k] && !prev_down_[k]) onKeyDown(uint8_t(k), raw[k]);
    else if (!now[k] && prev_down_[k]) onKeyUp(uint8_t(k));
    prev_down_[k] = now[k];
  }
}

void Ui::playNoteOn(int note) {
  if (note_refs_[note]++ == 0) {
    Event e = {EV_NOTE_ON, uint8_t(note), uint8_t(velocity_), 0, 0.0f};
    postEvent(e);
  }
}

void Ui::playNoteOff(int note) {
  if (note_refs_[note] == 0) return;
  if (--note_refs_[note] == 0) {
    Event e = {EV_NOTE_OFF, uint8_t(note), 0, 0, 0.0f};
    postEvent(e);
  }
}

void Ui::onKeyDown(uint8_t key, char raw) {
  if (!play_mode_) {
    handleChar(raw);   // raw keeps shift, for '_' and '+'
    return;
  }
  const int semi = ps::keyToSemitone(char(key));
  if (semi >= 0) {
    const int note = 12 * (octave_ + 1) + semi;
    if (note >= 0 && note <= 127) {
      held_note_[key] = int8_t(note);
      playNoteOn(note);
    }
    return;
  }
  switch (key) {   // play-mode controls on the keys the layout leaves free
    case '`': if (octave_ > 0) --octave_; dirty_ = true; break;
    case '1': if (octave_ < 7) ++octave_; dirty_ = true; break;
    case '\'':
      velocity_ = velocity_ >= 127 ? 40 : (velocity_ >= 100 ? 127 : (velocity_ >= 70 ? 100 : 70));
      dirty_ = true;
      break;
    case '-': postNudge(ps::P_ENGINE, -1); break;
    case '\\': postNudge(ps::P_ENGINE, +1); break;
    default: break;
  }
}

void Ui::onKeyUp(uint8_t key) {
  if (held_note_[key] >= 0) {
    playNoteOff(held_note_[key]);
    held_note_[key] = -1;
  }
}

void Ui::setPlayMode(bool on) {
  if (!on) {   // release anything still held
    for (int k = 0; k < 128; ++k) {
      if (held_note_[k] >= 0) { playNoteOff(held_note_[k]); held_note_[k] = -1; }
    }
  }
  play_mode_ = on;
  setStatus(on ? "PLAY mode - Tab to edit" : "EDIT mode - Tab to play");
}

void Ui::bootMessage(const char* line1, const char* line2) {
  auto& d = M5Cardputer.Display;
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::Font0);
  d.setTextSize(2);
  d.setTextDatum(textdatum_t::top_left);
  d.setTextColor(TFT_ORANGE, TFT_BLACK);
  d.drawString("Plaitsputer", 8, 20);
  d.setTextSize(1);
  d.setTextColor(TFT_WHITE, TFT_BLACK);
  d.drawString(line1, 8, 60);
  d.drawString(line2, 8, 74);
  d.setTextColor(TFT_DARKGREY, TFT_BLACK);
  d.drawString("Plaits DSP by Emilie Gillet", 8, 104);
  d.drawString("Mutable Instruments - MIT licence", 8, 116);
}

void Ui::setStatus(const char* s) {
  strncpy(status_, s, sizeof(status_) - 1);
  dirty_ = true;
}

void Ui::settingsChanged() {
  settings_dirty_ = true;
  settings_changed_ms_ = millis();
  dirty_ = true;
}

void Ui::nudgeSelected(int steps) {
  postNudge(ps::ParamId(sel_), steps);
}

void Ui::handleChar(char c) {
  switch (c) {
    // Arrow cluster as printed on the Cardputer keyboard
    case ';': if (sel_ > 0) --sel_; dirty_ = true; break;
    case '.': if (sel_ < ps::P_COUNT - 1) ++sel_; dirty_ = true; break;
    case ',': postNudge(ps::P_ENGINE, -1); break;
    case '/': postNudge(ps::P_ENGINE, +1); break;

    case '-': nudgeSelected(-1); break;
    case '=': nudgeSelected(+1); break;
    case '_': nudgeSelected(-8); break;
    case '+': nudgeSelected(+8); break;
    case 'r': postSet(ps::ParamId(sel_), ps::kParams[sel_].def); break;

    case 'e':
      synth_->encoderMode = ps::EncoderMode((synth_->encoderMode + 1) % ps::ENC_MODE_COUNT);
      settingsChanged();
      break;
    case 'x': {   // per-CC override for the last CC received
      int cc = synth_->lastCC;
      if (cc < 0) { setStatus("Move a knob first"); break; }
      synth_->ccModeOverride[cc] = (synth_->ccModeOverride[cc] + 1) % (ps::ENC_MODE_COUNT + 1);
      settingsChanged();
      break;
    }
    case 'c':
      synth_->midiChannel = (synth_->midiChannel + 1) % 17;
      settingsChanged();
      break;
    case '[':
    case ']': {
      int v = dev_->speakerVolume + (c == ']' ? 16 : -16);
      v = v < 0 ? 0 : (v > 255 ? 255 : v);
      dev_->speakerVolume = uint8_t(v);
      M5Cardputer.Speaker.setVolume(dev_->speakerVolume);
      settingsChanged();
      break;
    }
    case 'p': {
      Event e = {EV_PANIC, 0, 0, 0, 0.0f};
      postEvent(e);
      test_note_ = false;
      setStatus("Panic: all notes off");
      break;
    }
    case 't': {
      test_note_ = !test_note_;
      Event e = {uint8_t(test_note_ ? EV_NOTE_ON : EV_NOTE_OFF), kTestNote, 100, 0, 0.0f};
      postEvent(e);
      break;
    }
    case 's':
      patch_save_pending_ = true;
      setStatus("Saving when silent...");
      break;
    case 'l': {
      float params[ps::P_COUNT];
      if (loadPatchInto(params)) {
        for (int i = 0; i < ps::P_COUNT; ++i) postSet(ps::ParamId(i), params[i]);
        setStatus("Patch loaded");
      } else {
        setStatus("No saved patch");
      }
      break;
    }
    case 'd':
      for (int i = 0; i < ps::P_COUNT; ++i) postSet(ps::ParamId(i), ps::kParams[i].def);
      setStatus("Init patch");
      break;
    default:
      break;
  }
  if (sel_ < top_) top_ = sel_;
  if (sel_ >= top_ + kRows) top_ = sel_ - kRows + 1;
}

void Ui::loop() {
  M5Cardputer.update();
  // Arrow keys arrive as the ; . , / characters printed beneath them.
  scanKeyboard();

  // Track state changes that need a redraw.
  if (synth_->paramChangeCounter != last_param_counter_) {
    last_param_counter_ = synth_->paramChangeCounter;
    dirty_ = true;
  }
  if (synth_->lastCC != last_cc_) { last_cc_ = synth_->lastCC; dirty_ = true; }
  const int ble = bleMidiConnections();
  if (ble != last_ble_) { last_ble_ = ble; dirty_ = true; }
  if (!uart_seen_ && uartMidiLastRxMs() != 0) { uart_seen_ = true; dirty_ = true; }

  const bool idle = synth_->silent() &&
      (millis() - audioLastActivityMs()) > PS_UI_IDLE_MS;
  if (idle) {
    // Flash writes stall both cores, so they also wait for silence.
    if (patch_save_pending_) {
      savePatch(*synth_);
      patch_save_pending_ = false;
      setStatus("Patch saved");
    }
    if (settings_dirty_ && millis() - settings_changed_ms_ > 1500) {
      saveSettings(*synth_, *dev_);
      settings_dirty_ = false;
    }
    if (dirty_) {
      draw();
      dirty_ = false;
      audioResetPeak();
    }
  }
  delay(5);
}

void Ui::draw() {
  auto& d = M5Cardputer.Display;
  char buf[48];
  d.startWrite();
  d.fillScreen(TFT_BLACK);
  d.setFont(&fonts::Font0);
  d.setTextSize(1);

  // ---- Header ----
  d.fillRect(0, 0, 240, 12, kBarBg);
  d.setTextDatum(textdatum_t::top_left);
  d.setTextColor(TFT_WHITE, kBarBg);
  if (play_mode_) {
    d.setTextColor(TFT_BLACK, TFT_ORANGE);
    d.fillRect(0, 0, 88, 12, TFT_ORANGE);
    snprintf(buf, sizeof(buf), "PLAY C%d v%d", octave_, velocity_);
  } else {
    snprintf(buf, sizeof(buf), "PLAITS %d/%dv", synth_->polyphony(), synth_->numVoices());
  }
  d.drawString(buf, 3, 2);
  d.setTextColor(last_ble_ > 0 ? TFT_GREEN : TFT_DARKGREY, kBarBg);
  snprintf(buf, sizeof(buf), last_ble_ > 0 ? "BLE:%d" : "BLE", last_ble_);
  d.drawString(buf, 92, 2);
  d.setTextColor(uart_seen_ ? TFT_GREEN : TFT_DARKGREY, kBarBg);
  d.drawString("DIN", 132, 2);
  d.setTextColor(TFT_WHITE, kBarBg);
  if (synth_->midiChannel == 0) snprintf(buf, sizeof(buf), "ch:ALL");
  else snprintf(buf, sizeof(buf), "ch:%d", synth_->midiChannel);
  d.setTextDatum(textdatum_t::top_right);
  d.drawString(buf, 237, 2);

  // ---- Engine ----
  const int eng = ps::paramIndex(ps::P_ENGINE, synth_->param(ps::P_ENGINE));
  d.setTextDatum(textdatum_t::top_left);
  d.setTextSize(2);
  d.setTextColor(TFT_ORANGE, TFT_BLACK);
  snprintf(buf, sizeof(buf), "%2d %s", eng + 1, ps::kEngineNames[eng]);
  d.drawString(buf, 2, 16);
  d.setTextSize(1);

  // ---- Parameter list ----
  for (int r = 0; r < kRows; ++r) {
    const int i = top_ + r;
    if (i >= ps::P_COUNT) break;
    const int y = kListY + r * kRowH;
    const bool selected = (i == sel_);
    const uint16_t bg = selected ? kSelBg : TFT_BLACK;
    if (selected) d.fillRect(0, y - 1, 240, kRowH, bg);
    const auto& def = ps::kParams[i];
    const float v = synth_->param(ps::ParamId(i));

    d.setTextDatum(textdatum_t::top_left);
    d.setTextColor(selected ? TFT_WHITE : TFT_LIGHTGREY, bg);
    d.drawString(def.name, 4, y);
    d.setTextColor(TFT_DARKGREY, bg);
    snprintf(buf, sizeof(buf), "cc%d", def.cc);
    d.drawString(buf, 76, y);

    // value bar
    d.drawRect(104, y, 52, 7, TFT_DARKGREY);
    const int w = int(v * 50.0f + 0.5f);
    d.fillRect(105, y + 1, w, 5, selected ? TFT_ORANGE : TFT_CYAN);

    ps::formatParam(ps::ParamId(i), v, buf, sizeof(buf));
    d.setTextDatum(textdatum_t::top_right);
    d.setTextColor(TFT_WHITE, bg);
    d.drawString(buf, 237, y);
  }
  // scroll indicator
  const int track = kRows * kRowH;
  const int th = track * kRows / ps::P_COUNT;
  const int ty = kListY + (track - th) * top_ / (ps::P_COUNT - kRows);
  d.fillRect(238, ty, 2, th, TFT_DARKGREY);

  // ---- Footer ----
  d.fillRect(0, 123, 240, 12, kBarBg);
  d.setTextDatum(textdatum_t::top_left);
  if (status_[0]) {
    d.setTextColor(TFT_YELLOW, kBarBg);
    d.drawString(status_, 3, 125);
    status_[0] = 0;
  } else if (play_mode_) {
    d.setTextColor(TFT_WHITE, kBarBg);
    d.drawString("`/1 oct  ' vel  -/\\ engine  Tab", 3, 125);
  } else {
    d.setTextColor(TFT_WHITE, kBarBg);
    const int cc = synth_->lastCC;
    if (cc >= 0) {
      const uint8_t o = synth_->ccModeOverride[cc];
      const char* mode = o ? ps::kEncoderModeNames[o - 1] : "global";
      snprintf(buf, sizeof(buf), "%s | cc%d:%s", ps::kEncoderModeNames[synth_->encoderMode],
               cc, mode);
    } else {
      snprintf(buf, sizeof(buf), "enc: %s", ps::kEncoderModeNames[synth_->encoderMode]);
    }
    d.drawString(buf, 3, 125);
  }
  d.setTextDatum(textdatum_t::top_right);
  d.setTextColor(audioPeakLoad() > 0.85f ? TFT_RED : TFT_GREEN, kBarBg);
  snprintf(buf, sizeof(buf), "pk%d%%", int(audioPeakLoad() * 100.0f + 0.5f));
  d.drawString(buf, 237, 125);
  d.endWrite();
}

}  // namespace hw
