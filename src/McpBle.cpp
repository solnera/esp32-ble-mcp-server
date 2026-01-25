#include "McpBle.h"

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer* pServer, ble_gap_conn_desc* desc) override {
        McpBle::getInstance()._onConnect(pServer);
        // Update connection params for speed (min 7.5ms, max 15ms, latency 0, timeout 4000ms)
        pServer->updateConnParams(desc->conn_handle, 6, 12, 0, 400);
    }

    void onDisconnect(NimBLEServer* pServer) override {
        McpBle::getInstance()._onDisconnect(pServer);
    }
    
    void onMTUChange(uint16_t MTU, ble_gap_conn_desc* desc) override {
         McpBle::getInstance()._onMtuChange(MTU);
    }
};

class CharCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic* pCharacteristic) override {
        McpBle::getInstance()._onWrite(pCharacteristic);
    }
};

McpBle& McpBle::getInstance() {
    static McpBle instance;
    return instance;
}

McpBle::McpBle() {}

void McpBle::init(const std::string& deviceName) {
    NimBLEDevice::init(deviceName);
    NimBLEDevice::setPower(ESP_PWR_LVL_P9); 
    
    _pServer = NimBLEDevice::createServer();
    _pServer->setCallbacks(new ServerCallbacks());

    NimBLEService* pService = _pServer->createService(SERVICE_UUID);
    
    // RX Characteristic (Write)
    NimBLECharacteristic* pRxChar = pService->createCharacteristic(
        RX_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pRxChar->setCallbacks(new CharCallbacks());

    // TX Characteristic (Notify)
    _pTxCharacteristic = pService->createCharacteristic(
        TX_UUID,
        NIMBLE_PROPERTY::NOTIFY
    );
    
    pService->start();

    NimBLEAdvertising* pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->start();
}

void McpBle::setRxCallback(RxCallback cb) {
    _rxCallback = cb;
}

void McpBle::setMtuCallback(MtuCallback cb) {
    _mtuCallback = cb;
}

bool McpBle::sendNotification(const uint8_t* data, size_t len) {
    if (!_connected || !_pTxCharacteristic) return false;
    _pTxCharacteristic->notify(data, len);
    return true;
}

uint16_t McpBle::getMtu() const {
    return _mtu;
}

bool McpBle::isConnected() const {
    return _connected;
}

void McpBle::_onConnect(NimBLEServer* pServer) {
    _connected = true;
}

void McpBle::_onDisconnect(NimBLEServer* pServer) {
    _connected = false;
    _mtu = 23; // Reset MTU
    NimBLEDevice::startAdvertising();
}

void McpBle::_onMtuChange(uint16_t mtu) {
    _mtu = mtu;
    if (_mtuCallback) {
        _mtuCallback(_mtu);
    }
}

void McpBle::_onWrite(NimBLECharacteristic* pCharacteristic) {
    if (_rxCallback) {
        std::string value = pCharacteristic->getValue();
        if (!value.empty()) {
            _rxCallback((const uint8_t*)value.data(), value.length());
        }
    }
}
