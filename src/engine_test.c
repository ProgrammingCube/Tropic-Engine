#include "engine_test.h"

int main( int argc, char *argv[] )
{
    (void)argc; (void)argv; // Unused parameters
    Tropic engine;

    Tropic_init(&engine);
    Tropic_parseLevel(&engine, "../assets/levels/test_level.json");

    printf( "Hello, World!\n" );

    /* main game loop */
    while (1) {
        // Game loop logic would go here
        break;
    }
    Tropic_cleanup(&engine);
    return 0;
}