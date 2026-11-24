#pragma once

#include <atomic>
#include <cairo.h>
#include <gst/gst.h>
#include <gst/video/video.h>
#include <thread>

#include "TelemetryStore.h"

struct OverlayContext {
    TelemetryStore *telemetryStore{nullptr};
    GstVideoInfo vinfo{};
    bool valid_vinfo{false};
};

class CameraStream {
private:
    OverlayContext overlayContext{};
    std::atomic<bool> isRunning_{false};
    std::jthread worker_{};
    GstElement *pipeline_{nullptr};
    GMainLoop *loop_{nullptr};
    guint busWatchId_{0};

public:
    explicit CameraStream(TelemetryStore &telemetryStore);

    CameraStream(const CameraStream &) = delete;

    CameraStream &operator=(const CameraStream &) = delete;

    CameraStream(CameraStream &&) = delete;

    CameraStream &operator=(CameraStream &&) = delete;

    ~CameraStream() noexcept;

    void start();

    void stop();

private:
    static gboolean bus_callback(GstBus *bus, GstMessage *msg, gpointer data);

    static void on_overlay_caps_changed(
        GstElement *overlay,
        GstCaps *caps,
        gpointer user_data
    );

    static void on_draw_overlay(
        GstElement *overlay,
        cairo_t *cr,
        guint64 timestamp,
        guint64 duration,
        gpointer user_data
    );

    void loop() const;
};
