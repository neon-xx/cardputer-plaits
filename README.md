# Plaitsputer

A polyphonic port of Mutable Instruments **Plaits** for the **M5Stack Cardputer ADV**.

> **Credit:** Plaits was designed and written by **Émilie Gillet** of
> [Mutable Instruments](https://mutable-instruments.net/modules/plaits/). Every
> synthesis engine in this project is her code, taken from the open-source
> [eurorack](https://github.com/pichenettes/eurorack) and
> [stmlib](https://github.com/pichenettes/stmlib) repositories and used under
> the MIT licence. This project adds polyphony, a filter, envelopes, MIDI
> and the Cardputer hardware support around it.
> It is an unofficial, independent port and is not affiliated with or endorsed
> by Mutable Instruments or Émilie Gillet. See [Credits & licence](#credits--licence).

* All 24 Plaits 1.2 engines, using the original DSP code unchanged.
* Polyphony from 1 up to 8 voices, 5 by default (the maximum depends on free RAM at boot, typically 6).
* Per voice: Plaits → **24 dB/oct resonant ladder low-pass** (zero-delay feedback,
  saturating, self-oscillates) → **amp ADSR**. There's also a separate **filter ADSR**.
* MIDI input over **BLE-MIDI** and the **M5Stack Unit MIDI (U187)**, both at the same time.
* MIDI CC control with **absolute or relative encoder** support, set globally or per CC.
* The display **doesn't redraw while any voice is sounding**. It refreshes 250 ms after
  the last voice goes quiet. The keyboard still works while you play.
* Voices are split across both CPU cores. A load meter is built in, and voices are
  shed automatically if the CPU overloads.

> **Status:** the firmware compiles in CI (GitHub Actions, PlatformIO,
> M5Unified 0.3.0, M5Cardputer 1.1.1, NimBLE-Arduino 2.3.x). It uses about
> 39 KB of static RAM, which leaves the rest for Plaits voices, and about 1 MB of flash.
> The DSP core is tested on Linux by `host_test/`. It hasn't been tried on real
> hardware yet, so please report what you hear and the `pk%` load figures.

---

## Flash a prebuilt firmware (no toolchain needed)

Every push to this repo is built by GitHub Actions. To flash it:

1. Open the [Actions tab](../../actions), pick the latest green **build** run and
   download the **plaitsputer-firmware** artifact (you need to be signed in to GitHub),
   or grab the files from the [Releases page](../../releases).
2. Unzip it and flash `plaitsputer-merged.bin` at offset **0x0**, either with
   [ESP Web Tool / esptool-js](https://espressif.github.io/esptool-js/) in Chrome,
   or from a terminal:
   ```
   esptool.py --chip esp32s3 write_flash 0x0 plaitsputer-merged.bin
   ```
   If the device isn't found, enter download mode: switch the power off, hold
   **G0**, switch it on, then release G0.

## Install with M5Launcher

If you use [Launcher](https://github.com/bmorcelli/Launcher) (M5Launcher), use the
**`plaitsputer-…-m5launcher.bin`** file. It's the app-only image, which Launcher
installs into its own app slot without touching Launcher itself. Don't use the
`-merged.bin` file with Launcher: that one is for flashing a whole device.

1. Copy `plaitsputer-…-m5launcher.bin` to the Cardputer's SD card. You can also
   upload it from your phone or computer with Launcher's **WUI** (web UI).
2. In Launcher, open **SD**, select the file and choose **Install**.
3. Plaitsputer starts. On later boots Launcher opens it automatically unless
   you press a key.

The app is about 1 MB, well within Launcher's default Cardputer partition. It
stores settings and the saved patch in NVS, and needs no SPIFFS or FAT partition.

## Build & flash from source

1. Install [PlatformIO](https://platformio.org/) (VS Code extension or `pip install platformio`).
2. Connect the Cardputer ADV over USB-C and run:
   ```
   pio run -t upload
   pio device monitor        # optional: boot log shows voices allocated / free RAM
   ```
   If upload fails, enter download mode: switch the power off, hold **G0**, switch it on, then release G0.

The first build downloads M5Cardputer, M5Unified and NimBLE-Arduino. Émilie
Gillet's Plaits and stmlib sources are included in `lib/mutable` (see its README
and `THIRD_PARTY_NOTICES.md` for the three small portability patches).

## Connecting MIDI

**BLE-MIDI.** The Cardputer advertises as **"Plaitsputer"**.
* macOS: open *Audio MIDI Setup*, then *MIDI Studio*, then the Bluetooth button, and connect.
* iOS: connect from inside a BLE-MIDI-aware app (e.g. *midimittr*, AUM, KORG apps).
* Windows: use *MIDIberry*, *Bluetooth LE MIDI* or Windows MIDI Services.
* Android: use the *MIDI BLE Connect* app.
* Hardware BLE controllers that act as a BLE-MIDI *central* can connect directly.

Up to 2 connections are allowed at once. The header shows `BLE:n` in green when connected.

**M5Stack Unit MIDI (U187).** Plug it into the Cardputer's **Grove** port.
**Set the unit's switch to BYPASS.** In SEPARATE mode the unit doesn't pass its DIN
MIDI IN through to the Grove port, so nothing arrives. The pins are set in `src/config.h`:
RX = G1 (Grove white), TX = G2. To use the EXT header UART instead, set RX = 15, TX = 13.
`DIN` in the header turns green once serial MIDI bytes have been received.

## Keyboard controls

| Key | Action |
|---|---|
| `,` / `/` (← →) | previous / next engine |
| `;` / `.` (↑ ↓) | select parameter |
| `-` / `=` | selected parameter −1 / +1 step |
| `_` / `+` (shift) | −8 / +8 steps |
| `r` | reset the selected parameter to its default |
| `e` | cycle the **global** encoder mode |
| `x` | cycle the encoder mode **override for the last CC received** |
| `c` | MIDI channel: ALL, 1 … 16 |
| `[` / `]` | output level (speaker and headphone master volume) |
| `t` | test note on/off (C4) |
| `p` | panic (all sound off) |
| `s` | save the patch (written once the synth is silent) |
| `l` | load the saved patch |
| `d` | init patch (defaults) |

Settings (channel, encoder modes, output level) save automatically. The patch saves
only when you press `s`, and it loads at boot.

## MIDI implementation

### Notes & performance
* Note on/off with velocity. A repeated note retriggers its own voice. Voice
  stealing takes the quietest released voice, then the oldest one, with a 2 ms fade
  so there's no click.
* **Pitch bend** (±2 semitones by default; the range is set with CC 86).
* **Mod wheel (CC 1)** pushes Timbre up towards maximum.
* **Sustain (CC 64)**, **All sound off (120)**, **Reset controllers (121)**, **All notes off (123)**.

### CC map

| CC | Parameter | Notes |
|---:|---|---|
| **Plaits** |||
| 14 | Engine | 24 engines spread across 0–127 (relative mode: 1 engine per detent) |
| 15 | Harmonics | |
| 16 | Timbre | |
| 17 | Morph | |
| 18 | Out/Aux mix | 0 = OUT, 127 = AUX (Plaits' alternate output) |
| 19 | Velocity → Timbre | |
| **Filter (24 dB ladder)** |||
| 74 | Cutoff | 20 Hz – 20 kHz, exponential |
| 71 | Resonance | self-oscillates above ~95 % |
| 20 | Filter env amount | bipolar, 64 = none, ±7 octaves |
| 21 | Key tracking | 0–100 % |
| 22 | Drive | input saturation |
| 23 | Velocity → Cutoff | up to 3 octaves |
| **Amp ADSR** |||
| 73 | Attack | 1 ms – 10 s |
| 75 | Decay | 1 ms – 10 s |
| 79 | Sustain | level |
| 72 | Release | 1 ms – 10 s |
| **Filter ADSR** |||
| 102 | Attack | |
| 103 | Decay | |
| 104 | Sustain | |
| 105 | Release | |
| **Global** |||
| 7 | Volume | |
| 87 | Velocity → Amp | 0 = no velocity response |
| 85 | Polyphony | 1–8 voices (capped by allocated voices) |
| 86 | Bend range | 0–24 semitones |

CCs 71–75 follow the General MIDI 2 "sound controller" convention (resonance,
release, attack, cutoff, decay), so many controllers' default templates already do something useful.

### Absolute vs relative encoders

The **global mode** (`e`) applies to every parameter CC. A **per-CC override** (`x`)
lets you mix pots and endless encoders on one controller: move the knob, then press
`x` until the footer shows the mode you want for that CC.

| Mode | Increment | Decrement | Also known as |
|---|---|---|---|
| Absolute | – | – | 0–127 position |
| Rel 64+/- | 65, 66 … | 63, 62 … | "binary offset", "relative 64" |
| Rel 2s comp | 1, 2 … | 127, 126 … | "two's complement", "relative 1" |
| Rel signbit | 1, 2 … | 65, 66 … | "signed bit", "sign-magnitude" |

Accelerated encoders work: a value of +5 moves the parameter 5/127. Discrete
parameters (engine, polyphony, bend range) move one step per message, whatever the
acceleration. Mod wheel and sustain are always treated as absolute.

## Performance notes

* The audio runs at 48 kHz with 4 ms buffers (latency is roughly 10–15 ms).
  Active voices alternate between core 1 (with the mixer and speaker driver) and core 0 (with BLE).
* The footer shows **`pk%`**, the peak render load since the last redraw.
  It's 100 % when a buffer took as long as real time to render.
  Above 92 % for two buffers in a row, the quietest or oldest voice is faded out to
  prevent glitches. If you hear notes dropping early, lower polyphony (CC 85).
* Relative engine cost, measured on a PC: Particle, Chords, Inharmonic String, Modal
  and the 6-op FM engines are the heaviest. The analogue-style engines are cheapest.
  On the ESP32-S3, 5 voices of the light engines should fit. The heavy ones may need
  3–4 voices. This hasn't been measured on hardware yet; `pk%` tells you.
* **Denormals.** The resonator-based engines (Particle, Modal, Strings, drums, Chords)
  produce tiny "denormal" floats in their decay tails. These are very slow on x86 (see
  `host_test`). How fast the ESP32-S3 FPU handles them hasn't been checked. As a
  safeguard, self-enveloped engines free their voice after ~50 ms of silence.
* The Plaits internal LPG is held open, and its decay envelope routings are disabled,
  so the ADSRs shape every engine. Percussive engines still ring on their own envelope
  inside the ADSR, and velocity goes to their *accent* input.

## Making a release

Push a version tag, and CI builds the firmware and publishes a GitHub release
with all the images attached:
```
git tag v0.2.0 && git push origin v0.2.0
```
Tags starting with `v0.` or containing `-` (e.g. `v1.0-rc1`) are marked as
pre-releases. The release text comes from `.github/RELEASE_NOTES.md`.

## Host test (PC)

```
make -C host_test && ./host_test/host_test
```
This compiles Plaits and the synth core natively, runs the parser, CC, encoder and
voice-allocation tests, renders `host_test/demo.wav` (chords through most engines
plus filter sweeps), and prints per-engine timing.

## Layout

```
src/main.cpp            setup: display, speaker, BLE, voices, tasks
src/config.h            pins, buffer sizes, thresholds
src/synth/              portable core (also builds on a PC)
  synth.*               voices, allocation, MIDI/CC, rendering
  dsp_blocks.h          ADSR, ladder filter, soft clipper
  params.*              parameter/CC table, formatting, encoder decoding
  midi_parser.h         MIDI byte parser + BLE-MIDI packet decoder
src/hw/                 ESP32 / Cardputer layer
  audio_engine.*        dual-core render + M5 speaker streaming
  ble_midi.*            BLE-MIDI peripheral (NimBLE)
  uart_midi.*           Unit MIDI / serial MIDI input
  ui.*, storage.*       display, keyboard, NVS settings
lib/mutable/            vendored Plaits 1.2 DSP + stmlib (Émilie Gillet, MIT)
host_test/              native test harness
```

## Credits & licence

**Plaits: Émilie Gillet, Mutable Instruments.**
All the sound generation in this project comes from Plaits, Émilie Gillet's
macro-oscillator module, released as open source by Mutable Instruments:

* Plaits 1.2 DSP (`lib/mutable/src/plaits`), © 2016–2021 Emilie Gillet, MIT licence,
  from [pichenettes/eurorack](https://github.com/pichenettes/eurorack).
* stmlib (`lib/mutable/src/stmlib`), © 2012–2021 Emilie Gillet, MIT licence,
  from [pichenettes/stmlib](https://github.com/pichenettes/stmlib).
* Engine design, algorithms, wavetables, speech data and the built-in six-op FM
  banks are all part of the original Plaits code. The engine names shown on
  screen are short descriptions of Plaits' models.
* Read the [Plaits manual](https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/manual/)
  to learn what Harmonics, Timbre and Morph do in each engine.

Three small portability changes were made to that code. No DSP was changed.
They're listed in [`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md), which
also has the full MIT licence text and the exact upstream commits used. Every
vendored file keeps its original copyright header.

"Mutable Instruments" and "Plaits" refer to Émilie Gillet's company and module.
This is an unofficial community port, not a Mutable Instruments product, and it
isn't affiliated with or endorsed by Mutable Instruments or Émilie Gillet.
If you enjoy these sounds, the original Eurorack module and its documentation
are the real thing.

**This project:** the polyphony, voice allocation, ladder filter, envelopes,
MIDI and BLE-MIDI handling, UI and ESP32 code are released under the MIT licence
(see [`LICENSE`](LICENSE)).

Libraries downloaded at build time (M5Cardputer, M5Unified, M5GFX,
NimBLE-Arduino) are covered by their own licences. See
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md).
