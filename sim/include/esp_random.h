// Simulator: hardware RNG replacement.
#pragma once
#include <cstdint>
#include <random>

inline uint32_t esp_random() {
    static std::mt19937 rng{std::random_device{}()};
    return uint32_t(rng());
}
