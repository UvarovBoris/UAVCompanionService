#pragma once

#include <mutex>
#include "TelemetryData.h"

class TelemetryStore {
private:
    mutable std::mutex mutex_;
    TelemetryData data_{};

public:
    TelemetryStore() = default;

    TelemetryStore(const TelemetryStore &) = delete;

    TelemetryStore &operator=(const TelemetryStore &) = delete;

    TelemetryStore(TelemetryStore &&) = delete;

    TelemetryStore &operator=(TelemetryStore &&) = delete;

    void updateData(const TelemetryData &data);

    TelemetryData getData() const;
};
