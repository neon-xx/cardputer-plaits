// PLAITSPUTER: portable replacement for the STM32 flash-backed user data
// store. Original copyright (c) 2021 Emilie Gillet, MIT licence.
// No user wavetables / DX7 banks are stored on the Cardputer port, so ptr()
// always returns NULL and Plaits falls back to its built-in tables.

#ifndef PLAITS_USER_DATA_H_
#define PLAITS_USER_DATA_H_

#include "stmlib/stmlib.h"

namespace plaits {

class UserData {
 public:
  enum {
    SIZE = 0x1000
  };
  UserData() { }
  ~UserData() { }
  inline const uint8_t* ptr(int slot) const { return NULL; }
};

}  // namespace plaits

#endif  // PLAITS_USER_DATA_H_
