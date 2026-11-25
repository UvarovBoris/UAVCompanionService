#include "CameraStream.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>

CameraStream::CameraStream(TelemetryStore &telemetryStore)
    : overlayContext{.telemetryStore = &telemetryStore} {
}

CameraStream::~CameraStream() noexcept {
    stop();
}

void CameraStream::start() {
    if (isRunning_) {
        return;
    }

    gst_init(nullptr, nullptr);

    // Stream settings
    const std::string host = "127.0.0.1"; // IP-address receiver (GCS)
    const int port = 5601; // UDP-port

    /*
    std::string pipeline_str =
            "v4l2src device=/dev/video0 ! image/jpeg,width=1280,height=720,framerate=30/1 ! "
            "vajpegdec ! "
            "vapostproc ! video/x-raw ! "
            "cairooverlay name=osd ! "
            "vapostproc ! video/x-raw ! "
            "vah264lpenc bitrate=2000 ! "
            "h264parse ! "
            "rtph264pay config-interval=1 pt=96 ! "
            "udpsink host=" + host + " port=" + std::to_string(port);
*/
    std::string pipeline_str =
            "udpsrc port=5600 caps=\"application/x-rtp,media=video,clock-rate=90000,encoding-name=H264,payload=96\" ! "
            "rtph264depay ! h264parse ! "
            "vah264dec ! "
            "vapostproc ! video/x-raw ! "
            "cairooverlay name=osd ! "
            "vapostproc ! video/x-raw ! "
            "vah264lpenc bitrate=2000 ! "
            "h264parse ! "
            "rtph264pay config-interval=1 pt=96 ! "
            "udpsink host=" + host + " port=" + std::to_string(port);

    std::cout << "[INFO] Pipeline construction: " << pipeline_str << std::endl;
    GError *error = nullptr;
    pipeline_ = gst_parse_launch(pipeline_str.c_str(), &error);
    if (error != nullptr) {
        std::string errMsg = std::string("Pipeline construction error: ") + error->message;
        g_error_free(error);
        if (pipeline_ != nullptr) {
            gst_object_unref(pipeline_);
            pipeline_ = nullptr;
        }
        throw std::runtime_error(errMsg);
    }

    // Attach Cairo signals
    GstElement *cairo_element = gst_bin_get_by_name(GST_BIN(pipeline_), "osd");
    g_signal_connect(cairo_element, "caps-changed", G_CALLBACK(on_overlay_caps_changed), &overlayContext);
    g_signal_connect(cairo_element, "draw", G_CALLBACK(on_draw_overlay), &overlayContext);
    gst_object_unref(cairo_element);

    // Watch pipeline bus
    loop_ = g_main_loop_new(nullptr, FALSE);
    GstBus *bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline_));
    busWatchId_ = gst_bus_add_watch(bus, bus_callback, loop_);
    gst_object_unref(bus);

    std::cout << "[Pipeline] Starting playback and video egress to "<< host << ":" << port << std::endl;
    // Change pipeline state to PLAYING (streaming start)
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        g_source_remove(busWatchId_);
        busWatchId_ = 0;
        g_main_loop_unref(loop_);
        loop_ = nullptr;
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        throw std::runtime_error("Can't launch pipeline (check camera connection)");
    }

    isRunning_ = true;
    worker_ = std::jthread(&CameraStream::loop, this);
}

void CameraStream::stop() {
    if (!isRunning_.exchange(false)) {
        return;
    }

    if (loop_ != nullptr) {
        g_main_loop_quit(loop_);
    }
    if (worker_.joinable()) {
        worker_.join();
    }

    std::cout << "[Pipeline] Stopping...\n";
    if (pipeline_ != nullptr) {
        gst_element_set_state(pipeline_, GST_STATE_NULL);
    }
    if (busWatchId_ != 0) {
        g_source_remove(busWatchId_);
        busWatchId_ = 0;
    }
    if (loop_ != nullptr) {
        g_main_loop_unref(loop_);
        loop_ = nullptr;
    }
    if (pipeline_ != nullptr) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
    }
}

void CameraStream::loop() const {
    g_main_loop_run(loop_);
}

gboolean CameraStream::bus_callback([[maybe_unused]] GstBus *bus, GstMessage *msg, gpointer data) {
    auto *loop = static_cast<GMainLoop *>(data);
    switch (GST_MESSAGE_TYPE(msg)) {
        case GST_MESSAGE_ERROR: {
            GError *err = nullptr;
            gchar *dbg = nullptr;
            gst_message_parse_error(msg, &err, &dbg);
            std::cerr << "[GStreamer Error] " << err->message << "\n";
            if (dbg) std::cerr << "[Debug Info] " << dbg << "\n";
            g_error_free(err);
            g_free(dbg);
            g_main_loop_quit(loop);
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "[GStreamer] End of Stream reached\n";
            g_main_loop_quit(loop);
            break;
        default:
            break;
    }
    return TRUE;
}

void CameraStream::on_overlay_caps_changed(
    [[maybe_unused]] GstElement *overlay,
    GstCaps *caps,
    gpointer user_data
) {
    auto *ctx = static_cast<OverlayContext *>(user_data);
    ctx->valid_vinfo = gst_video_info_from_caps(&ctx->vinfo, caps);
}

void CameraStream::on_draw_overlay(
    [[maybe_unused]] GstElement *overlay,
    cairo_t *cr,
    [[maybe_unused]] guint64 timestamp,
    [[maybe_unused]] guint64 duration,
    gpointer user_data
) {
    auto *ctx = static_cast<OverlayContext *>(user_data);
    if (!ctx->valid_vinfo) return;

    TelemetryData data = ctx->telemetryStore->getData();
    const int width = GST_VIDEO_INFO_WIDTH(&ctx->vinfo);
    const int height = GST_VIDEO_INFO_HEIGHT(&ctx->vinfo);
    const double center_x = width / 2.0;
    const double center_y = height / 2.0;

    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    bool isTelemetryAlive = (now - data.lastUpdateTime < 2000);

    // --- Artificial Horizon / Pitch Ladder ---
    cairo_save(cr);
    cairo_translate(cr, center_x, center_y);
    cairo_rotate(cr, -data.roll);

    // Dynamic pitch shift (scaling factor: 4 pixels per degree)
    double pitch_deg = data.pitch * (180.0 / std::numbers::pi);
    double pitch_offset = pitch_deg * 4.0;
    cairo_translate(cr, 0.0, pitch_offset);

    // Horizon Line
    cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.85); // Tactical green
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, -100, 0);
    cairo_line_to(cr, 100, 0);
    cairo_stroke(cr);

    cairo_restore(cr);

    // --- Static Center Crosshair ---
    cairo_set_source_rgba(cr, 0.0, 1.0, 0.0, 0.9);
    cairo_set_line_width(cr, 2.0);
    cairo_move_to(cr, center_x - 15, center_y);
    cairo_line_to(cr, center_x - 5, center_y);
    cairo_move_to(cr, center_x + 5, center_y);
    cairo_line_to(cr, center_x + 15, center_y);
    cairo_move_to(cr, center_x, center_y - 15);
    cairo_line_to(cr, center_x, center_y - 5);
    cairo_move_to(cr, center_x, center_y + 5);
    cairo_line_to(cr, center_x, center_y + 15);
    cairo_stroke(cr);

    // --- Text Readouts (Top Left & Top Right) ---
    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 18.0);
    // Connection Status
    if (isTelemetryAlive) {
        cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
        cairo_move_to(cr, 30, 40);
        std::string hud_left = "ALT: " + std::to_string(static_cast<int>(data.alt)) + " m"
                               + " | BATT: " + std::to_string(data.batteryRemaining) + "%";
        cairo_show_text(cr, hud_left.c_str());

        // Heading / Roll / Pitch Readout
        cairo_move_to(cr, 30, 70);
        int heading = static_cast<int>(data.yaw * (180.0 / std::numbers::pi));
        if (heading < 0) {
            heading += 360;
        }
        std::string hud_nav = "HDG: " + std::to_string(heading) + " deg"
                              + " | PITCH: " + std::to_string(static_cast<int>(pitch_deg));
        cairo_show_text(cr, hud_nav.c_str());
    } else {
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
        cairo_move_to(cr, 30, 40);
        cairo_show_text(cr, "[MAVLINK LOST]");
    }
}
