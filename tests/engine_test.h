#ifndef ENGINE_H
#define ENGINE_H

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
    bool jump_was_down;
} EngineTestLoopState;

static char keyboard[256] = { 0 };

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