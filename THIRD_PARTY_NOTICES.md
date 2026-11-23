# Third-party notices

cardputer-plaits is built around code written by **Émilie Gillet** for
**Mutable Instruments**. That code is included, with small modifications, in
`lib/mutable/src` and is used under the MIT licence reproduced below.

## Mutable Instruments Plaits (firmware 1.2 DSP)

* **Author:** Émilie Gillet (emilie.o.gillet@gmail.com), Mutable Instruments
* **Source:** https://github.com/pichenettes/eurorack (`plaits/`), commit
  `f4cfa7697095a28d292bff758beb51deb49b5a86`
* **Files used:** `plaits/dsp/**`, `plaits/resources.h`, `plaits/resources.cc`,
  `plaits/user_data.h` (replaced with a stub, see below)
* **Licence:** MIT (the eurorack repository releases its STM32 projects,
  including Plaits, under the MIT licence; each file carries the notice below)

This covers all of the synthesis engines and the data they use: the virtual
analogue, waveshaping, FM, grain/formant, additive, wavetable, chord, speech,
swarm, noise, particle, string, modal, drum, VA-VCF, phase distortion, six-op FM,
wave terrain, string machine and chiptune engines, plus their lookup tables,
wavetables, speech data and built-in six-op FM patch banks.

The source notes that the SAM speech model (`plaits/dsp/speech/sam_speech_synth.*`)
is "SAM-inspired", as previously used in Mutable Instruments' Shruthi, Ambika
and Braids.

## stmlib

* **Author:** Émilie Gillet, Mutable Instruments
* **Source:** https://github.com/pichenettes/stmlib, commit
  `d18def816c51d1da0c108236928b2bbd25c17481`
* **Files used:** `stmlib.h`, `dsp/*`, `utils/*.h`, `utils/random.cc`
* **Licence:** MIT. The upstream licence file is kept unchanged as
  `lib/mutable/LICENSE-stmlib`. Its STMicroelectronics and stm32loader sections
  cover files that are **not** included here.
* `stmlib/utils/crc32.h` notes that its CRC32 code is derived from work by
  Gary S. Brown. It's included as part of stmlib's utilities and isn't used by
  this firmware.

## Modifications made for this port

Each change is marked `PLAITSPUTER` in the source:

1. `stmlib/dsp/dsp.h`: the ARM inline assembly (`ssat`, `usat`, `vsqrt`) is
   only used on ARM. Other targets use the portable C versions already in
   stmlib.
2. `stmlib/stmlib.h`: the `IN_RAM` linker-section attribute is only applied on ARM.
3. `plaits/user_data.h`: the STM32 flash-backed user-data store is replaced by
   a stub that always reports no user data, so the built-in wavetables and FM
   banks are used.

No DSP code was changed. The polyphony, voice allocation, ladder filter, ADSRs,
MIDI handling and ESP32 code in `src/` were written for this project and are
not part of Mutable Instruments' code.

## MIT licence text (Plaits and stmlib)

```
Copyright 2012-2021 Emilie Gillet.

Author: Emilie Gillet (emilie.o.gillet@gmail.com)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
```

Each individual source file keeps its original copyright header (years
2012–2021) unchanged.

## Libraries fetched at build time (not included in this repository)

PlatformIO downloads these when you build. Each is distributed under its own licence:

* M5Cardputer, M5Unified: M5Stack (MIT)
* M5GFX: M5Stack (see its repository for licence details)
* NimBLE-Arduino: h2zero (Apache 2.0)
* Arduino core for ESP32 and ESP-IDF: Espressif Systems
