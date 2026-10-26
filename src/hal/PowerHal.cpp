#include "PowerHal.h"
#include <M5Unified.h>
#include <driver/gpio.h>
#include <esp_pm.h>
#include <esp_sleep.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstdio>
#include <iterator>

namespace cd {
namespace power {

namespace {
esp_pm_lock_handle_t g_sleepLock = nullptr;
bool g_sleepAllowed = false;

// M5StopWatch: KEY A, KEY B and the touch controller's INT, all idle high and
// low while active (wararyoLauncher hal/InputWake.cpp).
constexpr gpio_num_t kPins[] = {GPIO_NUM_2, GPIO_NUM_1, GPIO_NUM_13};
TaskHandle_t g_waiter = nullptr;

void IRAM_ATTR onLow(void* arg) {
    // A level interrupt refires while the pin stays low: disable it here and
    // re-enable it before the next wait once the pin is back high.
    gpio_intr_disable(static_cast<gpio_num_t>(reinterpret_cast<intptr_t>(arg)));
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR(g_waiter, &woken);
    if (woken) portYIELD_FROM_ISR();
}

void rearm() {
    if (!g_waiter) return;
    for (const gpio_num_t pin : kPins)
        if (gpio_get_level(pin)) gpio_intr_enable(pin);
}

bool isStopWatch() { return M5.getBoard() == m5::board_t::board_M5StopWatch; }

}  // namespace

void begin(int maxMhz, int minMhz) {
    // IDF isolates GPIOs in light sleep. Keep the CO5300 deselected while its
    // clock/data pins float; the panel keeps showing its own memory.
    if (isStopWatch()) gpio_sleep_set_pull_mode(GPIO_NUM_39, GPIO_PULLUP_ONLY);
    // No frequency lock of our own: IDF holds the maximum whenever a task other
    // than idle runs, so frames draw at 240 MHz and only the waits drop to 80.
    esp_err_t err = esp_pm_lock_create(ESP_PM_NO_LIGHT_SLEEP, 0, "countdown", &g_sleepLock);
    if (err == ESP_OK) err = esp_pm_lock_acquire(g_sleepLock);
    // Without the lock, sleeping could cut a panel transfer or the USB console.
    const esp_pm_config_t cfg{maxMhz, minMhz, err == ESP_OK};
    const esp_err_t pm = esp_pm_configure(&cfg);
    std::printf("[Power] dfs max=%dMHz min=%dMHz light_sleep=%d lock=%s result=%s\n", maxMhz, minMhz,
                int(cfg.light_sleep_enable), esp_err_to_name(err), esp_err_to_name(pm));
}

bool declareReadableFramebuffer() {
    if (!isStopWatch()) return false;
    bool readable = true;
    for (const uint16_t c : {uint16_t(0x1234), uint16_t(0xedcb)}) {
        M5.Display.drawPixel(0, 0, c);
        readable = readable && M5.Display.readPixel(0, 0) == c;
    }
    M5.Display.drawPixel(0, 0, 0);
    auto* panel = static_cast<lgfx::Panel_Device*>(M5.Display.getPanel());
    if (readable && panel) {
        auto cfg = panel->config();
        cfg.readable = true;
        panel->config(cfg);
    }
    std::printf("[Display] framebuffer readback=%s\n", readable ? "yes" : "no");
    return readable;
}

bool beginInputWake() {
    if (!isStopWatch()) return false;
    const esp_err_t err = gpio_install_isr_service(0);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::printf("[Input] wake interrupts unavailable: %s\n", esp_err_to_name(err));
        return false;
    }
    g_waiter = xTaskGetCurrentTaskHandle();
    for (const gpio_num_t pin : kPins) {
        // Low level rather than an edge: light-sleep GPIO wake-up only
        // supports levels and shares the pin's interrupt type.
        gpio_set_intr_type(pin, GPIO_INTR_LOW_LEVEL);
        gpio_isr_handler_add(pin, onLow, reinterpret_cast<void*>(static_cast<intptr_t>(pin)));
        gpio_wakeup_enable(pin, GPIO_INTR_LOW_LEVEL);
    }
    const esp_err_t wake = esp_sleep_enable_gpio_wakeup();
    if (wake != ESP_OK) std::printf("[Input] light-sleep wake-up unavailable: %s\n", esp_err_to_name(wake));
    rearm();
    return true;
}

bool inputWakeAvailable() { return g_waiter != nullptr; }

void setLightSleepAllowed(bool allowed) {
    // Never without the input wake-up: nothing but the timer would end it.
    if (!g_sleepLock || !g_waiter) allowed = false;
    if (allowed == g_sleepAllowed) return;
    const esp_err_t r = allowed ? esp_pm_lock_release(g_sleepLock) : esp_pm_lock_acquire(g_sleepLock);
    if (r == ESP_OK) g_sleepAllowed = allowed;
}

void wait(uint32_t timeoutMs) {
    rearm();
    // At least one tick, so the idle task always gets to run.
    ulTaskNotifyTake(pdTRUE, timeoutMs ? pdMS_TO_TICKS(timeoutMs) : 1);
}

}  // namespace power
}  // namespace cd
