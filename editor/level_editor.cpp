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

extern "C" {
#include "tropic.h"
#include "level_loader.h"
}

namespace
{
    const size_t kNoSelection = std::numeric_limits<size_t>::max();
    const float kEditorMoveStep = 0.5f;
    const float kEditorRotateStep = 15.0f;
    const float kEditorScaleStep = 0.25f;
    const float kPreviewDefaultGravity = 9.81f;

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
        double play_speed;
    };

    struct EditorObject
    {
        ObjectType type;
        std::string type_name;
        vec3 position;
        vec3 scale;
        vec3 rotation;
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
        EditorMaterialUniforms platform_uniforms;
        EditorMaterialUniforms spike_uniforms;
        EditorMaterialUniforms jumppad_uniforms;
        EditorMaterialUniforms event_uniforms;
        EditorMaterialUniforms selected_uniforms;
        EditorMaterialUniforms player_uniforms;
    };

    struct PreviewGravityTriggerState
    {
        ObjectID player_id;
        bool triggered;
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
        std::vector<PreviewGravityTriggerState> gravity_trigger_states;
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
        std::vector<ObjectID> object_ids;
        size_t selected_index;
        std::string level_path;
        bool pending_mode_switch;
        EditorMode requested_mode;
        bool initialized;
        bool controls_printed;
        float orbit_yaw_degrees;
        float orbit_pitch_degrees;
        float orbit_distance;
    };

    void set_vec3(vec3 value, float x, float y, float z)
    {
        value[0] = x;
        value[1] = y;
        value[2] = z;
    }

    void copy_vec3(const vec3 source, vec3 destination)
    {
        glm_vec3_copy(source, destination);
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

    EditorObject make_editor_object(ObjectType type)
    {
        EditorObject object;
        object.type = type;
        object.type_name = object_type_name(type);
        set_vec3(object.position, 0.0f, 0.0f, 0.0f);
        set_vec3(object.rotation, 0.0f, 0.0f, 0.0f);

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

        return render.platform_material != 0 && render.spike_material != 0 &&
               render.jumppad_material != 0 && render.event_material != 0 &&
               render.selected_material != 0 && render.player_material != 0;
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

    void update_edit_camera(EditorApp& app, float delta_time)
    {
        vec3 focus;
        vec3 position;
        vec3 up;
        float yaw_speed = 90.0f;
        float pitch_speed = 75.0f;
        float zoom_speed = 18.0f;
        float yaw_radians;
        float pitch_radians;
        float cos_pitch;

        if (key_down(GLFW_KEY_LEFT)) app.orbit_yaw_degrees -= yaw_speed * delta_time;
        if (key_down(GLFW_KEY_RIGHT)) app.orbit_yaw_degrees += yaw_speed * delta_time;
        if (key_down(GLFW_KEY_UP)) app.orbit_pitch_degrees += pitch_speed * delta_time;
        if (key_down(GLFW_KEY_DOWN)) app.orbit_pitch_degrees -= pitch_speed * delta_time;
        if (key_down(GLFW_KEY_PAGE_UP)) app.orbit_distance -= zoom_speed * delta_time;
        if (key_down(GLFW_KEY_PAGE_DOWN)) app.orbit_distance += zoom_speed * delta_time;

        app.orbit_pitch_degrees = std::max(-80.0f, std::min(80.0f, app.orbit_pitch_degrees));
        app.orbit_distance = std::max(4.0f, std::min(100.0f, app.orbit_distance));

        compute_camera_focus(app, focus);
        yaw_radians = app.orbit_yaw_degrees * 3.14159265f / 180.0f;
        pitch_radians = app.orbit_pitch_degrees * 3.14159265f / 180.0f;
        cos_pitch = std::cos(pitch_radians);

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
        app.metadata.play_speed = 1.0;
        app.objects.clear();
        app.selected_index = kNoSelection;

        if (!level_spec)
        {
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

        for (size_t i = 0; i < level_spec->platform_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_PLATFORM);
            copy_vec3(level_spec->platforms[i].position, object.position);
            copy_vec3(level_spec->platforms[i].scale, object.scale);
            copy_vec3(level_spec->platforms[i].rotation, object.rotation);
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->spikes_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_SPIKE);
            copy_vec3(level_spec->spikes[i].position, object.position);
            copy_vec3(level_spec->spikes[i].scale, object.scale);
            copy_vec3(level_spec->spikes[i].rotation, object.rotation);
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->jumppads_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_JUMPPAD);
            copy_vec3(level_spec->jumppads[i].position, object.position);
            copy_vec3(level_spec->jumppads[i].scale, object.scale);
            copy_vec3(level_spec->jumppads[i].rotation, object.rotation);
            app.objects.push_back(object);
        }

        for (size_t i = 0; i < level_spec->events_count; ++i)
        {
            EditorObject object = make_editor_object(TYPE_EVENT);
            copy_vec3(level_spec->events[i].position, object.position);
            copy_vec3(level_spec->events[i].scale, object.scale);
            copy_vec3(level_spec->events[i].rotation, object.rotation);
            app.objects.push_back(object);
        }

        if (!app.objects.empty())
        {
            app.selected_index = 0;
        }

        level_free(level_spec);
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

    void write_transform_block(std::ofstream& file,
                               const EditorObject& object,
                               bool platform_style,
                               const std::string& indent)
    {
        file << indent << "{\n";
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
        file << indent << "  }\n";
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
        file << "  \"metadata\": {\n";
        file << "    \"game_title\": \"" << json_escape(app.metadata.game_title) << "\",\n";
        file << "    \"level_name\": \"" << json_escape(app.metadata.level_name) << "\",\n";
        file << std::fixed << std::setprecision(3);
        file << "    \"play_speed\": " << app.metadata.play_speed << "\n";
        file << "  },\n";
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
        copy_vec3(editor_object.position, prototype.pos);
        copy_vec3(editor_object.scale, prototype.scale);
        copy_vec3(editor_object.rotation, prototype.rot);

        out_id = Tropic_newObject(app.engine, &prototype);
        if (out_id == 0)
        {
            return false;
        }

        return Tropic_setObjectMaterial(app.engine, out_id, material_for_object(app, editor_object.type, false));
    }

    bool build_scene(EditorApp& app)
    {
        Scene* scene = NULL;
        float background[4] = { 0.03f, 0.03f, 0.05f, 1.0f };
        vec3 ambient;
        vec3 gravity;

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

        scene = Tropic_getCurrentScene(app.engine);
        if (!scene)
        {
            return false;
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

    extern "C" void gravity_trigger_callback(TropicID engine_id,
                                               const TropicCollisionEvent* event,
                                               void* user_data)
    {
        PreviewGravityTriggerState* state = static_cast<PreviewGravityTriggerState*>(user_data);
        if (!state || !event)
        {
            return;
        }

        if (state->triggered || event->phase != TROPIC_COLLISION_ENTER || event->other_id != state->player_id)
        {
            return;
        }

        state->triggered = true;
        (void)Tropic_invertGravity(engine_id, Tropic_getCurrentSceneID(engine_id));
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
        app.preview.gravity_trigger_states.clear();
        app.preview.gravity_trigger_states.reserve(app.object_ids.size());

        for (size_t i = 0; i < app.object_ids.size(); ++i)
        {
            if (app.objects[i].type != TYPE_EVENT)
            {
                continue;
            }

            PreviewGravityTriggerState state;
            state.player_id = app.preview.player_id;
            state.triggered = false;
            app.preview.gravity_trigger_states.push_back(state);
            if (!Tropic_setObjectCollisionCallback(app.engine,
                                                   app.object_ids[i],
                                                   gravity_trigger_callback,
                                                   &app.preview.gravity_trigger_states.back()))
            {
                return false;
            }
        }

        return true;
    }

    void reset_preview(EditorApp& app)
    {
        app.preview.player_id = 0;
        app.preview.gravity_trigger_states.clear();
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

        app.camera_id = Tropic_getActiveCameraId(app.engine);
        if (app.camera_id == 0)
        {
            return false;
        }

        if (!preview_mode)
        {
            update_edit_camera(app, 0.0f);
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
            }
            app.preview.loop_state.last_time = Tropic_getTime();
        }

        app.initialized = true;
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
            << "  Tab / Shift+Tab : select next or previous object\n"
            << "  1/2/3/4         : add platform, spike, jumppad, event\n"
            << "  G/R/T           : move, rotate, scale tool\n"
            << "  X/Y/Z           : active axis\n"
            << "  [ and ]         : nudge selected value on active axis\n"
            << "  Arrow keys      : orbit camera\n"
            << "  PageUp/PageDown : zoom camera\n"
            << "  F2              : save level JSON\n"
            << "  F5              : toggle play preview\n"
            << "  Preview: A/D move, Space jump, P pause, +/- play speed\n";
        app.controls_printed = true;
    }

    void sync_selected_object(EditorApp& app)
    {
        if (!has_selection(app))
        {
            return;
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
    }

    bool add_object(EditorApp& app, ObjectType type)
    {
        EditorObject object = make_editor_object(type);
        vec3 focus;
        ObjectID object_id = 0;

        compute_camera_focus(app, focus);
        copy_vec3(focus, object.position);
        object.position[1] += 1.0f;
        if (type == TYPE_PLATFORM)
        {
            object.position[1] -= 1.0f;
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
            }
        }

        update_play_speed(app, delta_time);

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

    while (running)
    {
        double frame_time = Tropic_getTime();
        double last_frame_time = frame_time;
        bool inner_loop_running = true;
        std::memset(g_keys, 0, sizeof(g_keys));
        std::memset(g_prev_keys, 0, sizeof(g_prev_keys));

        if (!initialize_engine(app))
        {
            std::fprintf(stderr, "Failed to initialize level editor runtime.\n");
            shutdown_engine(app);
            return 1;
        }

        while (Tropic_Update(app.engine))
        {
            double current_time = Tropic_getTime();
            float delta_time = static_cast<float>(current_time - last_frame_time);
            last_frame_time = current_time;

            if (app.mode == EditorMode::Edit)
            {
                handle_edit_shortcuts(app);
                update_edit_camera(app, delta_time);
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

        if (requested_switch)
        {
            app.pending_mode_switch = false;
            app.mode = app.requested_mode;
            continue;
        }

        running = false;
    }

    return 0;
}
