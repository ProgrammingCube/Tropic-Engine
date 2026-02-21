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

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

bool Tropic_init(Tropic* self)
{
    if (!self) return 0;

    /* Initialize game state strings */
    self->state.game_title = strdup("Tropic Engine Test");
    self->state.level_name = strdup("Test Level 1");
    self->state.play_speed = 1.0f;

    /* Allocate and initialize the current scene */
    self->current_scene = malloc(sizeof(*self->current_scene));
    if (!self->current_scene) return false;

    /* Initialize resource pools */
    self->objects = idmgr_create(256);
    self->meshes = idmgr_create(128);
    self->textures = idmgr_create(128);

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
    self->state.game_title = strdup(spec->game_title);
    self->state.level_name = strdup(spec->level_name);
    self->state.play_speed = (float)spec->play_speed;

    /* Copy object data ( platforms, spikes, jumppads ) to self->current_scene.entities */
    if (self->current_scene->entities) {
        vector_free(self->current_scene->entities);
        self->current_scene->entities = NULL;
    }

    /* Example: create a default object and register it (store handle in scene) */
    Object def = {0};
    ObjectID h = Tropic_newObject(self, &def);
    if (h != 0) {
        vector_push_back(self->current_scene->entities, h);
    }



    printf("game_title: %s\n", self->state.game_title);
    printf("level_name: %s\n", self->state.level_name);
    printf("play_speed: %f\n", self->state.play_speed);

    level_free(spec);
    return true;
}

ObjectID Tropic_newObject(Tropic* self, const Object* proto)
{
    if (!self) return 0;
    Object *o = (Object*)malloc(sizeof(Object));
    if (!o) return 0;
    if (proto) memcpy(o, proto, sizeof(Object));
    else memset(o, 0, sizeof(Object));

    /* ensure sensible defaults */
    if (o->type == 0) o->type = TYPE_GENERIC;
    o->active = 1;

    Handle h = idmgr_alloc(self->objects, o);
    if (h == 0) { free(o); return 0; }
    o->id = (uint32_t)h;
    return (ObjectID)h;
}

Object* Tropic_getObject(Tropic* self, ObjectID id)
{
    if (!self) return NULL;
    return (Object*)idmgr_get(self->objects, id);
}

bool Tropic_freeObject(Tropic* self, ObjectID id)
{
    if (!self) return false;
    Object *o = (Object*)idmgr_get(self->objects, id);
    if (!o) return false;
    bool ok = idmgr_free(self->objects, id);
    if (ok) free(o);
    return ok;
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

    /* Free all objects/meshes/textures payloads and destroy managers */
    if (self->objects) {
        idmgr_free_all(self->objects, free);
        self->objects = NULL;
    }
    if (self->meshes) {
        idmgr_free_all(self->meshes, free);
        self->meshes = NULL;
    }
    if (self->textures) {
        idmgr_free_all(self->textures, free);
        self->textures = NULL;
    }
}
