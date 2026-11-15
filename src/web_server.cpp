// =============================================================================
// web_server.cpp — HTTP API Server Implementation
// =============================================================================

#include "web_server.h"
#include "auth.h"
#include "config.h"
#include <ArduinoJson.h>
#include <WiFi.h>

// =============================================================================
// Initialization
// =============================================================================

void SmartHomeServer::begin(RelayController* relays, WolBridge* wol, unsigned long* uptimeRef) {
    relays_    = relays;
    wol_       = wol;
    bootTime_  = uptimeRef;
    rateLimiter_.windowStart  = millis();
    rateLimiter_.requestCount = 0;

    // Collect headers we need to inspect (WebServer does not collect them by default)
    const char* headerKeys[] = { "Authorization", "Content-Type" };
    server_.collectHeaders(headerKeys, 2);

    // --- Register Routes ---
    server_.on("/api/health",        HTTP_GET,  [this]() { handleHealth(); });
    server_.on("/api/status",        HTTP_GET,  [this]() { handleStatus(); });
    server_.on("/api/relay/status",  HTTP_GET,  [this]() { handleRelayStatus(); });
    server_.on("/api/relay/control", HTTP_POST, [this]() { handleRelayControl(); });
    server_.on("/api/wol/targets",   HTTP_GET,  [this]() { handleWolTargets(); });
    server_.on("/api/wol/wake",      HTTP_POST, [this]() { handleWolWake(); });
    server_.onNotFound([this]()                          { handleNotFound(); });

    server_.begin();
    Serial.printf("[HTTP] Server started on port %d\n", HTTP_PORT);
}

void SmartHomeServer::handleClient() {
    server_.handleClient();
}

// =============================================================================
// Middleware
// =============================================================================

bool SmartHomeServer::checkAuth() {
    if (!server_.hasHeader("Authorization")) {
        sendError(401, "Missing Authorization header. Use: Authorization: Bearer <token>");
        return false;
    }

    String authHeader = server_.header("Authorization");
    String token = extractBearerToken(authHeader);

    if (token.isEmpty()) {
        sendError(401, "Invalid authorization format. Use: Authorization: Bearer <token>");
        return false;
    }

    if (!validateToken(token)) {
        Serial.println("[Auth] Invalid token rejected.");
        sendError(403, "Invalid authentication token.");
        return false;
    }

    return true;
}

bool SmartHomeServer::checkRateLimit() {
    unsigned long now = millis();

    // Reset window if expired (handles millis() rollover after ~49 days)
    if (now - rateLimiter_.windowStart > RATE_LIMIT_WINDOW_MS) {
        rateLimiter_.windowStart  = now;
        rateLimiter_.requestCount = 0;
    }

    if (rateLimiter_.requestCount >= RATE_LIMIT_MAX_REQUESTS) {
        sendError(429, "Rate limit exceeded. Try again later.");
        Serial.println("[HTTP] Rate limit exceeded.");
        return false;
    }

    rateLimiter_.requestCount++;
    return true;
}

// =============================================================================
// Route Handlers
// =============================================================================

void SmartHomeServer::handleHealth() {
    // Health check — no authentication required.
    // Intentionally minimal: confirms the HTTP server is responsive.
    JsonDocument doc;
    doc["status"] = "ok";
    doc["firmware"] = FIRMWARE_NAME;
    doc["version"] = FIRMWARE_VERSION;

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void SmartHomeServer::handleStatus() {
    if (!checkRateLimit() || !checkAuth()) return;

    JsonDocument doc;
    doc["firmware"]         = FIRMWARE_NAME;
    doc["version"]          = FIRMWARE_VERSION;
    doc["uptime_seconds"]   = (millis() - *bootTime_) / 1000;
    doc["uptime_ms"]        = millis() - *bootTime_;
    doc["free_heap"]        = ESP.getFreeHeap();
    doc["wifi_connected"]   = (WiFi.status() == WL_CONNECTED);
    doc["wifi_ssid"]        = WiFi.SSID();
    doc["ip_address"]       = WiFi.localIP().toString();
    doc["mac_address"]      = WiFi.macAddress();
    doc["rssi_dbm"]         = WiFi.RSSI();
    doc["hostname"]         = DEVICE_HOSTNAME;

    // Relay summary
    JsonArray relays = doc["relays"].to<JsonArray>();
    for (uint8_t ch = 1; ch <= relays_->getCount(); ch++) {
        const RelayState* r = relays_->getRelay(ch);
        if (!r) continue;
        JsonObject obj = relays.add<JsonObject>();
        obj["channel"]  = r->channel;
        obj["name"]     = r->name;
        obj["state"]    = r->is_on ? "on" : "off";
        obj["gpio"]     = r->gpio;
    }

    // Last WoL result
    const WolResult& wr = wol_->getLastResult();
    if (strlen(wr.targetName) > 0) {
        JsonObject wolObj = doc["wol_last_result"].to<JsonObject>();
        wolObj["target"]           = wr.targetName;
        wolObj["mac"]              = wr.macStr;
        wolObj["attempted"]        = wr.attempted;
        wolObj["packets_sent"]     = wr.packetsSent;
        wolObj["packets_requested"] = wr.packetsRequested;
        wolObj["age_seconds"]      = (millis() - wr.timestamp) / 1000;
    }

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void SmartHomeServer::handleRelayStatus() {
    if (!checkRateLimit() || !checkAuth()) return;

    JsonDocument doc;
    doc["count"] = relays_->getCount();
    doc["note"]  = "States are software-reported. No hardware feedback sensor is present.";

    JsonArray arr = doc["relays"].to<JsonArray>();
    for (uint8_t ch = 1; ch <= relays_->getCount(); ch++) {
        const RelayState* r = relays_->getRelay(ch);
        if (!r) continue;
        JsonObject obj = arr.add<JsonObject>();
        obj["channel"]     = r->channel;
        obj["name"]        = r->name;
        obj["state"]       = r->is_on ? "on" : "off";
        obj["gpio"]        = r->gpio;
        obj["active_high"] = r->active_high;
        obj["switch_gpio"] = SWITCH_CONFIGS[ch - 1].gpio;
    }

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void SmartHomeServer::handleRelayControl() {
    if (!checkRateLimit() || !checkAuth()) return;

    // Check request body size
    if (!server_.hasArg("plain")) {
        sendError(400, "Missing JSON request body.");
        return;
    }

    String body = server_.arg("plain");
    if (body.length() > MAX_REQUEST_BODY_SIZE) {
        sendError(413, "Request body too large.");
        return;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        String msg = "JSON parse error: ";
        msg += err.c_str();
        sendError(400, msg.c_str());
        return;
    }

    // Validate required fields
    if (!doc["channel"].is<int>()) {
        sendError(400, ("Missing or invalid 'channel' (integer 1.." + String(NUM_RELAYS) + ").").c_str());
        return;
    }
    if (!doc["action"].is<const char*>()) {
        sendError(400, "Missing or invalid 'action' (string: \"on\", \"off\", or \"toggle\").");
        return;
    }

    int channel = doc["channel"].as<int>();
    String action = doc["action"].as<String>();
    action.toLowerCase();

    // Validate channel range
    if (channel < 1 || channel > (int)relays_->getCount()) {
        sendError(400, ("Invalid channel. Must be 1.." + String(relays_->getCount()) + ".").c_str());
        return;
    }

    // Validate and execute action
    bool success = false;
    if (action == "on") {
        success = relays_->setState(channel, true);
    } else if (action == "off") {
        success = relays_->setState(channel, false);
    } else if (action == "toggle") {
        success = relays_->toggle(channel);
    } else {
        sendError(400, "Invalid action. Use \"on\", \"off\", or \"toggle\".");
        return;
    }

    if (!success) {
        sendError(500, "Failed to execute relay command.");
        return;
    }

    // Build response
    const RelayState* r = relays_->getRelay(channel);
    JsonDocument resp;
    resp["success"] = true;
    resp["channel"] = r->channel;
    resp["name"]    = r->name;
    resp["state"]   = r->is_on ? "on" : "off";
    resp["action"]  = action;
    resp["note"]    = "Software-reported state. No hardware feedback sensor confirms physical relay position.";

    String output;
    serializeJson(resp, output);
    sendJson(200, output);
}

void SmartHomeServer::handleWolTargets() {
    if (!checkRateLimit() || !checkAuth()) return;

    JsonDocument doc;
    JsonArray arr = doc["targets"].to<JsonArray>();

    for (uint8_t i = 0; i < MAX_WOL_TARGETS; i++) {
        const WolTargetConfig* t = wol_->getTarget(i);
        if (!t || strlen(t->name) == 0) continue;

        char mac[18];
        WolBridge::formatMAC(t->mac, mac);

        JsonObject obj = arr.add<JsonObject>();
        obj["name"]    = t->name;
        obj["mac"]     = mac;
        obj["enabled"] = t->enabled;
    }

    doc["broadcast_address"] = WOL_BROADCAST_ADDRESS;
    doc["udp_port"]          = WOL_UDP_PORT;
    doc["default_retries"]   = WOL_DEFAULT_RETRIES;
    doc["max_retries"]       = WOL_MAX_RETRIES;

    String output;
    serializeJson(doc, output);
    sendJson(200, output);
}

void SmartHomeServer::handleWolWake() {
    if (!checkRateLimit() || !checkAuth()) return;

    // Check request body
    if (!server_.hasArg("plain")) {
        sendError(400, "Missing JSON request body.");
        return;
    }

    String body = server_.arg("plain");
    if (body.length() > MAX_REQUEST_BODY_SIZE) {
        sendError(413, "Request body too large.");
        return;
    }

    // Parse JSON
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body);
    if (err) {
        String msg = "JSON parse error: ";
        msg += err.c_str();
        sendError(400, msg.c_str());
        return;
    }

    // Validate target name
    if (!doc["target"].is<const char*>()) {
        sendError(400, "Missing or invalid 'target' (string matching a configured target name).");
        return;
    }

    String targetName = doc["target"].as<String>();

    // Check target exists
    int targetIdx = wol_->findTarget(targetName.c_str());
    if (targetIdx < 0) {
        // Build helpful error message with available targets
        String msg = "Unknown or disabled target: \"" + targetName + "\". Available: ";
        bool first = true;
        for (uint8_t i = 0; i < MAX_WOL_TARGETS; i++) {
            const WolTargetConfig* t = wol_->getTarget(i);
            if (!t || strlen(t->name) == 0 || !t->enabled) continue;
            if (!first) msg += ", ";
            msg += "\"";
            msg += t->name;
            msg += "\"";
            first = false;
        }
        sendError(404, msg.c_str());
        return;
    }

    // Optional parameters with validation
    uint8_t retries = WOL_DEFAULT_RETRIES;
    uint16_t retryDelayMs = WOL_RETRY_DELAY_MS;

    if (doc["retries"].is<int>()) {
        int r = doc["retries"].as<int>();
        if (r < 0 || r > WOL_MAX_RETRIES) {
            sendError(400, ("'retries' must be 0.." + String(WOL_MAX_RETRIES) + ".").c_str());
            return;
        }
        retries = (uint8_t)r;
    }

    if (doc["retry_delay_ms"].is<int>()) {
        int d = doc["retry_delay_ms"].as<int>();
        if (d < 0 || d > WOL_MAX_RETRY_DELAY_MS) {
            sendError(400, ("'retry_delay_ms' must be 0.." + String(WOL_MAX_RETRY_DELAY_MS) + ".").c_str());
            return;
        }
        retryDelayMs = (uint16_t)d;
    }

    // Send the magic packet
    bool success = wol_->sendWake(targetName.c_str(), retries, retryDelayMs);

    // Build response
    const WolResult& wr = wol_->getLastResult();
    JsonDocument resp;
    resp["success"]           = success;
    resp["target"]            = wr.targetName;
    resp["mac"]               = wr.macStr;
    resp["packets_sent"]      = wr.packetsSent;
    resp["packets_requested"] = wr.packetsRequested;
    resp["note"]              = success
        ? "Magic packet(s) transmitted. This does NOT confirm the target woke. "
          "Verify with a separate ping or service check."
        : "Packet transmission failed. Check Wi-Fi connectivity and UDP broadcast settings.";

    String output;
    serializeJson(resp, output);
    sendJson(success ? 200 : 502, output);
}

void SmartHomeServer::handleNotFound() {
    sendError(404, "Endpoint not found. See /api/health for available endpoints.");
}

// =============================================================================
// Helpers
// =============================================================================

void SmartHomeServer::sendJson(int code, const String& json) {
    setCORSHeaders();
    server_.send(code, "application/json", json);
}

void SmartHomeServer::sendError(int code, const char* message) {
    JsonDocument doc;
    doc["success"] = false;
    doc["error"]   = message;
    doc["code"]    = code;

    String output;
    serializeJson(doc, output);
    sendJson(code, output);
}

void SmartHomeServer::setCORSHeaders() {
    // Allow cross-origin requests from local development tools / dashboards.
    // In production on a trusted LAN, this is acceptable.
    // For public-facing deployments: restrict the origin.
    server_.sendHeader("Access-Control-Allow-Origin", "*");
    server_.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server_.sendHeader("Access-Control-Allow-Headers", "Authorization, Content-Type");
}
