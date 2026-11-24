#pragma once

#include <cstdint>

struct TelemetryData {
    float alt{0.0f};
    int32_t lat{0};
    int32_t lon{0};
    float roll{0.0f};
    float pitch{0.0f};
    float yaw{0.0f};
    int8_t batteryRemaining{0};
    int64_t lastUpdateTime{0};
};
