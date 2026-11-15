// =============================================================================
// config.h — Central Configuration for ESP32 Smart-Home Controller
// =============================================================================
//
// ALL user-configurable parameters live here. Edit this file before flashing.
//
// PLACEHOLDERS are marked with comments. You MUST replace them:
//   - Wi-Fi credentials
//   - API authentication token
//   - WoL target MAC addresses
//   - Subnet broadcast address
//   - GPIO pin assignments (verify against your wiring)
//   - Relay polarity (verify against your relay module)
//
// =============================================================================

#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// =============================================================================
// FIRMWARE VERSION
// =============================================================================
#define FIRMWARE_VERSION "1.0.0"
#define FIRMWARE_NAME    "ESP32-SmartHome"

// =============================================================================
// WI-FI CONFIGURATION
// =============================================================================
// PLACEHOLDER: Replace with your actual Wi-Fi credentials before flashing.
// For production, consider WiFiManager library for captive-portal provisioning.
//
// Recovery: If credentials become invalid, reflash with corrected values,
// or hold GPIO0 (BOOT button) during reset to enter serial-only mode
// (see wifi_manager.cpp for recovery logic).

#define WIFI_SSID               "Airtel_Shivalay"
#define WIFI_PASSWORD            "Pari#jk8585"

#define WIFI_CONNECT_TIMEOUT_MS  15000   // Initial connection timeout
#define WIFI_RECONNECT_INTERVAL  10000   // Retry interval after disconnect (ms)
#define WIFI_MAX_CONNECT_ATTEMPTS 30     // Attempts before logging failure

// =============================================================================
// AUTHENTICATION
// =============================================================================
// PLACEHOLDER: Replace with a strong random secret before flashing.
//
// Generate a token:
//   Linux/macOS:  openssl rand -hex 32
//   PowerShell:   -join ((1..32) | ForEach-Object { '{0:x2}' -f (Get-Random -Max 256) })
//
// Usage: HTTP header "Authorization: Bearer <token>"
//
// SECURITY NOTES:
//   - This token is stored in flash memory in plaintext.
//   - ESP32 WebServer uses HTTP (not HTTPS) by default. The token is
//     visible to anyone who can sniff your Wi-Fi traffic.
//   - Acceptable on a trusted home LAN or behind a TLS-terminating proxy.
//   - Do NOT commit real tokens to version control.

#define API_AUTH_TOKEN  "CHANGE_ME_GENERATE_WITH_openssl_rand_hex_32"

// =============================================================================
// RELAY CONFIGURATION
// =============================================================================
// Maximum relay channels the firmware supports. Increase if needed (max 15).
#define MAX_RELAYS 15

// Number of active relay channels (1..MAX_RELAYS).
#define NUM_RELAYS 5

// Relay definition structure.
struct RelayConfig {
    const char* name;       // Human-readable name (for API responses)
    uint8_t     gpio;       // GPIO pin number
    bool        active_high; // true: relay ON when GPIO HIGH; false: ON when GPIO LOW
};

// ---- GPIO Pin Selection Guide (ESP32 DevKit V1) ----
//
// SAFE for relay output:
//   GPIO 4, 13, 14, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33
//   (15 pins = exactly our MAX_RELAYS target)
//
// USE WITH CAUTION:
//   GPIO 2  — On-board LED; pulled low; may affect boot if driven
//   GPIO 5  — Strapping pin (VSPI SS); may pulse HIGH briefly during boot
//   GPIO 15 — Strapping pin; controls boot UART output; may pulse during boot
//
// DO NOT USE for relay output:
//   GPIO 0  — BOOT button; pulled high; must be HIGH for normal boot
//   GPIO 1  — TX0 (serial output); toggling would corrupt serial
//   GPIO 3  — RX0 (serial input); used for programming
//   GPIO 6–11 — Connected to internal SPI flash; will crash the chip
//   GPIO 12 — Strapping pin (MTDI); sets flash voltage; wrong level = no boot
//   GPIO 34–39 — INPUT ONLY; cannot drive outputs
//
// VERIFY: Your relay module's input characteristics (active-low vs active-high,
// required drive current, opto-isolator voltage). Most common modules with
// opto-isolators are active-LOW and need 3.3 V logic compatible inputs.

// PLACEHOLDER GPIO assignments — verify against your actual wiring.
// PLACEHOLDER active_high — verify against your relay module's datasheet.
//   Common opto-isolated relay modules: active_high = false (active-LOW)
//   Bare transistor-driven modules:     active_high = true  (active-HIGH)

const RelayConfig RELAY_CONFIGS[NUM_RELAYS] = {
    // { "Name",              GPIO,  active_high }
    { "Living Room Light",    16,    true },
    { "Bedroom Light",        17,    true },
    { "Desk Setup",           18,    true },
    { "Fan",                  19,    true },
    { "Spare Appliance",      23,    true },
};

// =============================================================================
// PHYSICAL SWITCH / MANUAL OVERRIDE CONFIGURATION
// =============================================================================
// Physical switches operate 100% locally. They work AT ALL TIMES:
//   - When connected to Wi-Fi / Internet
//   - When Wi-Fi is disconnected, re-associating, or router is powered off
//   - During initial Wi-Fi connection attempts at boot
//
// Switch behavior types:
//   - SWITCH_TYPE_TOGGLE: Standard wall rocker switch (spst). Any flip (UP/DOWN)
//     toggles the relay. Best for retrofitting existing wall switches (2-way control).
//   - SWITCH_TYPE_MOMENTARY: Push-button / bell-press switch. Pressing down toggles.
//   - SWITCH_TYPE_FOLLOW: Closed (LOW) = ON, Open (HIGH) = OFF.
//
// Wiring:
//   Connect each physical switch between the assigned GPIO and ESP32 GND.
//   Uses internal pull-up (INPUT_PULLUP) — NO external resistors needed.
//   Safe 3.3V logic level.

enum SwitchType {
    SWITCH_TYPE_TOGGLE,     // Rocker switch: any transition toggles relay (recommended)
    SWITCH_TYPE_MOMENTARY,  // Push-button: falling edge toggles relay
    SWITCH_TYPE_FOLLOW      // State follower: switch closed = relay ON, open = OFF
};

struct SwitchConfig {
    uint8_t    gpio;        // ESP32 input pin
    SwitchType type;        // Behavior mode
    bool       enabled;     // true = active, false = disabled
};

#define SWITCH_DEBOUNCE_MS  50   // Debounce window in milliseconds

const SwitchConfig SWITCH_CONFIGS[NUM_RELAYS] = {
    // { GPIO, type,                 enabled }
    { 25,    SWITCH_TYPE_TOGGLE,    true },  // SW1 → Relay 1 (Living Room Light)
    { 26,    SWITCH_TYPE_TOGGLE,    true },  // SW2 → Relay 2 (Bedroom Light)
    { 27,    SWITCH_TYPE_TOGGLE,    true },  // SW3 → Relay 3 (Desk Setup)
    { 32,    SWITCH_TYPE_TOGGLE,    true },  // SW4 → Relay 4 (Fan)
    { 33,    SWITCH_TYPE_TOGGLE,    true },  // SW5 → Relay 5 (Spare Appliance)
};

// =============================================================================
// WAKE-ON-LAN CONFIGURATION
// =============================================================================
#define MAX_WOL_TARGETS 4

struct WolTargetConfig {
    const char* name;       // Unique target identifier (used in API)
    uint8_t     mac[6];     // Target MAC address
    bool        enabled;    // false = skip this target (not yet verified)
};

// PLACEHOLDER MAC addresses — replace with verified addresses from each target.
//
// Find the MAC address:
//   Windows:   ipconfig /all → "Physical Address" of the Ethernet adapter
//   Linux:     ip link show  → look for the wired interface (e.g., enp0s31f6)
//
// IMPORTANT: Use the WIRED Ethernet MAC, not Wi-Fi. WoL requires the NIC
// that is connected to the LAN and has WoL enabled in BIOS/UEFI and OS.

const WolTargetConfig WOL_TARGETS[MAX_WOL_TARGETS] = {
    // { "name",           {MAC bytes},                     enabled }
    { "thinkpad_p53",      {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, true  },
    { "ubuntu_server",     {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, false },  // disabled until verified
    { "",                  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, false },  // spare slot
    { "",                  {0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, false },  // spare slot
};

// WoL network settings.
// PLACEHOLDER: Replace broadcast address with your actual subnet broadcast.
//   For 192.168.1.0/24 → "192.168.1.255"
//   For 192.168.0.0/24 → "192.168.0.255"
//   For 10.0.0.0/24    → "10.0.0.255"
#define WOL_BROADCAST_ADDRESS   "192.168.1.255"
#define WOL_UDP_PORT            9       // Standard WoL port (7 or 9)
#define WOL_DEFAULT_RETRIES     3       // Default retry count
#define WOL_MAX_RETRIES         10      // Hard cap on retries per request
#define WOL_RETRY_DELAY_MS      100     // Delay between retries (ms)
#define WOL_MAX_RETRY_DELAY_MS  1000    // Hard cap on retry delay (ms)

// =============================================================================
// HTTP SERVER CONFIGURATION
// =============================================================================
#define HTTP_PORT                80
#define MAX_REQUEST_BODY_SIZE    1024    // Max POST body in bytes
#define RATE_LIMIT_MAX_REQUESTS  60      // Max requests per window
#define RATE_LIMIT_WINDOW_MS     60000   // Rate limit window (ms)

// =============================================================================
// DEVICE IDENTITY
// =============================================================================
// mDNS hostname: access the device as http://<hostname>.local
#define DEVICE_HOSTNAME "esp32-controller"

// =============================================================================
// STATUS LED
// =============================================================================
// Built-in LED on most ESP32 DevKit V1 boards is GPIO 2.
// Used for visual connection status:
//   Blinking (burst on connect + continuous pulse) = Wi-Fi connected, operational
//   OFF                                           = Wi-Fi disconnected / not powered
#define STATUS_LED_PIN          2
#define STATUS_LED_ACTIVE_HIGH  true
#define STATUS_LED_BLINK_MS     500     // Blink interval when Wi-Fi is connected

#endif // CONFIG_H
