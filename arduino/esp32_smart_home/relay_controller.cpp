// =============================================================================
// relay_controller.cpp — Relay Channel Management Implementation
// =============================================================================

#include "relay_controller.h"

void RelayController::begin() {
    Serial.printf("[Relay] Initializing %d relay channel(s)...\n", NUM_RELAYS);

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        relays_[i].name        = RELAY_CONFIGS[i].name;
        relays_[i].gpio        = RELAY_CONFIGS[i].gpio;
        relays_[i].active_high = RELAY_CONFIGS[i].active_high;
        relays_[i].is_on       = false;
        relays_[i].channel     = i + 1;

        // CRITICAL: Write the OFF level to the pin BEFORE configuring it as
        // output. This minimizes the glitch window during pin mode transition.
        //
        // For active-high relays: OFF = LOW
        // For active-low relays:  OFF = HIGH
        digitalWrite(relays_[i].gpio, relays_[i].active_high ? LOW : HIGH);
        pinMode(relays_[i].gpio, OUTPUT);
        applyState(i);  // Ensure consistent state

        Serial.printf("[Relay] CH%d \"%s\" on GPIO%d (%s) → OFF\n",
                      relays_[i].channel, relays_[i].name, relays_[i].gpio,
                      relays_[i].active_high ? "active-HIGH" : "active-LOW");
    }

    Serial.println("[Relay] All channels initialized to OFF.");
}

bool RelayController::setState(uint8_t channel, bool on) {
    if (channel < 1 || channel > NUM_RELAYS) return false;
    uint8_t idx = channel - 1;
    relays_[idx].is_on = on;
    applyState(idx);
    Serial.printf("[Relay] CH%d \"%s\" → %s\n",
                  channel, relays_[idx].name, on ? "ON" : "OFF");
    return true;
}

bool RelayController::toggle(uint8_t channel) {
    if (channel < 1 || channel > NUM_RELAYS) return false;
    uint8_t idx = channel - 1;
    relays_[idx].is_on = !relays_[idx].is_on;
    applyState(idx);
    Serial.printf("[Relay] CH%d \"%s\" toggled → %s\n",
                  channel, relays_[idx].name, relays_[idx].is_on ? "ON" : "OFF");
    return true;
}

bool RelayController::getState(uint8_t channel) const {
    if (channel < 1 || channel > NUM_RELAYS) return false;
    return relays_[channel - 1].is_on;
}

const RelayState* RelayController::getRelay(uint8_t channel) const {
    if (channel < 1 || channel > NUM_RELAYS) return nullptr;
    return &relays_[channel - 1];
}

uint8_t RelayController::getCount() const {
    return NUM_RELAYS;
}

void RelayController::allOff() {
    Serial.println("[Relay] All channels → OFF");
    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        relays_[i].is_on = false;
        applyState(i);
    }
}

void RelayController::applyState(uint8_t index) {
    // Compute the physical GPIO level from commanded state + polarity.
    bool level;
    if (relays_[index].active_high) {
        level = relays_[index].is_on;   // ON → HIGH, OFF → LOW
    } else {
        level = !relays_[index].is_on;  // ON → LOW,  OFF → HIGH
    }
    digitalWrite(relays_[index].gpio, level ? HIGH : LOW);
}
