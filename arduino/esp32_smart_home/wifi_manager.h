// =============================================================================
// wifi_manager.h — Wi-Fi Connection Management
// =============================================================================
//
// Handles initial connection, automatic reconnection, mDNS registration,
// and status LED feedback.
//
// RECOVERY:
//   If Wi-Fi credentials are wrong or the network is unavailable, the device
//   will keep retrying. To change credentials:
//     1. Edit config.h and reflash via USB.
//     2. (Future) Hold BOOT button during reset for serial provisioning mode.
//
// NOTES:
//   - Wi-Fi loss prevents all network commands. Relay states are preserved
//     in RAM but not network-accessible until Wi-Fi reconnects.
//   - mDNS allows accessing the device as http://esp32-controller.local
//     (requires mDNS/Bonjour support on the client).
//
// =============================================================================

#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

class WiFiManager {
public:
    // Connect to Wi-Fi and register mDNS. Blocks for up to WIFI_CONNECT_TIMEOUT_MS.
    // Optional tickCallback is invoked periodically (e.g. to poll physical switches)
    // so physical switches respond immediately even during boot Wi-Fi connection.
    void begin(void (*tickCallback)() = nullptr);

    // Call in loop(). Handles reconnection and status LED.
    void loop();

    // Connection status
    bool isConnected() const;
    String getIPAddress() const;
    int32_t getRSSI() const;
    String getSSID() const;
    String getMAC() const;

    // Flash the onboard LED a given number of times
    void blinkConnectConfirmation(int times = 5, int delayMs = 100);

private:
    unsigned long lastReconnectAttempt_ = 0;
    unsigned long lastLedToggle_ = 0;
    bool wasConnected_ = false;
    bool ledState_ = false;

    void updateStatusLED(bool connected);
    void registerMDNS();
};

#endif // WIFI_MANAGER_H
