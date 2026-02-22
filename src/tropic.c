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

static bool _Tropic_init(Tropic* self)
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

/* Global engines manager */
static IDManager* _engines_mgr = NULL;

TropicID Tropic_create(void)
{
    if (!_engines_mgr) _engines_mgr = idmgr_create(16);
    Tropic *e = (Tropic*)malloc(sizeof(Tropic));
    if (!e) return 0;
    if (!_Tropic_init(e)) { free(e); return 0; }
    Handle h = idmgr_alloc(_engines_mgr, e);
    if (h == 0) { Tropic_cleanup(e); free(e); return 0; }
    return (TropicID)h;
}

Tropic* Tropic_getById(TropicID id)
{
    if (!_engines_mgr) return NULL;
    return (Tropic*)idmgr_get(_engines_mgr, id);
}

TropicID Tropic_getByPtr(Tropic* ptr)
{
    if (!_engines_mgr || !ptr) return 0;
    return (TropicID)idmgr_get_by_payload(_engines_mgr, ptr);
}

bool Tropic_destroy(TropicID id)
{
    if (!_engines_mgr) return false;
    Tropic *e = (Tropic*)idmgr_get(_engines_mgr, id);
    if (!e) return false;
    /* cleanup engine resources */
    Tropic_cleanup(e);
    idmgr_free(_engines_mgr, id);
    free(e);
    return true;
}

TropicGameState* Tropic_getGameState( TropicID id )
{
    Tropic *e = Tropic_getById(id);
    if (!e) return NULL;
    return &e->state;
}

bool Tropic_parseLevel(TropicID engine, const char* level_path)
{
    Tropic *self = Tropic_getById( engine );
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

    /* copy spec->platforms position, scale, rotation to new objects and add to scene */
    for (size_t i = 0; i < spec->platform_count; i++) {
        Object proto = {0};
        proto.type = TYPE_PLATFORM;
        memcpy(proto.pos, spec->platforms[i].position, sizeof(vec3));
        memcpy(proto.scale, spec->platforms[i].scale, sizeof(vec3));
        memcpy(proto.rot, spec->platforms[i].rotation, sizeof(vec3));
        ObjectID obj_id = Tropic_newObject(self, &proto);
        if (obj_id != 0) {
            vector_push_back(self->current_scene->entities, obj_id);
        }
    }

    /* copy spikes to scene as generic objects (no explicit spike enum present) */
    for (size_t i = 0; i < spec->spikes_count; i++) {
        Object proto = {0};
        proto.type = TYPE_SPIKE;
        memcpy(proto.pos, spec->spikes[i].position, sizeof(vec3));
        memcpy(proto.scale, spec->spikes[i].scale, sizeof(vec3));
        memcpy(proto.rot, spec->spikes[i].rotation, sizeof(vec3));
        ObjectID obj_id = Tropic_newObject(self, &proto);
        if (obj_id != 0) {
            vector_push_back(self->current_scene->entities, obj_id);
        }
    }

    /* copy jumppads to scene and tag as TYPE_JUMPPAD */
    for (size_t i = 0; i < spec->jumppads_count; i++) {
        Object proto = {0};
        proto.type = TYPE_JUMPPAD;
        memcpy(proto.pos, spec->jumppads[i].position, sizeof(vec3));
        memcpy(proto.scale, spec->jumppads[i].scale, sizeof(vec3));
        memcpy(proto.rot, spec->jumppads[i].rotation, sizeof(vec3));
        ObjectID obj_id = Tropic_newObject(self, &proto);
        if (obj_id != 0) {
            vector_push_back(self->current_scene->entities, obj_id);
        }
    }

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

/* Mesh pool functions */
MeshID Tropic_newMesh(Tropic* self, const Mesh* proto)
{
    if (!self) return 0;
    Mesh *m = (Mesh*)malloc(sizeof(Mesh));
    if (!m) return 0;
    if (proto) memcpy(m, proto, sizeof(Mesh));
    else memset(m, 0, sizeof(Mesh));
    Handle h = idmgr_alloc(self->meshes, m);
    if (h == 0) { free(m); return 0; }
    m->id = (uint32_t)h;
    return (MeshID)h;
}

Mesh* Tropic_getMesh(Tropic* self, MeshID id)
{
    if (!self) return NULL;
    return (Mesh*)idmgr_get(self->meshes, id);
}

bool Tropic_freeMesh(Tropic* self, MeshID id)
{
    if (!self) return false;
    Mesh *m = (Mesh*)idmgr_get(self->meshes, id);
    if (!m) return false;
    bool ok = idmgr_free(self->meshes, id);
    if (ok) free(m);
    return ok;
}

/* Texture pool functions */
TextureID Tropic_newTexture(Tropic* self, const Texture* proto)
{
    if (!self) return 0;
    Texture *t = (Texture*)malloc(sizeof(Texture));
    if (!t) return 0;
    if (proto) memcpy(t, proto, sizeof(Texture));
    else memset(t, 0, sizeof(Texture));
    Handle h = idmgr_alloc(self->textures, t);
    if (h == 0) { free(t); return 0; }
    t->id = (uint32_t)h;
    return (TextureID)h;
}

Texture* Tropic_getTexture(Tropic* self, TextureID id)
{
    if (!self) return NULL;
    return (Texture*)idmgr_get(self->textures, id);
}

bool Tropic_freeTexture(Tropic* self, TextureID id)
{
    if (!self) return false;
    Texture *t = (Texture*)idmgr_get(self->textures, id);
    if (!t) return false;
    bool ok = idmgr_free(self->textures, id);
    if (ok) free(t);
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
