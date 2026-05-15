#ifndef BLE_COMPOSITE_HID_H
#define BLE_COMPOSITE_HID_H

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED)

#include "NimBLECharacteristic.h"
#include "NimBLEHIDDevice.h"

// Media key report structure
typedef struct {
  uint8_t key1;
  uint8_t key2;
} MediaKeyReport;

// Composite HID Report IDs
#define KEYBOARD_ID 0x01
#define MEDIA_KEYS_ID 0x02
#define MOUSE_ID 0x03

class BleCompositeHID : public NimBLEServerCallbacks, public NimBLECharacteristicCallbacks {
private:
    NimBLEHIDDevice* hid;
    NimBLECharacteristic* inputKeyboard;
    NimBLECharacteristic* outputKeyboard;
    NimBLECharacteristic* inputMediaKeys;
    NimBLECharacteristic* inputMouse;
    
    std::string deviceName;
    std::string deviceManufacturer;
    uint8_t batteryLevel;
    bool connected = false;
    bool initialized = false;
    
    // Keyboard state
    uint8_t _keyReport[8] = {0};
    
    // Mouse state
    int8_t mouseX = 0;
    int8_t mouseY = 0;
    int8_t mouseWheel = 0;
    uint8_t mouseButtons = 0;
    
    void delay_ms(uint64_t ms);
    void sendKeyboardReport();
    void sendMediaReport(MediaKeyReport* keys);
    void sendMouseReport();

public:
    BleCompositeHID(std::string deviceName = "T-Watch HID", std::string deviceManufacturer = "LilyGo", uint8_t batteryLevel = 100);
    void begin(void);
    void end(void);
    
    // Keyboard methods
    size_t pressKey(uint8_t k);
    size_t releaseKey(uint8_t k);
    size_t writeKey(uint8_t c);
    void releaseAllKeys(void);
    
    // Media key methods
    void pressMediaKey(const MediaKeyReport k);
    void releaseMediaKey(const MediaKeyReport k);
    void writeMediaKey(const MediaKeyReport k);
    
    // Mouse methods
    void mouseMove(int8_t x, int8_t y, int8_t wheel = 0);
    void mouseClick(uint8_t button = 1);
    void mousePress(uint8_t button = 1);
    void mouseRelease(uint8_t button = 1);
    bool isMousePressed(uint8_t button = 1);
    
    // Connection
    bool isConnected(void);
    void setBatteryLevel(uint8_t level);
    
    // Callbacks
    void onConnect(NimBLEServer* pServer);
    void onDisconnect(NimBLEServer* pServer);
};

// Key codes
const uint8_t KEY_LEFT_CTRL = 0x80;
const uint8_t KEY_LEFT_SHIFT = 0x81;
const uint8_t KEY_LEFT_ALT = 0x82;
const uint8_t KEY_LEFT_GUI = 0x83;
const uint8_t KEY_RIGHT_CTRL = 0x84;
const uint8_t KEY_RIGHT_SHIFT = 0x85;
const uint8_t KEY_RIGHT_ALT = 0x86;
const uint8_t KEY_RIGHT_GUI = 0x87;

const uint8_t KEY_UP_ARROW = 0xDA;
const uint8_t KEY_DOWN_ARROW = 0xD9;
const uint8_t KEY_LEFT_ARROW = 0xD8;
const uint8_t KEY_RIGHT_ARROW = 0xD7;
const uint8_t KEY_BACKSPACE = 0xB2;
const uint8_t KEY_TAB = 0xB3;
const uint8_t KEY_RETURN = 0xB0;
const uint8_t KEY_ESC = 0xB1;
const uint8_t KEY_DELETE = 0xD4;
const uint8_t KEY_PAGE_UP = 0xD3;
const uint8_t KEY_PAGE_DOWN = 0xD6;
const uint8_t KEY_HOME = 0xD2;
const uint8_t KEY_END = 0xD5;

// Media keys
const MediaKeyReport KEY_MEDIA_NEXT_TRACK = {1, 0};
const MediaKeyReport KEY_MEDIA_PREVIOUS_TRACK = {2, 0};
const MediaKeyReport KEY_MEDIA_STOP = {4, 0};
const MediaKeyReport KEY_MEDIA_PLAY_PAUSE = {8, 0};
const MediaKeyReport KEY_MEDIA_MUTE = {16, 0};
const MediaKeyReport KEY_MEDIA_VOLUME_UP = {32, 0};
const MediaKeyReport KEY_MEDIA_VOLUME_DOWN = {64, 0};
const MediaKeyReport KEY_MEDIA_WWW_HOME = {128, 0};

// Mouse buttons
const uint8_t MOUSE_LEFT = 1;
const uint8_t MOUSE_RIGHT = 2;
const uint8_t MOUSE_MIDDLE = 4;

#endif // CONFIG_BT_ENABLED
#endif // BLE_COMPOSITE_HID_H
