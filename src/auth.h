// =============================================================================
// auth.h — Request Authentication
// =============================================================================
//
// Simple bearer-token authentication for the HTTP API.
//
// SECURITY MODEL:
//   - A shared secret (bearer token) is compared against each request's
//     Authorization header.
//   - Comparison uses constant-time logic to resist timing side-channel attacks.
//   - The token is stored in plaintext in flash. Physical access to the ESP32
//     or a flash dump can extract it.
//   - HTTP traffic on the LAN is unencrypted. Anyone on the same Wi-Fi network
//     can capture the token with a packet sniffer.
//   - Acceptable for a trusted home LAN. For remote access, route through a
//     TLS-terminating proxy or Tailscale-connected intermediary.
//
// =============================================================================

#ifndef AUTH_H
#define AUTH_H

#include <Arduino.h>

// Validate a bearer token against the configured secret.
// Returns true if the token matches.
bool validateToken(const String& token);

// Constant-time string comparison.
// Returns true if both strings are equal.
// NOTE: String length is NOT hidden (an attacker can infer token length).
// For fixed-length hex tokens this is acceptable.
bool constantTimeCompare(const String& a, const String& b);

// Extract the bearer token from an Authorization header value.
// Input: "Bearer <token>"
// Returns the token portion, or an empty string if the format is invalid.
String extractBearerToken(const String& authHeader);

#endif // AUTH_H
