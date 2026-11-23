#pragma once

namespace hw {

bool bleMidiBegin();       // start advertising as a BLE-MIDI device
int bleMidiConnections();  // number of connected centrals

}  // namespace hw
