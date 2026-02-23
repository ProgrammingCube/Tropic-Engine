#include "engine_test.h"

/*
* TODO:
* Add error handling and logging to all functions, especially those that can fail
* Get rid of the global _engines_mgr and instead have a more robust way to manage multiple engine instances if needed (e.g. a singleton pattern or a context struct that holds the manager).
* Add more functionality to the engine (e.g. rendering, input handling, physics) and test it in the main loop.
* Add unit tests for the engine functions and level parser.
* Add comments and documentation to all functions and data structures.
*/

int main( int argc, char *argv[] )
{
    (void)argc; (void)argv; // Unused parameters
    /* create engine via handle API */
    TropicID tropicEngine = Tropic_create();
    CameraID mainCamera = Tropic_newCamera( tropicEngine,
                                            (vec3){ 0.0f, 0.0f, 10.0f },
                                            (vec3){ 0.0f, 0.0f, 0.0f  },
                                            (vec3){ 0.0f, 1.0f, 0.0f  },
                                            60.0f,
                                            0.0f
                                          );
    Tropic_setCamera( tropicEngine, mainCamera );

    Tropic_CreateWindow( tropicEngine, 1280, 720, "Tropic Engine Test", false );

    int num_objects = 0;
    LevelSpec* parsedData = parseLevel("../assets/levels/test_level.json", &num_objects );
    ObjectSpec* objects = levelspec_to_objects( parsedData, tropicEngine, &num_objects );

    Tropic_loadObjects( tropicEngine, objects, num_objects );


     /* print level info from engine->state */
    TropicGameState* state = Tropic_getGameState( tropicEngine );
    printf("Level: %s (%s)\n", state->game_title, state->level_name);
    printf("Play Speed: %f\n", state->play_speed);


    /* main loop */
    while ( Tropic_Update( tropicEngine ) )
    {
        Tropic_Render( tropicEngine );
    }

    Tropic_destroy( tropicEngine );
    return 0;
}