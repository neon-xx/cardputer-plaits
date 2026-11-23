Polyphonic port of Mutable Instruments **Plaits** for the **M5Stack Cardputer ADV**.

> ⚠️ **Early release.** The firmware builds cleanly in CI and the synth core passes its tests on Linux, but it hasn't been run on a real Cardputer yet. Please open an issue with what you hear and the `pk%` load readings.

### Which file do I need?
| File | Use it for |
|---|---|
| `plaitsputer-__TAG__-m5launcher.bin` | **M5Launcher**: copy it to the SD card, then in Launcher go to **SD**, select the file and choose **Install** |
| `plaitsputer-__TAG__-merged.bin` | **Flashing the whole device** at offset `0x0` (this replaces Launcher if you have it) |
| `-app` / `-bootloader` / `-partitions` | Individual images, for manual flashing |
| `SHA256SUMS.txt` | Checksums |

Full-device flash, with the [esptool web flasher](https://espressif.github.io/esptool-js/) or:
`esptool.py --chip esp32s3 write_flash 0x0 plaitsputer-__TAG__-merged.bin`
If the device isn't found, switch the power off, hold **G0**, switch it on, then release G0.

### Features
- All 24 Plaits 1.2 engines (the original DSP, unchanged)
- 5 voices of polyphony by default. Up to 8 voices are allocated at boot if RAM allows, split across both CPU cores, with automatic voice shedding if the CPU overloads.
- Per voice: Plaits → 24 dB/oct resonant ladder low-pass (self-oscillating) → amp ADSR, plus a filter ADSR
- MIDI over **BLE-MIDI** ("Plaitsputer") and the **M5Stack Unit MIDI** on the Grove port (set its switch to BYPASS)
- Full CC map, with **absolute or relative encoders** set globally or per CC (see the [README](https://github.com/neon-xx/cardputer-plaits#readme))
- **Play from the Cardputer keyboard**: press Tab for a two-and-a-half-octave musical-typing layout (z-row and q-row, with black keys on the rows above)
- The display stays frozen while notes are sounding, so drawing never takes CPU from the audio

### Credits
All of the synthesis is **Plaits by Émilie Gillet / Mutable Instruments**, © 2016–2021 Emilie Gillet, MIT licence ([eurorack](https://github.com/pichenettes/eurorack), [stmlib](https://github.com/pichenettes/stmlib)). This is an unofficial port, not affiliated with or endorsed by Mutable Instruments. See [THIRD_PARTY_NOTICES.md](https://github.com/neon-xx/cardputer-plaits/blob/main/THIRD_PARTY_NOTICES.md).
