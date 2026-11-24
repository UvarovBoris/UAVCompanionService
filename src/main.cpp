#include <iostream>
#include <stdexcept>
#include <system_error>

#include "CameraStream.h"
#include "TelemetryStore.h"

int main() {
    TelemetryStore telemetryStore;

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

    return 0;
}
