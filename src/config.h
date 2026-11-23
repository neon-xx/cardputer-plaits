// Build-time configuration for the Cardputer ADV Plaits synth.
#pragma once

// Audio buffer handed to the speaker driver, in samples (multiple of 12).
// 192 samples = 4 ms at 48 kHz. Latency is ~2-3 buffers + DMA.
#define PS_AUDIO_BUFFER 192

// I2S DMA buffering inside M5Unified's speaker driver (keep small for latency).
#define PS_DMA_BUF_LEN 128
#define PS_DMA_BUF_COUNT 3

// Serial MIDI (M5Stack Unit MIDI U187 on the Grove port, DIP switch on BYPASS).
// Grove white wire = Unit TX -> G1 ; yellow = Unit RX <- G2.
// To use the EXT header UART instead, set RX=15, TX=13.
#define PS_MIDI_UART_RX_PIN 1
#define PS_MIDI_UART_TX_PIN 2
#define PS_MIDI_UART_BAUD 31250

#define PS_BLE_NAME "Plaitsputer"

// Bytes of internal RAM left free after allocating Plaits voices
// (task stacks, BLE buffers, display, etc.).
#define PS_HEAP_RESERVE (64 * 1024)

// Voices are shed (faded out) if a buffer takes longer than this fraction
// of real time to render, to avoid audio dropouts.
#define PS_OVERLOAD_THRESHOLD 0.92f

// The display is only redrawn after the synth has been silent this long.
#define PS_UI_IDLE_MS 250
