#include "engine_test.h"

/*
* TODO:
* Add way to keep track of number of platforms, spikes, jumppads in the level spec, so we can loop through them when creating objects.
* Refactor Tropic engine to accept TropicID instead of Tropic* in all functions, and do the lookup inside the function. This will simplify the API and make it more consistent.
* Isolate level_parser completely from the engine, and have it return a LevelSpec struct that the engine can then use to create objects and populate the scene. This will decouple the parsing logic from the engine logic and make it more modular. Tropic should not care about platforms, spikes, jumppads specifically, it should just take the data and create objects accordingly.
* Add error handling and logging to all functions, especially those that can fail
* Get rid of the global _engines_mgr and instead have a more robust way to manage multiple engine instances if needed (e.g. a singleton pattern or a context struct that holds the manager).
* Add more functionality to the engine (e.g. rendering, input handling, physics) and test it in the main loop.
* Add unit tests for the engine functions and level parser.
* Add comments and documentation to all functions and data structures.
*/

// Have engine_test parse the level. Tropic should not care about platforms, spikes, jumppads specifically, it should just take the data and create objects accordingly. This will decouple the parsing logic from the engine logic and make it more modular. Tropic should not care about platforms, spikes, jumppads specifically, it should just take the data and create objects accordingly.

int main( int argc, char *argv[] )
{
    (void)argc; (void)argv; // Unused parameters
    /* create engine via handle API */
    TropicID engine = Tropic_create();

    int num_objects = 0;
    LevelSpec* parsedData = parseLevel("../assets/levels/test_level.json", &num_objects );

    Tropic_LoadObjects( engine, parsedData, num_objects );












    Tropic_parseLevel(engine, "../assets/levels/test_level.json");

    /* print level info from engine->state */
    TropicGameState* state = Tropic_getGameState(engine);
    printf("Level: %s (%s)\n", state->game_title, state->level_name);
    printf("Play Speed: %f\n", state->play_speed);

    /* print objects from engine Scene vector(ObjectID) entities */
    Tropic* e = Tropic_getById(engine);
    printf("Objects in scene:\n");
    for (size_t i = 0; i < vector_size(e->current_scene->entities); i++) {
        ObjectID obj_id = *(vector_at(e->current_scene->entities, i));
        Object* obj = Tropic_getObject(e, obj_id);
        printf("Object ID: %u, Type: %d, Position: (%f, %f, %f), Scale: (%f, %f, %f), Rotation: (%f, %f, %f)\n",
            obj->id, obj->type,
            obj->pos[0], obj->pos[1], obj->pos[2],
            obj->scale[0], obj->scale[1], obj->scale[2],
            obj->rot[0], obj->rot[1], obj->rot[2]
        );
    }

    /*
    printf("Platforms: %zu\n", state->platform_count);
    printf("Spikes: %zu\n", state->spikes_count);
    printf("Jumppads: %zu\n", state->jumppads_count);
    */


    /* main game loop */
    while (1) {
        // Game loop logic would go here
        break;
    }
    Tropic_destroy(engine);
    return 0;
}