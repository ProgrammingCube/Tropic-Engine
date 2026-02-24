#include "tropic.h"
#include <vector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ask how to create a default camera without having circular dependencies
static void _initializeCurrentScene(Tropic* self)
{
    if (!self || !self->current_scene) return;

    self->current_scene->name = "Default Scene";
    self->current_scene->entities = NULL;
    self->current_scene->cameras = NULL;
    self->current_scene->active_camera = 0;
    self->current_scene->on_enter = NULL;  // Set to actual function pointers as needed
    self->current_scene->on_update = NULL;
    self->current_scene->on_render = NULL;
    self->current_scene->on_exit = NULL;
}

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

static bool _Tropic_init(Tropic* self)
{
    if (!self) return 0;

    self->window = NULL;

    /* Initialize game state strings */
    self->state.game_title = strdup("Tropic Engine Test");
    self->state.level_name = strdup("Test Level 1");
    self->state.play_speed = 1.0f;

    /* Allocate and initialize the current scene */
    self->current_scene = malloc(sizeof(*self->current_scene));
    if (!self->current_scene) return false;

    /* Initialize resource pools */
    self->current_scene->objects_manager = idmgr_create(256);
    self->current_scene->meshes_manager = idmgr_create(128);
    self->current_scene->textures_manager = idmgr_create(128);
    self->current_scene->cameras_manager = idmgr_create(32);

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

    CameraID default_camera = Tropic_newCamera((TropicID)h,
                                               (vec3){ 0.0f, 0.0f, 10.0f },
                                               (vec3){ 0.0f, 1.0f, 0.0f },
                                               (vec3){ 0.0f, 0.0f, 0.0f },
                                               60.0f,
                                               0.0f);
    if (default_camera == 0 || !Tropic_setCamera((TropicID)h, default_camera)) {
        Tropic_cleanup(e);
        idmgr_free(_engines_mgr, h);
        free(e);
        return 0;
    }

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

TropicWindowID* Tropic_CreateWindow( TropicID engine_id, int width, int height, const char* title, bool fullscreen )
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self) return NULL;

    /* Initialize GLFW */
    if (!glfwInit()) {
        fprintf(stderr, "Failed to initialize GLFW\n");
        return NULL;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint( GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE );
#endif
    
    /* Good practice for OpenGL Core Profiles (Mac/Linux like this) */
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); 

    self->window = glfwCreateWindow(width, height, title, fullscreen ? glfwGetPrimaryMonitor() : NULL, NULL);
    if (!self->window) {
        fprintf(stderr, "Failed to create GLFW window\n");
        glfwTerminate();
        return NULL;
    }

    glfwMakeContextCurrent(self->window);
    
    if ( !gladLoadGLLoader( ( GLADloadproc )glfwGetProcAddress ) )
    {
        fprintf(stderr, "Failed to initialize GLAD\n" );
        glfwDestroyWindow( self->window );
        glfwTerminate();
        return NULL;
    }

    printf( "Successfully loaded OpenGL %s\n", glGetString( GL_VERSION ) );
    printf( "Graphics Card: %s\n", glGetString( GL_RENDERER ) );

    glEnable( GL_DEPTH_TEST );

    return self->window;
}

int Tropic_Update( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return 0;

    /* Poll events and check if the window should close */
    glfwPollEvents();

    // check if window should close and return false to signal main loop to exit
    return !glfwWindowShouldClose( self->window );
}

void Tropic_Render( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return;

    /* Clear the screen */
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    /* render active scene and cameras here */

    glfwSwapBuffers(self->window);
}

void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects )
{
    Tropic *self = Tropic_getById( engine );
    if (!self || !objects || num_objects <= 0) return;

    for (int i = 0; i < num_objects; i++) {
        Object proto = {0};
        proto.type = objects[i].type_code;
        memcpy(proto.pos, objects[i].position, sizeof(vec3));
        memcpy(proto.scale, objects[i].scale, sizeof(vec3));
        memcpy(proto.rot, objects[i].rotation, sizeof(vec3));
        (void)Tropic_newObject( engine, &proto);
    }

    free( objects );
}

__attribute__((weak)) void* Tropic_parseLevel( TropicID engine,
                                                     const char* file,
                                                     int* out_num_objects
                                                    )
{
    (void)engine; (void)file; (void)out_num_objects;
    return NULL;
}

// perhaps change to Tropic_addObject and have a separate, true, Tropic_newObject that adds a generic object?
ObjectID Tropic_newObject( TropicID engine, const Object* proto)
{
    Tropic *self = Tropic_getById( engine );
    if (!self) return 0;
    Object *o = (Object*)malloc(sizeof(Object));
    if (!o) return 0;
    if (proto) memcpy(o, proto, sizeof(Object));
    else memset(o, 0, sizeof(Object));

    /* ensure sensible defaults */
    if (o->type == 0) o->type = TYPE_GENERIC;
    o->active = true;

    Handle h = idmgr_alloc(self->current_scene->objects_manager, o);
    if (h == 0) { free(o); return 0; }
    o->id = (ObjectID)h;

    if (o->id != 0)
    {
        vector_push_back(self->current_scene->entities, o->id );
    }

    return (ObjectID)h;
}

Object* Tropic_getObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    if (!self) return NULL;
    return (Object*)idmgr_get(self->current_scene->objects_manager, id);
}

bool Tropic_freeObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    if (!self) return false;
    Object *o = (Object*)idmgr_get(self->current_scene->objects_manager, id);
    if (!o) return false;
    bool ok = idmgr_free(self->current_scene->objects_manager, id);
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
    Handle h = idmgr_alloc(self->current_scene->meshes_manager, m);
    if (h == 0) { free(m); return 0; }
    m->id = (uint32_t)h;
    return (MeshID)h;
}

Mesh* Tropic_getMesh(Tropic* self, MeshID id)
{
    if (!self) return NULL;
    return (Mesh*)idmgr_get(self->current_scene->meshes_manager, id);
}

bool Tropic_freeMesh(Tropic* self, MeshID id)
{
    if (!self) return false;
    Mesh *m = (Mesh*)idmgr_get(self->current_scene->meshes_manager, id);
    if (!m) return false;
    bool ok = idmgr_free(self->current_scene->meshes_manager, id);
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
    Handle h = idmgr_alloc(self->current_scene->textures_manager, t);
    if (h == 0) { free(t); return 0; }
    t->id = (uint32_t)h;
    return (TextureID)h;
}

Texture* Tropic_getTexture(Tropic* self, TextureID id)
{
    if (!self) return NULL;
    return (Texture*)idmgr_get(self->current_scene->textures_manager, id);
}

bool Tropic_freeTexture(Tropic* self, TextureID id)
{
    if (!self) return false;
    Texture *t = (Texture*)idmgr_get(self->current_scene->textures_manager, id);
    if (!t) return false;
    bool ok = idmgr_free(self->current_scene->textures_manager, id);
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
        if (self->current_scene->cameras) {
            vector_free(self->current_scene->cameras);
            self->current_scene->cameras = NULL;
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
    if (self->current_scene->objects_manager) {
        idmgr_free_all(self->current_scene->objects_manager, free);
        self->current_scene->objects_manager = NULL;
    }
    if (self->current_scene->meshes_manager) {
        idmgr_free_all(self->current_scene->meshes_manager, free);
        self->current_scene->meshes_manager = NULL;
    }
    if (self->current_scene->textures_manager) {
        idmgr_free_all(self->current_scene->textures_manager, free);
        self->current_scene->textures_manager = NULL;
    }
    if (self->current_scene->cameras_manager) {
        idmgr_free_all(self->current_scene->cameras_manager, free);
        self->current_scene->cameras_manager = NULL;
    }
}
