#include "tropic.h"
#include "level_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static char* my_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char*)malloc(n);
    if (!d) return NULL;
    memcpy(d, s, n);
    return d;
}

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

bool Tropic_init(Tropic* self)
{
    // Initialize the game state
    self->state.game_title = my_strdup("Tropic Engine Test");
    self->state.level_name = my_strdup("Test Level 1");
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
    LevelSpec *spec = level_parse_file(level_path);
    if (!spec) {
        fprintf(stderr, "Failed to parse level file: %s\n", level_path);
        return false;
    }

    /* Replace state strings (free previous) */
    if (self->state.game_title) free(self->state.game_title);
    if (self->state.level_name) free(self->state.level_name);
    self->state.game_title = my_strdup(spec->game_title);
    self->state.level_name = my_strdup(spec->level_name);
    self->state.play_speed = (float)spec->play_speed;

    printf("game_title: %s\n", self->state.game_title);
    printf("level_name: %s\n", self->state.level_name);
    printf("play_speed: %f\n", self->state.play_speed);

    level_free(spec);
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

    if (self->state.game_title) {
        free(self->state.game_title);
        self->state.game_title = NULL;
    }
    if (self->state.level_name) {
        free(self->state.level_name);
        self->state.level_name = NULL;
    }
}
