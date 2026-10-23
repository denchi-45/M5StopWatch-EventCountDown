#include "NtpSync.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <cstring>
#include "../core/TimeMath.h"
#include "../hal/RtcHal.h"

namespace cd {

namespace {
struct Request {
    uint32_t generation;
    char server[64];
    int64_t startedMs;
};

uint32_t be32(const uint8_t* p) {
    return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3];
}

void fail(NtpResult& r, const char* msg) {
    r.ok = false;
    std::strncpy(r.error, msg, sizeof r.error - 1);
}

void query(const Request& req, NtpResult& r) {
    r.generation = req.generation;
    const int64_t deadline = req.startedMs + NtpSync::kTimeoutMs;
    IPAddress ip;
    if (!WiFi.hostByName(req.server, ip)) return fail(r, "DNS lookup failed");
    if (monoMs() >= deadline) return fail(r, "timeout");

    WiFiUDP udp;
    if (!udp.begin(0)) return fail(r, "UDP socket failed");
    uint8_t pkt[48] = {0};
    pkt[0] = 0x23;  // LI 0, version 4, mode 3 (client)
    // Random transmit timestamp; a genuine reply echoes it as "originate".
    const uint32_t nonceHi = esp_random(), nonceLo = esp_random();
    for (int i = 0; i < 4; ++i) {
        pkt[40 + i] = uint8_t(nonceHi >> (24 - 8 * i));
        pkt[44 + i] = uint8_t(nonceLo >> (24 - 8 * i));
    }
    const int64_t sentMs = monoMs();
    if (!udp.beginPacket(ip, 123) || udp.write(pkt, sizeof pkt) != sizeof pkt || !udp.endPacket()) {
        udp.stop();
        return fail(r, "send failed");
    }
    while (monoMs() < deadline) {
        if (udp.parsePacket() >= 48) {
            uint8_t in[48];
            udp.read(in, sizeof in);
            const int64_t rxMs = monoMs();
            const uint8_t li = in[0] >> 6, mode = in[0] & 7, stratum = in[1];
            if (mode != 4 || li == 3 || stratum == 0 || stratum > 15 ||
                be32(in + 24) != nonceHi || be32(in + 28) != nonceLo)
                continue;  // not our answer / kiss-of-death
            const uint32_t sec = be32(in + 40), frac = be32(in + 44);
            const int64_t txMs = ntpToUnixMs(sec, frac);
            if (!inUtcRange(txMs / 1000)) {
                udp.stop();
                return fail(r, "server time out of range");
            }
            udp.stop();
            r.ok = true;
            r.utcMs = txMs + (rxMs - sentMs) / 2;  // half the round trip
            r.monoMsAtRx = rxMs;
            return;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    udp.stop();
    fail(r, "timeout");
}
}  // namespace

bool NtpSync::begin() {
    reqQueue_ = xQueueCreate(1, sizeof(Request));
    resQueue_ = xQueueCreate(2, sizeof(NtpResult));
    if (!reqQueue_ || !resQueue_) return false;
    return xTaskCreatePinnedToCore(&NtpSync::task, "ntp", 4096, this, 1, nullptr, 0) == pdPASS;
}

bool NtpSync::request(uint32_t generation, const char* server) {
    if (!reqQueue_ || busy_.exchange(true)) return false;
    Request req{};
    req.generation = generation;
    std::strncpy(req.server, server, sizeof req.server - 1);
    req.startedMs = monoMs();
    if (xQueueSend(static_cast<QueueHandle_t>(reqQueue_), &req, 0) == pdTRUE) return true;
    busy_ = false;
    return false;
}

bool NtpSync::poll(NtpResult& out) {
    return resQueue_ && xQueueReceive(static_cast<QueueHandle_t>(resQueue_), &out, 0) == pdTRUE;
}

void NtpSync::task(void* arg) {
    auto* self = static_cast<NtpSync*>(arg);
    for (;;) {
        Request req;
        if (xQueueReceive(static_cast<QueueHandle_t>(self->reqQueue_), &req, portMAX_DELAY) != pdTRUE)
            continue;
        NtpResult res;
        query(req, res);
        xQueueSend(static_cast<QueueHandle_t>(self->resQueue_), &res, 0);
        self->busy_ = false;
    }
}

}  // namespace cd
