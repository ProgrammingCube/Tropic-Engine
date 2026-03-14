#include "engine_test.h"

#include <stdio.h>

/*
* TODO:
* Add error handling and logging to all functions, especially those that can fail
* Get rid of the global _engines_mgr and instead have a more robust way to manage multiple engine instances if needed (e.g. a singleton pattern or a context struct that holds the manager).
* Add more functionality to the engine (e.g. rendering, input handling, physics) and test it in the main loop.
* Add unit tests for the engine functions and level parser.
* Add comments and documentation to all functions and data structures.
*/

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    const EngineTestConfig config = {
        .move_speed = 5.0f,
        .forward_speed = 14.0f,
        .jump_speed = 9.0f,
        .fixed_delta = 1.0f / 120.0f,
        .jump_buffer_time = 0.12f,
        .coyote_time = 0.08f,
    };
    EngineTestLoopState loop_state = {0};
    TropicID tropicEngine = Tropic_create();
    CameraID camera_id;
    ObjectID player = 0;
    ObjectSpec* objects = NULL;
    int num_objects = 0;
    int exit_code = 1;

    if (tropicEngine == 0)
    {
        fprintf(stderr, "Failed to create engine.\n");
        return 1;
    }

    if (!Tropic_setActiveEngine(tropicEngine))
    {
        fprintf(stderr, "Failed to set active engine.\n");
        goto cleanup;
    }

    if (!Tropic_CreateWindow(tropicEngine, 1280, 720, "Tropic Engine Test", false))
    {
        fprintf(stderr, "Failed to create test window.\n");
        goto cleanup;
    }

    //glfwSetKeyCallback(Tropic_getWindow(tropicEngine), _key_callback);
	if ( !Tropic_setKeyCallback(tropicEngine, _key_callback) )
    {
        fprintf(stderr, "Failed to set key callback.\n");
        goto cleanup;
	}

    if (!_load_test_level(tropicEngine, &objects, &num_objects))
    {
        goto cleanup;
    }

    Tropic_loadObjects(tropicEngine, objects, num_objects);

    camera_id = Tropic_getActiveCameraId(tropicEngine);
    if (camera_id == 0)
    {
        fprintf(stderr, "No active camera found.\n");
        goto cleanup;
    }

    if (!_create_player(tropicEngine, &player))
    {
        goto cleanup;
    }

    if (Tropic_findFirstObjectOfType(tropicEngine, TYPE_PLATFORM) == 0)
    {
        fprintf(stderr, "No platform object found.\n");
        goto cleanup;
    }

    if (!_bind_follow_camera(tropicEngine, camera_id, player))
    {
        goto cleanup;
    }

    if (Tropic_getGameState(tropicEngine)->play_speed <= 0.0f)
    {
        Tropic_getGameState(tropicEngine)->play_speed = 1.0f;
    }

    loop_state.last_time = Tropic_getTime();

    while (Tropic_Update(tropicEngine))
    {
        double current_time = Tropic_getTime();
        double delta_time = current_time - loop_state.last_time;
        float time_scale = Tropic_getGameState(tropicEngine)->play_speed;
        bool jump_pressed = keyboard[GLFW_KEY_SPACE] != 0;
        bool jump_requested = jump_pressed && !loop_state.jump_was_down;

        loop_state.last_time = current_time;

        _update_play_speed(tropicEngine, delta_time, &loop_state.speed_adjust_timer);

        if (jump_requested)
        {
            loop_state.jump_buffer_timer = config.jump_buffer_time;
        }

        loop_state.physics_accumulator += delta_time;

        if (!_step_player_controller(tropicEngine, player, &config, &loop_state, time_scale))
        {
            goto cleanup;
        }

        loop_state.jump_was_down = jump_pressed;

        Tropic_Render(tropicEngine);
    }

    exit_code = 0;

cleanup:
    if (tropicEngine != 0)
    {
        Tropic_destroy(tropicEngine);
    }
    return exit_code;
}