# Android → ESP32: Getting Notifications & Media Metadata

**Date**: 2026-05-15
**Status**: Investigation only — not implemented
**Goal**: Show WhatsApp / SMS notifications and Spotify/YouTube track info on the T-Display-S3-Pro display, received from an Android phone over BLE.

---

## Architecture Context

The current firmware runs **BLE HID** (CompositeHID: mouse + keyboard + media keys) using **NimBLE-Arduino**. The ESP32 acts as a **BLE peripheral** — the Android phone connects to it. This is a one-way link: the ESP32 sends input events *to* the phone, but the phone sends nothing back via HID.

---

## Approaches Evaluated

### ✅ Approach A: Companion Android App (recommended)

A Kotlin Android app runs on the phone:

- **`NotificationListenerService`** (system permission) captures all notifications: WhatsApp, Telegram, SMS, calls, etc.
- **`MediaController`** captures now-playing metadata from Spotify, YouTube Music, etc.
- The app connects to the ESP32's **custom GATT service** (alongside the existing HID service) and writes notification/media data to characteristics.
- The ESP32 subscribes to notifications on these characteristics and updates the display.

**What needs to be built:**

| Component | Lines | Files |
|-----------|-------|-------|
| Android app (Kotlin) | ~600 | NotificationListenerService, MediaController, BLE GATT client |
| ESP32 custom GATT service | ~150 | BleNotificationService.cpp/h (alongside BleCompositeHID) |
| ESP32 notification display | ~200 | app_notifications.cpp/h — scrollable notification log |
| ESP32 media display | ~150 | Show now-playing in existing HID app or new page |

**Pros:**
- Full access to all notifications (WhatsApp, SMS, calls, any app)
- Media metadata (Spotify track + artist)
- Reliable, real-time
- Works in background
- No WiFi needed

**Cons:**
- Requires an Android app sideloaded or on Play Store
- `NotificationListenerService` requires user to grant notification access in Settings
- Need to handle phone-to-ESP32 pairing (BLE secure connection)

### ❌ Approach B: Tasker + Autonotification (no coding)

- **Tasker** (automation app) + **Autonotification** plugin capture notifications
- **Tasker** sends data over WiFi (HTTP POST or MQTT) to the ESP32
- ESP32 runs a lightweight HTTP/MQTT client alongside BLE HID

**Pros:**
- No Android app coding
- Visual Tasker configuration

**Cons:**
- $~7 for Tasker + Autonotification
- Requires WiFi connection (ESP32 must be on same network as phone)
- Tasker background polling can be unreliable
- Media metadata requires separate Tasker profile
- Tasker's BLE support (Web Bluetooth) is experimental and drops connections

### ❌ Approach C: MAP (Message Access Profile) — Bluetooth Classic

- **MAP** is a native Bluetooth Classic profile for SMS/MMS
- ESP32-S3 supports BT Classic + BLE simultaneously

**Pros:**
- Native — no app, no Tasker

**Cons:**
- **Only SMS/MMS** — no WhatsApp, Telegram, Signal, or any app
- No media metadata at all
- Uses Bluetooth Classic (different stack, different pairing)
- Requires major architecture change (Tear down NimBLE-only, add ESP-IDF BT Classic stack)
- HID still works over BLE but now you have two separate Bluetooth radios active
- SMS-only severely limits value

### ❌ Approach D: Web Bluetooth PWA

- Phone visits a webpage that uses Web Bluetooth API + `Notification.requestPermission`
- Page reads notifications and pushes to ESP32 over BLE

**Pros:**
- No install — just open a webpage once

**Cons:**
- Chrome on Android **drops BLE connection after ~30 seconds in background**
- Page must stay open in foreground
- `Notification.requestPermission` limited to same-origin notifications
- Unreliable for real-time notification display

### ❌ Approach E: Android's `BATTERY_SERVICE` / HIDP

- Android has no BLE service that broadcasts notification content to HID peripherals
- **HID raw reports** are the only native BLE data from peripheral → phone
- Nothing provides phone → peripheral notification data

---

## Decision

**Companion Android app (Approach A)** is the only viable path for Android. MAP (Approach C) could be explored later for SMS-only if needed, but the effort-to-value ratio is poor.

## Build Notes (for when this is picked up)

### ESP32 side

Add a custom GATT service to `BleCompositeHID.cpp` (or a new `BleNotificationService.cpp`):

```
Service UUID:  abc1-Notification-Service
  Char UUID:  abc2-notification-title    (notify)
  Char UUID:  abc3-notification-body     (notify)
  Char UUID:  abc4-notification-app      (notify)
  Char UUID:  abc5-media-track           (read/notify)
  Char UUID:  abc6-media-artist          (read/notify)
  Char UUID:  abc7-media-state           (read/notify)
```

The HID and notification services coexist on the same NimBLE device. The phone connects once and can read/write both.

### Android side

- Target: API 33+
- Permissions: `POST_NOTIFICATIONS`, `BIND_NOTIFICATION_LISTENER_SERVICE`
- BLE: Use `BluetoothGatt` to connect to ESP32 and write to the custom chars
- Required: Phone must pair with ESP32 first (through Android Bluetooth settings)

---

## Reference

- [NimBLE-Arduino multiple services example](https://github.com/h2zero/NimBLE-Arduino/blob/master/examples/BLE_multiconnect/BLE_multiconnect.ino)
- [Android NotificationListenerService docs](https://developer.android.com/reference/android/service/notification/NotificationListenerService)
- [Android MediaSession/MediaController docs](https://developer.android.com/reference/android/media/session/MediaController)
