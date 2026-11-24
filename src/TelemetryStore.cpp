#include "TelemetryStore.h"

void TelemetryStore::updateData(const TelemetryData &data) {
    std::lock_guard lock(mutex_);
    data_ = data;
}

TelemetryData TelemetryStore::getData() const {
    std::lock_guard lock(mutex_);
    return data_;
}
