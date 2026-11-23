// BLE-MIDI peripheral (standard Apple/MMA service), receive only.
// Written against NimBLE-Arduino 2.x.
#include "ble_midi.h"

#include <NimBLEDevice.h>

#include "config.h"
#include "events.h"
#include "synth/midi_parser.h"

namespace hw {

namespace {

const char* kMidiService = "03B80E5A-EDE8-4B33-A751-6CE34EC4C700";
const char* kMidiChar = "7772E5DB-3868-4112-A1A9-F2669D106BF3";

ps::MidiParser gBleParser;
volatile int gConnections = 0;

class ServerCallbacks : public NimBLEServerCallbacks {
  void onConnect(NimBLEServer* server, NimBLEConnInfo& info) override {
    ++gConnections;
    // Ask for a short connection interval (7.5-15 ms) for low latency.
    server->updateConnParams(info.getConnHandle(), 6, 12, 0, 200);
    // Keep advertising so a second controller can connect.
    NimBLEDevice::startAdvertising();
  }
  void onDisconnect(NimBLEServer*, NimBLEConnInfo&, int) override {
    if (gConnections > 0) --gConnections;
    NimBLEDevice::startAdvertising();
  }
};

class MidiCharCallbacks : public NimBLECharacteristicCallbacks {
  void onWrite(NimBLECharacteristic* c, NimBLEConnInfo&) override {
    NimBLEAttValue v = c->getValue();   // copy: the stack may update it
    ps::decodeBleMidi(v.data(), v.length(), gBleParser);
  }
};

}  // namespace

bool bleMidiBegin() {
  gBleParser.setCallback(postMidiCallback, nullptr);

  NimBLEDevice::init(PS_BLE_NAME);

  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks());

  NimBLEService* service = server->createService(kMidiService);
  NimBLECharacteristic* chr = service->createCharacteristic(
      kMidiChar,
      NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE_NR | NIMBLE_PROPERTY::NOTIFY);
  chr->setCallbacks(new MidiCharCallbacks());
  service->start();

  // 128-bit UUID + name don't fit in one 31-byte advert: name goes in the
  // scan response.
  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  NimBLEAdvertisementData advData;
  advData.setFlags(BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP);
  advData.setCompleteServices(NimBLEUUID(kMidiService));
  adv->setAdvertisementData(advData);
  NimBLEAdvertisementData scanData;
  scanData.setName(PS_BLE_NAME);
  adv->setScanResponseData(scanData);
  return adv->start();
}

int bleMidiConnections() { return gConnections; }

}  // namespace hw
