// =============================================================================
// wol_bridge.cpp — Wake-on-LAN Magic Packet Implementation
// =============================================================================

#include "wol_bridge.h"
#include <WiFi.h>

void WolBridge::begin() {
    memset(&lastResult_, 0, sizeof(lastResult_));
    lastResult_.targetName = "";
    lastResult_.macStr[0] = '\0';
    Serial.println("[WoL] Bridge initialized.");

    // Log configured targets
    for (uint8_t i = 0; i < MAX_WOL_TARGETS; i++) {
        if (strlen(WOL_TARGETS[i].name) == 0) continue;
        char mac[18];
        formatMAC(WOL_TARGETS[i].mac, mac);
        Serial.printf("[WoL] Target \"%s\" MAC=%s %s\n",
                      WOL_TARGETS[i].name, mac,
                      WOL_TARGETS[i].enabled ? "(enabled)" : "(disabled)");
    }
}

bool WolBridge::sendWake(const char* targetName, uint8_t retries, uint16_t retryDelayMs) {
    // Validate target
    int idx = findTarget(targetName);
    if (idx < 0) {
        Serial.printf("[WoL] Target \"%s\" not found or disabled.\n", targetName);
        lastResult_.targetName = targetName;
        lastResult_.attempted = false;
        lastResult_.packetsSent = 0;
        lastResult_.packetsRequested = 0;
        lastResult_.timestamp = millis();
        return false;
    }

    // Clamp parameters to configured limits
    retries = min(retries, (uint8_t)WOL_MAX_RETRIES);
    retryDelayMs = min(retryDelayMs, (uint16_t)WOL_MAX_RETRY_DELAY_MS);

    const uint8_t* mac = WOL_TARGETS[idx].mac;
    uint8_t totalAttempts = retries + 1;  // first send + retries

    // --- Construct the magic packet ---
    // Standard WoL payload: 6 × 0xFF, then target MAC repeated 16 times.
    // Total: 6 + (6 × 16) = 102 bytes.
    uint8_t packet[102];
    memset(packet, 0xFF, 6);
    for (uint8_t i = 0; i < 16; i++) {
        memcpy(packet + 6 + (i * 6), mac, 6);
    }

    // --- Send via UDP broadcast ---
    IPAddress broadcastIP;
    broadcastIP.fromString(WOL_BROADCAST_ADDRESS);

    char macStr[18];
    formatMAC(mac, macStr);
    Serial.printf("[WoL] Sending to \"%s\" (%s) via %s:%d, %d packet(s), %dms delay\n",
                  targetName, macStr, WOL_BROADCAST_ADDRESS, WOL_UDP_PORT,
                  totalAttempts, retryDelayMs);

    uint8_t packetsSent = 0;
    for (uint8_t attempt = 0; attempt < totalAttempts; attempt++) {
        if (attempt > 0) {
            // NOTE: delay() blocks. With default settings (3 retries × 100ms)
            // total blocking is ≤300ms, acceptable for infrequent WoL commands.
            // Hard-capped at WOL_MAX_RETRIES × WOL_MAX_RETRY_DELAY_MS.
            delay(retryDelayMs);
        }

        if (udp_.beginPacket(broadcastIP, WOL_UDP_PORT)) {
            udp_.write(packet, sizeof(packet));
            if (udp_.endPacket()) {
                packetsSent++;
            } else {
                Serial.printf("[WoL] endPacket failed on attempt %d\n", attempt + 1);
            }
        } else {
            Serial.printf("[WoL] beginPacket failed on attempt %d\n", attempt + 1);
        }
    }

    // Record result
    lastResult_.targetName     = WOL_TARGETS[idx].name;
    strncpy(lastResult_.macStr, macStr, sizeof(lastResult_.macStr) - 1);
    lastResult_.macStr[sizeof(lastResult_.macStr) - 1] = '\0';
    lastResult_.attempted      = true;
    lastResult_.packetsSent    = packetsSent;
    lastResult_.packetsRequested = totalAttempts;
    lastResult_.timestamp      = millis();

    Serial.printf("[WoL] Result: %d/%d packets sent.\n", packetsSent, totalAttempts);
    return packetsSent > 0;
}

int WolBridge::findTarget(const char* name) const {
    if (name == nullptr) return -1;
    for (uint8_t i = 0; i < MAX_WOL_TARGETS; i++) {
        if (strlen(WOL_TARGETS[i].name) == 0) continue;
        if (strcmp(WOL_TARGETS[i].name, name) == 0) {
            return WOL_TARGETS[i].enabled ? (int)i : -1;
        }
    }
    return -1;
}

uint8_t WolBridge::getTargetCount() const {
    uint8_t count = 0;
    for (uint8_t i = 0; i < MAX_WOL_TARGETS; i++) {
        if (strlen(WOL_TARGETS[i].name) > 0) count++;
    }
    return count;
}

const WolTargetConfig* WolBridge::getTarget(uint8_t index) const {
    if (index >= MAX_WOL_TARGETS) return nullptr;
    return &WOL_TARGETS[index];
}

const WolResult& WolBridge::getLastResult() const {
    return lastResult_;
}

void WolBridge::formatMAC(const uint8_t mac[6], char* buf) {
    snprintf(buf, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}
