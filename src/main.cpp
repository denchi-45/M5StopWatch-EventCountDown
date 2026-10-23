// M5StopWatchi_CountDown — event countdown for M5StopWatch (and other
// M5Unified boards). See README.md for build, install and return procedures.
#include <Arduino.h>
#include <M5Unified.h>
#include "app/App.h"
#include "hal/LauncherBridge.h"

void setup() {
    // MultiFirm: KEY B held at power-on returns to the Launcher before any
    // peripheral is initialised.
    cd::launcher::checkStartupEscape();
    Serial.begin(115200);
    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    M5.begin(cfg);
    cd::app().setup();
}

void loop() {
    M5.update();
    cd::app().loop();
    delay(4);
}
