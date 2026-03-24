#ifndef ENGINE_H
#define ENGINE_H

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tropic.h"
#include "level_loader.h"

typedef struct sEngineTestConfig
{
    float move_speed;
    float forward_speed;
    float jump_speed;
    float fixed_delta;
    float jump_buffer_time;
    float coyote_time;
} EngineTestConfig;

typedef struct sEngineTestLoopState
{
    double speed_adjust_timer;
    double last_time;
    double physics_accumulator;
    float jump_buffer_timer;
    float coyote_timer;
    bool paused;
    bool jump_was_down;
    bool pause_was_down;
} EngineTestLoopState;

typedef struct sEngineTestRenderResources
{
    MeshID cube_mesh;
    ShaderID volume_shader;
    ShaderID player_shader;
    MaterialID cube_material;
    MaterialID platform_material;
    MaterialID ghost_material;
} EngineTestRenderResources;

typedef struct sEngineTestMaterialUniforms
{
    vec3 color;
    float neon_amount;
    float brightness_scale;
    float alpha_scale;
} EngineTestMaterialUniforms;

typedef struct sEngineTestPlatformCollisionState
{
    ObjectID player_id;
    EngineTestMaterialUniforms *material_uniforms;
} EngineTestPlatformCollisionState;

typedef struct sEngineTestGravityTriggerState
{
    ObjectID player_id;
    bool triggered;
} EngineTestGravityTriggerState;

#define ENGINE_TEST_GHOST_COUNT 8
#define ENGINE_TEST_GHOST_SAMPLE_COUNT 48
#define ENGINE_TEST_GHOST_SAMPLE_STRIDE 4

typedef struct sEngineTestPlayerTrailState
{
    ObjectID player_id;
    ObjectID ghost_ids[ENGINE_TEST_GHOST_COUNT];
    vec3 samples[ENGINE_TEST_GHOST_SAMPLE_COUNT];
    vec3 last_sample_position;
    size_t sample_head;
    bool initialized;
    bool has_last_sample_position;
} EngineTestPlayerTrailState;

static char keyboard[256] = { 0 };
static EngineTestMaterialUniforms _cube_material_uniforms = {
    { 0.10f, 0.55f, 1.00f },
    1.0f,
    1.75f,
    2.80f,
};
static EngineTestMaterialUniforms _ghost_material_uniforms = {
    { 0.08f, 0.42f, 0.95f },
    0.24f,
    0.28f,
    0.14f,
};
static EngineTestMaterialUniforms _platform_material_uniforms = {
    { 0.35f, 0.75f, 0.45f },
    1.0f,
    1.0f,
    1.0f,
};
static EngineTestPlatformCollisionState _platform_collision_state = { 0 };
static EngineTestGravityTriggerState _gravity_trigger_state = { 0 };
static EngineTestPlayerTrailState _player_trail_state = { 0 };

static float _clampf(float value, float min_value, float max_value)
{
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static void _platform_collision_callback(TropicID engine_id,
    const TropicCollisionEvent* event,
    void* user_data)
{
    EngineTestPlatformCollisionState* state = (EngineTestPlatformCollisionState*)user_data;
    (void)engine_id;

    if (!state || !event || !state->material_uniforms) return;
    if (event->phase != TROPIC_COLLISION_ENTER || event->other_id != state->player_id) return;

    state->material_uniforms->neon_amount = _clampf(0.6f + (event->impact_speed * 0.08f),
                                                    0.6f,
                                                    3.0f);
}

static void _gravity_trigger_callback(TropicID engine_id,
    const TropicCollisionEvent* event,
    void* user_data)
{
    EngineTestGravityTriggerState* state = (EngineTestGravityTriggerState*)user_data;

    if (!state || !event) return;
    if (state->triggered || event->phase != TROPIC_COLLISION_ENTER || event->other_id != state->player_id) return;

    state->triggered = true;
    (void)Tropic_invertGravity(engine_id, Tropic_getCurrentSceneID(engine_id));
}

static bool _setup_test_collision_callbacks(TropicID engine_id, ObjectID player)
{
    Scene* scene = Tropic_getCurrentScene(engine_id);

    if (!scene) return false;

    _platform_collision_state.player_id = player;
    _platform_collision_state.material_uniforms = &_platform_material_uniforms;
    _gravity_trigger_state.player_id = player;
    _gravity_trigger_state.triggered = false;

    for (size_t i = 0; i < vector_size(scene->entities); ++i)
    {
        Object* object = Tropic_getObject(engine_id, scene->entities[i]);
        if (!object) continue;

        if (object->type == TYPE_PLATFORM)
        {
            if (!Tropic_setObjectCollisionCallback(engine_id,
                object->id,
                _platform_collision_callback,
                &_platform_collision_state))
            {
                return false;
            }
        }

        if (object->type == TYPE_EVENT)
        {
            if (!Tropic_setObjectCollisionCallback(engine_id,
                object->id,
                _gravity_trigger_callback,
                &_gravity_trigger_state))
            {
                return false;
            }
        }
    }

    return true;
}

static bool _load_test_volume_shader(TropicID engine_id, ShaderID *out_shader)
{
    static const char *vertex_candidates[] = {
        "assets/shaders/platform_volume_ripple_round_soft.vert",
        "assets/shaders/platform_volume_ripple_round.vert",
        "assets/shaders/platform_volume_ripple.vert",
        "assets/shaders/platform_volume.vert",
        "assets/shaders/platform_neon.vert",
        "assets/shaders/platform_normals.vert",
        "../assets/shaders/platform_volume_ripple_round_soft.vert",
        "../assets/shaders/platform_volume_ripple_round.vert",
        "../assets/shaders/platform_volume_ripple.vert",
        "../assets/shaders/platform_volume.vert",
        "../assets/shaders/platform_neon.vert",
        "../assets/shaders/platform_normals.vert",
        "../../assets/shaders/platform_volume_ripple_round_soft.vert",
        "../../assets/shaders/platform_volume_ripple_round.vert",
        "../../assets/shaders/platform_volume_ripple.vert",
        "../../assets/shaders/platform_volume.vert",
        "../../assets/shaders/platform_neon.vert",
        "../../assets/shaders/platform_normals.vert",
    };
    static const char *fragment_candidates[] = {
        "assets/shaders/platform_volume_ripple_round_soft.frag",
        "assets/shaders/platform_volume_ripple_round.frag",
        "assets/shaders/platform_volume_ripple.frag",
        "assets/shaders/platform_volume.frag",
        "assets/shaders/platform_neon.frag",
        "assets/shaders/platform_normals.frag",
        "../assets/shaders/platform_volume_ripple_round_soft.frag",
        "../assets/shaders/platform_volume_ripple_round.frag",
        "../assets/shaders/platform_volume_ripple.frag",
        "../assets/shaders/platform_volume.frag",
        "../assets/shaders/platform_neon.frag",
        "../assets/shaders/platform_normals.frag",
        "../../assets/shaders/platform_volume_ripple_round_soft.frag",
        "../../assets/shaders/platform_volume_ripple_round.frag",
        "../../assets/shaders/platform_volume_ripple.frag",
        "../../assets/shaders/platform_volume.frag",
        "../../assets/shaders/platform_neon.frag",
        "../../assets/shaders/platform_normals.frag",
    };

    if (!out_shader) return false;

    return Tropic_createShaderFromFileCandidates(engine_id,
                                                 vertex_candidates,
                                                 fragment_candidates,
                                                 sizeof(vertex_candidates) / sizeof(vertex_candidates[0]),
                                                 out_shader);
}

static bool _load_test_player_shader(TropicID engine_id, ShaderID *out_shader)
{
    static const char *vertex_candidates[] = {
        "assets/shaders/player_volume_ripple_round_soft.vert",
        "../assets/shaders/player_volume_ripple_round_soft.vert",
        "../../assets/shaders/player_volume_ripple_round_soft.vert",
    };
    static const char *fragment_candidates[] = {
        "assets/shaders/player_volume_ripple_round_soft.frag",
        "../assets/shaders/player_volume_ripple_round_soft.frag",
        "../../assets/shaders/player_volume_ripple_round_soft.frag",
    };

    if (!out_shader) return false;

    return Tropic_createShaderFromFileCandidates(engine_id,
                                                 vertex_candidates,
                                                 fragment_candidates,
                                                 sizeof(vertex_candidates) / sizeof(vertex_candidates[0]),
                                                 out_shader);
}

static void _test_object_render_callback(TropicID engine_id,
    Scene *scene,
    Object *object,
    TropicMaterial *material,
    ShaderID shader_id,
    const TropicCamera *camera)
{
    vec3 light_pos = { 20.0f, 35.0f, 20.0f };
    vec3 object_color = { 0.65f, 0.65f, 0.65f };
    vec3 ambient_color = { 0.2f, 0.2f, 0.2f };
    float neon_amount = 0.0f;
    float brightness_scale = 1.0f;
    float alpha_scale = 1.0f;
    EngineTestMaterialUniforms *material_uniforms;

    (void)camera;

    if (!object || !material || shader_id == 0) return;

    material_uniforms = material->user;
    if (!material_uniforms) return;

    if (scene)
    {
        glm_vec3_copy(scene->ambient_light_color, ambient_color);
    }

    glm_vec3_copy(material_uniforms->color, object_color);
    neon_amount = material_uniforms->neon_amount;
    brightness_scale = material_uniforms->brightness_scale;
    alpha_scale = material_uniforms->alpha_scale;

    (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "lightPos", light_pos);
    (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "objectColor", object_color);
    (void)Tropic_setShaderUniformVec3(engine_id, shader_id, "ambientColor", ambient_color);
    (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "neonAmount", neon_amount);
    (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "brightnessScale", brightness_scale);
    (void)Tropic_setShaderUniformFloat(engine_id, shader_id, "alphaScale", alpha_scale);
}

static bool _configure_test_object_rendering(TropicID engine_id,
    ObjectID object_id,
    const EngineTestRenderResources *resources)
{
    Object *object = Tropic_getObject(engine_id, object_id);

    if (!object || !resources || resources->cube_mesh == 0 || resources->volume_shader == 0)
    {
        return false;
    }

    switch (object->type)
    {
    case TYPE_CUBE:
        return Tropic_setObjectMaterial(engine_id, object_id, resources->cube_material);
    case TYPE_PLATFORM:
        return Tropic_setObjectMaterial(engine_id, object_id, resources->platform_material);
    default:
        break;
    }

    return true;
}

static bool _configure_test_scene_rendering(TropicID engine_id,
    const EngineTestRenderResources *resources)
{
    Scene *scene = Tropic_getCurrentScene(engine_id);

    if (!scene || !resources) return false;

    glm_vec3_copy((vec3) { 0.03f, 0.03f, 0.04f }, scene->background_color);
    glm_vec3_copy((vec3) { 0.2f, 0.2f, 0.2f }, scene->ambient_light_color);

    if (resources->cube_mesh == 0 || resources->volume_shader == 0) return false;

    for (size_t i = 0; i < vector_size(scene->entities); ++i)
    {
        if (!_configure_test_object_rendering(engine_id, scene->entities[i], resources))
        {
            return false;
        }
    }

    return true;
}

static void _update_collision_effects(double delta_time)
{
    float fade_amount = (float)(delta_time * 1.75f);

    if (_platform_material_uniforms.neon_amount > 1.0f)
    {
        _platform_material_uniforms.neon_amount -= fade_amount;
        if (_platform_material_uniforms.neon_amount < 1.0f)
        {
            _platform_material_uniforms.neon_amount = 1.0f;
        }
    }
}

static bool _init_test_materials(TropicID engine_id, EngineTestRenderResources *resources)
{
    if (!resources) return false;
    if (resources->cube_mesh == 0 || resources->volume_shader == 0 || resources->player_shader == 0) return false;

    resources->cube_material = Tropic_createMaterial(engine_id,
                                                     resources->cube_mesh,
                                                     resources->player_shader,
                                                     _test_object_render_callback,
                                                     &_cube_material_uniforms);
    if (resources->cube_material == 0) return false;

    resources->platform_material = Tropic_createMaterial(engine_id,
                                                         resources->cube_mesh,
                                                         resources->volume_shader,
                                                         _test_object_render_callback,
                                                         &_platform_material_uniforms);
    if (resources->platform_material == 0) return false;

    resources->ghost_material = Tropic_createMaterial(engine_id,
                                                      resources->cube_mesh,
                                                      resources->player_shader,
                                                      _test_object_render_callback,
                                                      &_ghost_material_uniforms);
    if (resources->ghost_material == 0) return false;

    return true;
}

static bool _initialize_player_trail(TropicID engine_id,
    ObjectID player_id,
    const EngineTestRenderResources* resources)
{
    Object* player_object = Tropic_getObject(engine_id, player_id);

    if (!player_object || !resources || resources->ghost_material == 0) return false;

    _player_trail_state.player_id = player_id;
    _player_trail_state.sample_head = 0;
    _player_trail_state.initialized = true;
    _player_trail_state.has_last_sample_position = true;
    glm_vec3_copy(player_object->pos, _player_trail_state.last_sample_position);

    for (size_t i = 0; i < ENGINE_TEST_GHOST_SAMPLE_COUNT; ++i)
    {
        glm_vec3_copy(player_object->pos, _player_trail_state.samples[i]);
    }

    for (size_t i = 0; i < ENGINE_TEST_GHOST_COUNT; ++i)
    {
        Object ghost_proto = {
            .type = TYPE_GENERIC,
            .rot = { 0.0f, 0.0f, 0.0f },
        };
        ObjectID ghost_id;
        float scale_factor = 0.90f - ((float)i * 0.07f);

        glm_vec3_copy(player_object->pos, ghost_proto.pos);
        glm_vec3_scale(player_object->scale, scale_factor, ghost_proto.scale);
        glm_vec3_copy(player_object->rot, ghost_proto.rot);

        ghost_id = Tropic_newObject(engine_id, &ghost_proto);
        if (ghost_id == 0) return false;
        if (!Tropic_setObjectMaterial(engine_id, ghost_id, resources->ghost_material)) return false;

        _player_trail_state.ghost_ids[i] = ghost_id;
    }

    return true;
}

static void _update_player_trail(TropicID engine_id)
{
    Object* player_object;
    vec3 delta;

    if (!_player_trail_state.initialized || _player_trail_state.player_id == 0) return;

    player_object = Tropic_getObject(engine_id, _player_trail_state.player_id);
    if (!player_object) return;

    if (!_player_trail_state.has_last_sample_position)
    {
        glm_vec3_copy(player_object->pos, _player_trail_state.last_sample_position);
        _player_trail_state.has_last_sample_position = true;
    }

    glm_vec3_sub(player_object->pos, _player_trail_state.last_sample_position, delta);
    if (glm_vec3_norm2(delta) >= (0.22f * 0.22f))
    {
        glm_vec3_copy(player_object->pos, _player_trail_state.samples[_player_trail_state.sample_head]);
        glm_vec3_copy(player_object->pos, _player_trail_state.last_sample_position);
        _player_trail_state.sample_head = (_player_trail_state.sample_head + 1u) % ENGINE_TEST_GHOST_SAMPLE_COUNT;
    }

    for (size_t i = 0; i < ENGINE_TEST_GHOST_COUNT; ++i)
    {
        size_t sample_index = (_player_trail_state.sample_head + ENGINE_TEST_GHOST_SAMPLE_COUNT - 1u - ((i + 1u) * ENGINE_TEST_GHOST_SAMPLE_STRIDE)) % ENGINE_TEST_GHOST_SAMPLE_COUNT;
        ObjectID ghost_id = _player_trail_state.ghost_ids[i];
        if (ghost_id != 0)
        {
            (void)Tropic_setObjectPosition(engine_id, ghost_id, _player_trail_state.samples[sample_index]);
        }
    }
}

static void _set_lateral_velocity(Object* object,
    vec3 desired_velocity,
    vec3 up,
    float response)
{
    vec3 vertical_velocity;
    vec3 lateral_velocity;
    vec3 delta;
    float vertical_speed;

    if (!object) return;

    vertical_speed = glm_vec3_dot(object->body.velocity, up);
    glm_vec3_scale(up, vertical_speed, vertical_velocity);
    glm_vec3_sub(object->body.velocity, vertical_velocity, lateral_velocity);
    glm_vec3_sub(desired_velocity, lateral_velocity, delta);
    glm_vec3_scale(delta, response, delta);
    glm_vec3_add(lateral_velocity, delta, lateral_velocity);
    glm_vec3_add(lateral_velocity, vertical_velocity, object->body.velocity);
}

static bool _player_jump(TropicID engine_id, ObjectID object_id, float jump_speed)
{
    Object* object = Tropic_getObject(engine_id, object_id);
    vec3 gravity;
    vec3 up;
    vec3 vertical_velocity;
    vec3 planar_velocity;
    float vertical_speed;

    if (!object || !object->body.enabled || !object->body.is_grounded || jump_speed <= 0.0f) {
        return false;
    }

    Tropic_getSceneGravity(engine_id, gravity);
    if (glm_vec3_norm2(gravity) <= 0.000001f) {
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


static void _key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    (void)scancode; (void)mods;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }

    key %= 256; // Ensure key is within bounds of keyboard array

    if (action == GLFW_PRESS)
    {
        keyboard[key] = 1;
    }
    else if (action == GLFW_RELEASE)
    {
        keyboard[key] = 0;
    }
}

static bool _load_test_level(TropicID engine_id, ObjectSpec** out_objects, int* out_num_objects)
{
    const char* level_candidates[] = {
        "assets/levels/test_level.json",
        "../assets/levels/test_level.json",
        "../../assets/levels/test_level.json",
    };
    LevelSpec* parsed_data = NULL;
    const char* loaded_level_path = NULL;
    int num_objects = 0;

    if (!out_objects || !out_num_objects) return false;

    *out_objects = NULL;
    *out_num_objects = 0;

    for (size_t i = 0; i < sizeof(level_candidates) / sizeof(level_candidates[0]); i++)
    {
        parsed_data = parseLevel(level_candidates[i], &num_objects);
        if (parsed_data)
        {
            loaded_level_path = level_candidates[i];
            break;
        }
    }

    if (!parsed_data)
    {
        fprintf(stderr, "Failed to load level file from known paths.\n");
        return false;
    }

    *out_objects = levelspec_to_objects(parsed_data, engine_id, &num_objects);
    level_free(parsed_data);

    if (!*out_objects || num_objects <= 0)
    {
        fprintf(stderr, "Level loaded (%s) but produced no objects.\n", loaded_level_path);
        *out_objects = NULL;
        return false;
    }

    *out_num_objects = num_objects;
    fprintf(stdout, "Loaded %d objects from %s\n", num_objects, loaded_level_path);
    return true;
}

static bool _create_player(TropicID engine_id, ObjectID* out_player)
{
    Object player_proto = {
        .type = TYPE_CUBE,
        .pos = { 0.0f, 1.0f, 0.0f },
        .scale = { 1.0f, 1.0f, 1.0f },
        .rot = { 0.0f, 0.0f, 0.0f },
    };
    ObjectID player;
    vec3 collider_extents;

    if (!out_player) return false;

    player = Tropic_newObject(engine_id, &player_proto);
    if (player == 0)
    {
        fprintf(stderr, "Failed to create player object.\n");
        return false;
    }

    Tropic_getObjectScale(engine_id, player, collider_extents);

    if (!Tropic_configureObjectCollider(engine_id,
        player,
        true,
        collider_extents,
        (vec3) {
        0.0f, 0.0f, 0.0f
    },
        TROPIC_COLLIDER_FLAG_SOLID))
    {
        fprintf(stderr, "Failed to configure player collider.\n");
        return false;
    }

    if (!Tropic_configurePhysicsBody(engine_id, player, true, false))
    {
        fprintf(stderr, "Failed to configure player physics body.\n");
        return false;
    }

    *out_player = player;
    return true;
}

static bool _bind_follow_camera(TropicID engine_id, CameraID camera_id, ObjectID player)
{
    TropicFollowConfig follow_cfg;

    glm_vec3_copy((vec3) { 0.0f, 4.0f, 10.0f }, follow_cfg.camera_offset);
    glm_vec3_copy((vec3) { 0.0f, 1.0f, 0.0f }, follow_cfg.target_offset);
    follow_cfg.space = FOLLOW_WORLD_SPACE;

    if (!Tropic_bindCameraToObject(engine_id, camera_id, player, &follow_cfg))
    {
        fprintf(stderr, "Failed to bind camera to object.\n");
        return false;
    }

    return true;
}

static void _update_play_speed(TropicID engine_id, double delta_time, double* speed_adjust_timer)
{
    TropicGameState* state = Tropic_getGameState(engine_id);

    if (!state || !speed_adjust_timer) return;

    if (*speed_adjust_timer > 0.0)
    {
        *speed_adjust_timer -= delta_time;
    }

    if (*speed_adjust_timer > 0.0)
    {
        return;
    }

    if (keyboard[GLFW_KEY_MINUS])
    {
        state->play_speed -= 0.25f;
        if (state->play_speed < 0.25f)
        {
            state->play_speed = 0.25f;
        }
        *speed_adjust_timer = 0.15;
    }
    else if (keyboard[GLFW_KEY_EQUAL])
    {
        state->play_speed += 0.25f;
        if (state->play_speed > 4.0f)
        {
            state->play_speed = 4.0f;
        }
        *speed_adjust_timer = 0.15;
    }
}

static bool _step_player_controller(TropicID engine_id,
    ObjectID player,
    const EngineTestConfig* config,
    EngineTestLoopState* loop_state,
    float time_scale)
{
    if (!config || !loop_state) return false;

    while (loop_state->physics_accumulator >= config->fixed_delta)
    {
        Object* player_object = Tropic_getObject(engine_id, player);
        vec3 right;
        vec3 up;
        vec3 forward;
        vec3 desired_velocity = { 0.0f, 0.0f, 0.0f };
        float step = config->fixed_delta * time_scale;
        float response;

        if (!player_object)
        {
            fprintf(stderr, "Failed to fetch player object.\n");
            return false;
        }

        if (!Tropic_buildControlBasis(engine_id,
            (vec3) {
            0.0f, 0.0f, -1.0f
        },
            right,
            up,
            forward))
        {
            fprintf(stderr, "Failed to build control basis.\n");
            return false;
        }

        if (glm_vec3_dot(right, (vec3) { 1.0f, 0.0f, 0.0f }) < 0.0f)
        {
            glm_vec3_negate(right);
        }

        glm_vec3_scale(forward, config->forward_speed * time_scale, desired_velocity);
        if (keyboard[GLFW_KEY_A])
        {
            glm_vec3_muladds(right, -config->move_speed * time_scale, desired_velocity);
        }
        if (keyboard[GLFW_KEY_D])
        {
            glm_vec3_muladds(right, config->move_speed * time_scale, desired_velocity);
        }

        response = player_object->body.is_grounded ? player_object->body.ground_friction : player_object->body.air_friction;
        if (response * step > 1.0f) response = 1.0f / step;
        _set_lateral_velocity(player_object, desired_velocity, up, response * step);

        (void)Tropic_stepPhysics(engine_id, step);

        if (loop_state->jump_buffer_timer > 0.0f)
        {
            loop_state->jump_buffer_timer -= step;
            if (loop_state->jump_buffer_timer < 0.0f) loop_state->jump_buffer_timer = 0.0f;
        }

        if (player_object->body.is_grounded)
        {
            loop_state->coyote_timer = config->coyote_time;
        }
        else if (loop_state->coyote_timer > 0.0f)
        {
            loop_state->coyote_timer -= step;
            if (loop_state->coyote_timer < 0.0f) loop_state->coyote_timer = 0.0f;
        }

        if (loop_state->jump_buffer_timer > 0.0f &&
            loop_state->coyote_timer > 0.0f &&
            _player_jump(engine_id, player, config->jump_speed * time_scale))
        {
            loop_state->jump_buffer_timer = 0.0f;
            loop_state->coyote_timer = 0.0f;
        }

        loop_state->physics_accumulator -= config->fixed_delta;
    }

    return true;
}

#endif /* ENGINE_H */