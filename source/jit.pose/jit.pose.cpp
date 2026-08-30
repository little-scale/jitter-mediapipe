// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Sebastian Tomczak

#include "ext.h"
#include "ext_obex.h"
#include "ext_path.h"
#include "jit.common.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "mediapipe/tasks/c/core/common.h"
#include "mediapipe/tasks/c/vision/core/image.h"
#include "mediapipe/tasks/c/vision/pose_landmarker/pose_landmarker.h"

namespace {

constexpr int kMaximumPoses = 4;

struct Frame {
    int width = 0;
    int height = 0;
    int64_t timestamp_ms = 0;
    std::vector<uint8_t> rgb;
};

struct Point3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float visibility = 0.0f;
    float presence = 0.0f;
};

struct PoseResult {
    std::vector<Point3> normalized;
    std::vector<Point3> world;
};

struct Result {
    int64_t timestamp_ms = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> rgb;
    std::vector<PoseResult> poses;
};

struct RuntimeConfig {
    std::string model_path;
    int poses = 1;
    float detection_threshold = 0.5f;
    float presence_threshold = 0.5f;
    float tracking_threshold = 0.5f;
};

struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::thread worker;
    bool quit = false;
    bool active = true;
    bool restart_requested = true;
    bool has_pending_frame = false;
    bool ready = false;
    bool processing = false;
    Frame pending_frame;
    RuntimeConfig config;
    Result latest_result;
    uint64_t result_serial = 0;
    uint64_t status_serial = 0;
    std::string status_text = "initializing";
    int status_code = 0;
    uint64_t frames_received = 0;
    uint64_t frames_processed = 0;
    uint64_t frames_dropped = 0;
    int64_t last_timestamp_ms = 0;
};

typedef struct _jit_pose {
    t_object object;
    void* landmarks_outlet = nullptr;
    void* world_outlet = nullptr;
    void* metadata_outlet = nullptr;
    void* annotated_outlet = nullptr;
    void* annotated_matrix = nullptr;
    t_symbol* annotated_matrix_name = nullptr;
    bool annotated_matrix_error_reported = false;
    bool show_background = false;
    bool mirror = true;
    t_qelem* output_qelem = nullptr;
    State* state = nullptr;
    uint64_t emitted_result_serial = 0;
    uint64_t emitted_status_serial = 0;
} t_jit_pose;

t_class* g_class = nullptr;

std::string consume_error(char* error_message, MpStatus status) {
    std::string text = error_message ? error_message : "MediaPipe error";
    if (error_message) MpErrorFree(error_message);
    if (text.empty()) text = "MediaPipe error " + std::to_string(status);
    return text;
}

bool resolve_file(const char* requested, std::string& resolved) {
    if (!requested || !requested[0]) return false;
    char filename[MAX_PATH_CHARS]{};
    strncpy_zero(filename, requested, MAX_PATH_CHARS);
    short path_id = 0;
    t_fourcc out_type = 0;
    if (locatefile_extended(filename, &path_id, &out_type, nullptr, 0)) return false;
    char absolute_path[MAX_PATH_CHARS]{};
    if (path_toabsolutesystempath(path_id, filename, absolute_path) != MAX_ERR_NONE)
        return false;
    resolved = absolute_path;
    return !resolved.empty();
}

void publish_status(t_jit_pose* x, bool ready, int code, std::string text) {
    if (!x || !x->state) return;
    {
        std::lock_guard<std::mutex> lock(x->state->mutex);
        x->state->ready = ready;
        x->state->status_code = code;
        x->state->status_text = std::move(text);
        ++x->state->status_serial;
    }
    if (x->output_qelem) qelem_set(x->output_qelem);
}

Result copy_result(const MpPoseLandmarkerResult& source, Frame&& frame) {
    Result destination;
    destination.timestamp_ms = frame.timestamp_ms;
    destination.width = frame.width;
    destination.height = frame.height;
    destination.rgb = std::move(frame.rgb);
    const uint32_t count =
        std::max(source.pose_landmarks_count, source.pose_world_landmarks_count);
    destination.poses.resize(count);
    for (uint32_t pose = 0; pose < count; ++pose) {
        auto& output = destination.poses[pose];
        if (pose < source.pose_landmarks_count) {
            const auto& landmarks = source.pose_landmarks[pose];
            output.normalized.reserve(landmarks.landmarks_count);
            for (uint32_t i = 0; i < landmarks.landmarks_count; ++i) {
                const auto& p = landmarks.landmarks[i];
                output.normalized.push_back(
                    {p.x, p.y, p.z, p.has_visibility ? p.visibility : 0.0f,
                     p.has_presence ? p.presence : 0.0f});
            }
        }
        if (pose < source.pose_world_landmarks_count) {
            const auto& landmarks = source.pose_world_landmarks[pose];
            output.world.reserve(landmarks.landmarks_count);
            for (uint32_t i = 0; i < landmarks.landmarks_count; ++i) {
                const auto& p = landmarks.landmarks[i];
                output.world.push_back(
                    {p.x, p.y, p.z, p.has_visibility ? p.visibility : 0.0f,
                     p.has_presence ? p.presence : 0.0f});
            }
        }
    }
    return destination;
}

MpPoseLandmarkerPtr create_landmarker(const RuntimeConfig& config,
                                      std::string& error_text, int& error_code) {
    MpPoseLandmarkerOptions options{};
    options.base_options.model_asset_path = config.model_path.c_str();
    options.base_options.delegate = MP_DELEGATE_CPU;
    options.base_options.host_environment = MP_HOST_ENVIRONMENT_UNKNOWN;
    options.base_options.host_system = MP_HOST_SYSTEM_MAC;
    options.base_options.app_id = "jit.pose";
    options.base_options.app_version = "1.1.0";
    options.running_mode = MP_RUNNING_MODE_VIDEO;
    options.num_poses = config.poses;
    options.min_pose_detection_confidence = config.detection_threshold;
    options.min_pose_presence_confidence = config.presence_threshold;
    options.min_tracking_confidence = config.tracking_threshold;
    options.output_segmentation_masks = false;
    MpPoseLandmarkerPtr landmarker = nullptr;
    char* message = nullptr;
    const MpStatus status = MpPoseLandmarkerCreate(&options, &landmarker, &message);
    if (status != kMpOk) {
        error_code = status;
        error_text = consume_error(message, status);
        return nullptr;
    }
    return landmarker;
}

void close_landmarker(MpPoseLandmarkerPtr& landmarker) {
    if (!landmarker) return;
    char* message = nullptr;
    MpPoseLandmarkerClose(landmarker, &message);
    if (message) MpErrorFree(message);
    landmarker = nullptr;
}

void worker_main(t_jit_pose* x) {
    State* state = x->state;
    MpPoseLandmarkerPtr landmarker = nullptr;
    for (;;) {
        Frame frame;
        RuntimeConfig config;
        bool restart = false;
        {
            std::unique_lock<std::mutex> lock(state->mutex);
            state->wake.wait(lock, [&] {
                return state->quit || state->restart_requested ||
                       (state->active && state->has_pending_frame);
            });
            if (state->quit) break;
            if (state->restart_requested) {
                restart = true;
                state->restart_requested = false;
                state->has_pending_frame = false;
                state->ready = false;
                state->processing = false;
                config = state->config;
            } else {
                frame = std::move(state->pending_frame);
                state->has_pending_frame = false;
                state->processing = true;
            }
        }
        if (restart) {
            close_landmarker(landmarker);
            if (config.model_path.empty()) {
                publish_status(x, false, kMpNotFound,
                               "pose_landmarker_lite.task was not found on Max's search path");
                continue;
            }
            int code = 0;
            std::string error;
            landmarker = create_landmarker(config, error, code);
            if (!landmarker) {
                publish_status(x, false, code, std::move(error));
                continue;
            }
            publish_status(x, true, 0, "ready");
            continue;
        }
        if (!landmarker) {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->processing = false;
            continue;
        }
        char* message = nullptr;
        MpImagePtr image = nullptr;
        MpStatus status = MpImageCreateFromUint8Data(
            kMpImageFormatSrgb, frame.width, frame.height, frame.rgb.data(),
            static_cast<int>(frame.rgb.size()), &image, &message);
        if (status != kMpOk) {
            const std::string error = consume_error(message, status);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->processing = false;
            }
            publish_status(x, true, status, error);
            continue;
        }
        MpPoseLandmarkerResult media_result{};
        message = nullptr;
        status = MpPoseLandmarkerDetectForVideo(
            landmarker, image, nullptr, frame.timestamp_ms, &media_result, &message);
        MpImageFree(image);
        if (status != kMpOk) {
            const std::string error = consume_error(message, status);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->processing = false;
            }
            publish_status(x, true, status, error);
            continue;
        }
        Result result = copy_result(media_result, std::move(frame));
        MpPoseLandmarkerCloseResult(&media_result);
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            state->latest_result = std::move(result);
            state->processing = false;
            ++state->frames_processed;
            ++state->result_serial;
        }
        if (x->output_qelem) qelem_set(x->output_qelem);
    }
    close_landmarker(landmarker);
}

struct Color { uint8_t r, g, b; };
constexpr Color kColors[] = {
    {35, 255, 95}, {255, 70, 210}, {40, 220, 255}, {255, 205, 35},
};
constexpr int kConnections[][2] = {
    {0,4},{4,5},{5,6},{6,8},{0,1},{1,2},{2,3},{3,7},{10,9},{12,11},
    {12,14},{14,16},{16,18},{16,20},{16,22},{18,20},{11,13},{13,15},
    {15,17},{15,19},{15,21},{17,19},{12,24},{11,23},{24,23},{24,26},
    {23,25},{26,28},{25,27},{28,30},{27,29},{30,32},{29,31},{28,32},
    {27,31},
};

void pixel(uint8_t* data, const t_jit_matrix_info& info, int x, int y, Color c) {
    if (x < 0 || y < 0 || x >= info.dim[0] || y >= info.dim[1]) return;
    uint8_t* p = data + static_cast<long>(y) * info.dimstride[1] +
                 static_cast<long>(x) * info.dimstride[0];
    p[0] = 255; p[1] = c.r; p[2] = c.g; p[3] = c.b;
}

void disc(uint8_t* data, const t_jit_matrix_info& info, int cx, int cy,
          int radius, Color color) {
    const int rr = radius * radius;
    for (int y = -radius; y <= radius; ++y)
        for (int x = -radius; x <= radius; ++x)
            if (x * x + y * y <= rr) pixel(data, info, cx + x, cy + y, color);
}

void line(uint8_t* data, const t_jit_matrix_info& info, int x0, int y0,
          int x1, int y1, int radius, Color color) {
    const int dx = std::abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    const int dy = -std::abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        disc(data, info, x0, y0, radius, color);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * error;
        if (e2 >= dy) { error += dy; x0 += sx; }
        if (e2 <= dx) { error += dx; y0 += sy; }
    }
}

bool point_to_pixel(const Point3& p, int width, int height, int& x, int& y) {
    if (!std::isfinite(p.x) || !std::isfinite(p.y) || p.x < -0.25f ||
        p.x > 1.25f || p.y < -0.25f || p.y > 1.25f) return false;
    x = static_cast<int>(std::lround(std::clamp(p.x, 0.0f, 1.0f) * (width - 1)));
    y = static_cast<int>(std::lround(std::clamp(p.y, 0.0f, 1.0f) * (height - 1)));
    return true;
}

bool configure_matrix(void* matrix, int width, int height) {
    if (!matrix || width <= 0 || height <= 0) return false;
    const long old_lock = reinterpret_cast<long>(
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(1)));
    t_jit_matrix_info current{};
    jit_object_method(matrix, _jit_sym_getinfo, &current);
    bool ok = true;
    if (current.type != _jit_sym_char || current.planecount != 4 ||
        current.dimcount != 2 || current.dim[0] != width || current.dim[1] != height) {
        t_jit_matrix_info desired{};
        jit_matrix_info_default(&desired);
        desired.type = _jit_sym_char;
        desired.planecount = 4;
        desired.dimcount = 2;
        desired.dim[0] = width;
        desired.dim[1] = height;
        desired.flags = 0;
        ok = reinterpret_cast<t_jit_err>(
                 jit_object_method(matrix, _jit_sym_setinfo, &desired)) == JIT_ERR_NONE;
    }
    jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(old_lock));
    return ok;
}

bool render_matrix(void* matrix, const Result& result, bool show_background) {
    if (result.rgb.size() != static_cast<size_t>(result.width) * result.height * 3 ||
        !configure_matrix(matrix, result.width, result.height)) return false;
    const long old_lock = reinterpret_cast<long>(
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(1)));
    t_jit_matrix_info info{};
    uint8_t* data = nullptr;
    jit_object_method(matrix, _jit_sym_getinfo, &info);
    jit_object_method(matrix, _jit_sym_getdata, &data);
    if (!data) {
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(old_lock));
        return false;
    }
    if (show_background) {
        for (int y = 0; y < result.height; ++y) {
            const uint8_t* src = result.rgb.data() + static_cast<size_t>(y) * result.width * 3;
            for (int x = 0; x < result.width; ++x) {
                uint8_t* dst = data + static_cast<long>(y) * info.dimstride[1] +
                               static_cast<long>(x) * info.dimstride[0];
                dst[0] = 255; dst[1] = src[x * 3]; dst[2] = src[x * 3 + 1];
                dst[3] = src[x * 3 + 2];
            }
        }
    }
    else {
        for (int y = 0; y < result.height; ++y)
            std::memset(data + static_cast<long>(y) * info.dimstride[1], 0,
                        static_cast<size_t>(info.dimstride[1]));
    }
    const int edge = std::min(result.width, result.height);
    const int line_radius = std::max(1, edge / 360);
    const int joint_radius = std::max(3, edge / 120);
    for (size_t h = 0; h < result.poses.size(); ++h) {
        const auto& points = result.poses[h].normalized;
        if (points.size() < 33) continue;
        const Color color = kColors[h % 4];
        for (const auto& connection : kConnections) {
            int x0, y0, x1, y1;
            if (point_to_pixel(points[connection[0]], result.width, result.height, x0, y0) &&
                point_to_pixel(points[connection[1]], result.width, result.height, x1, y1))
                line(data, info, x0, y0, x1, y1, line_radius, color);
        }
        for (const auto& point : points) {
            int x, y;
            if (point_to_pixel(point, result.width, result.height, x, y)) {
                disc(data, info, x, y, joint_radius, color);
                disc(data, info, x, y, std::max(1, joint_radius / 2), {255,255,255});
            }
        }
    }
    jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(old_lock));
    return true;
}

bool ensure_annotated_matrix(t_jit_pose* x, int width, int height) {
    if (!x || width <= 0 || height <= 0) return false;
    if (x->annotated_matrix) return true;

    t_jit_matrix_info info{};
    jit_matrix_info_default(&info);
    info.type = _jit_sym_char;
    info.planecount = 4;
    info.dimcount = 2;
    info.dim[0] = width;
    info.dim[1] = height;

    void* matrix = jit_object_new(gensym("jit_matrix"), &info);
    if (!matrix) {
        if (!x->annotated_matrix_error_reported) {
            object_error(reinterpret_cast<t_object*>(x),
                         "could not allocate annotated matrix after receiving a Jitter frame");
            x->annotated_matrix_error_reported = true;
        }
        return false;
    }

    t_symbol* name = jit_symbol_unique();
    void* registered = name ? jit_object_register(matrix, name) : nullptr;
    if (!registered) {
        jit_object_free(matrix);
        if (!x->annotated_matrix_error_reported) {
            object_error(reinterpret_cast<t_object*>(x),
                         "could not register annotated matrix");
            x->annotated_matrix_error_reported = true;
        }
        return false;
    }

    x->annotated_matrix = registered;
    x->annotated_matrix_name = name;
    x->annotated_matrix_error_reported = false;
    return true;
}

void output_point_list(void* outlet, t_symbol* selector, long pose_index,
                       const std::vector<Point3>& points) {
    std::vector<t_atom> atoms(1 + points.size() * 5);
    atom_setlong(atoms.data(), pose_index);
    for (size_t i = 0; i < points.size(); ++i) {
        atom_setfloat(atoms.data() + 1 + i * 5, points[i].x);
        atom_setfloat(atoms.data() + 2 + i * 5, points[i].y);
        atom_setfloat(atoms.data() + 3 + i * 5, points[i].z);
        atom_setfloat(atoms.data() + 4 + i * 5, points[i].visibility);
        atom_setfloat(atoms.data() + 5 + i * 5, points[i].presence);
    }
    outlet_anything(outlet, selector, static_cast<short>(atoms.size()), atoms.data());
}

void output_status(t_jit_pose* x, bool force) {
    bool ready, active, processing;
    int code;
    uint64_t received, processed, dropped, serial;
    std::string text;
    {
        std::lock_guard<std::mutex> lock(x->state->mutex);
        serial = x->state->status_serial;
        if (!force && serial == x->emitted_status_serial) return;
        ready = x->state->ready; active = x->state->active;
        processing = x->state->processing; code = x->state->status_code;
        received = x->state->frames_received; processed = x->state->frames_processed;
        dropped = x->state->frames_dropped; text = x->state->status_text;
    }
    x->emitted_status_serial = serial;
    t_atom atoms[8];
    atom_setlong(atoms, ready); atom_setlong(atoms + 1, active);
    atom_setlong(atoms + 2, processing);
    atom_setlong(atoms + 3, static_cast<t_atom_long>(received));
    atom_setlong(atoms + 4, static_cast<t_atom_long>(processed));
    atom_setlong(atoms + 5, static_cast<t_atom_long>(dropped));
    atom_setlong(atoms + 6, code); atom_setsym(atoms + 7, gensym(text.c_str()));
    outlet_anything(x->metadata_outlet, gensym("status"), 8, atoms);
}

void output_qelem_method(t_jit_pose* x) {
    if (!x || !x->state) return;
    output_status(x, false);
    Result result;
    uint64_t serial;
    {
        std::lock_guard<std::mutex> lock(x->state->mutex);
        serial = x->state->result_serial;
        if (serial == x->emitted_result_serial) return;
        result = std::move(x->state->latest_result);
    }
    x->emitted_result_serial = serial;
    if (ensure_annotated_matrix(x, result.width, result.height) &&
        render_matrix(x->annotated_matrix, result, x->show_background)) {
        t_atom matrix_atom;
        atom_setsym(&matrix_atom, x->annotated_matrix_name);
        outlet_anything(x->annotated_outlet, gensym("jit_matrix"), 1, &matrix_atom);
    }
    t_atom poses_atoms[2];
    atom_setlong(poses_atoms, static_cast<long>(result.poses.size()));
    atom_setlong(poses_atoms + 1, static_cast<t_atom_long>(result.timestamp_ms));
    outlet_anything(x->metadata_outlet, gensym("poses"), 2, poses_atoms);
    if (result.poses.empty()) {
        t_atom timestamp;
        atom_setlong(&timestamp, static_cast<t_atom_long>(result.timestamp_ms));
        outlet_anything(x->world_outlet, gensym("clear"), 1, &timestamp);
        outlet_anything(x->landmarks_outlet, gensym("clear"), 1, &timestamp);
        return;
    }
    for (size_t pose = result.poses.size(); pose-- > 0;) {
        const auto& item = result.poses[pose];
        output_point_list(x->world_outlet, gensym("world"), pose, item.world);
        output_point_list(x->landmarks_outlet, gensym("landmarks"), pose,
                          item.normalized);
    }
}

bool copy_jitter_to_rgb(void* matrix, Frame& frame, bool mirror, std::string& error) {
    if (!matrix || !jit_object_method(matrix, _jit_sym_class_jit_matrix)) {
        error = "unknown Jitter matrix"; return false;
    }
    const long old_lock = reinterpret_cast<long>(
        jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(1)));
    t_jit_matrix_info info{};
    uint8_t* data = nullptr;
    jit_object_method(matrix, _jit_sym_getinfo, &info);
    jit_object_method(matrix, _jit_sym_getdata, &data);
    bool ok = false;
    if (!data) error = "matrix has no pixel data";
    else if (info.type != _jit_sym_char || info.dimcount != 2)
        error = "expected a 2D char matrix";
    else if (info.planecount != 1 && info.planecount != 3 && info.planecount != 4)
        error = "expected 1-plane gray, 3-plane RGB, or 4-plane ARGB";
    else if (info.dim[0] <= 0 || info.dim[1] <= 0) error = "invalid matrix dimensions";
    else {
        frame.width = static_cast<int>(info.dim[0]);
        frame.height = static_cast<int>(info.dim[1]);
        frame.rgb.resize(static_cast<size_t>(frame.width) * frame.height * 3);
        for (int y = 0; y < frame.height; ++y) {
            const uint8_t* src = data + static_cast<long>(y) * info.dimstride[1];
            uint8_t* dst = frame.rgb.data() + static_cast<size_t>(y) * frame.width * 3;
            for (int x = 0; x < frame.width; ++x) {
                const int source_x = mirror ? frame.width - 1 - x : x;
                const uint8_t* p = src + static_cast<long>(source_x) * info.dimstride[0];
                const int offset = info.planecount == 4 ? 1 : 0;
                if (info.planecount == 1) dst[x*3] = dst[x*3+1] = dst[x*3+2] = p[0];
                else { dst[x*3] = p[offset]; dst[x*3+1] = p[offset+1]; dst[x*3+2] = p[offset+2]; }
            }
        }
        ok = true;
    }
    jit_object_method(matrix, _jit_sym_lock, reinterpret_cast<void*>(old_lock));
    return ok;
}

void matrix_method(t_jit_pose* x, t_symbol*, long argc, t_atom* argv) {
    if (!x || !x->state || argc < 1 || !argv) return;
    {
        std::lock_guard<std::mutex> lock(x->state->mutex);
        if (!x->state->active) return;
    }
    Frame frame;
    std::string error;
    if (!copy_jitter_to_rgb(jit_object_findregistered(atom_getsym(argv)), frame,
                            x->mirror, error)) {
        object_error(reinterpret_cast<t_object*>(x), "%s", error.c_str()); return;
    }
    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    {
        std::lock_guard<std::mutex> lock(x->state->mutex);
        frame.timestamp_ms = std::max(now, x->state->last_timestamp_ms + 1);
        x->state->last_timestamp_ms = frame.timestamp_ms;
        ++x->state->frames_received;
        if (x->state->has_pending_frame) ++x->state->frames_dropped;
        x->state->pending_frame = std::move(frame);
        x->state->has_pending_frame = true;
    }
    x->state->wake.notify_one();
}

void start_method(t_jit_pose* x) {
    if (!x || !x->state) return;
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->active = true; ++x->state->status_serial; }
    x->state->wake.notify_one(); qelem_set(x->output_qelem);
}
void stop_method(t_jit_pose* x) {
    if (!x || !x->state) return;
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->active = false; x->state->has_pending_frame = false; ++x->state->status_serial; }
    qelem_set(x->output_qelem);
}
void restart(t_jit_pose* x) {
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->restart_requested = true; x->state->ready = false; x->state->status_text = "initializing"; x->state->status_code = 0; ++x->state->status_serial; }
    x->state->wake.notify_one(); qelem_set(x->output_qelem);
}
void reset_method(t_jit_pose* x) { if (x && x->state) restart(x); }
void model_method(t_jit_pose* x, t_symbol* model) {
    if (!x || !x->state || !model) return;
    std::string path;
    if (!resolve_file(model->s_name, path)) { object_error(reinterpret_cast<t_object*>(x), "model file not found: %s", model->s_name); return; }
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->config.model_path = std::move(path); }
    restart(x);
}
void poses_method(t_jit_pose* x, long poses) {
    if (!x || !x->state) return;
    if (poses < 1 || poses > kMaximumPoses) { object_error(reinterpret_cast<t_object*>(x), "poses must be 1-4"); return; }
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->config.poses = static_cast<int>(poses); }
    restart(x);
}
void thresholds_method(t_jit_pose* x, t_symbol*, long argc, t_atom* argv) {
    if (!x || !x->state || argc != 3) return;
    const float d = atom_getfloat(argv), p = atom_getfloat(argv + 1), t = atom_getfloat(argv + 2);
    if (d < 0 || d > 1 || p < 0 || p > 1 || t < 0 || t > 1) { object_error(reinterpret_cast<t_object*>(x), "thresholds must be between 0 and 1"); return; }
    { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->config.detection_threshold = d; x->state->config.presence_threshold = p; x->state->config.tracking_threshold = t; }
    restart(x);
}
void status_method(t_jit_pose* x) { if (x && x->state) output_status(x, true); }
void background_method(t_jit_pose* x, long enabled) {
    if (x) x->show_background = enabled != 0;
}
void mirror_method(t_jit_pose* x, long enabled) {
    if (x) x->mirror = enabled != 0;
}

void assist_method(t_jit_pose*, void*, long message, long index, char* text) {
    if (message == ASSIST_INLET) { std::snprintf(text, 512, "Jitter matrix and control messages"); return; }
    static const char* labels[] = {"normalized landmarks", "world landmarks", "metadata", "annotated ARGB matrix"};
    std::snprintf(text, 512, "%s", labels[std::clamp<long>(index, 0, 3)]);
}

void free_method(t_jit_pose* x) {
    if (!x) return;
    if (x->state) {
        { std::lock_guard<std::mutex> lock(x->state->mutex); x->state->quit = true; }
        x->state->wake.notify_one();
        if (x->state->worker.joinable()) x->state->worker.join();
    }
    if (x->output_qelem) qelem_free(x->output_qelem);
    if (x->annotated_matrix) {
        jit_object_unregister(x->annotated_matrix);
        jit_object_free(x->annotated_matrix);
    }
    delete x->state;
}

void* new_method(t_symbol*, long argc, t_atom* argv) {
    auto* x = static_cast<t_jit_pose*>(object_alloc(g_class));
    if (!x) return nullptr;
    x->landmarks_outlet = x->world_outlet = x->metadata_outlet = nullptr;
    x->annotated_outlet = x->annotated_matrix = nullptr;
    x->annotated_matrix_name = nullptr;
    x->annotated_matrix_error_reported = false;
    x->show_background = false;
    x->mirror = true;
    x->output_qelem = nullptr;
    x->state = new State(); x->emitted_result_serial = x->emitted_status_serial = 0;
    if (argc > 0 && (atom_gettype(argv) == A_LONG || atom_gettype(argv) == A_FLOAT))
        x->state->config.poses = std::clamp<int>(atom_getlong(argv), 1, 4);
    resolve_file("pose_landmarker_lite.task", x->state->config.model_path);
    x->annotated_outlet = outlet_new(x, "jit_matrix");
    x->metadata_outlet = outlet_new(x, nullptr);
    x->world_outlet = outlet_new(x, nullptr);
    x->landmarks_outlet = outlet_new(x, nullptr);
    x->output_qelem = static_cast<t_qelem*>(qelem_new(x, reinterpret_cast<method>(output_qelem_method)));
    if (!x->output_qelem) { object_free(reinterpret_cast<t_object*>(x)); return nullptr; }
    x->state->worker = std::thread(worker_main, x);
    return x;
}

}  // namespace

void ext_main(void*) {
    t_class* klass = class_new("jit.pose", reinterpret_cast<method>(new_method),
        reinterpret_cast<method>(free_method), sizeof(t_jit_pose), nullptr, A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(matrix_method), "jit_matrix", A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(start_method), "start", 0);
    class_addmethod(klass, reinterpret_cast<method>(stop_method), "stop", 0);
    class_addmethod(klass, reinterpret_cast<method>(reset_method), "reset", 0);
    class_addmethod(klass, reinterpret_cast<method>(model_method), "model", A_SYM, 0);
    class_addmethod(klass, reinterpret_cast<method>(poses_method), "poses", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(thresholds_method), "thresholds", A_GIMME, 0);
    class_addmethod(klass, reinterpret_cast<method>(background_method), "background", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(mirror_method), "mirror", A_LONG, 0);
    class_addmethod(klass, reinterpret_cast<method>(status_method), "status", 0);
    class_addmethod(klass, reinterpret_cast<method>(assist_method), "assist", A_CANT, 0);
    class_register(CLASS_BOX, klass); g_class = klass;
}
