// =============================================================================
// web_server.h — Authenticated HTTP API Server
// =============================================================================
//
// Provides RESTful JSON endpoints for relay control, WoL commands, and
// system status. All control endpoints require Bearer token authentication.
//
// ENDPOINTS:
//   GET  /api/health          — Unauthenticated health check
//   GET  /api/status          — Full system status (auth required)
//   GET  /api/relay/status    — All relay states (auth required)
//   POST /api/relay/control   — Control a relay (auth required)
//   GET  /api/wol/targets     — List WoL targets (auth required)
//   POST /api/wol/wake        — Send WoL packet (auth required)
//
// TRANSPORT SECURITY:
//   ESP32 WebServer uses plain HTTP. The bearer token is visible to any
//   observer on the Wi-Fi network. This is acceptable on a trusted home
//   LAN. For remote access, use a TLS-terminating reverse proxy or route
//   through a Tailscale-connected intermediary host.
//
// =============================================================================

#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <WebServer.h>
#include "relay_controller.h"
#include "wol_bridge.h"

// Simple fixed-window rate limiter.
struct RateLimiter {
    unsigned long windowStart;
    uint16_t      requestCount;
};

class SmartHomeServer {
public:
    // Initialize routes and start the server.
    // Pointers to relay and WoL modules must remain valid for the server's lifetime.
    void begin(RelayController* relays, WolBridge* wol, unsigned long* uptimeRef);

    // Call in loop() to process incoming HTTP requests.
    void handleClient();

private:
    WebServer        server_{HTTP_PORT};
    RelayController* relays_  = nullptr;
    WolBridge*       wol_     = nullptr;
    unsigned long*   bootTime_ = nullptr;  // Pointer to boot millis for uptime
    RateLimiter      rateLimiter_{0, 0};

    // --- Middleware ---
    bool checkAuth();
    bool checkRateLimit();

    // --- Route Handlers ---
    void handleHealth();
    void handleStatus();
    void handleRelayStatus();
    void handleRelayControl();
    void handleWolTargets();
    void handleWolWake();
    void handleNotFound();

    // --- Helpers ---
    void sendJson(int code, const String& json);
    void sendError(int code, const char* message);
    void setCORSHeaders();
};

#endif // WEB_SERVER_H
