#include "uart_midi.h"

#include <Arduino.h>

#include "config.h"
#include "events.h"
#include "synth/midi_parser.h"

namespace hw {

namespace {

ps::MidiParser gUartParser;
volatile uint32_t gLastRx = 0;

void uartTask(void*) {
  for (;;) {
    int avail = Serial1.available();
    if (avail > 0) {
      gLastRx = millis();
      while (avail-- > 0) gUartParser.feed(uint8_t(Serial1.read()));
    }
    vTaskDelay(1);
  }
}

}  // namespace

bool uartMidiBegin() {
  gUartParser.setCallback(postMidiCallback, nullptr);
  Serial1.setRxBufferSize(512);
  Serial1.begin(PS_MIDI_UART_BAUD, SERIAL_8N1, PS_MIDI_UART_RX_PIN, PS_MIDI_UART_TX_PIN);
  return xTaskCreatePinnedToCore(uartTask, "ps_uart", 3072, nullptr, 10, nullptr, 0) == pdPASS;
}

uint32_t uartMidiLastRxMs() { return gLastRx; }

}  // namespace hw
