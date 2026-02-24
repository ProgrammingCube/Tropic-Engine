#include "tropic.h"
#include <vector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

Scene* Tropic_getCurrentScenePtr( Tropic* self )
{
    if ( !self || !self->scene_manager || self->current_scene == 0 ) return NULL;
    return ( Scene* )idmgr_get( self->scene_manager, self->current_scene );
}

static bool _Tropic_init(TropicID engine_id, Tropic* self)
{
    if (!self) return 0;

    self->window = NULL;

    /* Initialize game state strings */
    self->state.game_title = strdup("Tropic Engine Test");
    self->state.level_name = strdup("Test Level 1");
    self->state.play_speed = 1.0f;

    self->current_scene = 0;
    self->scenes = NULL;
    self->scene_manager = idmgr_create( 64 );
    if ( !self->scene_manager ) return false;

    SceneID default_scene = Tropic_createScene( engine_id, "Default Scene" );
    if ( default_scene == 0 ) return false;
    if ( !Tropic_setCurrentScene( engine_id, default_scene ) ) return false;

    return true;
}

/* Global engines manager */
static IDManager* _engines_mgr = NULL;

TropicID Tropic_create(void)
{
    if (!_engines_mgr) _engines_mgr = idmgr_create(16);
    Tropic *e = (Tropic*)malloc(sizeof(Tropic));
    if (!e) return 0;
    Handle h = idmgr_alloc(_engines_mgr, e);
    if (h == 0) { free(e); return 0; }
    if (!_Tropic_init((TropicID)h, e)) {
        Tropic_cleanup(e);
        idmgr_free(_engines_mgr, h);
        free(e);
        return 0;
    }

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
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return 0;
    Object *o = (Object*)malloc(sizeof(Object));
    if (!o) return 0;
    if (proto) memcpy(o, proto, sizeof(Object));
    else memset(o, 0, sizeof(Object));

    /* ensure sensible defaults */
    if (o->type == 0) o->type = TYPE_GENERIC;
    o->active = true;

    Handle h = idmgr_alloc(scene->objects_manager, o);
    if (h == 0) { free(o); return 0; }
    o->id = (ObjectID)h;

    if (o->id != 0)
    {
        vector_push_back(scene->entities, o->id );
    }

    return (ObjectID)h;
}

Object* Tropic_getObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return NULL;
    return (Object*)idmgr_get(scene->objects_manager, id);
}

bool Tropic_freeObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return false;
    Object *o = (Object*)idmgr_get(scene->objects_manager, id);
    if (!o) return false;
    bool ok = idmgr_free(scene->objects_manager, id);
    if (ok) free(o);
    return ok;
}

/* Mesh pool functions */
MeshID Tropic_newMesh(TropicID engine_id, const Mesh* proto)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return 0;
    Mesh *m = (Mesh*)malloc(sizeof(Mesh));
    if (!m) return 0;
    if (proto) memcpy(m, proto, sizeof(Mesh));
    else memset(m, 0, sizeof(Mesh));
    Handle h = idmgr_alloc(scene->meshes_manager, m);
    if (h == 0) { free(m); return 0; }
    m->id = (uint32_t)h;
    return (MeshID)h;
}

Mesh* Tropic_getMesh(TropicID engine_id, MeshID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return NULL;
    return (Mesh*)idmgr_get(scene->meshes_manager, id);
}

bool Tropic_freeMesh(TropicID engine_id, MeshID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return false;
    Mesh *m = (Mesh*)idmgr_get(scene->meshes_manager, id);
    if (!m) return false;
    bool ok = idmgr_free(scene->meshes_manager, id);
    if (ok) free(m);
    return ok;
}

/* Texture pool functions */
TextureID Tropic_newTexture(TropicID engine_id, const Texture* proto)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return 0;
    Texture *t = (Texture*)malloc(sizeof(Texture));
    if (!t) return 0;
    if (proto) memcpy(t, proto, sizeof(Texture));
    else memset(t, 0, sizeof(Texture));
    Handle h = idmgr_alloc(scene->textures_manager, t);
    if (h == 0) { free(t); return 0; }
    t->id = (uint32_t)h;
    return (TextureID)h;
}

Texture* Tropic_getTexture(TropicID engine_id, TextureID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return NULL;
    return (Texture*)idmgr_get(scene->textures_manager, id);
}

bool Tropic_freeTexture(TropicID engine_id, TextureID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return false;
    Texture *t = (Texture*)idmgr_get(scene->textures_manager, id);
    if (!t) return false;
    bool ok = idmgr_free(scene->textures_manager, id);
    if (ok) free(t);
    return ok;
}


void Tropic_cleanup(Tropic* self)
{
    if (!self) return;

    while ( self->scenes && vector_size( self->scenes ) > 0 ) {
        SceneID scene_id = self->scenes[0];
        (void)Tropic_freeScene( Tropic_getByPtr( self ), scene_id );
    }

    if ( self->scenes ) {
        vector_free( self->scenes );
        self->scenes = NULL;
    }

    if ( self->scene_manager ) {
        idmgr_destroy( self->scene_manager );
        self->scene_manager = NULL;
    }

    self->current_scene = 0;

    if (self->state.game_title) {
        free(self->state.game_title);
        self->state.game_title = NULL;
    }
    if (self->state.level_name) {
        free(self->state.level_name);
        self->state.level_name = NULL;
    }

}
