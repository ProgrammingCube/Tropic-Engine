#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

extern "C" {
#include "tropic.h"
#include "beat_grid_runtime.h"
#include "level_loader.h"
}

namespace
{
    const size_t kNoSelection = std::numeric_limits<size_t>::max();
    const float kEditorMoveStep = 0.5f;
    const float kEditorRotateStep = 15.0f;
    const float kEditorScaleStep = 0.25f;
    const float kPreviewDefaultGravity = 9.81f;
    const float kEditorCameraPanSpeed = 18.0f;
    const float kEditorCameraFastPanMultiplier = 2.0f;
    const float kEditorCameraButtonStep = 2.0f;
    const float kEditorMouseOrbitSpeed = 0.35f;
    const float kEditorMousePanSpeed = 0.02f;
    const float kEditorGizmoAxisPixelHitRadius = 14.0f;
    const float kEditorGizmoRotateDegreesPerPixel = 0.5f;

    unsigned char g_keys[GLFW_KEY_LAST + 1] = { 0 };
    unsigned char g_prev_keys[GLFW_KEY_LAST + 1] = { 0 };

    enum class EditorMode
    {
        Edit,
        Preview,
    };

    enum class EditorTool
    {
        Move,
        Rotate,
        Scale,
    };

    enum class EditorAxis
    {
        X = 0,
        Y = 1,
        Z = 2,
    };

    struct EditorLevelMetadata
    {
        std::string game_title;
        std::string level_name;
        std::string music_path;
        double play_speed;
        TropicBeatGridSettings beat_grid;
    };

    struct EditorObject
    {
        ObjectType type;
        std::string type_name;
        std::string uid;
        vec3 position;
        vec3 scale;
        vec3 rotation;
        TropicTrackPlacement placement;
        TropicEventSpec event;
    };

    struct EditorMaterialUniforms
    {
        vec3 color;
        float neon_amount;
        float brightness_scale;
        float alpha_scale;
    };

    struct EditorRenderResources
    {
        MeshID cube_mesh;
        ShaderID volume_shader;
        ShaderID player_shader;
        MaterialID platform_material;
        MaterialID spike_material;
        MaterialID jumppad_material;
        MaterialID event_material;
        MaterialID selected_material;
        MaterialID player_material;
        MaterialID gizmo_x_material;
        MaterialID gizmo_y_material;
        MaterialID gizmo_z_material;
        EditorMaterialUniforms platform_uniforms;
        EditorMaterialUniforms spike_uniforms;
        EditorMaterialUniforms jumppad_uniforms;
        EditorMaterialUniforms event_uniforms;
        EditorMaterialUniforms selected_uniforms;
        EditorMaterialUniforms player_uniforms;
        EditorMaterialUniforms gizmo_x_uniforms;
        EditorMaterialUniforms gizmo_y_uniforms;
        EditorMaterialUniforms gizmo_z_uniforms;
    };

    struct PreviewEventTriggerState
    {
        ObjectID player_id;
    };

    struct PreviewConfig
    {
        float move_speed;
        float forward_speed;
        float jump_speed;
        float fixed_delta;
        float jump_buffer_time;
        float coyote_time;
    };

    struct PreviewLoopState
    {
        double speed_adjust_timer;
        double last_time;
        double physics_accumulator;
        float jump_buffer_timer;
        float coyote_timer;
        bool paused;
        bool jump_was_down;
        bool pause_was_down;
    };

    struct PreviewState
    {
        ObjectID player_id;
        PreviewConfig config;
        PreviewLoopState loop_state;
        std::vector<PreviewEventTriggerState> event_trigger_states;
        float current_exact_beat;
        TropicBeatTime current_beat_time;
    };

    struct EditorApp
    {
        TropicID engine;
        CameraID camera_id;
        EditorMode mode;
        EditorTool tool;
        EditorAxis axis;
        EditorLevelMetadata metadata;
        EditorRenderResources render;
        PreviewState preview;
        std::vector<EditorObject> objects;
        std::vector<TropicTrackAnchor> track_anchors;
        std::vector<ObjectID> object_ids;
        size_t selected_index;
        size_t selected_anchor_index;
        std::string level_path;
        bool show_beat_grid_debug;
        bool pending_mode_switch;
        EditorMode requested_mode;
        bool initialized;
        bool controls_printed;
        float orbit_yaw_degrees;
        float orbit_pitch_degrees;
        float orbit_distance;
        vec3 camera_focus_point;
        bool camera_focus_initialized;
        bool camera_focus_tracks_selection;
        bool mouse_left_down;
        bool mouse_middle_down;
        bool mouse_right_down;
        double mouse_last_x;
        double mouse_last_y;
        bool mouse_has_last_position;
        double gizmo_drag_last_x;
        double gizmo_drag_last_y;
        bool mouse_left_was_down;
        bool gizmo_drag_active;
        EditorAxis gizmo_drag_axis;
        ObjectID gizmo_axis_ids[3];
        ObjectID gizmo_tip_ids[3];
        bool ui_dirty;
        bool exit_requested;
    };

    bool has_selection(const EditorApp& app);
    void sync_selected_object(EditorApp& app);
    void mark_ui_dirty(EditorApp& app);

    void set_vec3(vec3 value, float x, float y, float z)
    {
        value[0] = x;
        value[1] = y;
        value[2] = z;
    }

    void copy_vec3(const vec3 source, vec3 destination)
    {
        destination[0] = source[0];
        destination[1] = source[1];
        destination[2] = source[2];
    }

    float vec3_length_squared(const vec3 value)
    {
        return value[0] * value[0] + value[1] * value[1] + value[2] * value[2];
    }

    void normalize_vec3(vec3 value)
    {
        float length_squared = vec3_length_squared(value);

        if (length_squared <= 0.000001f)
        {
            return;
        }

        length_squared = std::sqrt(length_squared);
        value[0] /= length_squared;
        value[1] /= length_squared;
        value[2] /= length_squared;
    }

    void cross_vec3(const vec3 a, const vec3 b, vec3 result)
    {
        result[0] = a[1] * b[2] - a[2] * b[1];
        result[1] = a[2] * b[0] - a[0] * b[2];
        result[2] = a[0] * b[1] - a[1] * b[0];
    }

    void mul_add_vec3(vec3 value, const vec3 direction, float scale)
    {
        value[0] += direction[0] * scale;
        value[1] += direction[1] * scale;
        value[2] += direction[2] * scale;
    }

    float vec2_length(double x, double y)
    {
        return static_cast<float>(std::sqrt(x * x + y * y));
    }

    float distance_point_to_segment(double point_x,
                                    double point_y,
                                    double start_x,
                                    double start_y,
                                    double end_x,
                                    double end_y)
    {
        const double segment_x = end_x - start_x;
        const double segment_y = end_y - start_y;
        const double segment_length_squared = segment_x * segment_x + segment_y * segment_y;

        if (segment_length_squared <= 0.000001)
        {
            return vec2_length(point_x - start_x, point_y - start_y);
        }

        double t = ((point_x - start_x) * segment_x + (point_y - start_y) * segment_y) / segment_length_squared;
        t = std::max(0.0, std::min(1.0, t));

        return vec2_length(point_x - (start_x + segment_x * t), point_y - (start_y + segment_y * t));
    }

    void rotate_vec3_euler_xyz(const vec3 rotation_degrees, vec3 value)
    {
        const float radians_x = rotation_degrees[0] * 3.14159265f / 180.0f;
        const float radians_y = rotation_degrees[1] * 3.14159265f / 180.0f;
        const float radians_z = rotation_degrees[2] * 3.14159265f / 180.0f;
        const float cos_x = std::cos(radians_x);
        const float sin_x = std::sin(radians_x);
        const float cos_y = std::cos(radians_y);
        const float sin_y = std::sin(radians_y);
        const float cos_z = std::cos(radians_z);
        const float sin_z = std::sin(radians_z);
        float rotated_x;
        float rotated_y;
        float rotated_z;

        rotated_x = value[0];
        rotated_y = value[1] * cos_x - value[2] * sin_x;
        rotated_z = value[1] * sin_x + value[2] * cos_x;
        value[0] = rotated_x;
        value[1] = rotated_y;
        value[2] = rotated_z;

        rotated_x = value[0] * cos_y + value[2] * sin_y;
        rotated_y = value[1];
        rotated_z = -value[0] * sin_y + value[2] * cos_y;
        value[0] = rotated_x;
        value[1] = rotated_y;
        value[2] = rotated_z;

        rotated_x = value[0] * cos_z - value[1] * sin_z;
        rotated_y = value[0] * sin_z + value[1] * cos_z;
        rotated_z = value[2];
        value[0] = rotated_x;
        value[1] = rotated_y;
        value[2] = rotated_z;
    }

    void compute_object_axes(const EditorObject& object, vec3 axes[3])
    {
        set_vec3(axes[0], 1.0f, 0.0f, 0.0f);
        set_vec3(axes[1], 0.0f, 1.0f, 0.0f);
        set_vec3(axes[2], 0.0f, 0.0f, 1.0f);

        for (int i = 0; i < 3; ++i)
        {
            rotate_vec3_euler_xyz(object.rotation, axes[i]);
            normalize_vec3(axes[i]);
        }
    }

    bool get_camera_basis(const EditorApp& app,
                          vec3 out_right,
                          vec3 out_up,
                          vec3 out_forward,
                          float& out_vertical_world_units_per_pixel,
                          float* out_depth,
                          const float* world_point)
    {
        TropicCamera* camera = Tropic_getActiveCamera(app.engine);
        TropicWindowID* window = Tropic_getWindow(app.engine);
        vec3 forward;
        vec3 right;
        vec3 up;
        int width = 1;
        int height = 1;
        float depth = 1.0f;
        vec3 relative;
        const float* sample_point = world_point;

        if (!camera || !window)
        {
            return false;
        }

        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        forward[0] = camera->target[0] - camera->position[0];
        forward[1] = camera->target[1] - camera->position[1];
        forward[2] = camera->target[2] - camera->position[2];
        normalize_vec3(forward);
        cross_vec3(forward, camera->up, right);
        normalize_vec3(right);
        cross_vec3(right, forward, up);
        normalize_vec3(up);

        if (!sample_point)
        {
            sample_point = camera->target;
        }

        relative[0] = sample_point[0] - camera->position[0];
        relative[1] = sample_point[1] - camera->position[1];
        relative[2] = sample_point[2] - camera->position[2];
        depth = glm_vec3_dot(relative, forward);
        if (depth < 0.1f)
        {
            depth = 0.1f;
        }

        out_vertical_world_units_per_pixel = (2.0f * depth * std::tan(camera->fov * 3.14159265f / 360.0f)) / static_cast<float>(height);
        if (out_depth)
        {
            *out_depth = depth;
        }
        copy_vec3(right, out_right);
        copy_vec3(up, out_up);
        copy_vec3(forward, out_forward);
        return true;
    }

    bool project_world_to_screen(const EditorApp& app, const vec3 world_position, double& out_x, double& out_y)
    {
        TropicCamera* camera = Tropic_getActiveCamera(app.engine);
        TropicWindowID* window = Tropic_getWindow(app.engine);
        vec3 right;
        vec3 up;
        vec3 forward;
        float vertical_world_units_per_pixel = 0.0f;
        vec3 relative;
        float view_x;
        float view_y;
        float view_z;
        int width = 1;
        int height = 1;
        float half_height;
        float half_width;

        if (!camera || !window)
        {
            return false;
        }

        if (!get_camera_basis(app, right, up, forward, vertical_world_units_per_pixel, NULL, world_position))
        {
            return false;
        }

        glfwGetFramebufferSize(window, &width, &height);
        if (width <= 0 || height <= 0)
        {
            return false;
        }

        relative[0] = world_position[0] - camera->position[0];
        relative[1] = world_position[1] - camera->position[1];
        relative[2] = world_position[2] - camera->position[2];
        view_x = glm_vec3_dot(relative, right);
        view_y = glm_vec3_dot(relative, up);
        view_z = glm_vec3_dot(relative, forward);
        if (view_z <= 0.05f)
        {
            return false;
        }

        half_height = view_z * std::tan(camera->fov * 3.14159265f / 360.0f);
        half_width = half_height * (static_cast<float>(width) / static_cast<float>(height));
        if (half_width <= 0.000001f || half_height <= 0.000001f)
        {
            return false;
        }

        out_x = (static_cast<double>(view_x / half_width) * 0.5 + 0.5) * static_cast<double>(width);
        out_y = (0.5 - static_cast<double>(view_y / half_height) * 0.5) * static_cast<double>(height);
        return true;
    }

    bool key_down(int key)
    {
        return key >= 0 && key <= GLFW_KEY_LAST && g_keys[key] != 0;
    }

    bool key_pressed(int key)
    {
        return key >= 0 && key <= GLFW_KEY_LAST && g_keys[key] != 0 && g_prev_keys[key] == 0;
    }

    bool shift_down()
    {
        return key_down(GLFW_KEY_LEFT_SHIFT) || key_down(GLFW_KEY_RIGHT_SHIFT);
    }

    const char* object_type_name(ObjectType type)
    {
        switch (type)
        {
        case TYPE_PLATFORM: return "platform";
        case TYPE_SPIKE: return "spike";
        case TYPE_JUMPPAD: return "jumppad";
        case TYPE_EVENT: return "event";
        case TYPE_CUBE: return "cube";
        default: return "generic";
        }
    }

    const char* event_action_name(TropicEventActionType action_type)
    {
        switch (action_type)
        {
        case TROPIC_EVENT_ACTION_GRAVITY_SET: return "gravity_set";
        case TROPIC_EVENT_ACTION_GRAVITY_FLIP: return "gravity_flip";
        case TROPIC_EVENT_ACTION_WORLD_SPIN: return "world_spin";
        case TROPIC_EVENT_ACTION_CAMERA_SPIN: return "camera_spin";
        case TROPIC_EVENT_ACTION_CUSTOM: return "custom";
        case TROPIC_EVENT_ACTION_NONE:
        default:
            return "none";
        }
    }

    const char* event_trigger_name(TropicEventTriggerMode trigger_mode)
    {
        switch (trigger_mode)
        {
        case TROPIC_EVENT_TRIGGER_STAY: return "stay";
        case TROPIC_EVENT_TRIGGER_EXIT: return "exit";
        case TROPIC_EVENT_TRIGGER_ENTER:
        default:
            return "enter";
        }
    }

    TropicEventSpec make_default_event_spec()
    {
        TropicEventSpec spec;
        std::memset(&spec, 0, sizeof(spec));
        spec.action_type = TROPIC_EVENT_ACTION_GRAVITY_FLIP;
        spec.trigger_mode = TROPIC_EVENT_TRIGGER_ENTER;
        spec.trigger_once = true;
        spec.axis[2] = 1.0f;
        return spec;
    }

    bool uid_exists(const EditorApp& app, const std::string& uid)
    {
        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            if (app.objects[i].uid == uid)
            {
                return true;
            }
        }

        return false;
    }

    std::string make_unique_uid(const EditorApp& app, ObjectType type)
    {
        const std::string prefix = object_type_name(type);

        for (size_t index = 1; ; ++index)
        {
            std::ostringstream stream;
            stream << prefix << '_' << std::setw(3) << std::setfill('0') << index;
            if (!uid_exists(app, stream.str()))
            {
                return stream.str();
            }
        }
    }

    EditorObject make_editor_object(ObjectType type)
    {
        EditorObject object;
        object.type = type;
        object.type_name = object_type_name(type);
        object.uid.clear();
        set_vec3(object.position, 0.0f, 0.0f, 0.0f);
        set_vec3(object.rotation, 0.0f, 0.0f, 0.0f);
        std::memset(&object.placement, 0, sizeof(object.placement));
        object.placement.space = TROPIC_PLACEMENT_SPACE_WORLD;
        object.placement.snap_x = true;
        object.placement.snap_y = true;
        object.event = make_default_event_spec();

        switch (type)
        {
        case TYPE_PLATFORM:
            set_vec3(object.scale, 4.0f, 0.5f, 4.0f);
            break;
        case TYPE_JUMPPAD:
            set_vec3(object.scale, 1.0f, 0.2f, 1.0f);
            break;
        case TYPE_EVENT:
            set_vec3(object.scale, 2.0f, 2.0f, 2.0f);
            break;
        case TYPE_SPIKE:
        default:
            set_vec3(object.scale, 1.0f, 1.0f, 1.0f);
            break;
        }

        return object;
    }

    float editor_track_snap_step_x(const EditorApp& app)
    {
        return std::max(0.001f, app.metadata.beat_grid.snap_unit_x);
    }

    float editor_track_snap_step_y(const EditorApp& app)
    {
        return std::max(0.001f, app.metadata.beat_grid.snap_unit_y);
    }

    int editor_subdivisions_per_beat(const EditorApp& app)
    {
        return std::max(1, static_cast<int>(app.metadata.beat_grid.subdivisions_per_beat));
    }

    float snap_to_step(float value, float step)
    {
        if (step <= 0.0f)
        {
            return value;
        }

        return std::round(value / step) * step;
    }

    void normalize_beat_time(TropicBeatTime& time, int subdivisions_per_beat)
    {
        if (subdivisions_per_beat <= 0)
        {
            subdivisions_per_beat = 1;
        }

        if (time.substep >= subdivisions_per_beat || time.substep < 0)
        {
            int beat_adjust = time.substep / subdivisions_per_beat;
            int remainder = time.substep % subdivisions_per_beat;

            if (remainder < 0)
            {
                remainder += subdivisions_per_beat;
                --beat_adjust;
            }

            time.beat += beat_adjust;
            time.substep = remainder;
        }
    }

    void offset_beat_time(TropicBeatTime& time, int substep_delta, int subdivisions_per_beat)
    {
        time.substep += substep_delta;
        normalize_beat_time(time, subdivisions_per_beat);
    }

    float default_track_y_for_type(ObjectType type)
    {
        return type == TYPE_PLATFORM ? 0.0f : 1.0f;
    }

    void seed_track_placement_from_selection(EditorApp& app, EditorObject& object)
    {
        if (has_selection(app) && app.objects[app.selected_index].placement.space == TROPIC_PLACEMENT_SPACE_TRACK)
        {
            object.placement = app.objects[app.selected_index].placement;
            object.placement.time = app.objects[app.selected_index].placement.time;
            offset_beat_time(object.placement.time, editor_subdivisions_per_beat(app), editor_subdivisions_per_beat(app));
            object.placement.length_beats = app.objects[app.selected_index].placement.length_beats;
            object.placement.snap_x = app.objects[app.selected_index].placement.snap_x;
            object.placement.snap_y = app.objects[app.selected_index].placement.snap_y;
            object.position[0] = app.objects[app.selected_index].position[0];
            object.position[1] = app.objects[app.selected_index].position[1];
            object.position[2] = app.objects[app.selected_index].position[2];
            return;
        }

        object.placement.space = TROPIC_PLACEMENT_SPACE_TRACK;
        object.placement.time.beat = 0;
        object.placement.time.substep = 0;
        object.placement.track_x = 0.0f;
        object.placement.track_y = default_track_y_for_type(object.type);
        object.placement.snap_x = true;
        object.placement.snap_y = true;
        object.placement.length_beats = 0.0f;
    }

    bool nudge_selected_track_placement(EditorApp& app, int axis, float delta)
    {
        EditorObject& object = app.objects[app.selected_index];

        if (object.placement.space != TROPIC_PLACEMENT_SPACE_TRACK)
        {
            return false;
        }

        switch (axis)
        {
        case 0:
            object.placement.track_x += (delta < 0.0f ? -editor_track_snap_step_x(app) : editor_track_snap_step_x(app));
            if (object.placement.snap_x)
            {
                object.placement.track_x = snap_to_step(object.placement.track_x, editor_track_snap_step_x(app));
            }
            break;
        case 1:
            object.placement.track_y += (delta < 0.0f ? -editor_track_snap_step_y(app) : editor_track_snap_step_y(app));
            if (object.placement.snap_y)
            {
                object.placement.track_y = snap_to_step(object.placement.track_y, editor_track_snap_step_y(app));
            }
            break;
        case 2:
            offset_beat_time(object.placement.time, delta < 0.0f ? -1 : 1, editor_subdivisions_per_beat(app));
            break;
        default:
            return false;
        }

        if (app.initialized)
        {
            sync_selected_object(app);
        }
        else
        {
            mark_ui_dirty(app);
        }

        return true;
    }

    bool stamp_selected_object_to_current_preview_beat(EditorApp& app)
    {
        if (!has_selection(app))
        {
            return false;
        }

        app.objects[app.selected_index].placement.space = TROPIC_PLACEMENT_SPACE_TRACK;
        app.objects[app.selected_index].placement.time = app.preview.current_beat_time;
        app.objects[app.selected_index].placement.snap_x = true;
        app.objects[app.selected_index].placement.snap_y = true;

        if (app.initialized)
        {
            sync_selected_object(app);
        }
        else
        {
            mark_ui_dirty(app);
        }

        return true;
    }

    TropicTrackAnchor make_default_track_anchor()
    {
        TropicTrackAnchor anchor = {};
        anchor.start_time.beat = 0;
        anchor.start_time.substep = 0;
        anchor.pivot_x = 0.0f;
        anchor.pivot_y = 0.0f;
        anchor.pivot_beat = 0.0f;
        set_vec3(anchor.local_axis, 0.0f, 1.0f, 0.0f);
        anchor.degrees = -90.0f;
        return anchor;
    }

    std::string format_number(double value)
    {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(3) << value;
        return stream.str();
    }

    std::string describe_track_anchor_list_item(const TropicTrackAnchor& anchor)
    {
        std::ostringstream stream;
        stream << "beat " << anchor.start_time.beat;
        if (anchor.start_time.substep != 0)
        {
            stream << '.' << anchor.start_time.substep;
        }
        stream << " pivot(" << format_number(anchor.pivot_x)
               << ", " << format_number(anchor.pivot_y)
               << ", " << format_number(anchor.pivot_beat)
               << ") rot " << format_number(anchor.degrees);
        return stream.str();
    }

    bool uid_available_for_index(const EditorApp& app, const std::string& uid, size_t ignore_index)
    {
        if (uid.empty())
        {
            return false;
        }

        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            if (i != ignore_index && app.objects[i].uid == uid)
            {
                return false;
            }
        }

        return true;
    }

#ifdef _WIN32
    enum EditorControlId
    {
        IDC_METADATA_GAME_TITLE = 1000,
        IDC_METADATA_LEVEL_NAME,
        IDC_METADATA_MUSIC_PATH,
        IDC_METADATA_PLAY_SPEED,
        IDC_METADATA_BPM,
        IDC_METADATA_SUBDIVISIONS,
        IDC_METADATA_UNITS_PER_BEAT,
        IDC_OBJECT_LIST,
        IDC_ADD_PLATFORM,
        IDC_ADD_SPIKE,
        IDC_ADD_JUMPPAD,
        IDC_ADD_EVENT,
        IDC_DUPLICATE_OBJECT,
        IDC_DELETE_OBJECT,
        IDC_SAVE_LEVEL,
        IDC_RELOAD_LEVEL,
        IDC_OPEN_LEVEL,
        IDC_SAVE_AS_LEVEL,
        IDC_TOGGLE_PREVIEW,
        IDC_APPLY_CHANGES,
        IDC_BROWSE_MUSIC,
        IDC_AUTO_UID,
        IDC_MOVE_X_NEG,
        IDC_MOVE_X_POS,
        IDC_MOVE_Y_NEG,
        IDC_MOVE_Y_POS,
        IDC_MOVE_Z_NEG,
        IDC_MOVE_Z_POS,
        IDC_UID_EDIT,
        IDC_POSITION_X,
        IDC_POSITION_Y,
        IDC_POSITION_Z,
        IDC_SCALE_X,
        IDC_SCALE_Y,
        IDC_SCALE_Z,
        IDC_ROTATION_X,
        IDC_ROTATION_Y,
        IDC_ROTATION_Z,
        IDC_EVENT_ACTION,
        IDC_EVENT_TRIGGER,
        IDC_EVENT_ONCE,
        IDC_EVENT_TARGET_UID,
        IDC_EVENT_FUNCTION,
        IDC_EVENT_GRAVITY_X,
        IDC_EVENT_GRAVITY_Y,
        IDC_EVENT_GRAVITY_Z,
        IDC_EVENT_AXIS_X,
        IDC_EVENT_AXIS_Y,
        IDC_EVENT_AXIS_Z,
        IDC_EVENT_DEGREES,
        IDC_EVENT_SPEED,
        IDC_EVENT_DURATION,
        IDC_PLACEMENT_SPACE,
        IDC_PLACEMENT_BEAT,
        IDC_PLACEMENT_SUBSTEP,
        IDC_PLACEMENT_TRACK_X,
        IDC_PLACEMENT_TRACK_Y,
        IDC_PLACEMENT_LENGTH_BEATS,
        IDC_ANCHOR_LIST,
        IDC_ADD_ANCHOR,
        IDC_DELETE_ANCHOR,
        IDC_ANCHOR_BEAT,
        IDC_ANCHOR_SUBSTEP,
        IDC_ANCHOR_PIVOT_X,
        IDC_ANCHOR_PIVOT_Y,
        IDC_ANCHOR_PIVOT_BEAT,
        IDC_ANCHOR_AXIS_X,
        IDC_ANCHOR_AXIS_Y,
        IDC_ANCHOR_AXIS_Z,
        IDC_ANCHOR_DEGREES,
        IDC_STAMP_TO_CURRENT_BEAT,
    };

    struct EditorPanelState
    {
        HWND window;
        HWND object_list;
        HWND metadata_game_title;
        HWND metadata_level_name;
        HWND metadata_music_path;
        HWND metadata_play_speed;
        HWND metadata_bpm;
        HWND metadata_subdivisions;
        HWND metadata_units_per_beat;
        HWND uid_edit;
        HWND add_platform_button;
        HWND add_spike_button;
        HWND add_jumppad_button;
        HWND add_event_button;
        HWND duplicate_button;
        HWND delete_button;
        HWND save_button;
        HWND reload_button;
        HWND open_button;
        HWND save_as_button;
        HWND preview_button;
        HWND apply_button;
        HWND browse_music_button;
        HWND auto_uid_button;
        HWND move_buttons[6];
        HWND position_edits[3];
        HWND scale_edits[3];
        HWND rotation_edits[3];
        HWND placement_space_combo;
        HWND placement_beat_edit;
        HWND placement_substep_edit;
        HWND placement_track_x_edit;
        HWND placement_track_y_edit;
        HWND placement_length_beats_edit;
        HWND stamp_to_current_beat_button;
        HWND anchor_list;
        HWND add_anchor_button;
        HWND delete_anchor_button;
        HWND anchor_beat_edit;
        HWND anchor_substep_edit;
        HWND anchor_pivot_x_edit;
        HWND anchor_pivot_y_edit;
        HWND anchor_pivot_beat_edit;
        HWND anchor_axis_edits[3];
        HWND anchor_degrees_edit;
        HWND event_action_combo;
        HWND event_trigger_combo;
        HWND event_once_check;
        HWND event_target_uid_edit;
        HWND event_function_edit;
        HWND event_gravity_edits[3];
        HWND event_axis_edits[3];
        HWND event_degrees_edit;
        HWND event_speed_edit;
        HWND event_duration_edit;
        bool suppress_events;
    };

    EditorPanelState g_editor_panel = {};
    EditorApp* g_editor_panel_app = NULL;
    WNDPROC g_editor_edit_proc = NULL;

    const char* kEditorActionLabels[] = {
        "none",
        "gravity_set",
        "gravity_flip",
        "world_spin",
        "camera_spin",
        "custom",
    };

    const TropicEventActionType kEditorActionTypes[] = {
        TROPIC_EVENT_ACTION_NONE,
        TROPIC_EVENT_ACTION_GRAVITY_SET,
        TROPIC_EVENT_ACTION_GRAVITY_FLIP,
        TROPIC_EVENT_ACTION_WORLD_SPIN,
        TROPIC_EVENT_ACTION_CAMERA_SPIN,
        TROPIC_EVENT_ACTION_CUSTOM,
    };

    const char* kEditorTriggerLabels[] = {
        "enter",
        "stay",
        "exit",
    };

    const TropicEventTriggerMode kEditorTriggerTypes[] = {
        TROPIC_EVENT_TRIGGER_ENTER,
        TROPIC_EVENT_TRIGGER_STAY,
        TROPIC_EVENT_TRIGGER_EXIT,
    };

    const char* kEditorPlacementSpaceLabels[] = {
        "world",
        "track",
    };

    LRESULT CALLBACK editor_edit_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
    {
        if (message == WM_KEYDOWN && w_param == VK_RETURN)
        {
            HWND parent = GetParent(hwnd);
            if (parent)
            {
                PostMessageA(parent, WM_COMMAND, MAKEWPARAM(IDC_APPLY_CHANGES, 0), 0);
            }
            return 0;
        }

        return CallWindowProcA(g_editor_edit_proc, hwnd, message, w_param, l_param);
    }

    HWND create_label(HWND parent, const char* text, int x, int y, int width, int height)
    {
        return CreateWindowExA(0,
                               "STATIC",
                               text,
                               WS_CHILD | WS_VISIBLE,
                               x,
                               y,
                               width,
                               height,
                               parent,
                               NULL,
                               GetModuleHandleA(NULL),
                               NULL);
    }

    HWND create_edit(HWND parent, int id, int x, int y, int width, int height)
    {
        HWND control = CreateWindowExA(WS_EX_CLIENTEDGE,
                                       "EDIT",
                                       "",
                                       WS_CHILD | WS_VISIBLE | WS_TABSTOP | ES_AUTOHSCROLL,
                                       x,
                                       y,
                                       width,
                                       height,
                                       parent,
                                       reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                       GetModuleHandleA(NULL),
                                       NULL);

        if (control)
        {
            if (!g_editor_edit_proc)
            {
                g_editor_edit_proc = reinterpret_cast<WNDPROC>(GetWindowLongPtrA(control, GWLP_WNDPROC));
            }
            SetWindowLongPtrA(control, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(editor_edit_proc));
        }

        return control;
    }

    HWND create_button(HWND parent, const char* text, int id, int x, int y, int width, int height)
    {
        return CreateWindowExA(0,
                               "BUTTON",
                               text,
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                               x,
                               y,
                               width,
                               height,
                               parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleA(NULL),
                               NULL);
    }

    HWND create_checkbox(HWND parent, const char* text, int id, int x, int y, int width, int height)
    {
        return CreateWindowExA(0,
                               "BUTTON",
                               text,
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                               x,
                               y,
                               width,
                               height,
                               parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleA(NULL),
                               NULL);
    }

    HWND create_combo(HWND parent, int id, int x, int y, int width, int height)
    {
        return CreateWindowExA(0,
                               "COMBOBOX",
                               "",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | CBS_DROPDOWNLIST,
                               x,
                               y,
                               width,
                               height,
                               parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleA(NULL),
                               NULL);
    }

    HWND create_listbox(HWND parent, int id, int x, int y, int width, int height)
    {
        return CreateWindowExA(WS_EX_CLIENTEDGE,
                               "LISTBOX",
                               "",
                               WS_CHILD | WS_VISIBLE | WS_TABSTOP | LBS_NOTIFY | WS_VSCROLL,
                               x,
                               y,
                               width,
                               height,
                               parent,
                               reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                               GetModuleHandleA(NULL),
                               NULL);
    }

    RECT make_window_rect_for_client_size(DWORD style, DWORD ex_style, int client_width, int client_height)
    {
        RECT rect = { 0, 0, client_width, client_height };
        AdjustWindowRectEx(&rect, style, FALSE, ex_style);
        return rect;
    }

    void set_window_text(HWND control, const std::string& text)
    {
        if (control)
        {
            SetWindowTextA(control, text.c_str());
        }
    }

    std::string format_preview_beat_label(const EditorApp& app)
    {
        std::ostringstream stream;
        stream << "Apply Changes";
        if (app.mode == EditorMode::Preview)
        {
            stream << " | Beat "
                   << app.preview.current_beat_time.beat
                   << '.'
                   << app.preview.current_beat_time.substep;
        }
        return stream.str();
    }

    std::string get_window_text(HWND control)
    {
        int length;
        std::vector<char> buffer;

        if (!control)
        {
            return std::string();
        }

        length = GetWindowTextLengthA(control);
        buffer.resize(static_cast<size_t>(length) + 1u, '\0');
        GetWindowTextA(control, buffer.data(), length + 1);
        return std::string(buffer.data());
    }

    float read_float_or_default(HWND control, float default_value)
    {
        std::string text = get_window_text(control);
        char* end = NULL;
        float value;

        if (text.empty())
        {
            return default_value;
        }

        value = std::strtof(text.c_str(), &end);
        return end != text.c_str() ? value : default_value;
    }

    double read_double_or_default(HWND control, double default_value)
    {
        std::string text = get_window_text(control);
        char* end = NULL;
        double value;

        if (text.empty())
        {
            return default_value;
        }

        value = std::strtod(text.c_str(), &end);
        return end != text.c_str() ? value : default_value;
    }

    void set_edit_float(HWND control, float value)
    {
        set_window_text(control, format_number(value));
    }

    void set_edit_double(HWND control, double value)
    {
        set_window_text(control, format_number(value));
    }

    std::string normalize_path_slashes(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    std::string make_workspace_relative_path(const std::string& path)
    {
        const std::string normalized_path = normalize_path_slashes(path);
        const std::string source_root = normalize_path_slashes(std::string(TROPIC_SOURCE_DIR));

        if (normalized_path.compare(0, source_root.size(), source_root) == 0)
        {
            size_t offset = source_root.size();
            while (offset < normalized_path.size() && (normalized_path[offset] == '/' || normalized_path[offset] == '\\'))
            {
                ++offset;
            }
            return normalized_path.substr(offset);
        }

        return normalized_path;
    }

    bool browse_for_file(HWND owner,
                         const char* filter,
                         const char* default_extension,
                         const char* title,
                         bool save_dialog,
                         std::string& out_path)
    {
        char file_buffer[MAX_PATH] = { 0 };
        OPENFILENAMEA dialog = {};

        dialog.lStructSize = sizeof(dialog);
        dialog.hwndOwner = owner;
        dialog.lpstrFilter = filter;
        dialog.lpstrFile = file_buffer;
        dialog.nMaxFile = static_cast<DWORD>(sizeof(file_buffer));
        dialog.lpstrDefExt = default_extension;
        dialog.lpstrTitle = title;
        dialog.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST;
        if (!save_dialog)
        {
            dialog.Flags |= OFN_FILEMUSTEXIST;
        }
        else
        {
            dialog.Flags |= OFN_OVERWRITEPROMPT;
        }

        if ((save_dialog ? GetSaveFileNameA(&dialog) : GetOpenFileNameA(&dialog)) == FALSE)
        {
            return false;
        }

        out_path = normalize_path_slashes(std::string(file_buffer));
        return true;
    }

    int find_action_combo_index(TropicEventActionType action)
    {
        for (size_t i = 0; i < sizeof(kEditorActionTypes) / sizeof(kEditorActionTypes[0]); ++i)
        {
            if (kEditorActionTypes[i] == action)
            {
                return static_cast<int>(i);
            }
        }
        return 0;
    }

    int find_trigger_combo_index(TropicEventTriggerMode trigger)
    {
        for (size_t i = 0; i < sizeof(kEditorTriggerTypes) / sizeof(kEditorTriggerTypes[0]); ++i)
        {
            if (kEditorTriggerTypes[i] == trigger)
            {
                return static_cast<int>(i);
            }
        }
        return 0;
    }

    void set_event_controls_enabled(bool enabled)
    {
        EnableWindow(g_editor_panel.event_action_combo, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_trigger_combo, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_once_check, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_target_uid_edit, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_function_edit, enabled ? TRUE : FALSE);
        for (int i = 0; i < 3; ++i)
        {
            EnableWindow(g_editor_panel.event_gravity_edits[i], enabled ? TRUE : FALSE);
            EnableWindow(g_editor_panel.event_axis_edits[i], enabled ? TRUE : FALSE);
        }
        EnableWindow(g_editor_panel.event_degrees_edit, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_speed_edit, enabled ? TRUE : FALSE);
        EnableWindow(g_editor_panel.event_duration_edit, enabled ? TRUE : FALSE);
    }

    bool is_live_apply_control(int control_id, int notification_code)
    {
        if (control_id == IDC_OBJECT_LIST || control_id == IDC_ANCHOR_LIST)
        {
            return false;
        }

        if (control_id == IDC_UID_EDIT)
        {
            return notification_code == EN_KILLFOCUS;
        }

        if ((control_id >= IDC_METADATA_GAME_TITLE && control_id <= IDC_METADATA_UNITS_PER_BEAT) ||
            (control_id >= IDC_POSITION_X && control_id <= IDC_ROTATION_Z) ||
            (control_id >= IDC_PLACEMENT_BEAT && control_id <= IDC_PLACEMENT_LENGTH_BEATS) ||
            (control_id >= IDC_EVENT_TARGET_UID && control_id <= IDC_EVENT_DURATION) ||
            (control_id >= IDC_ANCHOR_BEAT && control_id <= IDC_ANCHOR_DEGREES))
        {
            return notification_code == EN_KILLFOCUS;
        }

        if (control_id == IDC_EVENT_ACTION || control_id == IDC_EVENT_TRIGGER || control_id == IDC_PLACEMENT_SPACE)
        {
            return notification_code == CBN_SELCHANGE;
        }

        if (control_id == IDC_EVENT_ONCE)
        {
            return notification_code == BN_CLICKED;
        }

        return false;
    }

    void nudge_selected_position(EditorApp& app, int axis, float delta)
    {
        if (!has_selection(app) || axis < 0 || axis > 2)
        {
            return;
        }

        if (nudge_selected_track_placement(app, axis, delta))
        {
            return;
        }

        app.objects[app.selected_index].position[axis] += delta;
        if (app.initialized)
        {
            sync_selected_object(app);
        }
        else
        {
            mark_ui_dirty(app);
        }
    }

    void mark_ui_dirty(EditorApp& app)
    {
        app.ui_dirty = true;
    }
#endif

    std::vector<std::string> level_path_candidates()
    {
        std::vector<std::string> candidates;
        candidates.push_back(std::string(TROPIC_SOURCE_DIR) + "/assets/levels/test_level.json");
        candidates.push_back("assets/levels/test_level.json");
        candidates.push_back("../assets/levels/test_level.json");
        candidates.push_back("../../assets/levels/test_level.json");
        return candidates;
    }

    std::vector<std::string> make_asset_candidates(const char* relative_path)
    {
        std::vector<std::string> candidates;
        candidates.push_back(std::string(TROPIC_SOURCE_DIR) + "/" + relative_path);
        candidates.push_back(relative_path);
        candidates.push_back(std::string("../") + relative_path);
        candidates.push_back(std::string("../../") + relative_path);
        return candidates;
    }

    bool load_shader_from_candidates(TropicID engine_id,
                                     const std::vector<std::string>& vertex_candidates,
                                     const std::vector<std::string>& fragment_candidates,
                                     ShaderID* out_shader)
    {
        std::vector<const char*> vertex_paths;
        std::vector<const char*> fragment_paths;
        size_t candidate_count = std::min(vertex_candidates.size(), fragment_candidates.size());

        if (!out_shader || candidate_count == 0)
        {
            return false;
        }

        vertex_paths.reserve(candidate_count);
        fragment_paths.reserve(candidate_count);
        for (size_t i = 0; i < candidate_count; ++i)
        {
            vertex_paths.push_back(vertex_candidates[i].c_str());
            fragment_paths.push_back(fragment_candidates[i].c_str());
        }

        return Tropic_createShaderFromFileCandidates(engine_id,
                                                     vertex_paths.data(),
                                                     fragment_paths.data(),
                                                     candidate_count,
                                                     out_shader);
    }

    bool load_editor_shaders(TropicID engine_id, EditorRenderResources& render)
    {
        std::vector<std::string> volume_vertex = make_asset_candidates("assets/shaders/platform_volume_ripple_round_soft.vert");
        std::vector<std::string> volume_fragment = make_asset_candidates("assets/shaders/platform_volume_ripple_round_soft.frag");
        std::vector<std::string> player_vertex = make_asset_candidates("assets/shaders/player_volume_ripple_round_soft.vert");
        std::vector<std::string> player_fragment = make_asset_candidates("assets/shaders/player_volume_ripple_round_soft.frag");

        if (!load_shader_from_candidates(engine_id, volume_vertex, volume_fragment, &render.volume_shader))
        {
            return false;
        }

        if (!load_shader_from_candidates(engine_id, player_vertex, player_fragment, &render.player_shader))
        {
            return false;
        }

        return true;
    }

    extern "C" void editor_render_callback(TropicID engine_id,
                                             Scene* scene,
                                             Object* object,
                                             TropicMaterial* material,
                                             ShaderID shader_id,
                                             const TropicCamera* camera)
    {
        vec3 light_pos;
        vec3 ambient_color;
        EditorMaterialUniforms* uniforms = NULL;

        (void)object;
        (void)camera;

        if (!material || shader_id == 0)
        {
            return;
        }

        uniforms = static_cast<EditorMaterialUniforms*>(material->user);
        if (!uniforms)
        {
            return;
        }

        set_vec3(light_pos, 20.0f, 35.0f, 20.0f);
        set_vec3(ambient_color, 0.2f, 0.2f, 0.2f);
        if (scene)
        {
            glm_vec3_copy(scene->ambient_light_color, ambient_color);
        }

        (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "lightPos", light_pos);
        (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "objectColor", uniforms->color);
        (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "ambientColor", ambient_color);
        (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "neonAmount", uniforms->neon_amount);
        (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "brightnessScale", uniforms->brightness_scale);
        (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "alphaScale", uniforms->alpha_scale);
    }

    void init_uniform(EditorMaterialUniforms& uniform,
                      float r,
                      float g,
                      float b,
                      float neon,
                      float brightness,
                      float alpha)
    {
        set_vec3(uniform.color, r, g, b);
        uniform.neon_amount = neon;
        uniform.brightness_scale = brightness;
        uniform.alpha_scale = alpha;
    }

    bool init_materials(TropicID engine_id, EditorRenderResources& render)
    {
        render.cube_mesh = Tropic_createCubeMesh(engine_id);
        if (render.cube_mesh == 0)
        {
            return false;
        }

        if (!load_editor_shaders(engine_id, render))
        {
            return false;
        }

        init_uniform(render.platform_uniforms, 0.35f, 0.75f, 0.45f, 1.0f, 1.0f, 1.0f);
        init_uniform(render.spike_uniforms, 0.92f, 0.30f, 0.30f, 1.0f, 1.1f, 1.0f);
        init_uniform(render.jumppad_uniforms, 0.20f, 0.75f, 1.00f, 1.0f, 1.1f, 1.0f);
        init_uniform(render.event_uniforms, 0.75f, 0.35f, 1.00f, 1.0f, 1.0f, 0.65f);
        init_uniform(render.selected_uniforms, 1.00f, 0.92f, 0.25f, 1.4f, 1.7f, 1.0f);
        init_uniform(render.player_uniforms, 0.10f, 0.55f, 1.00f, 1.0f, 1.4f, 1.0f);
        init_uniform(render.gizmo_x_uniforms, 1.00f, 0.30f, 0.30f, 1.0f, 1.35f, 1.0f);
        init_uniform(render.gizmo_y_uniforms, 0.30f, 1.00f, 0.35f, 1.0f, 1.35f, 1.0f);
        init_uniform(render.gizmo_z_uniforms, 0.25f, 0.60f, 1.00f, 1.0f, 1.35f, 1.0f);

        render.platform_material = Tropic_createMaterial(engine_id,
                                                         render.cube_mesh,
                                                         render.volume_shader,
                                                         editor_render_callback,
                                                         &render.platform_uniforms);
        render.spike_material = Tropic_createMaterial(engine_id,
                                                      render.cube_mesh,
                                                      render.volume_shader,
                                                      editor_render_callback,
                                                      &render.spike_uniforms);
        render.jumppad_material = Tropic_createMaterial(engine_id,
                                                        render.cube_mesh,
                                                        render.volume_shader,
                                                        editor_render_callback,
                                                        &render.jumppad_uniforms);
        render.event_material = Tropic_createMaterial(engine_id,
                                                      render.cube_mesh,
                                                      render.volume_shader,
                                                      editor_render_callback,
                                                      &render.event_uniforms);
        render.selected_material = Tropic_createMaterial(engine_id,
                                                         render.cube_mesh,
                                                         render.volume_shader,
                                                         editor_render_callback,
                                                         &render.selected_uniforms);
        render.player_material = Tropic_createMaterial(engine_id,
                                                       render.cube_mesh,
                                                       render.player_shader,
                                                       editor_render_callback,
                                                       &render.player_uniforms);
        render.gizmo_x_material = Tropic_createMaterial(engine_id,
                                                        render.cube_mesh,
                                                        render.volume_shader,
                                                        editor_render_callback,
                                                        &render.gizmo_x_uniforms);
        render.gizmo_y_material = Tropic_createMaterial(engine_id,
                                                        render.cube_mesh,
                                                        render.volume_shader,
                                                        editor_render_callback,
                                                        &render.gizmo_y_uniforms);
        render.gizmo_z_material = Tropic_createMaterial(engine_id,
                                                        render.cube_mesh,
                                                        render.volume_shader,
                                                        editor_render_callback,
                                                        &render.gizmo_z_uniforms);

        return render.platform_material != 0 && render.spike_material != 0 &&
               render.jumppad_material != 0 && render.event_material != 0 &&
               render.selected_material != 0 && render.player_material != 0 &&
               render.gizmo_x_material != 0 && render.gizmo_y_material != 0 &&
               render.gizmo_z_material != 0;
    }

    MaterialID material_for_gizmo_axis(const EditorApp& app, int axis)
    {
        switch (axis)
        {
        case 0: return app.render.gizmo_x_material;
        case 1: return app.render.gizmo_y_material;
        case 2: return app.render.gizmo_z_material;
        default: return app.render.selected_material;
        }
    }

    MaterialID material_for_object(const EditorApp& app, ObjectType type, bool selected)
    {
        if (selected && app.mode == EditorMode::Edit)
        {
            return app.render.selected_material;
        }

        switch (type)
        {
        case TYPE_PLATFORM: return app.render.platform_material;
        case TYPE_SPIKE: return app.render.spike_material;
        case TYPE_JUMPPAD: return app.render.jumppad_material;
        case TYPE_EVENT: return app.render.event_material;
        case TYPE_CUBE: return app.render.player_material;
        default: return app.render.platform_material;
        }
    }

    void update_selection_materials(EditorApp& app)
    {
        for (size_t i = 0; i < app.object_ids.size() && i < app.objects.size(); ++i)
        {
            MaterialID material_id = material_for_object(app, app.objects[i].type, i == app.selected_index);
            (void)Tropic_setObjectMaterial(app.engine, app.object_ids[i], material_id);
        }
    }

    bool has_selection(const EditorApp& app)
    {
        return app.selected_index < app.objects.size() && app.selected_index < app.object_ids.size();
    }

    void compute_scene_center(const EditorApp& app, vec3 center)
    {
        set_vec3(center, 0.0f, 0.0f, 0.0f);
        if (app.objects.empty())
        {
            return;
        }

        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            center[0] += app.objects[i].position[0];
            center[1] += app.objects[i].position[1];
            center[2] += app.objects[i].position[2];
        }

        center[0] /= static_cast<float>(app.objects.size());
        center[1] /= static_cast<float>(app.objects.size());
        center[2] /= static_cast<float>(app.objects.size());
    }

    void compute_camera_focus(const EditorApp& app, vec3 focus)
    {
        if (app.selected_index < app.objects.size())
        {
            copy_vec3(app.objects[app.selected_index].position, focus);
            return;
        }

        compute_scene_center(app, focus);
    }

    void ensure_camera_focus_initialized(EditorApp& app)
    {
        if (app.camera_focus_initialized)
        {
            return;
        }

        compute_camera_focus(app, app.camera_focus_point);
        app.camera_focus_initialized = true;
        app.camera_focus_tracks_selection = true;
    }

    void update_edit_camera(EditorApp& app, float delta_time)
    {
        vec3 focus;
        vec3 position;
        vec3 up;
        vec3 right;
        vec3 forward;
        vec3 pan_delta;
        float yaw_speed = 90.0f;
        float pitch_speed = 75.0f;
        float zoom_speed = 18.0f;
        float pan_speed = kEditorCameraPanSpeed;
        float yaw_radians;
        float pitch_radians;
        float cos_pitch;
        bool did_pan = false;

        if (key_down(GLFW_KEY_LEFT)) app.orbit_yaw_degrees -= yaw_speed * delta_time;
        if (key_down(GLFW_KEY_RIGHT)) app.orbit_yaw_degrees += yaw_speed * delta_time;
        if (key_down(GLFW_KEY_UP)) app.orbit_pitch_degrees += pitch_speed * delta_time;
        if (key_down(GLFW_KEY_DOWN)) app.orbit_pitch_degrees -= pitch_speed * delta_time;
        if (key_down(GLFW_KEY_PAGE_UP)) app.orbit_distance -= zoom_speed * delta_time;
        if (key_down(GLFW_KEY_PAGE_DOWN)) app.orbit_distance += zoom_speed * delta_time;

        ensure_camera_focus_initialized(app);

        if (shift_down())
        {
            pan_speed *= kEditorCameraFastPanMultiplier;
        }

        app.orbit_pitch_degrees = std::max(-80.0f, std::min(80.0f, app.orbit_pitch_degrees));
        app.orbit_distance = std::max(4.0f, std::min(100.0f, app.orbit_distance));

        if (app.camera_focus_tracks_selection)
        {
            compute_camera_focus(app, app.camera_focus_point);
        }

        yaw_radians = app.orbit_yaw_degrees * 3.14159265f / 180.0f;
        pitch_radians = app.orbit_pitch_degrees * 3.14159265f / 180.0f;
        cos_pitch = std::cos(pitch_radians);

        set_vec3(right, std::sin(yaw_radians), 0.0f, -std::cos(yaw_radians));
        set_vec3(forward, -std::cos(yaw_radians), 0.0f, -std::sin(yaw_radians));
        normalize_vec3(right);
        normalize_vec3(forward);
        set_vec3(pan_delta, 0.0f, 0.0f, 0.0f);

        if (key_down(GLFW_KEY_A))
        {
            mul_add_vec3(pan_delta, right, -pan_speed * delta_time);
            did_pan = true;
        }
        if (key_down(GLFW_KEY_D))
        {
            mul_add_vec3(pan_delta, right, pan_speed * delta_time);
            did_pan = true;
        }
        if (key_down(GLFW_KEY_W))
        {
            mul_add_vec3(pan_delta, forward, pan_speed * delta_time);
            did_pan = true;
        }
        if (key_down(GLFW_KEY_S))
        {
            mul_add_vec3(pan_delta, forward, -pan_speed * delta_time);
            did_pan = true;
        }
        if (key_down(GLFW_KEY_E))
        {
            pan_delta[1] += pan_speed * delta_time;
            did_pan = true;
        }
        if (key_down(GLFW_KEY_Q))
        {
            pan_delta[1] -= pan_speed * delta_time;
            did_pan = true;
        }

        if (did_pan)
        {
            app.camera_focus_tracks_selection = false;
            mul_add_vec3(app.camera_focus_point, pan_delta, 1.0f);
        }

        copy_vec3(app.camera_focus_point, focus);

        position[0] = focus[0] + std::cos(yaw_radians) * cos_pitch * app.orbit_distance;
        position[1] = focus[1] + std::sin(pitch_radians) * app.orbit_distance;
        position[2] = focus[2] + std::sin(yaw_radians) * cos_pitch * app.orbit_distance;
        set_vec3(up, 0.0f, 1.0f, 0.0f);

        (void)Tropic_unbindCamera(app.engine, app.camera_id);
        (void)Tropic_setCameraPosition(app.engine, app.camera_id, position);
        (void)Tropic_setCameraTarget(app.engine, app.camera_id, focus);
        (void)Tropic_setCameraUp(app.engine, app.camera_id, up);
    }

    bool load_level_model(EditorApp& app)
    {
        int num_objects = 0;
        LevelSpec* level_spec = parseLevel(app.level_path.c_str(), &num_objects);
        (void)num_objects;

        app.metadata.game_title = "Cyclone";
        app.metadata.level_name = "Untitled Level";
        app.metadata.music_path.clear();
        app.metadata.play_speed = 1.0;
        Tropic_setDefaultBeatGridSettings(&app.metadata.beat_grid);
        app.objects.clear();
        app.track_anchors.clear();
        app.selected_index = kNoSelection;
        app.selected_anchor_index = kNoSelection;

        if (!level_spec)
        {
            mark_ui_dirty(app);
            return false;
        }

        if (level_spec->game_title)
        {
            app.metadata.game_title = level_spec->game_title;
        }
        if (level_spec->level_name)
        {
            app.metadata.level_name = level_spec->level_name;
        }
        if (level_spec->play_speed > 0.0)
        {
            app.metadata.play_speed = level_spec->play_speed;
        }
        if (level_spec->music_path)
        {
            app.metadata.music_path = level_spec->music_path;
        }
        app.metadata.beat_grid = level_spec->beat_grid;
        app.track_anchors.assign(level_spec->track_anchors,
                                 level_spec->track_anchors + level_spec->track_anchor_count);
        if (!app.track_anchors.empty())
        {
            app.selected_anchor_index = 0;
        }

        for (size_t i = 0; i < level_spec->platform_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_PLATFORM);
            object.uid = level_spec->platforms[i].uid;
            copy_vec3(level_spec->platforms[i].position, object.position);
            copy_vec3(level_spec->platforms[i].scale, object.scale);
            copy_vec3(level_spec->platforms[i].rotation, object.rotation);
            object.placement = level_spec->platforms[i].placement;
            object.event = level_spec->platforms[i].event;
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->spikes_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_SPIKE);
            object.uid = level_spec->spikes[i].uid;
            copy_vec3(level_spec->spikes[i].position, object.position);
            copy_vec3(level_spec->spikes[i].scale, object.scale);
            copy_vec3(level_spec->spikes[i].rotation, object.rotation);
            object.placement = level_spec->spikes[i].placement;
            object.event = level_spec->spikes[i].event;
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->jumppads_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_JUMPPAD);
            object.uid = level_spec->jumppads[i].uid;
            copy_vec3(level_spec->jumppads[i].position, object.position);
            copy_vec3(level_spec->jumppads[i].scale, object.scale);
            copy_vec3(level_spec->jumppads[i].rotation, object.rotation);
            object.placement = level_spec->jumppads[i].placement;
            object.event = level_spec->jumppads[i].event;
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->events_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_EVENT);
            object.uid = level_spec->events[i].uid;
            copy_vec3(level_spec->events[i].position, object.position);
            copy_vec3(level_spec->events[i].scale, object.scale);
            copy_vec3(level_spec->events[i].rotation, object.rotation);
            object.placement = level_spec->events[i].placement;
            object.event = level_spec->events[i].event;
            app.objects.push_back(object);
        }

        if (!app.objects.empty())
        {
            app.selected_index = 0;
        }

        level_free(level_spec);
        mark_ui_dirty(app);
        return true;
    }

    std::string json_escape(const std::string& value)
    {
        std::ostringstream stream;
        for (size_t i = 0; i < value.size(); ++i)
        {
            const char ch = value[i];
            switch (ch)
            {
            case '\\': stream << "\\\\"; break;
            case '"': stream << "\\\""; break;
            case '\n': stream << "\\n"; break;
            case '\r': stream << "\\r"; break;
            case '\t': stream << "\\t"; break;
            default: stream << ch; break;
            }
        }
        return stream.str();
    }

    void write_vec3_block(std::ofstream& file,
                          const char* name,
                          const vec3 value,
                          const std::string& indent)
    {
        file << indent << "\"" << name << "\": {\n";
        file << indent << "  \"x\": " << value[0] << ",\n";
        file << indent << "  \"y\": " << value[1] << ",\n";
        file << indent << "  \"z\": " << value[2] << "\n";
        file << indent << '}';
    }

    void write_event_block(std::ofstream& file,
                           const EditorObject& object,
                           const std::string& indent)
    {
        file << indent << "\"event_type\": \"" << event_action_name(object.event.action_type) << "\",\n";
        file << indent << "\"trigger\": \"" << event_trigger_name(object.event.trigger_mode) << "\",\n";
        file << indent << "\"once\": " << (object.event.trigger_once ? "true" : "false");

        switch (object.event.action_type)
        {
        case TROPIC_EVENT_ACTION_GRAVITY_SET:
            file << ",\n";
            write_vec3_block(file, "gravity", object.event.gravity, indent);
            break;
        case TROPIC_EVENT_ACTION_WORLD_SPIN:
        case TROPIC_EVENT_ACTION_CAMERA_SPIN:
            file << ",\n";
            write_vec3_block(file, "axis", object.event.axis, indent);
            file << ",\n";
            file << indent << "\"degrees\": " << object.event.degrees << ",\n";
            file << indent << "\"speed\": " << object.event.speed << ",\n";
            file << indent << "\"duration\": " << object.event.duration_seconds;
            if (object.event.action_type == TROPIC_EVENT_ACTION_WORLD_SPIN && object.event.target_uid[0] != '\0')
            {
                file << ",\n";
                file << indent << "\"target_uid\": \"" << json_escape(object.event.target_uid) << "\"";
            }
            break;
        case TROPIC_EVENT_ACTION_CUSTOM:
            if (object.event.custom_function[0] != '\0')
            {
                file << ",\n";
                file << indent << "\"function\": \"" << json_escape(object.event.custom_function) << "\"";
            }
            break;
        case TROPIC_EVENT_ACTION_GRAVITY_FLIP:
        case TROPIC_EVENT_ACTION_NONE:
        default:
            break;
        }
    }

    void write_placement_block(std::ofstream& file,
                               const EditorObject& object,
                               const std::string& indent)
    {
        if (object.placement.space != TROPIC_PLACEMENT_SPACE_TRACK)
        {
            return;
        }

        file << indent << "\"placement\": {\n";
        file << std::fixed << std::setprecision(3);
        file << indent << "  \"beat\": " << object.placement.time.beat << ",\n";
        file << indent << "  \"substep\": " << object.placement.time.substep << ",\n";
        file << indent << "  \"x\": " << object.placement.track_x << ",\n";
        file << indent << "  \"y\": " << object.placement.track_y << ",\n";
        file << indent << "  \"snap_x\": " << (object.placement.snap_x ? "true" : "false") << ",\n";
        file << indent << "  \"snap_y\": " << (object.placement.snap_y ? "true" : "false");
        if (object.placement.length_beats != 0.0f)
        {
            file << ",\n";
            file << indent << "  \"length_beats\": " << object.placement.length_beats << "\n";
        }
        else
        {
            file << "\n";
        }
        file << indent << "},\n";
    }

    void write_metadata_block(std::ofstream& file, const EditorLevelMetadata& metadata)
    {
        file << "  \"metadata\": {\n";
        file << "    \"game_title\": \"" << json_escape(metadata.game_title) << "\",\n";
        file << "    \"level_name\": \"" << json_escape(metadata.level_name) << "\",\n";
        file << "    \"music_path\": \"" << json_escape(metadata.music_path) << "\",\n";
        file << std::fixed << std::setprecision(3);
        file << "    \"play_speed\": " << metadata.play_speed << ",\n";
        file << "    \"bpm\": " << metadata.beat_grid.bpm << ",\n";
        file << "    \"music_offset_seconds\": " << metadata.beat_grid.music_offset_seconds << ",\n";
        file << "    \"subdivisions_per_beat\": " << metadata.beat_grid.subdivisions_per_beat << ",\n";
        file << "    \"units_per_beat\": " << metadata.beat_grid.units_per_beat << ",\n";
        file << "    \"snap_unit_x\": " << metadata.beat_grid.snap_unit_x << ",\n";
        file << "    \"snap_unit_y\": " << metadata.beat_grid.snap_unit_y << ",\n";
        write_vec3_block(file, "track_origin", metadata.beat_grid.origin, "    ");
        file << ",\n";
        write_vec3_block(file, "track_right", metadata.beat_grid.initial_right, "    ");
        file << ",\n";
        write_vec3_block(file, "track_up", metadata.beat_grid.initial_up, "    ");
        file << ",\n";
        write_vec3_block(file, "track_forward", metadata.beat_grid.initial_forward, "    ");
        file << "\n  }";
    }

    void write_track_anchor_group(std::ofstream& file,
                                  const std::vector<TropicTrackAnchor>& track_anchors)
    {
        file << "  \"track_anchors\": [\n";
        for (size_t i = 0; i < track_anchors.size(); ++i)
        {
            const TropicTrackAnchor& anchor = track_anchors[i];
            if (i > 0)
            {
                file << ",\n";
            }

            file << "    {\n";
            file << std::fixed << std::setprecision(3);
            file << "      \"beat\": " << anchor.start_time.beat << ",\n";
            file << "      \"substep\": " << anchor.start_time.substep << ",\n";
            file << "      \"pivot\": {\n";
            file << "        \"x\": " << anchor.pivot_x << ",\n";
            file << "        \"y\": " << anchor.pivot_y << ",\n";
            file << "        \"beat\": " << anchor.pivot_beat << "\n";
            file << "      },\n";
            write_vec3_block(file, "local_axis", anchor.local_axis, "      ");
            file << ",\n";
            file << "      \"degrees\": " << anchor.degrees << "\n";
            file << "    }";
        }
        file << "\n  ],\n";
    }

    void write_transform_block(std::ofstream& file,
                               const EditorObject& object,
                               bool platform_style,
                               const std::string& indent)
    {
        file << indent << "{\n";
        file << indent << "  \"uid\": \"" << json_escape(object.uid) << "\",\n";
        write_placement_block(file, object, indent + "  ");
        file << indent << "  \"pos\": {\n";
        file << std::fixed << std::setprecision(3);
        file << indent << "    \"x\": " << object.position[0] << ",\n";
        file << indent << "    \"y\": " << object.position[1] << ",\n";
        file << indent << "    \"z\": " << object.position[2] << ",\n";
        file << indent << "    \"width\": " << (object.scale[0] * 2.0f) << ",\n";
        file << indent << "    \"height\": " << (object.scale[1] * 2.0f) << ",\n";
        file << indent << "    \"length\": " << (object.scale[2] * 2.0f) << "\n";
        file << indent << "  },\n";
        file << indent << "  \"scale\": {\n";
        if (platform_style)
        {
            file << indent << "    \"x\": 1.000,\n";
            file << indent << "    \"y\": 1.000,\n";
            file << indent << "    \"z\": 1.000\n";
        }
        else
        {
            file << indent << "    \"x\": " << object.scale[0] << ",\n";
            file << indent << "    \"y\": " << object.scale[1] << ",\n";
            file << indent << "    \"z\": " << object.scale[2] << "\n";
        }
        file << indent << "  },\n";
        file << indent << "  \"rot\": {\n";
        file << indent << "    \"x\": " << object.rotation[0] << ",\n";
        file << indent << "    \"y\": " << object.rotation[1] << ",\n";
        file << indent << "    \"z\": " << object.rotation[2] << "\n";
        file << indent << "  }";
        if (object.type == TYPE_EVENT)
        {
            file << ",\n";
            write_event_block(file, object, indent + "  ");
            file << "\n";
        }
        else
        {
            file << "\n";
        }
        file << indent << "}";
    }

    void write_object_group(std::ofstream& file,
                            const char* group_name,
                            const std::vector<EditorObject>& objects,
                            ObjectType type,
                            bool platform_style,
                            bool trailing_comma)
    {
        bool first = true;
        file << "  \"" << group_name << "\": [\n";
        for (size_t i = 0; i < objects.size(); ++i)
        {
            if (objects[i].type != type)
            {
                continue;
            }

            if (!first)
            {
                file << ",\n";
            }
            write_transform_block(file, objects[i], platform_style, "    ");
            first = false;
        }
        file << "\n  ]";
        if (trailing_comma)
        {
            file << ",";
        }
        file << "\n";
    }

    bool save_level_model(const EditorApp& app)
    {
        std::ofstream file(app.level_path.c_str(), std::ios::out | std::ios::trunc);
        if (!file)
        {
            std::fprintf(stderr, "Failed to save level to %s\n", app.level_path.c_str());
            return false;
        }

        file << "{\n";
        write_metadata_block(file, app.metadata);
        file << ",\n";
        write_track_anchor_group(file, app.track_anchors);
        write_object_group(file, "platforms", app.objects, TYPE_PLATFORM, true, true);
        write_object_group(file, "spikes", app.objects, TYPE_SPIKE, false, true);
        write_object_group(file, "jumppads", app.objects, TYPE_JUMPPAD, false, true);
        write_object_group(file, "events", app.objects, TYPE_EVENT, false, false);
        file << "}\n";
        return true;
    }

    bool create_runtime_object(EditorApp& app, const EditorObject& editor_object, ObjectID& out_id)
    {
        Object prototype;
        std::memset(&prototype, 0, sizeof(prototype));
        prototype.type = editor_object.type;
        std::strncpy(prototype.uid, editor_object.uid.c_str(), sizeof(prototype.uid));
        prototype.uid[sizeof(prototype.uid) - 1] = '\0';
        copy_vec3(editor_object.position, prototype.pos);
        copy_vec3(editor_object.scale, prototype.scale);
        copy_vec3(editor_object.rotation, prototype.rot);
        prototype.placement = editor_object.placement;
        prototype.event = editor_object.event;
        prototype.event.has_fired = false;

        if (prototype.placement.space == TROPIC_PLACEMENT_SPACE_TRACK)
        {
            vec3 resolved_position;
            if (Tropic_resolvePlacementPosition(app.engine,
                                               Tropic_getCurrentSceneID(app.engine),
                                               &prototype.placement,
                                               resolved_position))
            {
                copy_vec3(resolved_position, prototype.pos);
            }
        }

        out_id = Tropic_newObject(app.engine, &prototype);
        if (out_id == 0)
        {
            return false;
        }

        return Tropic_setObjectMaterial(app.engine, out_id, material_for_object(app, editor_object.type, false));
    }

    bool create_gizmo_part(EditorApp& app, MaterialID material_id, ObjectID& out_id)
    {
        Object prototype;

        std::memset(&prototype, 0, sizeof(prototype));
        prototype.type = TYPE_GENERIC;
        set_vec3(prototype.pos, 0.0f, -10000.0f, 0.0f);
        set_vec3(prototype.scale, 0.01f, 0.01f, 0.01f);
        set_vec3(prototype.rot, 0.0f, 0.0f, 0.0f);
        prototype.collider.enabled = false;
        prototype.body.enabled = false;

        out_id = Tropic_newObject(app.engine, &prototype);
        if (out_id == 0)
        {
            return false;
        }

        return Tropic_setObjectMaterial(app.engine, out_id, material_id);
    }

    void hide_editor_gizmo(EditorApp& app)
    {
        vec3 hidden_position;
        vec3 hidden_scale;
        vec3 zero_rotation;

        set_vec3(hidden_position, 0.0f, -10000.0f, 0.0f);
        set_vec3(hidden_scale, 0.01f, 0.01f, 0.01f);
        set_vec3(zero_rotation, 0.0f, 0.0f, 0.0f);

        for (int i = 0; i < 3; ++i)
        {
            if (app.gizmo_axis_ids[i] != 0)
            {
                (void)Tropic_setObjectPosition(app.engine, app.gizmo_axis_ids[i], hidden_position);
                (void)Tropic_setObjectScale(app.engine, app.gizmo_axis_ids[i], hidden_scale);
                (void)Tropic_setObjectRotation(app.engine, app.gizmo_axis_ids[i], zero_rotation);
            }
            if (app.gizmo_tip_ids[i] != 0)
            {
                (void)Tropic_setObjectPosition(app.engine, app.gizmo_tip_ids[i], hidden_position);
                (void)Tropic_setObjectScale(app.engine, app.gizmo_tip_ids[i], hidden_scale);
                (void)Tropic_setObjectRotation(app.engine, app.gizmo_tip_ids[i], zero_rotation);
            }
        }
    }

    bool create_editor_gizmo(EditorApp& app)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (!create_gizmo_part(app, material_for_gizmo_axis(app, i), app.gizmo_axis_ids[i]) ||
                !create_gizmo_part(app, material_for_gizmo_axis(app, i), app.gizmo_tip_ids[i]))
            {
                return false;
            }
        }

        hide_editor_gizmo(app);
        return true;
    }

    float current_gizmo_axis_length(const EditorApp& app)
    {
        if (!has_selection(app) || app.engine == 0)
        {
            return 2.0f;
        }

        TropicCamera* camera = Tropic_getActiveCamera(app.engine);
        vec3 offset;
        float distance_to_object;

        if (!camera)
        {
            return 2.0f;
        }

        offset[0] = app.objects[app.selected_index].position[0] - camera->position[0];
        offset[1] = app.objects[app.selected_index].position[1] - camera->position[1];
        offset[2] = app.objects[app.selected_index].position[2] - camera->position[2];
        distance_to_object = std::sqrt(vec3_length_squared(offset));
        return std::max(1.5f, distance_to_object * 0.16f);
    }

    void update_editor_gizmo(EditorApp& app)
    {
        vec3 axes[3];
        float axis_length;
        float shaft_thickness;
        float tip_size;

        if (app.mode != EditorMode::Edit || !has_selection(app))
        {
            hide_editor_gizmo(app);
            return;
        }

        compute_object_axes(app.objects[app.selected_index], axes);
        axis_length = current_gizmo_axis_length(app);
        shaft_thickness = std::max(0.08f, axis_length * 0.08f);
        tip_size = shaft_thickness * 2.2f;

        for (int i = 0; i < 3; ++i)
        {
            vec3 shaft_position;
            vec3 shaft_scale;
            vec3 tip_position;
            vec3 tip_scale;

            copy_vec3(app.objects[app.selected_index].position, shaft_position);
            mul_add_vec3(shaft_position, axes[i], axis_length * 0.5f);
            set_vec3(shaft_scale, shaft_thickness, shaft_thickness, shaft_thickness);
            shaft_scale[i] = axis_length;

            copy_vec3(app.objects[app.selected_index].position, tip_position);
            mul_add_vec3(tip_position, axes[i], axis_length + tip_size * 0.6f);
            set_vec3(tip_scale, tip_size, tip_size, tip_size);

            if (app.gizmo_axis_ids[i] != 0)
            {
                (void)Tropic_setObjectPosition(app.engine, app.gizmo_axis_ids[i], shaft_position);
                (void)Tropic_setObjectScale(app.engine, app.gizmo_axis_ids[i], shaft_scale);
                (void)Tropic_setObjectRotation(app.engine, app.gizmo_axis_ids[i], app.objects[app.selected_index].rotation);
            }
            if (app.gizmo_tip_ids[i] != 0)
            {
                (void)Tropic_setObjectPosition(app.engine, app.gizmo_tip_ids[i], tip_position);
                (void)Tropic_setObjectScale(app.engine, app.gizmo_tip_ids[i], tip_scale);
                (void)Tropic_setObjectRotation(app.engine, app.gizmo_tip_ids[i], app.objects[app.selected_index].rotation);
            }
        }
    }

    bool pick_gizmo_axis(const EditorApp& app, double mouse_x, double mouse_y, EditorAxis& out_axis)
    {
        vec3 axes[3];
        float axis_length;
        float best_distance = kEditorGizmoAxisPixelHitRadius;
        bool found = false;

        if (app.mode != EditorMode::Edit || !has_selection(app))
        {
            return false;
        }

        compute_object_axes(app.objects[app.selected_index], axes);
        axis_length = current_gizmo_axis_length(app);

        for (int i = 0; i < 3; ++i)
        {
            vec3 start;
            vec3 end;
            double start_x;
            double start_y;
            double end_x;
            double end_y;
            float distance;

            copy_vec3(app.objects[app.selected_index].position, start);
            copy_vec3(start, end);
            mul_add_vec3(end, axes[i], axis_length * 1.25f);

            if (!project_world_to_screen(app, start, start_x, start_y) || !project_world_to_screen(app, end, end_x, end_y))
            {
                continue;
            }

            distance = distance_point_to_segment(mouse_x, mouse_y, start_x, start_y, end_x, end_y);
            if (distance <= best_distance)
            {
                best_distance = distance;
                out_axis = static_cast<EditorAxis>(i);
                found = true;
            }
        }

        return found;
    }

    void apply_gizmo_drag(EditorApp& app, double mouse_delta_x, double mouse_delta_y)
    {
        vec3 axes[3];
        vec3 center;
        vec3 axis_end;
        double center_x;
        double center_y;
        double axis_x;
        double axis_y;
        double screen_axis_x;
        double screen_axis_y;
        float screen_axis_length;
        float world_units_per_pixel = 0.0f;
        TropicCamera* camera;
        TropicWindowID* window;
        vec3 camera_forward;
        vec3 camera_to_center;
        int width = 1;
        int height = 1;
        EditorObject& object = app.objects[app.selected_index];
        const int axis_index = static_cast<int>(app.gizmo_drag_axis);

        if (!has_selection(app) || axis_index < 0 || axis_index > 2)
        {
            return;
        }

        compute_object_axes(object, axes);
        copy_vec3(object.position, center);
        copy_vec3(center, axis_end);
        mul_add_vec3(axis_end, axes[axis_index], current_gizmo_axis_length(app));

        if (!project_world_to_screen(app, center, center_x, center_y) || !project_world_to_screen(app, axis_end, axis_x, axis_y))
        {
            return;
        }

        camera = Tropic_getActiveCamera(app.engine);
        window = Tropic_getWindow(app.engine);
        if (!camera || !window)
        {
            return;
        }

        glfwGetFramebufferSize(window, &width, &height);
        if (height <= 0)
        {
            return;
        }

        camera_forward[0] = camera->target[0] - camera->position[0];
        camera_forward[1] = camera->target[1] - camera->position[1];
        camera_forward[2] = camera->target[2] - camera->position[2];
        normalize_vec3(camera_forward);
        camera_to_center[0] = center[0] - camera->position[0];
        camera_to_center[1] = center[1] - camera->position[1];
        camera_to_center[2] = center[2] - camera->position[2];
        world_units_per_pixel = (2.0f * std::max(0.1f, glm_vec3_dot(camera_to_center, camera_forward)) * std::tan(camera->fov * 3.14159265f / 360.0f)) / static_cast<float>(height);

        screen_axis_x = axis_x - center_x;
        screen_axis_y = axis_y - center_y;
        screen_axis_length = vec2_length(screen_axis_x, screen_axis_y);
        if (screen_axis_length <= 0.000001f)
        {
            return;
        }

        screen_axis_x /= screen_axis_length;
        screen_axis_y /= screen_axis_length;

        switch (app.tool)
        {
        case EditorTool::Move:
        {
            const float delta_along_axis = static_cast<float>(mouse_delta_x * screen_axis_x + mouse_delta_y * screen_axis_y);
            mul_add_vec3(object.position, axes[axis_index], delta_along_axis * world_units_per_pixel);
            break;
        }
        case EditorTool::Rotate:
        {
            const double tangent_x = -screen_axis_y;
            const double tangent_y = screen_axis_x;
            const float delta_degrees = static_cast<float>((mouse_delta_x * tangent_x + mouse_delta_y * tangent_y) * kEditorGizmoRotateDegreesPerPixel);
            object.rotation[axis_index] += delta_degrees;
            break;
        }
        case EditorTool::Scale:
        {
            const float delta_along_axis = static_cast<float>(mouse_delta_x * screen_axis_x + mouse_delta_y * screen_axis_y);
            object.scale[axis_index] = std::max(0.1f, object.scale[axis_index] + delta_along_axis * world_units_per_pixel);
            break;
        }
        }

        sync_selected_object(app);
        update_editor_gizmo(app);
    }

    void update_edit_mouse_interaction(EditorApp& app)
    {
        TropicWindowID* window = Tropic_getWindow(app.engine);
        const bool left_was_down = app.mouse_left_was_down;
        bool left_pressed;
        bool left_released;

        if (!window)
        {
            return;
        }

        glfwGetCursorPos(window, &app.mouse_last_x, &app.mouse_last_y);
        app.mouse_left_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
        app.mouse_middle_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
        app.mouse_right_down = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
        app.mouse_has_last_position = true;

        left_pressed = app.mouse_left_down && !left_was_down;
        left_released = !app.mouse_left_down && left_was_down;

        if (left_released)
        {
            app.gizmo_drag_active = false;
        }

        if (left_pressed)
        {
            EditorAxis picked_axis = EditorAxis::X;
            if (pick_gizmo_axis(app, app.mouse_last_x, app.mouse_last_y, picked_axis))
            {
                app.gizmo_drag_active = true;
                app.gizmo_drag_axis = picked_axis;
                app.axis = picked_axis;
                app.gizmo_drag_last_x = app.mouse_last_x;
                app.gizmo_drag_last_y = app.mouse_last_y;
            }
        }

        if (app.gizmo_drag_active && app.mouse_left_down)
        {
            apply_gizmo_drag(app,
                             app.mouse_last_x - app.gizmo_drag_last_x,
                             app.mouse_last_y - app.gizmo_drag_last_y);
            app.gizmo_drag_last_x = app.mouse_last_x;
            app.gizmo_drag_last_y = app.mouse_last_y;
        }

        app.mouse_left_was_down = app.mouse_left_down;
    }

    bool build_scene(EditorApp& app)
    {
        Scene* scene = NULL;
        float background[4] = { 0.03f, 0.03f, 0.05f, 1.0f };
        vec3 ambient;
        vec3 gravity;

        scene = Tropic_getCurrentScene(app.engine);
        if (!scene)
        {
            return false;
        }

        if (!Tropic_setSceneBeatGridSettings(app.engine, scene->id, &app.metadata.beat_grid))
        {
            return false;
        }
        if (!Tropic_clearSceneTrackAnchors(app.engine, scene->id))
        {
            return false;
        }
        for (size_t i = 0; i < app.track_anchors.size(); ++i)
        {
            if (!Tropic_addSceneTrackAnchor(app.engine, scene->id, &app.track_anchors[i]))
            {
                return false;
            }
        }

        app.object_ids.clear();
        app.object_ids.reserve(app.objects.size());

        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            ObjectID object_id = 0;
            if (!create_runtime_object(app, app.objects[i], object_id))
            {
                return false;
            }
            app.object_ids.push_back(object_id);
        }

        set_vec3(ambient, 0.18f, 0.18f, 0.20f);
        glm_vec3_copy(ambient, scene->ambient_light_color);
        (void)Tropic_setBackgroundColor(app.engine, background);
        set_vec3(gravity, 0.0f, -kPreviewDefaultGravity, 0.0f);
        (void)Tropic_setSceneGravity(app.engine, gravity);
        update_selection_materials(app);
        return true;
    }

    bool create_player(EditorApp& app)
    {
        Object prototype;
        vec3 half_extents;
        vec3 collider_offset;
        std::memset(&prototype, 0, sizeof(prototype));
        prototype.type = TYPE_CUBE;
        set_vec3(prototype.pos, 0.0f, 1.0f, 0.0f);
        set_vec3(prototype.scale, 1.0f, 1.0f, 1.0f);
        set_vec3(prototype.rot, 0.0f, 0.0f, 0.0f);

        app.preview.player_id = Tropic_newObject(app.engine, &prototype);
        if (app.preview.player_id == 0)
        {
            return false;
        }

        if (!Tropic_setObjectMaterial(app.engine, app.preview.player_id, app.render.player_material))
        {
            return false;
        }

        if (!Tropic_getObjectScale(app.engine, app.preview.player_id, half_extents))
        {
            return false;
        }

        set_vec3(collider_offset, 0.0f, 0.0f, 0.0f);

        if (!Tropic_configureObjectCollider(app.engine,
                                            app.preview.player_id,
                                            true,
                                            half_extents,
                                            collider_offset,
                                            TROPIC_COLLIDER_FLAG_SOLID))
        {
            return false;
        }

        if (!Tropic_configurePhysicsBody(app.engine, app.preview.player_id, true, false))
        {
            return false;
        }

        return true;
    }

    void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
    {
        (void)scancode;
        (void)mods;

        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (key < 0 || key > GLFW_KEY_LAST)
        {
            return;
        }

        if (action == GLFW_PRESS)
        {
            g_keys[key] = 1;
        }
        else if (action == GLFW_RELEASE)
        {
            g_keys[key] = 0;
        }
    }

    bool dispatch_preview_custom_event(TropicID engine_id,
                                       Object* object,
                                       const TropicCollisionEvent* event)
    {
        if (!object || !event || object->event.action_type != TROPIC_EVENT_ACTION_CUSTOM)
        {
            return false;
        }

        if (std::strcmp(object->event.custom_function, "invert_gravity") == 0)
        {
            return Tropic_invertGravity(engine_id, Tropic_getCurrentSceneID(engine_id));
        }

        if (std::strcmp(object->event.custom_function, "spin_camera_90") == 0)
        {
            vec3 axis;
            set_vec3(axis, 0.0f, 0.0f, 1.0f);
            return Tropic_spinCamera(engine_id,
                                     Tropic_getActiveCameraId(engine_id),
                                     axis,
                                     90.0f,
                                     0.5f);
        }

        (void)event;
        return false;
    }

    extern "C" void preview_event_callback(TropicID engine_id,
                                            const TropicCollisionEvent* event,
                                            void* user_data)
    {
        PreviewEventTriggerState* state = static_cast<PreviewEventTriggerState*>(user_data);
        Object* object;

        if (!state || !event || event->other_id != state->player_id)
        {
            return;
        }

        if (!Tropic_shouldTriggerObjectEvent(engine_id, event->self_id, event))
        {
            return;
        }

        if (Tropic_executeObjectBuiltinEvent(engine_id, event->self_id))
        {
            return;
        }

        object = Tropic_getObject(engine_id, event->self_id);
        (void)dispatch_preview_custom_event(engine_id, object, event);
    }

    bool bind_preview_camera(EditorApp& app)
    {
        TropicFollowConfig follow_config;
        set_vec3(follow_config.camera_offset, 0.0f, 4.0f, 10.0f);
        set_vec3(follow_config.target_offset, 0.0f, 1.0f, 0.0f);
        follow_config.space = FOLLOW_WORLD_SPACE;
        return Tropic_bindCameraToObject(app.engine, app.camera_id, app.preview.player_id, &follow_config);
    }

    bool setup_preview_callbacks(EditorApp& app)
    {
        app.preview.event_trigger_states.clear();
        app.preview.event_trigger_states.reserve(app.object_ids.size());

        for (size_t i = 0; i < app.object_ids.size(); ++i)
        {
            if (app.objects[i].type != TYPE_EVENT)
            {
                continue;
            }

            PreviewEventTriggerState state;
            state.player_id = app.preview.player_id;
            app.preview.event_trigger_states.push_back(state);
            if (!Tropic_setObjectCollisionCallback(app.engine,
                                                   app.object_ids[i],
                                                   preview_event_callback,
                                                   &app.preview.event_trigger_states.back()))
            {
                return false;
            }
        }

        return true;
    }

    void reset_preview(EditorApp& app)
    {
        app.preview.player_id = 0;
        app.preview.event_trigger_states.clear();
        app.preview.current_exact_beat = 0.0f;
        app.preview.current_beat_time.beat = 0;
        app.preview.current_beat_time.substep = 0;
        app.preview.config.move_speed = 5.0f;
        app.preview.config.forward_speed = 16.0f;
        app.preview.config.jump_speed = 9.0f;
        app.preview.config.fixed_delta = 1.0f / 120.0f;
        app.preview.config.jump_buffer_time = 0.12f;
        app.preview.config.coyote_time = 0.08f;
        std::memset(&app.preview.loop_state, 0, sizeof(app.preview.loop_state));
    }

    void set_lateral_velocity(Object* object, vec3 desired_velocity, vec3 up, float response)
    {
        vec3 vertical_velocity;
        vec3 lateral_velocity;
        vec3 delta;
        float vertical_speed;

        if (!object)
        {
            return;
        }

        vertical_speed = glm_vec3_dot(object->body.velocity, up);
        glm_vec3_scale(up, vertical_speed, vertical_velocity);
        glm_vec3_sub(object->body.velocity, vertical_velocity, lateral_velocity);
        glm_vec3_sub(desired_velocity, lateral_velocity, delta);
        glm_vec3_scale(delta, response, delta);
        glm_vec3_add(lateral_velocity, delta, lateral_velocity);
        glm_vec3_add(lateral_velocity, vertical_velocity, object->body.velocity);
    }

    bool player_jump(EditorApp& app, float jump_speed)
    {
        Object* object = Tropic_getObject(app.engine, app.preview.player_id);
        vec3 gravity;
        vec3 up;
        vec3 vertical_velocity;
        vec3 planar_velocity;
        float vertical_speed;

        if (!object || !object->body.enabled || !object->body.is_grounded || jump_speed <= 0.0f)
        {
            return false;
        }

        Tropic_getSceneGravity(app.engine, gravity);
        if (glm_vec3_norm2(gravity) <= 0.000001f)
        {
            return false;
        }

        glm_vec3_normalize(gravity);
        glm_vec3_negate_to(gravity, up);
        vertical_speed = glm_vec3_dot(object->body.velocity, up);
        glm_vec3_scale(up, vertical_speed, vertical_velocity);
        glm_vec3_sub(object->body.velocity, vertical_velocity, planar_velocity);
        glm_vec3_scale(up, jump_speed, vertical_velocity);
        glm_vec3_add(planar_velocity, vertical_velocity, object->body.velocity);
        object->body.is_grounded = false;
        object->body.support_object_id = 0;
        return true;
    }

    bool step_preview_player(EditorApp& app, float time_scale)
    {
        PreviewLoopState& loop = app.preview.loop_state;
        const PreviewConfig& config = app.preview.config;

        while (loop.physics_accumulator >= config.fixed_delta)
        {
            Object* player_object = Tropic_getObject(app.engine, app.preview.player_id);
            vec3 right;
            vec3 up;
            vec3 forward;
            vec3 desired_velocity;
            float step = config.fixed_delta * time_scale;
            float response;
            vec3 reference_forward;
            vec3 world_right;

            if (!player_object)
            {
                return false;
            }

            set_vec3(reference_forward, 0.0f, 0.0f, -1.0f);
            set_vec3(desired_velocity, 0.0f, 0.0f, 0.0f);
            set_vec3(world_right, 1.0f, 0.0f, 0.0f);

            if (!Tropic_buildControlBasis(app.engine, reference_forward, right, up, forward))
            {
                return false;
            }

            if (glm_vec3_dot(right, world_right) < 0.0f)
            {
                glm_vec3_negate(right);
            }

            glm_vec3_scale(forward, config.forward_speed * time_scale, desired_velocity);
            if (key_down(GLFW_KEY_A))
            {
                glm_vec3_muladds(right, -config.move_speed * time_scale, desired_velocity);
            }
            if (key_down(GLFW_KEY_D))
            {
                glm_vec3_muladds(right, config.move_speed * time_scale, desired_velocity);
            }

            response = player_object->body.is_grounded ? player_object->body.ground_friction : player_object->body.air_friction;
            if (response * step > 1.0f)
            {
                response = 1.0f / step;
            }
            set_lateral_velocity(player_object, desired_velocity, up, response * step);

            (void)Tropic_stepPhysics(app.engine, step);

            if (loop.jump_buffer_timer > 0.0f)
            {
                loop.jump_buffer_timer -= step;
                if (loop.jump_buffer_timer < 0.0f)
                {
                    loop.jump_buffer_timer = 0.0f;
                }
            }

            if (player_object->body.is_grounded)
            {
                loop.coyote_timer = config.coyote_time;
            }
            else if (loop.coyote_timer > 0.0f)
            {
                loop.coyote_timer -= step;
                if (loop.coyote_timer < 0.0f)
                {
                    loop.coyote_timer = 0.0f;
                }
            }

            if (loop.jump_buffer_timer > 0.0f &&
                loop.coyote_timer > 0.0f &&
                player_jump(app, config.jump_speed * time_scale))
            {
                loop.jump_buffer_timer = 0.0f;
                loop.coyote_timer = 0.0f;
            }

            loop.physics_accumulator -= config.fixed_delta;
        }

        return true;
    }

    void update_play_speed(EditorApp& app, double delta_time)
    {
        TropicGameState* state = Tropic_getGameState(app.engine);
        double& timer = app.preview.loop_state.speed_adjust_timer;

        if (!state)
        {
            return;
        }

        if (timer > 0.0)
        {
            timer -= delta_time;
        }

        if (timer > 0.0)
        {
            return;
        }

        if (key_down(GLFW_KEY_MINUS))
        {
            state->play_speed = std::max(0.25f, state->play_speed - 0.25f);
            timer = 0.15;
        }
        else if (key_down(GLFW_KEY_EQUAL))
        {
            state->play_speed = std::min(4.0f, state->play_speed + 0.25f);
            timer = 0.15;
        }
    }

    bool initialize_engine(EditorApp& app)
    {
        bool preview_mode = app.mode == EditorMode::Preview;

        reset_preview(app);
        app.engine = Tropic_create();
        if (app.engine == 0)
        {
            return false;
        }

        if (!Tropic_setActiveEngine(app.engine))
        {
            return false;
        }

        if (!Tropic_CreateWindow(app.engine, 1440, 900, "Tropic Level Editor", false))
        {
            return false;
        }

        Tropic_enableVSync(app.engine, true);
        if (!Tropic_setKeyCallback(app.engine, reinterpret_cast<void*>(key_callback)))
        {
            return false;
        }

        if (!init_materials(app.engine, app.render))
        {
            return false;
        }

        if (!build_scene(app))
        {
            return false;
        }

        Tropic_enableBeatGridDebug(app.engine, app.show_beat_grid_debug);

        if (!preview_mode && !create_editor_gizmo(app))
        {
            return false;
        }

        app.camera_id = Tropic_getActiveCameraId(app.engine);
        if (app.camera_id == 0)
        {
            return false;
        }

        if (!preview_mode)
        {
            update_edit_camera(app, 0.0f);
            update_editor_gizmo(app);
        }
        else
        {
            if (!create_player(app))
            {
                return false;
            }
            if (!setup_preview_callbacks(app))
            {
                return false;
            }
            if (!bind_preview_camera(app))
            {
                return false;
            }
            if (Tropic_getGameState(app.engine))
            {
                Tropic_getGameState(app.engine)->play_speed = static_cast<float>(app.metadata.play_speed > 0.0 ? app.metadata.play_speed : 1.0);
                if (Tropic_getGameState(app.engine)->music_path) free(Tropic_getGameState(app.engine)->music_path);
                Tropic_getGameState(app.engine)->music_path = _strdup(app.metadata.music_path.c_str());
            }
            if (!app.metadata.music_path.empty())
            {
                if (!Tropic_LoadMusic(app.engine, app.metadata.music_path.c_str()))
                {
                    return false;
                }
                if (!Tropic_StopMusic(app.engine) || !Tropic_PlayMusic(app.engine))
                {
                    return false;
                }
            }
            app.preview.loop_state.last_time = Tropic_getTime();
        }

        app.initialized = true;
        mark_ui_dirty(app);
        return true;
    }

    void shutdown_engine(EditorApp& app)
    {
        if (app.engine != 0)
        {
            Tropic_destroy(app.engine);
            app.engine = 0;
        }
        app.camera_id = 0;
        app.object_ids.clear();
        app.initialized = false;
        mark_ui_dirty(app);
    }

    void queue_mode_switch(EditorApp& app, EditorMode mode)
    {
        app.pending_mode_switch = true;
        app.requested_mode = mode;
    }

    void print_controls_once(EditorApp& app)
    {
        if (app.controls_printed)
        {
            return;
        }

        std::cout
            << "Tropic Level Editor controls\n"
            << "  Use the Tools window for object lists, buttons, and text fields\n"
            << "  Tab / Shift+Tab : select next or previous object\n"
            << "  1/2/3/4         : add platform, spike, jumppad, event\n"
            << "  G/R/T           : move, rotate, scale tool\n"
            << "  X/Y/Z           : active axis\n"
            << "  [ and ]         : nudge the selected object's current tool value on the active axis\n"
            << "  Arrow keys      : orbit camera\n"
            << "  W/A/S/D         : pan camera forward/left/back/right\n"
            << "  Q/E             : pan camera down/up (hold Shift for faster pan)\n"
            << "  PageUp/PageDown : zoom camera\n"
            << "  Left mouse      : drag selected gizmo axis to move/rotate/scale the selected object\n"
            << "  B               : toggle beat-grid debug rendering\n"
            << "  F2              : save level JSON\n"
            << "  F5              : toggle play preview\n"
            << "  Preview: A/D move, Space jump, P pause, +/- play speed\n";
        app.controls_printed = true;
    }

    void sync_selected_object(EditorApp& app)
    {
        Object* runtime_object;
        vec3 resolved_position;

        if (!has_selection(app))
        {
            return;
        }

        if (app.objects[app.selected_index].placement.space == TROPIC_PLACEMENT_SPACE_TRACK &&
            Tropic_resolvePlacementPosition(app.engine,
                                            Tropic_getCurrentSceneID(app.engine),
                                            &app.objects[app.selected_index].placement,
                                            resolved_position))
        {
            copy_vec3(resolved_position, app.objects[app.selected_index].position);
        }

        (void)Tropic_setObjectPosition(app.engine,
                                       app.object_ids[app.selected_index],
                                       app.objects[app.selected_index].position);
        (void)Tropic_setObjectRotation(app.engine,
                                       app.object_ids[app.selected_index],
                                       app.objects[app.selected_index].rotation);
        (void)Tropic_setObjectScale(app.engine,
                                    app.object_ids[app.selected_index],
                                    app.objects[app.selected_index].scale);

        runtime_object = Tropic_getObject(app.engine, app.object_ids[app.selected_index]);
        if (runtime_object)
        {
            std::strncpy(runtime_object->uid, app.objects[app.selected_index].uid.c_str(), sizeof(runtime_object->uid));
            runtime_object->uid[sizeof(runtime_object->uid) - 1] = '\0';
            runtime_object->event = app.objects[app.selected_index].event;
            runtime_object->event.has_fired = false;
        }

        mark_ui_dirty(app);
    }

    void refresh_track_placement_positions(EditorApp& app)
    {
        SceneID scene_id;

        if (!app.initialized)
        {
            return;
        }

        scene_id = Tropic_getCurrentSceneID(app.engine);
        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            vec3 resolved_position;

            if (app.objects[i].placement.space != TROPIC_PLACEMENT_SPACE_TRACK)
            {
                continue;
            }

            if (!Tropic_resolvePlacementPosition(app.engine,
                                                 scene_id,
                                                 &app.objects[i].placement,
                                                 resolved_position))
            {
                continue;
            }

            copy_vec3(resolved_position, app.objects[i].position);
            if (i < app.object_ids.size())
            {
                (void)Tropic_setObjectPosition(app.engine, app.object_ids[i], app.objects[i].position);
            }
        }

        mark_ui_dirty(app);
    }

    void adjust_selected_object(EditorApp& app, float direction)
    {
        EditorObject* object = NULL;
        const int axis = static_cast<int>(app.axis);

        if (!has_selection(app))
        {
            return;
        }

        object = &app.objects[app.selected_index];
        switch (app.tool)
        {
        case EditorTool::Move:
            if (nudge_selected_track_placement(app, axis, direction))
            {
                return;
            }
            object->position[axis] += direction * kEditorMoveStep;
            break;
        case EditorTool::Rotate:
            object->rotation[axis] += direction * kEditorRotateStep;
            break;
        case EditorTool::Scale:
            object->scale[axis] = std::max(0.1f, object->scale[axis] + direction * kEditorScaleStep);
            break;
        }

        sync_selected_object(app);
    }

    void select_next_object(EditorApp& app, bool reverse)
    {
        if (app.objects.empty())
        {
            app.selected_index = kNoSelection;
            return;
        }

        if (app.selected_index == kNoSelection)
        {
            app.selected_index = 0;
        }
        else if (reverse)
        {
            app.selected_index = (app.selected_index == 0) ? app.objects.size() - 1 : app.selected_index - 1;
        }
        else
        {
            app.selected_index = (app.selected_index + 1) % app.objects.size();
        }

        update_selection_materials(app);
        mark_ui_dirty(app);
    }

    bool duplicate_selected_object(EditorApp& app)
    {
        EditorObject object;
        ObjectID object_id = 0;

        if (!has_selection(app))
        {
            return false;
        }

        object = app.objects[app.selected_index];
        object.uid = make_unique_uid(app, object.type);
        if (object.placement.space == TROPIC_PLACEMENT_SPACE_TRACK)
        {
            offset_beat_time(object.placement.time, editor_subdivisions_per_beat(app), editor_subdivisions_per_beat(app));
            if (object.placement.snap_x)
            {
                object.placement.track_x = snap_to_step(object.placement.track_x + editor_track_snap_step_x(app), editor_track_snap_step_x(app));
            }
            else
            {
                object.placement.track_x += editor_track_snap_step_x(app);
            }
        }
        else
        {
            object.position[0] += 1.0f;
            object.position[2] += 1.0f;
        }

        app.objects.push_back(object);
        if (!create_runtime_object(app, app.objects.back(), object_id))
        {
            app.objects.pop_back();
            return false;
        }

        app.object_ids.push_back(object_id);
        app.selected_index = app.objects.size() - 1;
        update_selection_materials(app);
        mark_ui_dirty(app);
        return true;
    }

    bool delete_selected_object(EditorApp& app)
    {
        if (!has_selection(app))
        {
            return false;
        }

        (void)Tropic_freeObject(app.engine, app.object_ids[app.selected_index]);
        app.objects.erase(app.objects.begin() + static_cast<std::ptrdiff_t>(app.selected_index));
        app.object_ids.erase(app.object_ids.begin() + static_cast<std::ptrdiff_t>(app.selected_index));

        if (app.objects.empty())
        {
            app.selected_index = kNoSelection;
        }
        else if (app.selected_index >= app.objects.size())
        {
            app.selected_index = app.objects.size() - 1;
        }

        update_selection_materials(app);
        mark_ui_dirty(app);
        return true;
    }

    bool add_object(EditorApp& app, ObjectType type)
    {
        EditorObject object = make_editor_object(type);
        vec3 focus;
        ObjectID object_id = 0;

        ensure_camera_focus_initialized(app);
        copy_vec3(app.camera_focus_point, focus);
        copy_vec3(focus, object.position);
        object.position[1] += 1.0f;
        object.uid = make_unique_uid(app, type);
        if (type == TYPE_PLATFORM)
        {
            object.position[1] -= 1.0f;
        }
        seed_track_placement_from_selection(app, object);

        app.objects.push_back(object);
        if (!create_runtime_object(app, app.objects.back(), object_id))
        {
            app.objects.pop_back();
            return false;
        }

        app.object_ids.push_back(object_id);
        app.selected_index = app.objects.size() - 1;
        update_selection_materials(app);
        mark_ui_dirty(app);
        return true;
    }

    void handle_edit_shortcuts(EditorApp& app)
    {
        if (key_pressed(GLFW_KEY_TAB))
        {
            select_next_object(app, shift_down());
        }
        if (key_pressed(GLFW_KEY_G)) app.tool = EditorTool::Move;
        if (key_pressed(GLFW_KEY_R)) app.tool = EditorTool::Rotate;
        if (key_pressed(GLFW_KEY_T)) app.tool = EditorTool::Scale;
        if (key_pressed(GLFW_KEY_X)) app.axis = EditorAxis::X;
        if (key_pressed(GLFW_KEY_Y)) app.axis = EditorAxis::Y;
        if (key_pressed(GLFW_KEY_Z)) app.axis = EditorAxis::Z;
        if (key_pressed(GLFW_KEY_LEFT_BRACKET)) adjust_selected_object(app, -1.0f);
        if (key_pressed(GLFW_KEY_RIGHT_BRACKET)) adjust_selected_object(app, 1.0f);
        if (key_pressed(GLFW_KEY_1)) (void)add_object(app, TYPE_PLATFORM);
        if (key_pressed(GLFW_KEY_2)) (void)add_object(app, TYPE_SPIKE);
        if (key_pressed(GLFW_KEY_3)) (void)add_object(app, TYPE_JUMPPAD);
        if (key_pressed(GLFW_KEY_4)) (void)add_object(app, TYPE_EVENT);
        if (key_pressed(GLFW_KEY_B)) {
            app.show_beat_grid_debug = !app.show_beat_grid_debug;
            Tropic_enableBeatGridDebug(app.engine, app.show_beat_grid_debug);
        }

        if (key_pressed(GLFW_KEY_F2) && save_level_model(app))
        {
            std::cout << "Saved level to " << app.level_path << "\n";
        }
        if (key_pressed(GLFW_KEY_F5))
        {
            queue_mode_switch(app, EditorMode::Preview);
        }
    }

    bool update_preview(EditorApp& app)
    {
        PreviewLoopState& loop = app.preview.loop_state;
        double current_time = Tropic_getTime();
        double delta_time = current_time - loop.last_time;
        TropicGameState* state = Tropic_getGameState(app.engine);
        bool jump_pressed = key_down(GLFW_KEY_SPACE);
        bool jump_requested = jump_pressed && !loop.jump_was_down;
        bool pause_pressed = key_down(GLFW_KEY_P);
        bool pause_requested = pause_pressed && !loop.pause_was_down;
        float time_scale = state && state->play_speed > 0.0f ? state->play_speed : 1.0f;

        loop.last_time = current_time;

        if (pause_requested)
        {
            loop.paused = !loop.paused;
            if (loop.paused)
            {
                loop.physics_accumulator = 0.0;
                if (!app.metadata.music_path.empty())
                {
                    (void)Tropic_PauseMusic(app.engine);
                }
            }
            else
            {
                if (!app.metadata.music_path.empty())
                {
                    (void)Tropic_PlayMusic(app.engine);
                }
            }
        }

        update_play_speed(app, delta_time);

        (void)Tropic_getCurrentSceneMusicBeatTime(app.engine,
                                                  &app.preview.current_beat_time,
                                                  &app.preview.current_exact_beat);

        if (!loop.paused && jump_requested)
        {
            loop.jump_buffer_timer = app.preview.config.jump_buffer_time;
        }

        if (!loop.paused)
        {
            loop.physics_accumulator += delta_time;
        }

        if (!loop.paused && !step_preview_player(app, time_scale))
        {
            return false;
        }

        loop.jump_was_down = jump_pressed;
        loop.pause_was_down = pause_pressed;

        if (key_pressed(GLFW_KEY_F5))
        {
            queue_mode_switch(app, EditorMode::Edit);
        }

        return true;
    }

    void snapshot_keys()
    {
        std::memcpy(g_prev_keys, g_keys, sizeof(g_keys));
    }

#ifdef _WIN32
    std::string describe_object_list_item(const EditorObject& object)
    {
        std::ostringstream stream;
        stream << object.uid << " [" << object.type_name << ']';
        return stream.str();
    }

    void sync_panel_from_app(EditorApp& app)
    {
        int selection = LB_ERR;
        int anchor_selection = LB_ERR;
        bool event_selected = false;

        if (!g_editor_panel.window || !app.ui_dirty)
        {
            return;
        }

        g_editor_panel.suppress_events = true;

        set_window_text(g_editor_panel.metadata_game_title, app.metadata.game_title);
        set_window_text(g_editor_panel.metadata_level_name, app.metadata.level_name);
        set_window_text(g_editor_panel.metadata_music_path, app.metadata.music_path);
        set_edit_double(g_editor_panel.metadata_play_speed, app.metadata.play_speed);
        set_edit_float(g_editor_panel.metadata_bpm, app.metadata.beat_grid.bpm);
        set_edit_float(g_editor_panel.metadata_subdivisions, static_cast<float>(app.metadata.beat_grid.subdivisions_per_beat));
        set_edit_float(g_editor_panel.metadata_units_per_beat, app.metadata.beat_grid.units_per_beat);

        SendMessageA(g_editor_panel.object_list, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < app.objects.size(); ++i)
        {
            const std::string label = describe_object_list_item(app.objects[i]);
            SendMessageA(g_editor_panel.object_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        SendMessageA(g_editor_panel.anchor_list, LB_RESETCONTENT, 0, 0);
        for (size_t i = 0; i < app.track_anchors.size(); ++i)
        {
            const std::string label = describe_track_anchor_list_item(app.track_anchors[i]);
            SendMessageA(g_editor_panel.anchor_list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(label.c_str()));
        }
        if (app.selected_anchor_index < app.track_anchors.size())
        {
            anchor_selection = static_cast<int>(app.selected_anchor_index);
            SendMessageA(g_editor_panel.anchor_list, LB_SETCURSEL, static_cast<WPARAM>(anchor_selection), 0);
            set_edit_float(g_editor_panel.anchor_beat_edit, static_cast<float>(app.track_anchors[app.selected_anchor_index].start_time.beat));
            set_edit_float(g_editor_panel.anchor_substep_edit, static_cast<float>(app.track_anchors[app.selected_anchor_index].start_time.substep));
            set_edit_float(g_editor_panel.anchor_pivot_x_edit, app.track_anchors[app.selected_anchor_index].pivot_x);
            set_edit_float(g_editor_panel.anchor_pivot_y_edit, app.track_anchors[app.selected_anchor_index].pivot_y);
            set_edit_float(g_editor_panel.anchor_pivot_beat_edit, app.track_anchors[app.selected_anchor_index].pivot_beat);
            for (int i = 0; i < 3; ++i)
            {
                set_edit_float(g_editor_panel.anchor_axis_edits[i], app.track_anchors[app.selected_anchor_index].local_axis[i]);
            }
            set_edit_float(g_editor_panel.anchor_degrees_edit, app.track_anchors[app.selected_anchor_index].degrees);
        }
        else
        {
            SendMessageA(g_editor_panel.anchor_list, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
            set_window_text(g_editor_panel.anchor_beat_edit, "");
            set_window_text(g_editor_panel.anchor_substep_edit, "");
            set_window_text(g_editor_panel.anchor_pivot_x_edit, "");
            set_window_text(g_editor_panel.anchor_pivot_y_edit, "");
            set_window_text(g_editor_panel.anchor_pivot_beat_edit, "");
            for (int i = 0; i < 3; ++i)
            {
                set_window_text(g_editor_panel.anchor_axis_edits[i], "");
            }
            set_window_text(g_editor_panel.anchor_degrees_edit, "");
        }

        if (app.selected_index < app.objects.size())
        {
            selection = static_cast<int>(app.selected_index);
            SendMessageA(g_editor_panel.object_list, LB_SETCURSEL, static_cast<WPARAM>(selection), 0);

            set_window_text(g_editor_panel.uid_edit, app.objects[app.selected_index].uid);
            for (int i = 0; i < 3; ++i)
            {
                set_edit_float(g_editor_panel.position_edits[i], app.objects[app.selected_index].position[i]);
                set_edit_float(g_editor_panel.scale_edits[i], app.objects[app.selected_index].scale[i]);
                set_edit_float(g_editor_panel.rotation_edits[i], app.objects[app.selected_index].rotation[i]);
            }
            SendMessageA(g_editor_panel.placement_space_combo,
                         CB_SETCURSEL,
                         app.objects[app.selected_index].placement.space == TROPIC_PLACEMENT_SPACE_TRACK ? 1 : 0,
                         0);
            set_edit_float(g_editor_panel.placement_beat_edit, static_cast<float>(app.objects[app.selected_index].placement.time.beat));
            set_edit_float(g_editor_panel.placement_substep_edit, static_cast<float>(app.objects[app.selected_index].placement.time.substep));
            set_edit_float(g_editor_panel.placement_track_x_edit, app.objects[app.selected_index].placement.track_x);
            set_edit_float(g_editor_panel.placement_track_y_edit, app.objects[app.selected_index].placement.track_y);
            set_edit_float(g_editor_panel.placement_length_beats_edit, app.objects[app.selected_index].placement.length_beats);

            event_selected = app.objects[app.selected_index].type == TYPE_EVENT;
            set_event_controls_enabled(event_selected);
            if (event_selected)
            {
                const EditorObject& object = app.objects[app.selected_index];
                SendMessageA(g_editor_panel.event_action_combo, CB_SETCURSEL, static_cast<WPARAM>(find_action_combo_index(object.event.action_type)), 0);
                SendMessageA(g_editor_panel.event_trigger_combo, CB_SETCURSEL, static_cast<WPARAM>(find_trigger_combo_index(object.event.trigger_mode)), 0);
                SendMessageA(g_editor_panel.event_once_check, BM_SETCHECK, object.event.trigger_once ? BST_CHECKED : BST_UNCHECKED, 0);
                set_window_text(g_editor_panel.event_target_uid_edit, object.event.target_uid);
                set_window_text(g_editor_panel.event_function_edit, object.event.custom_function);
                for (int i = 0; i < 3; ++i)
                {
                    set_edit_float(g_editor_panel.event_gravity_edits[i], object.event.gravity[i]);
                    set_edit_float(g_editor_panel.event_axis_edits[i], object.event.axis[i]);
                }
                set_edit_float(g_editor_panel.event_degrees_edit, object.event.degrees);
                set_edit_float(g_editor_panel.event_speed_edit, object.event.speed);
                set_edit_float(g_editor_panel.event_duration_edit, object.event.duration_seconds);
            }
            else
            {
                SendMessageA(g_editor_panel.event_action_combo, CB_SETCURSEL, 0, 0);
                SendMessageA(g_editor_panel.event_trigger_combo, CB_SETCURSEL, 0, 0);
                SendMessageA(g_editor_panel.event_once_check, BM_SETCHECK, BST_UNCHECKED, 0);
                set_window_text(g_editor_panel.event_target_uid_edit, "");
                set_window_text(g_editor_panel.event_function_edit, "");
                for (int i = 0; i < 3; ++i)
                {
                    set_window_text(g_editor_panel.event_gravity_edits[i], "");
                    set_window_text(g_editor_panel.event_axis_edits[i], "");
                }
                set_window_text(g_editor_panel.event_degrees_edit, "");
                set_window_text(g_editor_panel.event_speed_edit, "");
                set_window_text(g_editor_panel.event_duration_edit, "");
            }
        }
        else
        {
            SendMessageA(g_editor_panel.object_list, LB_SETCURSEL, static_cast<WPARAM>(-1), 0);
            set_window_text(g_editor_panel.uid_edit, "");
            for (int i = 0; i < 3; ++i)
            {
                set_window_text(g_editor_panel.position_edits[i], "");
                set_window_text(g_editor_panel.scale_edits[i], "");
                set_window_text(g_editor_panel.rotation_edits[i], "");
                set_window_text(g_editor_panel.event_gravity_edits[i], "");
                set_window_text(g_editor_panel.event_axis_edits[i], "");
            }
            SendMessageA(g_editor_panel.placement_space_combo, CB_SETCURSEL, 0, 0);
            set_window_text(g_editor_panel.placement_beat_edit, "");
            set_window_text(g_editor_panel.placement_substep_edit, "");
            set_window_text(g_editor_panel.placement_track_x_edit, "");
            set_window_text(g_editor_panel.placement_track_y_edit, "");
            set_window_text(g_editor_panel.placement_length_beats_edit, "");
            set_window_text(g_editor_panel.event_target_uid_edit, "");
            set_window_text(g_editor_panel.event_function_edit, "");
            set_window_text(g_editor_panel.event_degrees_edit, "");
            set_window_text(g_editor_panel.event_speed_edit, "");
            set_window_text(g_editor_panel.event_duration_edit, "");
            set_event_controls_enabled(false);
        }

        SetWindowTextA(g_editor_panel.preview_button, app.mode == EditorMode::Preview ? "Return to Edit" : "Enter Preview");
        SetWindowTextA(g_editor_panel.apply_button, format_preview_beat_label(app).c_str());
        g_editor_panel.suppress_events = false;
        app.ui_dirty = false;
    }

    bool apply_panel_to_app(EditorApp& app)
    {
        if (g_editor_panel.suppress_events)
        {
            return true;
        }

        app.metadata.game_title = get_window_text(g_editor_panel.metadata_game_title);
        app.metadata.level_name = get_window_text(g_editor_panel.metadata_level_name);
        app.metadata.music_path = make_workspace_relative_path(get_window_text(g_editor_panel.metadata_music_path));
        app.metadata.play_speed = std::max(0.1, read_double_or_default(g_editor_panel.metadata_play_speed, app.metadata.play_speed));
        app.metadata.beat_grid.bpm = std::max(1.0f, read_float_or_default(g_editor_panel.metadata_bpm, app.metadata.beat_grid.bpm));
        app.metadata.beat_grid.subdivisions_per_beat = std::max(1, static_cast<int>(read_float_or_default(g_editor_panel.metadata_subdivisions, static_cast<float>(app.metadata.beat_grid.subdivisions_per_beat))));
        app.metadata.beat_grid.units_per_beat = std::max(0.1f, read_float_or_default(g_editor_panel.metadata_units_per_beat, app.metadata.beat_grid.units_per_beat));

        if (app.selected_anchor_index < app.track_anchors.size())
        {
            TropicTrackAnchor& anchor = app.track_anchors[app.selected_anchor_index];
            anchor.start_time.beat = static_cast<int32_t>(read_float_or_default(g_editor_panel.anchor_beat_edit, static_cast<float>(anchor.start_time.beat)));
            anchor.start_time.substep = static_cast<int32_t>(read_float_or_default(g_editor_panel.anchor_substep_edit, static_cast<float>(anchor.start_time.substep)));
            anchor.pivot_x = read_float_or_default(g_editor_panel.anchor_pivot_x_edit, anchor.pivot_x);
            anchor.pivot_y = read_float_or_default(g_editor_panel.anchor_pivot_y_edit, anchor.pivot_y);
            anchor.pivot_beat = read_float_or_default(g_editor_panel.anchor_pivot_beat_edit, anchor.pivot_beat);
            for (int i = 0; i < 3; ++i)
            {
                anchor.local_axis[i] = read_float_or_default(g_editor_panel.anchor_axis_edits[i], anchor.local_axis[i]);
            }
            anchor.degrees = read_float_or_default(g_editor_panel.anchor_degrees_edit, anchor.degrees);
        }

        if (has_selection(app))
        {
            EditorObject& object = app.objects[app.selected_index];
            const std::string previous_uid = object.uid;
            std::string candidate_uid = get_window_text(g_editor_panel.uid_edit);

            if (!uid_available_for_index(app, candidate_uid, app.selected_index))
            {
                MessageBoxA(g_editor_panel.window,
                            "UIDs must be non-empty and unique across the whole level.",
                            "Invalid UID",
                            MB_OK | MB_ICONWARNING);
                return false;
            }

            object.uid = candidate_uid;
            for (int i = 0; i < 3; ++i)
            {
                object.position[i] = read_float_or_default(g_editor_panel.position_edits[i], object.position[i]);
                object.scale[i] = std::max(0.1f, read_float_or_default(g_editor_panel.scale_edits[i], object.scale[i]));
                object.rotation[i] = read_float_or_default(g_editor_panel.rotation_edits[i], object.rotation[i]);
            }
            object.placement.space = static_cast<int>(SendMessageA(g_editor_panel.placement_space_combo, CB_GETCURSEL, 0, 0)) == 1
                ? TROPIC_PLACEMENT_SPACE_TRACK
                : TROPIC_PLACEMENT_SPACE_WORLD;
            object.placement.time.beat = static_cast<int32_t>(read_float_or_default(g_editor_panel.placement_beat_edit, static_cast<float>(object.placement.time.beat)));
            object.placement.time.substep = static_cast<int32_t>(read_float_or_default(g_editor_panel.placement_substep_edit, static_cast<float>(object.placement.time.substep)));
            object.placement.track_x = read_float_or_default(g_editor_panel.placement_track_x_edit, object.placement.track_x);
            object.placement.track_y = read_float_or_default(g_editor_panel.placement_track_y_edit, object.placement.track_y);
            object.placement.length_beats = read_float_or_default(g_editor_panel.placement_length_beats_edit, object.placement.length_beats);

            if (object.type == TYPE_EVENT)
            {
                int action_index = static_cast<int>(SendMessageA(g_editor_panel.event_action_combo, CB_GETCURSEL, 0, 0));
                int trigger_index = static_cast<int>(SendMessageA(g_editor_panel.event_trigger_combo, CB_GETCURSEL, 0, 0));
                std::string target_uid = get_window_text(g_editor_panel.event_target_uid_edit);

                if (action_index < 0) action_index = 0;
                if (trigger_index < 0) trigger_index = 0;
                object.event.action_type = kEditorActionTypes[action_index];
                object.event.trigger_mode = kEditorTriggerTypes[trigger_index];
                object.event.trigger_once = SendMessageA(g_editor_panel.event_once_check, BM_GETCHECK, 0, 0) == BST_CHECKED;
                object.event.degrees = read_float_or_default(g_editor_panel.event_degrees_edit, object.event.degrees);
                object.event.speed = read_float_or_default(g_editor_panel.event_speed_edit, object.event.speed);
                object.event.duration_seconds = read_float_or_default(g_editor_panel.event_duration_edit, object.event.duration_seconds);
                for (int i = 0; i < 3; ++i)
                {
                    object.event.gravity[i] = read_float_or_default(g_editor_panel.event_gravity_edits[i], object.event.gravity[i]);
                    object.event.axis[i] = read_float_or_default(g_editor_panel.event_axis_edits[i], object.event.axis[i]);
                }
                std::strncpy(object.event.custom_function, get_window_text(g_editor_panel.event_function_edit).c_str(), sizeof(object.event.custom_function));
                object.event.custom_function[sizeof(object.event.custom_function) - 1] = '\0';
                if (target_uid == "self")
                {
                    target_uid = object.uid;
                }
                std::strncpy(object.event.target_uid, target_uid.c_str(), sizeof(object.event.target_uid));
                object.event.target_uid[sizeof(object.event.target_uid) - 1] = '\0';
            }

            if (object.type != TYPE_EVENT)
            {
                std::memset(&object.event, 0, sizeof(object.event));
                object.event = make_default_event_spec();
            }

            if (!previous_uid.empty())
            {
                for (size_t i = 0; i < app.objects.size(); ++i)
                {
                    if (i != app.selected_index && app.objects[i].type == TYPE_EVENT && std::strcmp(app.objects[i].event.target_uid, previous_uid.c_str()) == 0)
                    {
                        std::strncpy(app.objects[i].event.target_uid, object.uid.c_str(), sizeof(app.objects[i].event.target_uid));
                        app.objects[i].event.target_uid[sizeof(app.objects[i].event.target_uid) - 1] = '\0';
                    }
                }
            }

            if (app.initialized)
            {
                sync_selected_object(app);
            }
        }

        if (app.initialized && Tropic_getGameState(app.engine))
        {
            TropicGameState* state = Tropic_getGameState(app.engine);
            SceneID scene_id = Tropic_getCurrentSceneID(app.engine);
            Tropic_getGameState(app.engine)->play_speed = static_cast<float>(app.metadata.play_speed);
            if (state->music_path) free(state->music_path);
            state->music_path = _strdup(app.metadata.music_path.c_str());
            (void)Tropic_setSceneBeatGridSettings(app.engine, scene_id, &app.metadata.beat_grid);
            (void)Tropic_clearSceneTrackAnchors(app.engine, scene_id);
            for (size_t i = 0; i < app.track_anchors.size(); ++i)
            {
                (void)Tropic_addSceneTrackAnchor(app.engine, scene_id, &app.track_anchors[i]);
            }
            refresh_track_placement_positions(app);
        }

        mark_ui_dirty(app);
        return true;
    }

    void reload_level_into_app(EditorApp& app)
    {
        if (!load_level_model(app))
        {
            MessageBoxA(g_editor_panel.window,
                        "Failed to reload the level from disk. Check the JSON for duplicate UIDs or malformed data.",
                        "Reload Failed",
                        MB_OK | MB_ICONERROR);
            return;
        }

        mark_ui_dirty(app);
        queue_mode_switch(app, app.mode);
    }

    void open_level_from_dialog(EditorApp& app)
    {
        std::string path;

        if (!browse_for_file(g_editor_panel.window,
                             "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0",
                             "json",
                             "Open Level JSON",
                             false,
                             path))
        {
            return;
        }

        app.level_path = path;
        reload_level_into_app(app);
    }

    void save_level_as_from_dialog(EditorApp& app)
    {
        std::string path;

        if (!apply_panel_to_app(app))
        {
            return;
        }

        if (!browse_for_file(g_editor_panel.window,
                             "JSON Files (*.json)\0*.json\0All Files (*.*)\0*.*\0",
                             "json",
                             "Save Level JSON As",
                             true,
                             path))
        {
            return;
        }

        app.level_path = path;
        if (save_level_model(app))
        {
            MessageBoxA(g_editor_panel.window, "Level saved to the selected file.", "Save Complete", MB_OK | MB_ICONINFORMATION);
        }
    }

    void browse_music_for_app(EditorApp& app)
    {
        std::string path;

        if (!browse_for_file(g_editor_panel.window,
                             "Audio Files (*.mp3;*.wav;*.ogg;*.flac)\0*.mp3;*.wav;*.ogg;*.flac\0All Files (*.*)\0*.*\0",
                             "mp3",
                             "Choose Music File",
                             false,
                             path))
        {
            return;
        }

        app.metadata.music_path = make_workspace_relative_path(path);
        set_window_text(g_editor_panel.metadata_music_path, app.metadata.music_path);
        mark_ui_dirty(app);
        (void)apply_panel_to_app(app);
    }

    void handle_editor_panel_command(EditorApp& app, int control_id, int notification_code)
    {
        if (g_editor_panel.suppress_events)
        {
            return;
        }

        if (is_live_apply_control(control_id, notification_code))
        {
            (void)apply_panel_to_app(app);
            return;
        }

        switch (control_id)
        {
        case IDC_ANCHOR_LIST:
            if (notification_code == LBN_SELCHANGE)
            {
                int selection = static_cast<int>(SendMessageA(g_editor_panel.anchor_list, LB_GETCURSEL, 0, 0));
                if (selection >= 0)
                {
                    app.selected_anchor_index = static_cast<size_t>(selection);
                    mark_ui_dirty(app);
                }
            }
            break;
        case IDC_OBJECT_LIST:
            if (notification_code == LBN_SELCHANGE)
            {
                int selection = static_cast<int>(SendMessageA(g_editor_panel.object_list, LB_GETCURSEL, 0, 0));
                if (selection >= 0)
                {
                    app.selected_index = static_cast<size_t>(selection);
                    update_selection_materials(app);
                    mark_ui_dirty(app);
                }
            }
            break;
        case IDC_ADD_ANCHOR:
            app.track_anchors.push_back(make_default_track_anchor());
            app.selected_anchor_index = app.track_anchors.size() - 1;
            (void)apply_panel_to_app(app);
            break;
        case IDC_DELETE_ANCHOR:
            if (app.selected_anchor_index < app.track_anchors.size())
            {
                app.track_anchors.erase(app.track_anchors.begin() + static_cast<std::ptrdiff_t>(app.selected_anchor_index));
                if (app.track_anchors.empty())
                {
                    app.selected_anchor_index = kNoSelection;
                }
                else if (app.selected_anchor_index >= app.track_anchors.size())
                {
                    app.selected_anchor_index = app.track_anchors.size() - 1;
                }
                (void)apply_panel_to_app(app);
            }
            break;
        case IDC_ADD_PLATFORM:
            (void)add_object(app, TYPE_PLATFORM);
            break;
        case IDC_ADD_SPIKE:
            (void)add_object(app, TYPE_SPIKE);
            break;
        case IDC_ADD_JUMPPAD:
            (void)add_object(app, TYPE_JUMPPAD);
            break;
        case IDC_ADD_EVENT:
            (void)add_object(app, TYPE_EVENT);
            break;
        case IDC_DUPLICATE_OBJECT:
            (void)duplicate_selected_object(app);
            break;
        case IDC_DELETE_OBJECT:
            (void)delete_selected_object(app);
            break;
        case IDC_SAVE_LEVEL:
            if (apply_panel_to_app(app) && save_level_model(app))
            {
                MessageBoxA(g_editor_panel.window, "Level saved.", "Save Complete", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case IDC_SAVE_AS_LEVEL:
            save_level_as_from_dialog(app);
            break;
        case IDC_OPEN_LEVEL:
            open_level_from_dialog(app);
            break;
        case IDC_RELOAD_LEVEL:
            reload_level_into_app(app);
            break;
        case IDC_TOGGLE_PREVIEW:
            if (apply_panel_to_app(app))
            {
                queue_mode_switch(app, app.mode == EditorMode::Preview ? EditorMode::Edit : EditorMode::Preview);
            }
            break;
        case IDC_APPLY_CHANGES:
            (void)apply_panel_to_app(app);
            break;
        case IDC_STAMP_TO_CURRENT_BEAT:
            if (app.mode == EditorMode::Preview)
            {
                (void)stamp_selected_object_to_current_preview_beat(app);
            }
            break;
        case IDC_AUTO_UID:
            if (has_selection(app))
            {
                app.objects[app.selected_index].uid = make_unique_uid(app, app.objects[app.selected_index].type);
                if (app.initialized)
                {
                    sync_selected_object(app);
                }
                mark_ui_dirty(app);
            }
            break;
        case IDC_BROWSE_MUSIC:
            browse_music_for_app(app);
            break;
        case IDC_MOVE_X_NEG:
            nudge_selected_position(app, 0, -kEditorMoveStep);
            break;
        case IDC_MOVE_X_POS:
            nudge_selected_position(app, 0, kEditorMoveStep);
            break;
        case IDC_MOVE_Y_NEG:
            nudge_selected_position(app, 1, -kEditorMoveStep);
            break;
        case IDC_MOVE_Y_POS:
            nudge_selected_position(app, 1, kEditorMoveStep);
            break;
        case IDC_MOVE_Z_NEG:
            nudge_selected_position(app, 2, -kEditorMoveStep);
            break;
        case IDC_MOVE_Z_POS:
            nudge_selected_position(app, 2, kEditorMoveStep);
            break;
        default:
            break;
        }
    }

    LRESULT CALLBACK editor_panel_proc(HWND hwnd, UINT message, WPARAM w_param, LPARAM l_param)
    {
        (void)l_param;

        switch (message)
        {
        case WM_COMMAND:
            if (g_editor_panel_app)
            {
                handle_editor_panel_command(*g_editor_panel_app, LOWORD(w_param), HIWORD(w_param));
            }
            return 0;
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
        default:
            return DefWindowProcA(hwnd, message, w_param, l_param);
        }
    }

    bool create_editor_panel(EditorApp& app)
    {
        WNDCLASSA window_class = {};
        HINSTANCE instance = GetModuleHandleA(NULL);
        HWND window;
        const DWORD window_style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        const DWORD window_ex_style = 0;
        const RECT window_rect = make_window_rect_for_client_size(window_style, window_ex_style, 640, 930);

        if (g_editor_panel.window)
        {
            g_editor_panel_app = &app;
            return true;
        }

        window_class.lpfnWndProc = editor_panel_proc;
        window_class.hInstance = instance;
        window_class.lpszClassName = "TropicLevelEditorPanel";
        window_class.hCursor = LoadCursor(NULL, IDC_ARROW);
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassA(&window_class);

        window = CreateWindowExA(window_ex_style,
                                 window_class.lpszClassName,
                                 "Tropic Level Editor Tools",
                                 window_style,
                                 40,
                                 40,
                                 window_rect.right - window_rect.left,
                                 window_rect.bottom - window_rect.top,
                                 NULL,
                                 NULL,
                                 instance,
                                 NULL);
        if (!window)
        {
            return false;
        }

        g_editor_panel.window = window;
        g_editor_panel_app = &app;

        create_label(window, "Game Title", 12, 12, 90, 20);
        g_editor_panel.metadata_game_title = create_edit(window, IDC_METADATA_GAME_TITLE, 110, 10, 180, 24);
        create_label(window, "Level Name", 12, 42, 90, 20);
        g_editor_panel.metadata_level_name = create_edit(window, IDC_METADATA_LEVEL_NAME, 110, 40, 180, 24);
        create_label(window, "Music File", 300, 12, 80, 20);
        g_editor_panel.metadata_music_path = create_edit(window, IDC_METADATA_MUSIC_PATH, 378, 10, 108, 24);
        g_editor_panel.browse_music_button = create_button(window, "Browse...", IDC_BROWSE_MUSIC, 492, 10, 68, 24);
        create_label(window, "Play Speed", 300, 42, 80, 20);
        g_editor_panel.metadata_play_speed = create_edit(window, IDC_METADATA_PLAY_SPEED, 390, 40, 80, 24);

        create_label(window, "Objects", 12, 80, 90, 20);
        g_editor_panel.object_list = create_listbox(window, IDC_OBJECT_LIST, 12, 104, 250, 240);
        g_editor_panel.add_platform_button = create_button(window, "Add Platform", IDC_ADD_PLATFORM, 12, 352, 118, 28);
        g_editor_panel.add_spike_button = create_button(window, "Add Spike", IDC_ADD_SPIKE, 144, 352, 118, 28);
        g_editor_panel.add_jumppad_button = create_button(window, "Add JumpPad", IDC_ADD_JUMPPAD, 12, 386, 118, 28);
        g_editor_panel.add_event_button = create_button(window, "Add Event", IDC_ADD_EVENT, 144, 386, 118, 28);
        g_editor_panel.duplicate_button = create_button(window, "Duplicate", IDC_DUPLICATE_OBJECT, 12, 420, 118, 28);
        g_editor_panel.delete_button = create_button(window, "Delete", IDC_DELETE_OBJECT, 144, 420, 118, 28);
        g_editor_panel.save_button = create_button(window, "Save", IDC_SAVE_LEVEL, 12, 454, 76, 28);
        g_editor_panel.save_as_button = create_button(window, "Save As...", IDC_SAVE_AS_LEVEL, 96, 454, 82, 28);
        g_editor_panel.open_button = create_button(window, "Open...", IDC_OPEN_LEVEL, 186, 454, 76, 28);
        g_editor_panel.reload_button = create_button(window, "Reload", IDC_RELOAD_LEVEL, 12, 488, 118, 28);
        g_editor_panel.preview_button = create_button(window, "Enter Preview", IDC_TOGGLE_PREVIEW, 144, 488, 118, 28);

        create_label(window, "Selected Object", 286, 80, 120, 20);
        create_label(window, "UID", 286, 106, 50, 20);
        g_editor_panel.uid_edit = create_edit(window, IDC_UID_EDIT, 330, 104, 150, 24);
        g_editor_panel.auto_uid_button = create_button(window, "Auto UID", IDC_AUTO_UID, 486, 104, 60, 24);

        create_label(window, "Position", 286, 140, 60, 20);
        for (int i = 0; i < 3; ++i)
        {
            create_label(window, i == 0 ? "X" : i == 1 ? "Y" : "Z", 350 + i * 64, 140, 16, 20);
            g_editor_panel.position_edits[i] = create_edit(window, IDC_POSITION_X + i, 366 + i * 64, 138, 54, 24);
        }
        g_editor_panel.move_buttons[0] = create_button(window, "X-", IDC_MOVE_X_NEG, 350, 166, 36, 22);
        g_editor_panel.move_buttons[1] = create_button(window, "X+", IDC_MOVE_X_POS, 388, 166, 36, 22);
        g_editor_panel.move_buttons[2] = create_button(window, "Y-", IDC_MOVE_Y_NEG, 430, 166, 36, 22);
        g_editor_panel.move_buttons[3] = create_button(window, "Y+", IDC_MOVE_Y_POS, 468, 166, 36, 22);
        g_editor_panel.move_buttons[4] = create_button(window, "Z-", IDC_MOVE_Z_NEG, 510, 166, 36, 22);
        g_editor_panel.move_buttons[5] = create_button(window, "Z+", IDC_MOVE_Z_POS, 548, 166, 36, 22);

        create_label(window, "Scale", 286, 200, 60, 20);
        for (int i = 0; i < 3; ++i)
        {
            create_label(window, i == 0 ? "X" : i == 1 ? "Y" : "Z", 350 + i * 64, 200, 16, 20);
            g_editor_panel.scale_edits[i] = create_edit(window, IDC_SCALE_X + i, 366 + i * 64, 198, 54, 24);
        }

        create_label(window, "Rotation", 286, 234, 60, 20);
        for (int i = 0; i < 3; ++i)
        {
            create_label(window, i == 0 ? "X" : i == 1 ? "Y" : "Z", 350 + i * 64, 234, 16, 20);
            g_editor_panel.rotation_edits[i] = create_edit(window, IDC_ROTATION_X + i, 366 + i * 64, 232, 54, 24);
        }

        create_label(window, "Event Settings", 286, 276, 120, 20);
        create_label(window, "Action", 286, 304, 50, 20);
        g_editor_panel.event_action_combo = create_combo(window, IDC_EVENT_ACTION, 346, 300, 120, 200);
        create_label(window, "Trigger", 286, 336, 50, 20);
        g_editor_panel.event_trigger_combo = create_combo(window, IDC_EVENT_TRIGGER, 346, 332, 120, 120);
        g_editor_panel.event_once_check = create_checkbox(window, "Trigger Once", IDC_EVENT_ONCE, 474, 332, 110, 24);

        create_label(window, "Target UID", 286, 368, 60, 20);
        g_editor_panel.event_target_uid_edit = create_edit(window, IDC_EVENT_TARGET_UID, 360, 366, 186, 24);
        create_label(window, "Function", 286, 400, 60, 20);
        g_editor_panel.event_function_edit = create_edit(window, IDC_EVENT_FUNCTION, 360, 398, 186, 24);

        create_label(window, "Gravity", 286, 434, 60, 20);
        for (int i = 0; i < 3; ++i)
        {
            create_label(window, i == 0 ? "X" : i == 1 ? "Y" : "Z", 350 + i * 64, 434, 16, 20);
            g_editor_panel.event_gravity_edits[i] = create_edit(window, IDC_EVENT_GRAVITY_X + i, 366 + i * 64, 432, 54, 24);
        }

        create_label(window, "Axis", 286, 468, 60, 20);
        for (int i = 0; i < 3; ++i)
        {
            create_label(window, i == 0 ? "X" : i == 1 ? "Y" : "Z", 350 + i * 64, 468, 16, 20);
            g_editor_panel.event_axis_edits[i] = create_edit(window, IDC_EVENT_AXIS_X + i, 366 + i * 64, 466, 54, 24);
        }

        create_label(window, "Degrees", 286, 502, 60, 20);
        g_editor_panel.event_degrees_edit = create_edit(window, IDC_EVENT_DEGREES, 346, 500, 70, 24);
        create_label(window, "Speed", 424, 502, 46, 20);
        g_editor_panel.event_speed_edit = create_edit(window, IDC_EVENT_SPEED, 470, 500, 76, 24);
        create_label(window, "Duration", 286, 534, 60, 20);
        g_editor_panel.event_duration_edit = create_edit(window, IDC_EVENT_DURATION, 346, 532, 70, 24);

        g_editor_panel.apply_button = create_button(window, "Apply Changes", IDC_APPLY_CHANGES, 286, 572, 180, 30);
        g_editor_panel.stamp_to_current_beat_button = create_button(window, "Stamp Current Beat", IDC_STAMP_TO_CURRENT_BEAT, 472, 572, 112, 30);
        create_label(window, "Tip: edit numbers directly or use X/Y/Z move buttons. [ and ] still nudge the active tool axis. Preview shows live beat and can stamp selection to it.", 12, 614, 612, 32);

        for (size_t i = 0; i < sizeof(kEditorActionLabels) / sizeof(kEditorActionLabels[0]); ++i)
        {
            SendMessageA(g_editor_panel.event_action_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kEditorActionLabels[i]));
        }
        for (size_t i = 0; i < sizeof(kEditorTriggerLabels) / sizeof(kEditorTriggerLabels[0]); ++i)
        {
            SendMessageA(g_editor_panel.event_trigger_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kEditorTriggerLabels[i]));
        }
        for (size_t i = 0; i < sizeof(kEditorPlacementSpaceLabels) / sizeof(kEditorPlacementSpaceLabels[0]); ++i)
        {
            SendMessageA(g_editor_panel.placement_space_combo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(kEditorPlacementSpaceLabels[i]));
        }

        app.ui_dirty = true;
        return true;
    }

    void destroy_editor_panel()
    {
        if (g_editor_panel.window)
        {
            DestroyWindow(g_editor_panel.window);
        }
        std::memset(&g_editor_panel, 0, sizeof(g_editor_panel));
        g_editor_panel_app = NULL;
    }

    void pump_editor_panel_messages(EditorApp& app)
    {
        MSG message;
        g_editor_panel_app = &app;

        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageA(&message);
        }

        sync_panel_from_app(app);
    }
#else
    void mark_ui_dirty(EditorApp& app)
    {
        app.ui_dirty = true;
    }

    bool create_editor_panel(EditorApp& app)
    {
        app.ui_dirty = false;
        return true;
    }

    void destroy_editor_panel()
    {
    }

    void pump_editor_panel_messages(EditorApp& app)
    {
        app.ui_dirty = false;
    }
#endif
}

int main(int argc, char* argv[])
{
    EditorApp app = {};
    bool running = true;
    app.mode = EditorMode::Edit;
    app.requested_mode = EditorMode::Edit;
    app.tool = EditorTool::Move;
    app.axis = EditorAxis::X;
    app.selected_index = kNoSelection;
    app.orbit_yaw_degrees = 45.0f;
    app.orbit_pitch_degrees = 25.0f;
    app.orbit_distance = 28.0f;
    app.ui_dirty = true;
    app.level_path = argc > 1 ? argv[1] : std::string();

    if (app.level_path.empty())
    {
        std::vector<std::string> candidates = level_path_candidates();
        for (size_t i = 0; i < candidates.size(); ++i)
        {
            int ignored = 0;
            LevelSpec* level_spec = parseLevel(candidates[i].c_str(), &ignored);
            if (level_spec)
            {
                app.level_path = candidates[i];
                level_free(level_spec);
                break;
            }
        }
    }

    if (app.level_path.empty())
    {
        app.level_path = std::string(TROPIC_SOURCE_DIR) + "/assets/levels/test_level.json";
    }

    if (!load_level_model(app))
    {
        std::cout << "Starting editor with an empty in-memory level at " << app.level_path << "\n";
    }

    print_controls_once(app);

    if (!create_editor_panel(app))
    {
        std::fprintf(stderr, "Failed to create the editor tool panel.\n");
    }

    while (running)
    {
        double frame_time = Tropic_getTime();
        double last_frame_time = frame_time;
        bool requested_switch = false;
        std::memset(g_keys, 0, sizeof(g_keys));
        std::memset(g_prev_keys, 0, sizeof(g_prev_keys));

        if (!initialize_engine(app))
        {
            std::fprintf(stderr, "Failed to initialize level editor runtime.\n");
            shutdown_engine(app);
            destroy_editor_panel();
            return 1;
        }

        while (Tropic_Update(app.engine))
        {
            pump_editor_panel_messages(app);
            double current_time = Tropic_getTime();
            float delta_time = static_cast<float>(current_time - last_frame_time);
            last_frame_time = current_time;

            if (app.mode == EditorMode::Edit)
            {
                update_edit_mouse_interaction(app);
                handle_edit_shortcuts(app);
                update_edit_camera(app, delta_time);
                update_editor_gizmo(app);
            }
            else if (!update_preview(app))
            {
                shutdown_engine(app);
                return 1;
            }

            Tropic_Render(app.engine);

            if (app.pending_mode_switch)
            {
                requested_switch = true;
                break;
            }

            snapshot_keys();
        }

        shutdown_engine(app);

        if (app.exit_requested)
        {
            running = false;
            break;
        }

        if (requested_switch)
        {
            app.pending_mode_switch = false;
            app.mode = app.requested_mode;
            continue;
        }

        running = false;
    }

    destroy_editor_panel();

    return 0;
}
