// Byte-stream MIDI 1.0 parser (running status, real-time interleaving,
// SysEx skipping) plus a BLE-MIDI packet decoder that feeds it.
#pragma once

#include <stddef.h>
#include <stdint.h>

namespace ps {

struct MidiMsg {
  uint8_t status, d1, d2;
};

class MidiParser {
 public:
  typedef void (*Callback)(void* ctx, const MidiMsg& m);

  void setCallback(Callback cb, void* ctx) { cb_ = cb; ctx_ = ctx; }
  bool inSysex() const { return sysex_; }

  // Returns true when this byte completed a message (or was a real-time
  // byte / SysEx end), i.e. the stream is at a message boundary.
  bool feed(uint8_t b) {
    if (b >= 0xF8) {               // real-time: never disturbs state
      emit(b, 0, 0);
      return true;
    }
    if (b & 0x80) {
      if (b == 0xF0) { sysex_ = true; running_ = 0; return false; }
      if (b == 0xF7) { sysex_ = false; running_ = 0; return true; }
      sysex_ = false;
      if (b >= 0xF0) {             // system common
        running_ = 0;
        need_ = (b == 0xF2) ? 2 : (b == 0xF1 || b == 0xF3) ? 1 : 0;
        if (need_ == 0) { emit(b, 0, 0); return true; }
        status_ = b;
        idx_ = 0;
        return false;
      }
      running_ = status_ = b;
      need_ = ((b & 0xE0) == 0xC0) ? 1 : 2;   // Cx, Dx take one data byte
      idx_ = 0;
      return false;
    }
    // data byte
    if (sysex_) return false;
    if (status_ == 0) return false;    // no status yet: ignore
    data_[idx_++] = b;
    if (idx_ >= need_) {
      emit(status_, data_[0], need_ > 1 ? data_[1] : 0);
      idx_ = 0;
      if (running_ == 0) status_ = 0;  // system common: no running status
      return true;
    }
    return false;
  }

 private:
  void emit(uint8_t s, uint8_t a, uint8_t b) {
    if (cb_) { MidiMsg m = {s, a, b}; cb_(ctx_, m); }
  }

  Callback cb_ = nullptr;
  void* ctx_ = nullptr;
  uint8_t running_ = 0, status_ = 0, need_ = 0, idx_ = 0;
  uint8_t data_[2] = {0, 0};
  bool sysex_ = false;
};

// Decode one BLE-MIDI characteristic write (Apple / MMA BLE-MIDI spec).
// Packet: [header][ts][status][data..][ts][status][data..]... with optional
// running status (data bytes without a new timestamp/status) and SysEx that
// may span packets.
inline void decodeBleMidi(const uint8_t* p, size_t len, MidiParser& parser) {
  if (len < 2 || !(p[0] & 0x80)) return;   // need a header byte
  bool expect_ts = true;     // a high-bit byte here is a timestamp
  bool last_was_ts = false;
  for (size_t i = 1; i < len; ++i) {
    const uint8_t b = p[i];
    if (b & 0x80) {
      if (!last_was_ts && (expect_ts || parser.inSysex())) {
        last_was_ts = true;    // timestamp-low byte, discard
        continue;
      }
      last_was_ts = false;
      expect_ts = parser.feed(b);
    } else {
      last_was_ts = false;
      expect_ts = parser.feed(b);
    }
  }
}

}  // namespace ps
