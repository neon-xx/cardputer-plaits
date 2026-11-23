#pragma once

#include "storage.h"
#include "synth/synth.h"

namespace hw {

// Keyboard + display. Runs from Arduino loop() (core 1, lowest priority).
// The screen is only redrawn after the synth has been silent for
// PS_UI_IDLE_MS, so drawing never competes with audio rendering.
class Ui {
 public:
  void begin(ps::Synth* synth, DeviceSettings* dev);
  void loop();
  void bootMessage(const char* line1, const char* line2);

 private:
  void handleChar(char c);
  void nudgeSelected(int steps);
  void setStatus(const char* s);
  void settingsChanged();
  void draw();

  ps::Synth* synth_ = nullptr;
  DeviceSettings* dev_ = nullptr;
  int sel_ = 0;
  int top_ = 0;
  bool dirty_ = true;
  uint32_t last_param_counter_ = 0;
  int last_cc_ = -2;
  int last_ble_ = -1;
  bool uart_seen_ = false;
  bool test_note_ = false;
  bool settings_dirty_ = false;
  uint32_t settings_changed_ms_ = 0;
  bool patch_save_pending_ = false;
  char status_[40] = {0};
};

}  // namespace hw
