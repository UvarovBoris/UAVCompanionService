#pragma once

#include <atomic>
#include <cstdint>
#include <thread>
#include <common/mavlink.h>
#include "TelemetryData.h"
#include "TelemetryStore.h"

class MavlinkReceiver {
private:
    const uint16_t port_{};
    TelemetryStore &telemetryStore_;
    int socketFd_{-1};
    std::atomic<bool> isRunning_{false};
    std::jthread worker_{};

public:
    explicit MavlinkReceiver(uint16_t port, TelemetryStore &store);

    MavlinkReceiver(const MavlinkReceiver &) = delete;

    MavlinkReceiver &operator=(const MavlinkReceiver &) = delete;

    MavlinkReceiver(MavlinkReceiver &&) = delete;

    MavlinkReceiver &operator=(MavlinkReceiver &&) = delete;

    ~MavlinkReceiver() noexcept;

    void start();

    void stop();

private:
    void listenLoop() const;

    static void handleMessage(const mavlink_message_t &msg, TelemetryData &currentData);
};
