# PlatformIO supplies the lib_deps include paths and libraries (M5Unified,
# M5GFX, ArduinoJson; MultiFirm only for env:m5stopwatch-launcher).
# The env:native simulator does not use this file.
idf_component_register(SRCS "main.cpp" "app/App.cpp"
    "core/ClockService.cpp" "core/CountdownEngine.cpp" "core/DateEditor.cpp" "core/EventRepository.cpp"
    "core/SyncScheduler.cpp" "core/Utf8.cpp"
    "hal/DeviceCapabilities.cpp" "hal/KvStore.cpp" "hal/LauncherBridge.cpp" "hal/PowerHal.cpp" "hal/RtcHal.cpp" "hal/UiTask.cpp"
    "input/InputRouter.cpp"
    "services/JsonCodec.cpp" "services/NetworkService.cpp" "services/NtpSync.cpp" "services/SettingsStore.cpp"
    "services/WebConfigServer.cpp"
    "ui/CountdownScreen.cpp" "ui/HudFont.cpp" "ui/HudWidgets.cpp" "ui/LayoutPolicy.cpp" "ui/Painter.cpp"
    "ui/ProgressRing.cpp" "ui/SettingsScreen.cpp" "ui/SevenSegmentRenderer.cpp" "ui/TextUtil.cpp"
    INCLUDE_DIRS "."
    # The linker symbols (_binary_index_html_start) come from the file name alone.
    EMBED_TXTFILES "../web/index.html"
    REQUIRES nvs_flash esp_wifi esp_netif esp_event esp_http_server esp_timer esp_pm esp_hw_support
        esp_driver_gpio lwip app_update)
target_compile_features(${COMPONENT_LIB} PRIVATE cxx_std_17)
