# Mutable Instruments Plaits + stmlib (vendored)

All code in `src/` was written by **Émilie Gillet** for **Mutable Instruments**,
© 2012–2021 Emilie Gillet, and released under the MIT licence (see `LICENSE`
in this folder; every file also keeps its original header).

* `src/plaits/dsp`, `src/plaits/resources.*`: Plaits firmware 1.2 DSP, from
  https://github.com/pichenettes/eurorack (commit f4cfa7697095a28d292bff758beb51deb49b5a86).
* `src/stmlib` (dsp/, utils/, stmlib.h): from https://github.com/pichenettes/stmlib
  (commit d18def816c51d1da0c108236928b2bbd25c17481). The upstream licence file is
  kept unchanged as `LICENSE-stmlib`.

Only the DSP is included. Plaits' hardware drivers, UI and bootloader are not.

## Changes for the ESP32 port

All marked `PLAITSPUTER`. No DSP code was changed.

1. `stmlib/dsp/dsp.h`: ARM `ssat`/`usat`/`vsqrt` inline assembly is only used
   on `__arm__`; the portable C paths from stmlib are used otherwise.
2. `stmlib/stmlib.h`: the `IN_RAM` section attribute is only applied on ARM.
3. `plaits/user_data.h`: the STM32 flash-backed user-data store is replaced
   with a stub (no user wavetables / DX7 banks; built-in tables are used).

This is an unofficial port, not affiliated with or endorsed by Mutable
Instruments or Émilie Gillet. Full notices: `../../THIRD_PARTY_NOTICES.md`.
