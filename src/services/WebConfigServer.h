#pragma once
// Setup web UI + JSON API served on the SoftAP only (spec 3, 7).
// Protection: WPA2 key shown on the device screen, requests accepted only on
// the AP interface with Host == AP IP (DNS rebinding), and every mutating call
// needs the per-session token in X-CD-Session (blocks cross-site requests,
// which cannot set custom headers without a CORS pre-flight we never allow).
// esp_http_server answers on its own task; each handler runs under the UI lock
// (hal/UiTask.h), so App is never used by two tasks at once.
#include <atomic>
#include <cstdint>
#include <string>

namespace cd {

class App;

class WebConfigServer {
public:
    explicit WebConfigServer(App& app);
    ~WebConfigServer();
    void start();
    void stop();
    void loop() {}  // the server has its own task; kept for the call site
    bool exitRequested() const { return exitAt_ != 0; }
    int64_t exitAt() const { return exitAt_; }

private:
    App& app_;
    void* server_ = nullptr;  // httpd_handle_t
    std::atomic<bool> stopping_{false};
    // The request being handled (valid only inside a handler).
    void* req_ = nullptr;     // httpd_req_t*
    std::string body_;        // its body
    std::string pathId_;      // {id} of /api/events/{id}
    std::string session_;
    int64_t exitAt_ = 0;
    // RTC preview token (POST /api/rtc/preview).
    std::string previewToken_;
    int64_t previewExpires_ = 0;
    uint32_t previewGen_ = 0;
    uint8_t previewMode_ = 0, previewFromMode_ = 0;

    struct Route;
    static int dispatch(void* req);  // esp_err_t (httpd_req_t*)
    void routes();
    bool allowed(bool mutating);
    bool readBody();
    bool readJson(void* doc);
    void sendJson(int code, const std::string& body);
    void sendError(int code, const char* msg);
    void sendOk();
    void handleIndex();
    void handleSession();

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
