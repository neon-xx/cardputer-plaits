#pragma once

#include <stdint.h>

namespace hw {

// Serial MIDI input (M5Stack Unit MIDI / any 31250-baud MIDI on a UART).
bool uartMidiBegin();
uint32_t uartMidiLastRxMs();  // millis() of the last received byte (0 = never)

}  // namespace hw
