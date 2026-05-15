#include "BleCompositeHID.h"

#if defined(CONFIG_BT_ENABLED)

#include <NimBLEDevice.h>
#include <NimBLEUtils.h>
#include <NimBLEServer.h>
#include "NimBLEHIDDevice.h"
#include "HIDTypes.h"
#include <driver/adc.h>

// Composite HID Report Descriptor - combines Keyboard, Media Keys, and Mouse
static const uint8_t _hidReportDescriptor[] = {
    // Keyboard Report
    USAGE_PAGE(1), 0x01,          // Generic Desktop
    USAGE(1), 0x06,                // Keyboard
    COLLECTION(1), 0x01,           // Application
    REPORT_ID(1), KEYBOARD_ID,
    USAGE_PAGE(1), 0x07,           // Key Codes
    USAGE_MINIMUM(1), 0xE0,
    USAGE_MAXIMUM(1), 0xE7,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01,
    REPORT_COUNT(1), 0x08,
    HIDINPUT(1), 0x02,             // Data, Variable, Absolute
    REPORT_COUNT(1), 0x01,
    REPORT_SIZE(1), 0x08,
    HIDINPUT(1), 0x01,             // Constant
    REPORT_COUNT(1), 0x06,
    REPORT_SIZE(1), 0x08,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x65,
    USAGE_MINIMUM(1), 0x00,
    USAGE_MAXIMUM(1), 0x65,
    HIDINPUT(1), 0x00,             // Data, Array
    END_COLLECTION(0),
    
    // Media Keys Report
    USAGE_PAGE(1), 0x0C,           // Consumer
    USAGE(1), 0x01,                // Consumer Control
    COLLECTION(1), 0x01,           // Application
    REPORT_ID(1), MEDIA_KEYS_ID,
    USAGE_PAGE(1), 0x0C,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_SIZE(1), 0x01,
    REPORT_COUNT(1), 0x10,
    USAGE(1), 0xB5,                // Next Track
    USAGE(1), 0xB6,                // Previous Track
    USAGE(1), 0xB7,                // Stop
    USAGE(1), 0xCD,                // Play/Pause
    USAGE(1), 0xE2,                // Mute
    USAGE(1), 0xE9,                // Volume Up
    USAGE(1), 0xEA,                // Volume Down
    USAGE(2), 0x23, 0x02,          // WWW Home
    USAGE(2), 0x94, 0x01,          // My Computer
    USAGE(2), 0x92, 0x01,          // Calculator
    USAGE(2), 0x2A, 0x02,          // WWW Bookmarks
    USAGE(2), 0x21, 0x02,          // WWW Search
    USAGE(2), 0x26, 0x02,          // WWW Stop
    USAGE(2), 0x24, 0x02,          // WWW Back
    USAGE(2), 0x83, 0x01,          // Media Selection
    USAGE(2), 0x8A, 0x01,          // Mail
    HIDINPUT(1), 0x02,             // Data, Variable, Absolute
    END_COLLECTION(0),
    
    // Mouse Report
    USAGE_PAGE(1), 0x01,           // Generic Desktop
    USAGE(1), 0x02,                // Mouse
    COLLECTION(1), 0x01,           // Application
    USAGE(1), 0x01,                // Pointer
    COLLECTION(1), 0x00,           // Physical
    REPORT_ID(1), MOUSE_ID,
    USAGE_PAGE(1), 0x09,           // Buttons
    USAGE_MINIMUM(1), 0x01,
    USAGE_MAXIMUM(1), 0x03,
    LOGICAL_MINIMUM(1), 0x00,
    LOGICAL_MAXIMUM(1), 0x01,
    REPORT_COUNT(1), 0x03,
    REPORT_SIZE(1), 0x01,
    HIDINPUT(1), 0x02,             // Data, Variable, Absolute
    REPORT_COUNT(1), 0x01,
    REPORT_SIZE(1), 0x05,
    HIDINPUT(1), 0x01,             // Constant
    USAGE_PAGE(1), 0x01,           // Generic Desktop
    USAGE(1), 0x30,                // X
    USAGE(1), 0x31,                // Y
    USAGE(1), 0x38,                // Wheel
    LOGICAL_MINIMUM(1), 0x81,
    LOGICAL_MAXIMUM(1), 0x7F,
    REPORT_SIZE(1), 0x08,
    REPORT_COUNT(1), 0x03,
    HIDINPUT(1), 0x06,             // Data, Variable, Relative
    END_COLLECTION(0),
    END_COLLECTION(0)
};

BleCompositeHID::BleCompositeHID(std::string deviceName, std::string deviceManufacturer, uint8_t batteryLevel) {
    this->deviceName = deviceName;
    this->deviceManufacturer = deviceManufacturer;
    this->batteryLevel = batteryLevel;
}

void BleCompositeHID::begin(void) {
    if (!initialized) {
        // First time initialization
        NimBLEDevice::init(deviceName);
        NimBLEServer *pServer = NimBLEDevice::createServer();
        pServer->setCallbacks(this);

        hid = new NimBLEHIDDevice(pServer);
        inputKeyboard = hid->inputReport(KEYBOARD_ID);
        outputKeyboard = hid->outputReport(KEYBOARD_ID);
        inputMediaKeys = hid->inputReport(MEDIA_KEYS_ID);
        inputMouse = hid->inputReport(MOUSE_ID);

        hid->manufacturer()->setValue(deviceManufacturer);
        hid->pnp(0x02, 0x05ac, 0x820a, 0x0210);
        hid->hidInfo(0x00, 0x01);
        
        NimBLEDevice::setSecurityAuth(true, true, true);
        
        hid->reportMap((uint8_t*)_hidReportDescriptor, sizeof(_hidReportDescriptor));
        hid->startServices();

        hid->setBatteryLevel(batteryLevel);
        initialized = true;
    }
    
    // Start or restart advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    pAdvertising->setAppearance(HID_KEYBOARD);
    pAdvertising->addServiceUUID(hid->hidService()->getUUID());
    pAdvertising->start();
}

void BleCompositeHID::end(void) {
    // Stop advertising
    NimBLEAdvertising *pAdvertising = NimBLEDevice::getAdvertising();
    if (pAdvertising) {
        pAdvertising->stop();
    }
    
    // Disconnect any connected clients
    if (connected) {
        NimBLEServer* pServer = NimBLEDevice::getServer();
        if (pServer) {
            // Disconnect all connected clients
            pServer->disconnect(0);
        }
    }
}

void BleCompositeHID::delay_ms(uint64_t ms) {
    uint64_t m = esp_timer_get_time();
    if(ms){
        uint64_t e = (m + (ms * 1000));
        if(m > e){
            while(esp_timer_get_time() > e) { }
        }
        while(esp_timer_get_time() < e) { }
    }
}

void BleCompositeHID::sendKeyboardReport() {
    if(this->isConnected()) {
        this->inputKeyboard->setValue(_keyReport, sizeof(_keyReport));
        this->inputKeyboard->notify();
    }
}

void BleCompositeHID::sendMediaReport(MediaKeyReport* keys) {
    if(this->isConnected()) {
        uint8_t report[2];
        report[0] = keys->key1;
        report[1] = keys->key2;
        this->inputMediaKeys->setValue(report, sizeof(report));
        this->inputMediaKeys->notify();
    }
}

void BleCompositeHID::sendMouseReport() {
    if(this->isConnected()) {
        uint8_t report[4];
        report[0] = mouseButtons;
        report[1] = mouseX;
        report[2] = mouseY;
        report[3] = mouseWheel;
        this->inputMouse->setValue(report, sizeof(report));
        this->inputMouse->notify();
    }
}

// Keyboard implementation
size_t BleCompositeHID::pressKey(uint8_t k) {
    uint8_t i;
    if (k >= 0x80) {  // Modifier key
        _keyReport[0] |= (1 << (k - 0x80));
    } else {
        for (i = 2; i < 8; i++) {
            if (_keyReport[i] == 0x00) {
                _keyReport[i] = k;
                break;
            }
        }
        if (i == 8) {
            return 0;
        }
    }
    sendKeyboardReport();
    return 1;
}

size_t BleCompositeHID::releaseKey(uint8_t k) {
    uint8_t i;
    if (k >= 0x80) {  // Modifier key
        _keyReport[0] &= ~(1 << (k - 0x80));
    } else {
        for (i = 2; i < 8; i++) {
            if (_keyReport[i] == k) {
                _keyReport[i] = 0x00;
            }
        }
    }
    sendKeyboardReport();
    return 1;
}

size_t BleCompositeHID::writeKey(uint8_t c) {
    pressKey(c);
    delay_ms(7);
    releaseKey(c);
    return 1;
}

void BleCompositeHID::releaseAllKeys(void) {
    memset(_keyReport, 0, sizeof(_keyReport));
    sendKeyboardReport();
}

// Media key implementation
void BleCompositeHID::pressMediaKey(const MediaKeyReport k) {
    MediaKeyReport keys = k;
    sendMediaReport(&keys);
}

void BleCompositeHID::releaseMediaKey(const MediaKeyReport k) {
    MediaKeyReport keys = {0, 0};
    sendMediaReport(&keys);
}

void BleCompositeHID::writeMediaKey(const MediaKeyReport k) {
    pressMediaKey(k);
    delay_ms(7);
    releaseMediaKey(k);
}

// Mouse implementation
void BleCompositeHID::mouseMove(int8_t x, int8_t y, int8_t wheel) {
    mouseX = x;
    mouseY = y;
    mouseWheel = wheel;
    sendMouseReport();
    mouseX = 0;
    mouseY = 0;
    mouseWheel = 0;
}

void BleCompositeHID::mouseClick(uint8_t button) {
    mousePress(button);
    delay_ms(10);
    mouseRelease(button);
}

void BleCompositeHID::mousePress(uint8_t button) {
    mouseButtons |= button;
    sendMouseReport();
}

void BleCompositeHID::mouseRelease(uint8_t button) {
    mouseButtons &= ~button;
    sendMouseReport();
}

bool BleCompositeHID::isMousePressed(uint8_t button) {
    return (mouseButtons & button) != 0;
}

// Connection
bool BleCompositeHID::isConnected(void) {
    return this->connected;
}

void BleCompositeHID::setBatteryLevel(uint8_t level) {
    this->batteryLevel = level;
    if(hid != nullptr)
        this->hid->setBatteryLevel(this->batteryLevel);
}

void BleCompositeHID::onConnect(NimBLEServer* pServer) {
    this->connected = true;
}

void BleCompositeHID::onDisconnect(NimBLEServer* pServer) {
    this->connected = false;
    NimBLEDevice::startAdvertising();
}

#endif // CONFIG_BT_ENABLED
