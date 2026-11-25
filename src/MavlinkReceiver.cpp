#include "MavlinkReceiver.h"

#include <array>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <system_error>
#include <unistd.h>

MavlinkReceiver::MavlinkReceiver(const uint16_t port, TelemetryStore &store)
    : port_(port),
      telemetryStore_(store) {
}

MavlinkReceiver::~MavlinkReceiver() noexcept {
    stop();
}

void MavlinkReceiver::start() {
    if (isRunning_) {
        return;
    }

    // 1. Create UDP socket (SOCK_DGRAM)
    socketFd_ = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd_ < 0) {
        throw std::system_error(errno, std::generic_category(), "socket() failed");
    }

    // Allow immediate reuse of the port
    int enable = 1;
    ::setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    // Set receive timeout so stop() never blocks indefinitely when no packets arrive
    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200'000; // 200 ms
    ::setsockopt(socketFd_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    // 2. Bind to local interface (INADDR_ANY listens on 0.0.0.0 and 127.0.0.1)
    sockaddr_in localAddr{};
    localAddr.sin_family = AF_INET;
    localAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    localAddr.sin_port = htons(port_);

    if (::bind(socketFd_, reinterpret_cast<sockaddr *>(&localAddr), sizeof(localAddr)) < 0) {
        int err = errno;
        ::close(socketFd_);
        socketFd_ = -1;
        throw std::system_error(err, std::generic_category(), "bind() failed");
    }

    std::cout << "Listening for MAVLink datagrams on UDP...\n";

    isRunning_ = true;
    worker_ = std::jthread(&MavlinkReceiver::listenLoop, this);
}

void MavlinkReceiver::stop() {
    if (!isRunning_.exchange(false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    if (socketFd_ >= 0) {
        ::close(socketFd_);
        socketFd_ = -1;
    }
}

void MavlinkReceiver::listenLoop() const {
    std::array<uint8_t, 2048> rxBuffer{}; // Typical MTU size buffer
    mavlink_message_t msg{};
    mavlink_status_t status{};
    TelemetryData currentData{};

    while (isRunning_) {
        // 3. Receive single UDP datagram
        ssize_t bytesReceived = ::recv(
            socketFd_,
            rxBuffer.data(),
            rxBuffer.size(),
            0
        );

        if (bytesReceived < 0) {
            if (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            std::cerr << "recvfrom error: " << std::strerror(errno) << '\n';
            break;
        }

        // 4. Feed datagram bytes to MAVLink state machine
        for (ssize_t i = 0; i < bytesReceived; ++i) {
            if (mavlink_parse_char(MAVLINK_COMM_0, rxBuffer[static_cast<size_t>(i)], &msg, &status)) {
                handleMessage(msg, currentData);
            }
        }
        telemetryStore_.updateData(currentData);
    }
}

void MavlinkReceiver::handleMessage(const mavlink_message_t &msg, TelemetryData &currentData) {
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    switch (msg.msgid) {
        case MAVLINK_MSG_ID_HEARTBEAT: {
            mavlink_heartbeat_t hb;
            mavlink_msg_heartbeat_decode(&msg, &hb);
            std::cout << "[HEARTBEAT] Type: " << static_cast<int>(hb.type)
                    << "| Autopilot: " << static_cast<int>(hb.autopilot)
                    << "| Mode: " << static_cast<int>(hb.base_mode) << '\n';
            break;
        }
        case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
            mavlink_global_position_int_t pos;
            mavlink_msg_global_position_int_decode(&msg, &pos);
            currentData.alt = static_cast<float>(pos.relative_alt / 1000.0);
            currentData.lat = pos.lat;
            currentData.lon = pos.lon;
            currentData.lastUpdateTime = now;
            std::cout << "[POS] Lat: " << currentData.lat / 1e7
                    << " | Lon: " << currentData.lon / 1e7
                    << " | Alt: " << currentData.alt << " m\n";
            break;
        }
        case MAVLINK_MSG_ID_ATTITUDE: {
            mavlink_attitude_t att;
            mavlink_msg_attitude_decode(&msg, &att);
            currentData.roll = att.roll;
            currentData.pitch = att.pitch;
            currentData.yaw = att.yaw;
            currentData.lastUpdateTime = now;
            break;
        }
        case MAVLINK_MSG_ID_SYS_STATUS: {
            mavlink_sys_status_t sys;
            mavlink_msg_sys_status_decode(&msg, &sys);
            currentData.batteryRemaining = sys.battery_remaining;
            currentData.lastUpdateTime = now;
            break;
        }
        default:
            break;
    }
}
