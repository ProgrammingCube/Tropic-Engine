#include "tropic.h"
#include <stdio.h>
#include <stdlib.h>

static void _initializeCurrentScene(Tropic* self)
{
    if (!self || !self->current_scene) return;

    self->current_scene->name = "Default Scene";
    self->current_scene->entities = NULL;
    self->current_scene->on_enter = NULL;  // Set to actual function pointers as needed
    self->current_scene->on_update = NULL;
    self->current_scene->on_render = NULL;
    self->current_scene->on_exit = NULL;
}

bool Tropic_init(Tropic* self)
{
    // Initialize the game state
    self->state.game_title = "Tropic Engine Test";
    self->state.level_name = "Test Level 1";
    self->state.play_speed = 1.0f;

    /* Allocate and initialize the current scene */
    self->current_scene = malloc(sizeof(*self->current_scene));
    if (!self->current_scene) return false;

    // Initialize the current scene with default values
    _initializeCurrentScene(self);

    return true;
}

bool Tropic_parseLevel(Tropic* self, const char* level_path)
{
    printf("Parsing level from path: %s\n", level_path);
    char* level_text_buffer = NULL;

    //level_text_buffer = raw_json_to_str( level_path );
    return true;
}


void Tropic_cleanup(Tropic* self)
{
    if (!self) return;

    if (self->current_scene) {
        if (self->current_scene->entities) {
            vector_free(self->current_scene->entities);
            self->current_scene->entities = NULL;
        }
        free(self->current_scene);
        self->current_scene = NULL;
    }

    /* Note: `state.game_title` and `state.level_name` are currently string
       literals; only free them here if they were dynamically allocated. */
}
