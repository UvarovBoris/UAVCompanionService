cmake_minimum_required(VERSION 4.4)
project(UAVCompanionService)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(GST REQUIRED gstreamer-1.0 gstreamer-video-1.0 cairo)

add_executable(UAVCompanionService
        src/main.cpp
        src/TelemetryStore.cpp
        src/CameraStream.cpp
        src/TelemetryData.h
        src/TelemetryStore.h
        src/CameraStream.h
        src/MavlinkReceiver.cpp
)

target_compile_options(UAVCompanionService PRIVATE -Wall -Wextra ${GST_CFLAGS_OTHER})

target_include_directories(UAVCompanionService SYSTEM PRIVATE
        ${GST_INCLUDE_DIRS}
        /home/boris/mavlink/c_library_v2
)

target_link_libraries(UAVCompanionService PRIVATE ${GST_LIBRARIES})
