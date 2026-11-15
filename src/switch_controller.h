// =============================================================================
// switch_controller.h — Physical Wall Switch / Button Controller
// =============================================================================
//
// Provides manual local control for all relay channels using physical switches
// or push-buttons.
//
// INDEPENDENCE GUARANTEE:
//   This module operates 100% locally on the ESP32. It does NOT depend on
//   Wi-Fi, Internet, Home Assistant, or any network stack. Whether the device
//   is connected to Wi-Fi, searching for a router, or completely offline,
//   flipping a physical switch will immediately actuate the relay.
//
// SWITCH MODES:
//   - SWITCH_TYPE_TOGGLE: Standard rocker wall switch. Any flip (change in state)
//     toggles the relay, preserving 2-way control with the web/Stream Deck.
//   - SWITCH_TYPE_MOMENTARY: Push-button (bell press). Pressing down toggles.
//   - SWITCH_TYPE_FOLLOW: Direct state (LOW = ON, HIGH = OFF).
//
// WIRING:
//   Connect each switch between its designated GPIO and ESP32 GND.
//   Internal pull-up resistors are enabled (no external resistors needed).
//
// =============================================================================

#ifndef SWITCH_CONTROLLER_H
#define SWITCH_CONTROLLER_H

#include <Arduino.h>
#include "config.h"
#include "relay_controller.h"

struct SwitchRuntimeState {
    uint8_t       gpio;
    SwitchType    type;
    bool          enabled;
    uint8_t       relayChannel;     // 1-indexed (1..NUM_RELAYS)
    bool          rawReading;       // Immediate digitalRead
    bool          debouncedState;   // Stable filtered state
    unsigned long lastDebounceTime; // Timestamp of last raw state change
};

class SwitchController {
public:
    // Initialize switch GPIOs as INPUT_PULLUP and read initial states.
    // Call in setup() right after relays.begin().
    void begin(RelayController* relays);

    // Call continuously in loop(). Non-blocking, instant response.
    void loop();

    // Get current debounced state of a switch pin (returns HIGH or LOW).
    bool getSwitchState(uint8_t channel) const;

private:
    RelayController*   relays_ = nullptr;
    SwitchRuntimeState switches_[NUM_RELAYS];
};

#endif // SWITCH_CONTROLLER_H
