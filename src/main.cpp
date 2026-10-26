// EventCountDown — event countdown for M5StopWatch (and other M5Unified
// boards). See README.md for build, install and return procedures.
#include <M5Unified.h>
#include <esp_app_desc.h>
#include <nvs_flash.h>
#include <cstdio>
#include "app/App.h"
#include "hal/LauncherBridge.h"
#include "hal/PowerHal.h"
#include "hal/UiTask.h"

extern "C" void app_main() {
    // MultiFirm: KEY B held at power-on returns to the Launcher before any
    // peripheral is initialised.
    cd::launcher::checkStartupEscape();
    // The NVS partition is shared with the Launcher and other apps: never
    // erased here. Without it the app runs on defaults and saves nothing.
    const esp_err_t nvs = nvs_flash_init();
    if (nvs != ESP_OK) std::printf("[NVS] unavailable: %s; settings will not be kept\n", esp_err_to_name(nvs));

    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    M5.begin(cfg);
    const bool framebuffer = cd::power::declareReadableFramebuffer();
    cd::power::begin(240, 80);
    const bool inputWake = cd::power::beginInputWake();
    std::printf("[EventCountDown] version=%s idf=%s input_wake=%d\n", esp_app_get_description()->version,
                esp_get_idf_version(), int(inputWake));

    cd::uitask::begin();
    cd::uitask::lock();
    cd::App& app = cd::app();
    app.setup(framebuffer, inputWake);
    for (;;) {
        M5.update();
        app.loop();
        const uint32_t waitMs = app.idleMs();
        const bool sleep = app.lightSleepAllowed();
        if (sleep) M5.Display.waitDisplay();  // no light sleep during a transfer
        cd::uitask::unlock();
        cd::power::setLightSleepAllowed(sleep);
        cd::power::wait(waitMs);
        cd::power::setLightSleepAllowed(false);
        cd::uitask::lock();
    }
}
