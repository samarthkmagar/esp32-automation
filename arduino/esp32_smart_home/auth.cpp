// =============================================================================
// auth.cpp — Authentication Implementation
// =============================================================================

#include "auth.h"
#include "config.h"

bool validateToken(const String& token) {
    String expected(API_AUTH_TOKEN);
    return constantTimeCompare(token, expected);
}

bool constantTimeCompare(const String& a, const String& b) {
    // Length mismatch reveals the expected length, which is acceptable
    // for a fixed-format hex token.
    if (a.length() != b.length()) {
        return false;
    }

    // XOR all bytes; any difference sets bits in `result`.
    volatile uint8_t result = 0;
    for (size_t i = 0; i < a.length(); i++) {
        result |= (uint8_t)a[i] ^ (uint8_t)b[i];
    }
    return result == 0;
}

String extractBearerToken(const String& authHeader) {
    if (authHeader.startsWith("Bearer ")) {
        return authHeader.substring(7);
    }
    return "";
}
