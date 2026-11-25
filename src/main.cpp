#include <iostream>
#include <stdexcept>
#include <system_error>

#include "CameraStream.h"
#include "MavlinkReceiver.h"
#include "TelemetryStore.h"

int main() {
    TelemetryStore telemetryStore;

    MavlinkReceiver mavlinkReceiver(14551, telemetryStore);
    try {
        mavlinkReceiver.start();
    } catch (const std::system_error &e) {
        std::cerr << "[MAVLink Error] " << e.what() << '\n';
        return 1;
    }

    CameraStream cameraStream(telemetryStore);
    try {
        cameraStream.start();
    } catch (const std::runtime_error &e) {
        std::cerr << "[Camera Error] " << e.what() << '\n';
        return 1;
    }

    std::cout << "Press Enter to stop streaming...\n";
    std::cin.get();

    cameraStream.stop();
    mavlinkReceiver.stop();

    return 0;
}
