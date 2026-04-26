#include "DebugWifiOta.h"

#include "Logger.h"

#if BOATLOCK_DEBUG_WIFI_OTA

#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

#include "mbedtls/sha256.h"

#ifndef BOATLOCK_DEBUG_WIFI_SSID
#error "BOATLOCK_DEBUG_WIFI_SSID must be defined for BOATLOCK_DEBUG_WIFI_OTA"
#endif

#ifndef BOATLOCK_DEBUG_WIFI_PASS
#error "BOATLOCK_DEBUG_WIFI_PASS must be defined for BOATLOCK_DEBUG_WIFI_OTA"
#endif

#ifndef BOATLOCK_DEBUG_OTA_PASS
#error "BOATLOCK_DEBUG_OTA_PASS must be defined for BOATLOCK_DEBUG_WIFI_OTA"
#endif

namespace DebugWifiOta {
namespace {
constexpr unsigned long kReconnectIntervalMs = 10000;
constexpr unsigned long kStatusIntervalMs = 5000;
constexpr unsigned long kBleDelayMs = 45000;
constexpr uint16_t kHttpOtaPort = 8080;
OtaStartCallback otaStartCallback = nullptr;
unsigned long beginMs = 0;
unsigned long lastReconnectAttemptMs = 0;
unsigned long lastStatusLogMs = 0;
bool wasConnected = false;
bool mdnsStarted = false;
bool httpOtaStarted = false;
bool bleDelayClosed = false;
bool wifiClosedForBle = false;
char ipBuffer[16] = "0.0.0.0";
char hostnameBuffer[32] = "";
WebServer httpServer(kHttpOtaPort);

void updateIpBuffer() {
  const IPAddress ip = WiFi.localIP();
  snprintf(ipBuffer, sizeof(ipBuffer), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
}

const char* hostname() {
  if (hostnameBuffer[0] == '\0') {
    const uint32_t suffix = static_cast<uint32_t>(ESP.getEfuseMac() & 0xFFFFFFUL);
    snprintf(hostnameBuffer, sizeof(hostnameBuffer), "boatlock-%06lx", (unsigned long)suffix);
  }
  return hostnameBuffer;
}

void beginMdns() {
  if (mdnsStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  if (!MDNS.begin(hostname())) {
    logMessage("[NET] mdns failed host=%s\n", hostname());
    return;
  }
  MDNS.addService("http", "tcp", kHttpOtaPort);
  mdnsStarted = true;
  updateIpBuffer();
  logMessage("[NET] mdns ready host=%s ip=%s service=http port=%u\n",
             hostname(),
             ipBuffer,
             static_cast<unsigned int>(kHttpOtaPort));
}

bool httpOtaAuthorized() {
  String secret = httpServer.header("X-BoatLock-OTA");
  if (secret.length() == 0) {
    secret = httpServer.arg("pass");
  }
  return secret.equals(BOATLOCK_DEBUG_OTA_PASS);
}

bool isSha256Hex(const String& value) {
  if (value.length() != 64) {
    return false;
  }
  for (size_t i = 0; i < value.length(); ++i) {
    const char c = value.charAt(i);
    const bool digit = c >= '0' && c <= '9';
    const bool lower = c >= 'a' && c <= 'f';
    const bool upper = c >= 'A' && c <= 'F';
    if (!digit && !lower && !upper) {
      return false;
    }
  }
  return true;
}

char lowerHex(char c) {
  return (c >= 'A' && c <= 'F') ? static_cast<char>(c - 'A' + 'a') : c;
}

void digestToHex(const unsigned char* digest, char* output, size_t outputSize) {
  static const char* kHex = "0123456789abcdef";
  if (outputSize < 65) {
    return;
  }
  for (size_t i = 0; i < 32; ++i) {
    output[i * 2] = kHex[(digest[i] >> 4) & 0x0f];
    output[i * 2 + 1] = kHex[digest[i] & 0x0f];
  }
  output[64] = '\0';
}

bool shaMatches(const char* actual, const String& expected) {
  for (size_t i = 0; i < 64; ++i) {
    if (actual[i] != lowerHex(expected.charAt(i))) {
      return false;
    }
  }
  return true;
}

bool pullUpdateFromUrl(const String& url, const String& expectedSha, String& detail) {
  const bool useHttps = url.startsWith("https://");
  if (!url.startsWith("http://") && !useHttps) {
    detail = "url must be http or https";
    return false;
  }
  if (!isSha256Hex(expectedSha)) {
    detail = "sha256 required";
    return false;
  }

  WiFiClient client;
  WiFiClientSecure secureClient;
  HTTPClient http;
  http.setTimeout(15000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  bool httpReady = false;
  if (useHttps) {
    secureClient.setInsecure();
    httpReady = http.begin(secureClient, url);
  } else {
    httpReady = http.begin(client, url);
  }
  if (!httpReady) {
    detail = "http begin failed";
    return false;
  }

  const int code = http.GET();
  if (code != HTTP_CODE_OK) {
    detail = "http code ";
    detail += code;
    http.end();
    return false;
  }

  const int contentLength = http.getSize();
  if (otaStartCallback) {
    otaStartCallback();
  }
  if (!Update.begin(contentLength > 0 ? static_cast<size_t>(contentLength) : UPDATE_SIZE_UNKNOWN)) {
    detail = "update begin ";
    detail += Update.getError();
    http.end();
    return false;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  WiFiClient* stream = http.getStreamPtr();
  uint8_t buffer[4096];
  size_t total = 0;
  unsigned long lastDataMs = millis();
  bool ok = true;

  while (http.connected() && (contentLength < 0 || total < static_cast<size_t>(contentLength))) {
    const size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataMs > 15000) {
        detail = "download timeout";
        ok = false;
        break;
      }
      delay(1);
      continue;
    }

    const size_t chunkSize = available > sizeof(buffer) ? sizeof(buffer) : available;
    const int readLen = stream->readBytes(buffer, chunkSize);
    if (readLen <= 0) {
      detail = "download read";
      ok = false;
      break;
    }
    lastDataMs = millis();
    const size_t written = Update.write(buffer, static_cast<size_t>(readLen));
    if (written != static_cast<size_t>(readLen)) {
      detail = "update write ";
      detail += Update.getError();
      ok = false;
      break;
    }
    mbedtls_sha256_update(&sha, buffer, static_cast<size_t>(readLen));
    total += static_cast<size_t>(readLen);
    delay(1);
  }

  unsigned char digest[32];
  char digestHex[65];
  mbedtls_sha256_finish(&sha, digest);
  mbedtls_sha256_free(&sha);
  digestToHex(digest, digestHex, sizeof(digestHex));

  if (ok && contentLength >= 0 && total != static_cast<size_t>(contentLength)) {
    detail = "size mismatch";
    ok = false;
  }
  if (ok && !shaMatches(digestHex, expectedSha)) {
    detail = "sha mismatch";
    ok = false;
  }
  if (!ok) {
    Update.abort();
    http.end();
    return false;
  }
  if (!Update.end(true)) {
    detail = "update end ";
    detail += Update.getError();
    http.end();
    return false;
  }

  http.end();
  detail = "size=";
  detail += static_cast<unsigned int>(total);
  detail += " sha256=";
  detail += digestHex;
  return true;
}

void sendWebUi() {
  updateIpBuffer();
  const unsigned long now = millis();
  const unsigned long remainingMs =
      (beginMs > 0 && now - beginMs < kBleDelayMs) ? (kBleDelayMs - (now - beginMs)) : 0;
  String html;
  html.reserve(2600);
  html += F("<!doctype html><html><head><meta charset=\"utf-8\">"
            "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
            "<title>BoatLock OTA</title><style>"
            "body{font-family:-apple-system,BlinkMacSystemFont,Segoe UI,sans-serif;margin:24px;max-width:720px}"
            "label{display:block;margin:14px 0 6px;font-weight:600}"
            "input{box-sizing:border-box;width:100%;font:inherit;padding:10px;border:1px solid #999;border-radius:6px}"
            "button{margin-top:18px;padding:10px 16px;font:inherit;font-weight:700}"
            "code{background:#eee;padding:2px 5px;border-radius:4px}"
            ".meta{line-height:1.6;margin-bottom:22px}</style></head><body>"
            "<h1>BoatLock OTA</h1><div class=\"meta\">Host: <code>");
  html += hostname();
  html += F(".local</code><br>IP: <code>");
  html += ipBuffer;
  html += F("</code><br>OTA window: <code>");
  html += static_cast<unsigned long>((remainingMs + 999) / 1000);
  html += F("s</code></div><form method=\"post\" action=\"/pull-update\">"
            "<label for=\"url\">Firmware URL</label>"
            "<input id=\"url\" name=\"url\" type=\"url\" required placeholder=\"https://.../firmware.bin\">"
            "<label for=\"sha256\">SHA-256</label>"
            "<input id=\"sha256\" name=\"sha256\" required minlength=\"64\" maxlength=\"64\" pattern=\"[A-Fa-f0-9]{64}\">"
            "<label for=\"pass\">OTA password</label>"
            "<input id=\"pass\" name=\"pass\" type=\"password\" required>"
            "<button type=\"submit\">Pull update</button></form></body></html>");
  httpServer.send(200, "text/html", html);
}

void sendStatusJson() {
  updateIpBuffer();
  const unsigned long now = millis();
  const unsigned long remainingMs =
      (beginMs > 0 && now - beginMs < kBleDelayMs) ? (kBleDelayMs - (now - beginMs)) : 0;
  String body;
  body.reserve(180);
  body += F("{\"host\":\"");
  body += hostname();
  body += F("\",\"ip\":\"");
  body += ipBuffer;
  body += F("\",\"otaWindowSeconds\":");
  body += static_cast<unsigned long>((remainingMs + 999) / 1000);
  body += F(",\"wifiRssi\":");
  body += WiFi.RSSI();
  body += F("}");
  httpServer.send(200, "application/json", body);
}

void beginHttpOta() {
  if (httpOtaStarted || WiFi.status() != WL_CONNECTED) {
    return;
  }

  static const char* headerKeys[] = {"X-BoatLock-OTA"};
  httpServer.collectHeaders(headerKeys, 1);
  httpServer.on("/", HTTP_GET, []() { sendWebUi(); });
  httpServer.on("/status", HTTP_GET, []() { sendStatusJson(); });
  httpServer.on("/pull-update", HTTP_POST, []() {
    httpServer.sendHeader("Connection", "close");
    if (!httpOtaAuthorized()) {
      logMessage("[NET] pull ota rejected reason=auth\n");
      httpServer.send(401, "text/plain", "unauthorized\n");
      return;
    }
    const String url = httpServer.arg("url");
    const String expectedSha = httpServer.arg("sha256");
    if (url.length() == 0) {
      httpServer.send(400, "text/plain", "url is required\n");
      return;
    }

    logMessage("[NET] pull ota start sha=%d\n", expectedSha.length() > 0 ? 1 : 0);
    String detail;
    if (!pullUpdateFromUrl(url, expectedSha, detail)) {
      logMessage("[NET] pull ota failed reason=%s\n", detail.c_str());
      httpServer.send(500, "text/plain", detail + "\n");
      return;
    }
    logMessage("[NET] pull ota uploaded %s\n", detail.c_str());
    httpServer.send(200, "text/plain", "OK\n");
    logMessage("[NET] pull ota end reboot=1\n");
    delay(100);
    ESP.restart();
  });
  httpServer.onNotFound([]() { httpServer.send(404, "text/plain", "not found\n"); });
  httpServer.begin();
  httpOtaStarted = true;
  updateIpBuffer();
  logMessage("[NET] http ota ready host=%s ip=%s port=%u auth=1 path=/pull-update\n",
             hostname(),
             ipBuffer,
             static_cast<unsigned int>(kHttpOtaPort));
}

void reconnectWifi(unsigned long now) {
  if (now - lastReconnectAttemptMs < kReconnectIntervalMs) {
    return;
  }
  lastReconnectAttemptMs = now;
  WiFi.disconnect();
  WiFi.begin(BOATLOCK_DEBUG_WIFI_SSID, BOATLOCK_DEBUG_WIFI_PASS);
  logMessage("[NET] wifi reconnect ssid=%s\n", BOATLOCK_DEBUG_WIFI_SSID);
}
} // namespace

void begin(OtaStartCallback onStart) {
  otaStartCallback = onStart;
  beginMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.begin(BOATLOCK_DEBUG_WIFI_SSID, BOATLOCK_DEBUG_WIFI_PASS);
  lastReconnectAttemptMs = millis();
  logMessage("[NET] wifi begin ssid=%s ota=1 ble_delay_ms=%lu\n",
             BOATLOCK_DEBUG_WIFI_SSID,
             kBleDelayMs);
}

void loop() {
  if (wifiClosedForBle) {
    return;
  }

  const unsigned long now = millis();
  if (WiFi.status() == WL_CONNECTED) {
    updateIpBuffer();
    if (!wasConnected) {
      wasConnected = true;
      logMessage("[NET] wifi connected ssid=%s ip=%s rssi=%d host=%s\n",
                 BOATLOCK_DEBUG_WIFI_SSID,
                 ipBuffer,
                 WiFi.RSSI(),
                 hostname());
    } else if (now - lastStatusLogMs >= kStatusIntervalMs) {
      lastStatusLogMs = now;
      logMessage("[NET] wifi status ip=%s rssi=%d\n", ipBuffer, WiFi.RSSI());
    }
    beginMdns();
    beginHttpOta();
    httpServer.handleClient();
    return;
  }

  if (wasConnected) {
    wasConnected = false;
    logMessage("[NET] wifi lost status=%d\n", static_cast<int>(WiFi.status()));
  }
  reconnectWifi(now);
}

bool enabled() {
  return true;
}

bool wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* ipString() {
  updateIpBuffer();
  return ipBuffer;
}

bool bleMayStart() {
  if (beginMs == 0 || millis() - beginMs < kBleDelayMs) {
    return false;
  }
  if (!bleDelayClosed) {
    bleDelayClosed = true;
    httpServer.stop();
    if (mdnsStarted) {
      MDNS.end();
    }
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
    wifiClosedForBle = true;
    logMessage("[NET] wifi off ble_start=1\n");
  }
  return true;
}

} // namespace DebugWifiOta

#else

namespace DebugWifiOta {

void begin(OtaStartCallback) {}

void loop() {}

bool enabled() {
  return false;
}

bool wifiConnected() {
  return false;
}

const char* ipString() {
  return "0.0.0.0";
}

bool bleMayStart() {
  return true;
}

} // namespace DebugWifiOta

#endif
