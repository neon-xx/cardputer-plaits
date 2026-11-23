# Vendored Mutable Instruments code

* `src/plaits/dsp`, `src/plaits/resources.*` - from github.com/pichenettes/eurorack
  (commit f4cfa7697095a28d292bff758beb51deb49b5a86), Plaits firmware 1.2 DSP.
* `src/stmlib` (dsp/, utils/, stmlib.h) - from github.com/pichenettes/stmlib
  (commit d18def816c51d1da0c108236928b2bbd25c17481).

Copyright Emilie Gillet, MIT licence (see the header of every file).

Changes for the ESP32 port (all marked `PLAITSPUTER`):
1. `stmlib/dsp/dsp.h` - ARM `ssat`/`usat`/`vsqrt` inline asm only on `__arm__`;
   portable C otherwise.
2. `stmlib/stmlib.h` - `IN_RAM` section attribute only on ARM.
3. `plaits/user_data.h` - replaced the STM32 flash-backed store with a stub
   (no user wavetables / DX7 banks; built-in tables are used).
