// =============================================================================
// esp32_smart_home.ino — ESP32 Smart-Home Controller + WoL Bridge (Arduino IDE)
// =============================================================================
//
// Arduino IDE Sketch Entry Point.
// Open this file in Arduino IDE 2.x or 1.8.x.
// All helper modules (.h and .cpp) will appear as tabs in the editor.
//
// Required Library (install via Tools > Manage Libraries...):
//   - ArduinoJson (by Benoit Blanchon, version 7.x)
//
// Required Board Package (install via Tools > Board > Boards Manager...):
//   - esp32 (by Espressif Systems)
//   - Select Board: "DOIT ESP32 DEVKIT V1" or "ESP32 Dev Module"
//
// =============================================================================

#include <Arduino.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "config.h"
#include "relay_controller.h"
#include "switch_controller.h"
#include "wol_bridge.h"
#include "wifi_manager.h"
#include "web_server.h"

// --- Module instances ---
RelayController   relays;
SwitchController  switchController;
WolBridge         wolBridge;
WiFiManager       wifiMgr;
SmartHomeServer   apiServer;

// Boot timestamp for uptime calculation
unsigned long bootTime = 0;

// Helper callback so Wi-Fi connection loop during setup() can still service switches
void pollSwitches() {
    switchController.loop();
}

// =============================================================================
// Setup
// =============================================================================
void setup() {
#if DISABLE_BROWNOUT_DETECTOR
    // Disable hardware brownout detector immediately on boot so brief USB voltage
    // drops when Wi-Fi radio powers up do not trigger an MCU reset.
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
#endif

    bootTime = millis();

    // 1. Serial — for debugging and provisioning
    Serial.begin(115200);
    delay(100);  // Brief delay for serial port to stabilize

    Serial.println();
    Serial.println("========================================");
    Serial.printf("  %s v%s\n", FIRMWARE_NAME, FIRMWARE_VERSION);
    Serial.println("  ESP32 Smart-Home Controller + WoL Bridge");
    Serial.println("========================================");
    Serial.printf("  Channels: %d relays, %d WoL targets\n", NUM_RELAYS, MAX_WOL_TARGETS);
    Serial.printf("  Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println();

    // 2. Relays — set all to OFF immediately, before Wi-Fi or server
    //    This is the FIRST hardware action to minimize glitch window.
    relays.begin();
    Serial.println();

    // 3. Physical Switches — activate inputs immediately after relays.
    //    Switches will work offline and online from this exact moment onward.
    switchController.begin(&relays);
    Serial.println();

    // 4. Wi-Fi — connect to the configured network (passes pollSwitches so manual
    //    control works even during boot connection attempts)
    wifiMgr.begin(pollSwitches);
    Serial.println();

    // 5. WoL bridge — initialize target list
    wolBridge.begin();
    Serial.println();

    // 6. HTTP API server — register routes and start listening
    apiServer.begin(&relays, &wolBridge, &bootTime);

    Serial.println();
    Serial.println("========================================");
    Serial.println("  System ready.");
    if (wifiMgr.isConnected()) {
        Serial.printf("  HTTP API: http://%s:%d\n", wifiMgr.getIPAddress().c_str(), HTTP_PORT);
        Serial.printf("  mDNS:     http://%s.local:%d\n", DEVICE_HOSTNAME, HTTP_PORT);
    } else {
        Serial.println("  WARNING: Wi-Fi not connected. Retrying...");
    }
    Serial.printf("  Health:   GET /api/health\n");
    Serial.printf("  Status:   GET /api/status\n");
    Serial.printf("  Free heap: %u bytes\n", ESP.getFreeHeap());
    Serial.println("========================================");
    Serial.println();
}

// =============================================================================
// Main Loop
// =============================================================================
void loop() {
    // 1. Service physical switches with highest priority (instant, offline & online)
    switchController.loop();

    // 2. Service HTTP requests (non-blocking; processes one request per call)
    apiServer.handleClient();

    // 3. Monitor Wi-Fi and handle reconnection + LED
    wifiMgr.loop();

    // Yield to the ESP32 RTOS watchdog (prevents WDT reset on tight loops)
    yield();
}
