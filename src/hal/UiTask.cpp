#include "UiTask.h"
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace cd {
namespace uitask {

namespace {
SemaphoreHandle_t g_lock = nullptr;
TaskHandle_t g_task = nullptr;
}  // namespace

void begin() {
    g_task = xTaskGetCurrentTaskHandle();
    g_lock = xSemaphoreCreateMutex();
}

void lock() { xSemaphoreTake(g_lock, portMAX_DELAY); }
void unlock() { xSemaphoreGive(g_lock); }
bool tryLock(uint32_t timeoutMs) { return xSemaphoreTake(g_lock, pdMS_TO_TICKS(timeoutMs)) == pdTRUE; }

void notify() {
    if (g_task) xTaskNotifyGive(g_task);
}

}  // namespace uitask
}  // namespace cd
