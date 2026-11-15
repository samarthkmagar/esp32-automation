// =============================================================================
// relay_controller.h — Independent Relay Channel Management
// =============================================================================
//
// Manages N relay channels with configurable GPIO, polarity, and safe startup.
//
// IMPORTANT DISTINCTION:
//   The "state" reported by this module is the SOFTWARE-COMMANDED state.
//   Without a hardware feedback sensor (e.g., current transformer, aux contact),
//   there is no confirmation that the physical relay actually switched.
//   Possible discrepancies: blown fuse, welded contact, wiring fault, relay
//   coil failure, insufficient GPIO drive current.
//
// =============================================================================

#ifndef RELAY_CONTROLLER_H
#define RELAY_CONTROLLER_H

#include <Arduino.h>
#include "config.h"

// Relay runtime state (one per channel).
struct RelayState {
    const char* name;
    uint8_t     gpio;
    bool        active_high;
    bool        is_on;       // Software-commanded state (not hardware-confirmed)
    uint8_t     channel;     // 1-indexed channel number
};

class RelayController {
public:
    // Initialize all relay GPIOs to the OFF (de-energized) state.
    // Call this as early as possible in setup().
    void begin();

    // Set a relay ON (true) or OFF (false). Returns false if channel is invalid.
    bool setState(uint8_t channel, bool on);

    // Toggle a relay. Returns false if channel is invalid.
    bool toggle(uint8_t channel);

    // Get the software-commanded state. Returns false if channel is invalid.
    bool getState(uint8_t channel) const;

    // Get full relay info. Returns nullptr if channel is invalid.
    const RelayState* getRelay(uint8_t channel) const;

    // Number of active relay channels.
    uint8_t getCount() const;

    // Turn all relays OFF (emergency or shutdown).
    void allOff();

private:
    RelayState relays_[NUM_RELAYS];

    // Write the GPIO to match the commanded state and polarity.
    void applyState(uint8_t index);
};

#endif // RELAY_CONTROLLER_H
