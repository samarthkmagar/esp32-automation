// =============================================================================
// switch_controller.cpp — Physical Switch Controller Implementation
// =============================================================================

#include "switch_controller.h"

void SwitchController::begin(RelayController* relays) {
    relays_ = relays;
    Serial.printf("[Switch] Initializing %d physical switch input(s)...\n", NUM_RELAYS);

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        switches_[i].gpio             = SWITCH_CONFIGS[i].gpio;
        switches_[i].type             = SWITCH_CONFIGS[i].type;
        switches_[i].enabled          = SWITCH_CONFIGS[i].enabled;
        switches_[i].relayChannel     = i + 1;
        switches_[i].lastDebounceTime = millis();

        if (switches_[i].enabled) {
            // Enable internal pull-up resistor.
            // Switch is wired between this GPIO and GND.
            // Unpressed/Open = HIGH (3.3V)
            // Pressed/Closed = LOW (GND)
            pinMode(switches_[i].gpio, INPUT_PULLUP);

            // Read the initial state so we don't trigger false toggles on boot
            bool initialState = digitalRead(switches_[i].gpio);
            switches_[i].rawReading     = initialState;
            switches_[i].debouncedState = initialState;

            const char* typeStr = "TOGGLE (Rocker)";
            if (switches_[i].type == SWITCH_TYPE_MOMENTARY) typeStr = "MOMENTARY (Push)";
            else if (switches_[i].type == SWITCH_TYPE_FOLLOW) typeStr = "FOLLOW";

            Serial.printf("[Switch] SW%d on GPIO%d → Relay CH%d [%s, init=%s]\n",
                          switches_[i].relayChannel, switches_[i].gpio,
                          switches_[i].relayChannel, typeStr,
                          initialState == HIGH ? "OPEN/HIGH" : "CLOSED/LOW");
        } else {
            Serial.printf("[Switch] SW%d (GPIO%d) disabled in config.\n",
                          switches_[i].relayChannel, switches_[i].gpio);
        }
    }
    Serial.println("[Switch] Physical switch inputs active (works offline and online).");
}

void SwitchController::loop() {
    if (!relays_) return;

    unsigned long now = millis();

    for (uint8_t i = 0; i < NUM_RELAYS; i++) {
        if (!switches_[i].enabled) continue;

        bool currentReading = digitalRead(switches_[i].gpio);

        // Check if raw pin reading changed (bounce or genuine transition)
        if (currentReading != switches_[i].rawReading) {
            switches_[i].lastDebounceTime = now;
            switches_[i].rawReading = currentReading;
        }

        // Has reading remained stable longer than the debounce window?
        if ((now - switches_[i].lastDebounceTime) >= SWITCH_DEBOUNCE_MS) {
            // Check if debounced state has changed
            if (currentReading != switches_[i].debouncedState) {
                bool previousState = switches_[i].debouncedState;
                switches_[i].debouncedState = currentReading;

                // --- Trigger Relay based on Switch Mode ---
                if (switches_[i].type == SWITCH_TYPE_MOMENTARY) {
                    // Push button: trigger only on press (HIGH -> LOW falling edge)
                    if (previousState == HIGH && switches_[i].debouncedState == LOW) {
                        Serial.printf("[Switch] SW%d (GPIO%d) pressed → toggling Relay CH%d\n",
                                      switches_[i].relayChannel, switches_[i].gpio,
                                      switches_[i].relayChannel);
                        relays_->toggle(switches_[i].relayChannel);
                    }
                } else if (switches_[i].type == SWITCH_TYPE_TOGGLE) {
                    // Standard rocker wall switch: ANY flip (HIGH->LOW or LOW->HIGH) toggles relay
                    Serial.printf("[Switch] SW%d (GPIO%d) flipped (%s -> %s) → toggling Relay CH%d\n",
                                  switches_[i].relayChannel, switches_[i].gpio,
                                  previousState == HIGH ? "OPEN" : "CLOSED",
                                  switches_[i].debouncedState == HIGH ? "OPEN" : "CLOSED",
                                  switches_[i].relayChannel);
                    relays_->toggle(switches_[i].relayChannel);
                } else if (switches_[i].type == SWITCH_TYPE_FOLLOW) {
                    // Direct follower: closed (LOW) = ON, open (HIGH) = OFF
                    bool targetState = (switches_[i].debouncedState == LOW);
                    Serial.printf("[Switch] SW%d (GPIO%d) follow → Relay CH%d = %s\n",
                                  switches_[i].relayChannel, switches_[i].gpio,
                                  switches_[i].relayChannel, targetState ? "ON" : "OFF");
                    relays_->setState(switches_[i].relayChannel, targetState);
                }
            }
        }
    }
}

bool SwitchController::getSwitchState(uint8_t channel) const {
    if (channel < 1 || channel > NUM_RELAYS) return HIGH;
    return switches_[channel - 1].debouncedState;
}
