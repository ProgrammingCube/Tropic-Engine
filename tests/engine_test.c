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

char keyboard[256] = { 0 };

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

int main(int argc, char* argv[])
{
    (void)argc; (void)argv;
    TropicID tropicEngine = Tropic_create();
    Tropic_setActiveEngine(tropicEngine);

    Tropic_CreateWindow(tropicEngine, 1280, 720, "Tropic Engine Test", false);
    glfwSetKeyCallback(Tropic_getWindow(tropicEngine), _key_callback);

    int num_objects = 0;
    const char* level_candidates[] = {
        "assets/levels/test_level.json",
        "../assets/levels/test_level.json",
        "../../assets/levels/test_level.json",
    };

    LevelSpec* parsedData = NULL;
    const char* loaded_level_path = NULL;
    for (size_t i = 0; i < sizeof(level_candidates) / sizeof(level_candidates[0]); i++)
    {
        parsedData = parseLevel(level_candidates[i], &num_objects);
        if (parsedData)
        {
            loaded_level_path = level_candidates[i];
            break;
        }
    }

    if (!parsedData)
    {
        fprintf(stderr, "Failed to load level file from known paths.\n");
        Tropic_destroy(tropicEngine);
        return 1;
    }

    ObjectSpec* objects = levelspec_to_objects(parsedData, tropicEngine, &num_objects);
    level_free(parsedData);

    if (!objects || num_objects <= 0)
    {
        fprintf(stderr, "Level loaded (%s) but produced no objects.\n", loaded_level_path);
        Tropic_destroy(tropicEngine);
        return 1;
    }

    fprintf(stdout, "Loaded %d objects from %s\n", num_objects, loaded_level_path);

    Tropic_loadObjects(tropicEngine, objects, num_objects);

    CameraID camera_id = Tropic_getActiveCameraId(tropicEngine);
    if (camera_id == 0)
    {
        fprintf(stderr, "No active camera found.\n");
        Tropic_destroy(tropicEngine);
        return 1;
    }

    ObjectID id = Tropic_findFirstObjectOfType(tropicEngine, TYPE_PLATFORM);
    if (id == 0)
    {
        fprintf(stderr, "No platform object found.\n");
        Tropic_destroy(tropicEngine);
        return 1;
    }

    /* Keep the camera on the same side as the engine's default camera (+Z).
     * Bind the camera to follow the platform automatically each frame. */
    TropicFollowConfig follow_cfg;
    glm_vec3_copy( (vec3){ 0.0f, 4.0f, 10.0f }, follow_cfg.camera_offset );
    glm_vec3_copy( (vec3){ 0.0f, 1.0f,  0.0f }, follow_cfg.target_offset );
    follow_cfg.space = FOLLOW_WORLD_SPACE;

    if ( !Tropic_bindCameraToObject( tropicEngine, camera_id, id, &follow_cfg ) )
    {
        fprintf( stderr, "Failed to bind camera to object.\n" );
        Tropic_destroy( tropicEngine );
        return 1;
    }

    double last_time = glfwGetTime();

    /* main loop */
    while (Tropic_Update(tropicEngine))
    {
        double current_time = glfwGetTime();
        double delta_time = current_time - last_time;
        last_time = current_time;

        float speed = 5.0f * Tropic_getGameState(tropicEngine)->play_speed;
        float step = (float)(speed * delta_time);

        vec3 translation = { 0.0f, 0.0f, 0.0f };

        if (keyboard[GLFW_KEY_W]) translation[1] += step;
        if (keyboard[GLFW_KEY_S]) translation[1] -= step;
        if (keyboard[GLFW_KEY_A]) translation[0] -= step;
        if (keyboard[GLFW_KEY_D]) translation[0] += step;
        if (keyboard[GLFW_KEY_Q]) translation[2] += step;
        if (keyboard[GLFW_KEY_E]) translation[2] -= step;

        if (!Tropic_translateObject(tropicEngine, id, translation))
        {
            fprintf(stderr, "Failed to move platform.\n");
        }

        Tropic_Render(tropicEngine);
    }

    Tropic_destroy(tropicEngine);
    return 0;
}