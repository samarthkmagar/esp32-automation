// =============================================================================
// wol_bridge.h — Independent Wake-on-LAN Magic Packet Sender
// =============================================================================
//
// Constructs and sends standard WoL magic packets over UDP broadcast.
// Operates independently of Home Assistant — only requires the ESP32 to be
// powered and connected to the same LAN (or reachable broadcast domain)
// as the target machine.
//
// LIMITATIONS:
//   - WoL is a best-effort protocol. Sending a magic packet does NOT guarantee
//     the target will wake. Factors: NIC WoL support, BIOS/UEFI settings,
//     OS power policy, router/switch broadcast forwarding, Wi-Fi client
//     isolation, power state (S5 vs S4 vs S3).
//   - The ESP32 sends via Wi-Fi; the target must be on the WIRED Ethernet
//     of the same broadcast domain. Some routers block broadcast forwarding
//     between Wi-Fi and wired segments.
//   - No confirmation mechanism is built in. A separate ping or service
//     check is needed to verify the target actually woke.
//
// =============================================================================

#ifndef WOL_BRIDGE_H
#define WOL_BRIDGE_H

#include <Arduino.h>
#include <WiFiUdp.h>
#include "config.h"

// Result of the last WoL operation (for status reporting).
struct WolResult {
    const char*   targetName;
    char          macStr[18];   // "AA:BB:CC:DD:EE:FF"
    bool          attempted;
    uint8_t       packetsSent;
    uint8_t       packetsRequested;
    unsigned long timestamp;    // millis() when last attempted
};

class WolBridge {
public:
    void begin();

    // Send WoL magic packet(s) to a named target.
    //   targetName: must match a name in WOL_TARGETS
    //   retries:    additional packets after the first (0 = send once)
    //   retryDelayMs: delay between packets in ms
    //
    // Returns true if at least one packet was transmitted.
    // NOTE: "transmitted" means handed to the UDP stack, not that the
    // target received it or woke up.
    bool sendWake(const char* targetName,
                  uint8_t retries = WOL_DEFAULT_RETRIES,
                  uint16_t retryDelayMs = WOL_RETRY_DELAY_MS);

    // Find a target index by name. Returns -1 if not found or disabled.
    int findTarget(const char* name) const;

    // Get the number of configured targets.
    uint8_t getTargetCount() const;

    // Get target config by index (0-based). Returns nullptr if invalid.
    const WolTargetConfig* getTarget(uint8_t index) const;

    // Get the result of the last WoL operation.
    const WolResult& getLastResult() const;

    // Format a MAC address as "AA:BB:CC:DD:EE:FF" into a buffer (min 18 bytes).
    static void formatMAC(const uint8_t mac[6], char* buf);

private:
    WiFiUDP    udp_;
    WolResult  lastResult_;
};

#endif // WOL_BRIDGE_H
