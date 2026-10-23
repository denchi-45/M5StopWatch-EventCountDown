#pragma once
// Setup web UI + JSON API served on the SoftAP only (spec 3, 7).
// Protection: WPA2 key shown on the device screen, requests accepted only on
// the AP interface with Host == AP IP (DNS rebinding), and every mutating call
// needs the per-session token in X-CD-Session (blocks cross-site requests,
// which cannot set custom headers without a CORS pre-flight we never allow).
#include <cstdint>
#include <memory>
#include <string>

class WebServer;

namespace cd {

class App;

class WebConfigServer {
public:
    explicit WebConfigServer(App& app);
    ~WebConfigServer();
    void start();
    void stop();
    void loop();  // handleClient + deferred exit
    bool exitRequested() const { return exitAt_ != 0; }
    int64_t exitAt() const { return exitAt_; }

private:
    App& app_;
    std::unique_ptr<WebServer> server_;
    std::string session_;
    int64_t exitAt_ = 0;
    // RTC preview token (POST /api/rtc/preview).
    std::string previewToken_;
    int64_t previewExpires_ = 0;
    uint32_t previewGen_ = 0;
    uint8_t previewMode_ = 0, previewFromMode_ = 0;

    void routes();
    bool allowed(bool mutating);
    bool readJson(void* doc);
    void sendJson(int code, const std::string& body);
    void sendError(int code, const char* msg);
    void sendOk();

    void handleStatus();
    void handleEventsGet();
    void handleEventsPost();
    void handleEventPut();
    void handleEventDelete();
    void handleOrder();
    void handleSelection();
    void handleSettingsGet();
    void handleSettingsPut();
    void handleRtcPreview();
    void handleWifiGet();
    void handleWifiPut();
    void handleTime();
    void handleNtp();
    void handleActivity();
    void handleExit();
};

}  // namespace cd
