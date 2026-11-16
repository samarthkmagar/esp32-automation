// =============================================================================
// wifi_manager.cpp — Wi-Fi Connection Management Implementation
// =============================================================================

#include "wifi_manager.h"
#include "config.h"
#include <WiFi.h>
#include <ESPmDNS.h>

void WiFiManager::begin(void (*tickCallback)()) {
    // Configure status LED
    pinMode(STATUS_LED_PIN, OUTPUT);
    digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);  // LED off

    // Allow power rail to stabilize before turning on RF radio
    delay(100);

    // Configure Wi-Fi as station (client) mode
    WiFi.mode(WIFI_STA);

    // Lower Tx power slightly to reduce peak RF power amplifier surge
    WiFi.setTxPower(WIFI_TX_POWER);

    WiFi.setHostname(DEVICE_HOSTNAME);
    WiFi.setAutoReconnect(true);  // ESP32 IDF auto-reconnect
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[WiFi] Connecting to \"%s\"", WIFI_SSID);

    // Wait for initial connection with timeout.
    // In each iteration, tickCallback is executed so physical switches
    // remain completely responsive even if the router is powered off.
    unsigned long startMs = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - startMs > WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println();
            Serial.println("[WiFi] Initial connection timed out. Will retry in loop().");
            return;
        }
        for (int i = 0; i < 5; i++) {
            if (tickCallback) tickCallback();
            delay(50);
        }
        Serial.print(".");
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);  // Off while waiting
    }

    Serial.println();
    Serial.printf("[WiFi] Connected. IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] MAC: %s\n", WiFi.macAddress().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm\n", WiFi.RSSI());

    wasConnected_ = true;
    registerMDNS();

    // Blink on-board blue LED to signal Wi-Fi connection established
    blinkConnectConfirmation(5, 100);
    lastLedToggle_ = millis();
    ledState_ = false;
}

void WiFiManager::loop() {
    bool connected = (WiFi.status() == WL_CONNECTED);

    // Detect state transitions
    if (connected && !wasConnected_) {
        // Just reconnected
        Serial.printf("[WiFi] Reconnected. IP: %s, RSSI: %d dBm\n",
                      WiFi.localIP().toString().c_str(), WiFi.RSSI());
        wasConnected_ = true;
        registerMDNS();  // Re-register mDNS after reconnect
        blinkConnectConfirmation(3, 100);
        lastLedToggle_ = millis();
        ledState_ = false;
    } else if (!connected && wasConnected_) {
        // Just lost connection
        Serial.println("[WiFi] Connection lost. Network commands unavailable until reconnected.");
        wasConnected_ = false;
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);  // Off when disconnected
        ledState_ = false;
    }

    // Manual reconnect attempt if auto-reconnect hasn't recovered
    if (!connected) {
        unsigned long now = millis();
        if (now - lastReconnectAttempt_ > WIFI_RECONNECT_INTERVAL) {
            lastReconnectAttempt_ = now;
            Serial.println("[WiFi] Attempting reconnection...");
            WiFi.disconnect();
            WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
        }
    }

    updateStatusLED(connected);
}

bool WiFiManager::isConnected() const {
    return WiFi.status() == WL_CONNECTED;
}

String WiFiManager::getIPAddress() const {
    return WiFi.localIP().toString();
}

int32_t WiFiManager::getRSSI() const {
    return WiFi.RSSI();
}

String WiFiManager::getSSID() const {
    return WiFi.SSID();
}

String WiFiManager::getMAC() const {
    return WiFi.macAddress();
}

void WiFiManager::updateStatusLED(bool connected) {
    if (connected) {
        // Blink on-board LED when connected to Wi-Fi
        unsigned long now = millis();
        if (now - lastLedToggle_ > STATUS_LED_BLINK_MS) {
            lastLedToggle_ = now;
            ledState_ = !ledState_;
            if (STATUS_LED_ACTIVE_HIGH) {
                digitalWrite(STATUS_LED_PIN, ledState_ ? HIGH : LOW);
            } else {
                digitalWrite(STATUS_LED_PIN, ledState_ ? LOW : HIGH);
            }
        }
    } else {
        // Off when disconnected
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
        ledState_ = false;
    }
}

void WiFiManager::blinkConnectConfirmation(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? HIGH : LOW);
        delay(delayMs);
        digitalWrite(STATUS_LED_PIN, STATUS_LED_ACTIVE_HIGH ? LOW : HIGH);
        delay(delayMs);
    }
}

void WiFiManager::registerMDNS() {
    if (MDNS.begin(DEVICE_HOSTNAME)) {
        MDNS.addService("http", "tcp", HTTP_PORT);
        Serial.printf("[mDNS] Registered: http://%s.local:%d\n", DEVICE_HOSTNAME, HTTP_PORT);
    } else {
        Serial.println("[mDNS] Registration failed.");
    }
}
