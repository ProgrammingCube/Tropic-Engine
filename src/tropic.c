#include "tropic.h"

#include <cglm/cglm.h>
#include <vector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

TropicID _TROPIC_ACTIVE_ENGINE = 0;

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

static bool _Tropic_init(TropicID engine_id, Tropic* self)
{
    if (!self) return 0;

    /* Initialize game state strings */
    self->state.game_title = strdup("Tropic Engine Test");
    self->state.level_name = strdup("Test Level 1");
    self->state.music_path = NULL;
    self->state.play_speed = 1.0f;

    self->current_scene = 0;
    self->scenes = NULL;
    self->scene_manager = idmgr_create( 64 );
    self->last_update_time = 0.0;
    self->has_last_update_time = false;
    self->fps_overlay_enabled = true;
    self->fps_overlay_sample_start_time = 0.0;
    self->fps_overlay_frame_count = 0;
    self->fps_overlay_displayed_fps = 0;
    self->fps_overlay_initialized = false;
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
    Tropic *e = (Tropic*)calloc(1, sizeof(Tropic));
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
    glEnable( GL_BLEND );
    glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );

    return self->window;
}

TropicWindowID* Tropic_getWindow( TropicID engine_id )
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self) return NULL;
    return self->window;
}

int Tropic_Update( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    double current_time;
    if ( !self ) return 0;

    current_time = Tropic_getTime();
    if (self->has_last_update_time) {
        double delta_time = current_time - self->last_update_time;
        if (delta_time > 0.0) {
            Tropic_updateSceneAnimations(engine_id, (float)delta_time);
        }
    }
    self->last_update_time = current_time;
    self->has_last_update_time = true;

    /* Poll events and check if the window should close */
    glfwPollEvents();

    // check if window should close and return false to signal main loop to exit
    return !glfwWindowShouldClose( self->window );
}

bool Tropic_setKeyCallback(TropicID engine_id, void* callback)
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self || !self->window) return false;
    glfwSetKeyCallback(self->window, (GLFWkeyfun)callback);
	return true;
}

void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects )
{
    Tropic *self = Tropic_getById( engine );
    if (!self || !objects || num_objects <= 0) return;

    for (int i = 0; i < num_objects; i++) {
        Object proto = {0};
        proto.type = objects[i].type_code;
        memcpy(proto.uid, objects[i].uid, sizeof(proto.uid));
        memcpy(proto.pos, objects[i].position, sizeof(vec3));
        memcpy(proto.scale, objects[i].scale, sizeof(vec3));
        memcpy(proto.rot, objects[i].rotation, sizeof(vec3));
        memcpy(&proto.event, &objects[i].event, sizeof(TropicEventSpec));
        proto.event.has_fired = false;

        (void)Tropic_newObject( engine, &proto);
    }

    free( objects );
}

int Tropic_getNumObjectsInScene( TropicID engine )
{
    Tropic *self = Tropic_getById(engine);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return 0;
    return (int)vector_size(scene->entities);
}

int Tropic_getNumObjectsByType( TropicID engine, ObjectType type )
{
    Tropic *self = Tropic_getById(engine);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return 0;

    int count = 0;
    for (size_t i = 0; i < vector_size(scene->entities); i++) {
        Object *object = Tropic_getObject(engine, scene->entities[i]);
        if (object && object->type == type) {
            count++;
        }
    }
    return count;
}

bool Tropic_setActiveEngine( TropicID engine_id )
{
    Tropic *engine = Tropic_getById(engine_id);
    if (!engine) return false;
    _TROPIC_ACTIVE_ENGINE = engine_id;
    return true;
}

Tropic* Tropic_getActiveEnginePtr( void )
{
    return Tropic_getById(_TROPIC_ACTIVE_ENGINE);
}

TropicID Tropic_getActiveEngineId( void )
{
    return _TROPIC_ACTIVE_ENGINE;
}

/*
__attribute__((weak)) void* Tropic_parseLevel( TropicID engine,
                                                     const char* file,
                                                     int* out_num_objects
                                                    )
{
    (void)engine; (void)file; (void)out_num_objects;
    return NULL;
}
*/

void* Tropic_defaultParseLevel(TropicID engine, const char* file, int* out_num_objects)
{
    (void)engine;
    (void)file;
    (void)out_num_objects;
    return NULL;
}

#if defined(_MSC_VER)
#if defined(_M_IX86)
#pragma comment(linker, "/alternatename:_Tropic_parseLevel=_Tropic_defaultParseLevel")
#else
#pragma comment(linker, "/alternatename:Tropic_parseLevel=Tropic_defaultParseLevel")
#endif
#elif defined(__GNUC__) || defined(__clang__)
void* Tropic_parseLevel(TropicID engine, const char* file, int* out_num_objects)
__attribute__((weak, alias("Tropic_defaultParseLevel")));
#endif

/* Mesh pool functions */
MeshID Tropic_newMesh(TropicID engine_id, const Mesh* proto)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    MeshID mesh_id;
    if (!self || !scene) return 0;
    Mesh *m = (Mesh*)malloc(sizeof(Mesh));
    if (!m) return 0;
    if (proto) memcpy(m, proto, sizeof(Mesh));
    else memset(m, 0, sizeof(Mesh));
    Handle h = idmgr_alloc(scene->meshes_manager, m);
    if (h == 0) { free(m); return 0; }
    mesh_id = Tropic_makeMeshID(scene->id, h);
    m->id = mesh_id;
    return mesh_id;
}

Mesh* Tropic_getMesh(TropicID engine_id, MeshID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromMeshID(id));
    Handle local_id = Tropic_getLocalHandleFromMeshID(id);
    if (!self || !scene || local_id == 0) return NULL;
    return (Mesh*)idmgr_get(scene->meshes_manager, local_id);
}

bool Tropic_freeMesh(TropicID engine_id, MeshID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromMeshID(id));
    Handle local_id = Tropic_getLocalHandleFromMeshID(id);
    if (!self || !scene || local_id == 0) return false;
    Mesh *m = (Mesh*)idmgr_get(scene->meshes_manager, local_id);
    if (!m) return false;
    if (m->vbo != 0) glDeleteBuffers(1, &m->vbo);
    if (m->ebo != 0) glDeleteBuffers(1, &m->ebo);
    if (m->vao != 0) glDeleteVertexArrays(1, &m->vao);
    bool ok = idmgr_free(scene->meshes_manager, local_id);
    if (ok) free(m);
    return ok;
}

/* Texture pool functions */
TextureID Tropic_newTexture(TropicID engine_id, const Texture* proto)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    TextureID texture_id;
    if (!self || !scene) return 0;
    Texture *t = (Texture*)malloc(sizeof(Texture));
    if (!t) return 0;
    if (proto) memcpy(t, proto, sizeof(Texture));
    else memset(t, 0, sizeof(Texture));
    Handle h = idmgr_alloc(scene->textures_manager, t);
    if (h == 0) { free(t); return 0; }
    texture_id = Tropic_makeTextureID(scene->id, h);
    t->id = texture_id;
    return texture_id;
}

Texture* Tropic_getTexture(TropicID engine_id, TextureID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromTextureID(id));
    Handle local_id = Tropic_getLocalHandleFromTextureID(id);
    if (!self || !scene || local_id == 0) return NULL;
    return (Texture*)idmgr_get(scene->textures_manager, local_id);
}

bool Tropic_freeTexture(TropicID engine_id, TextureID id)
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromTextureID(id));
    Handle local_id = Tropic_getLocalHandleFromTextureID(id);
    if (!self || !scene || local_id == 0) return false;
    Texture *t = (Texture*)idmgr_get(scene->textures_manager, local_id);
    if (!t) return false;
    bool ok = idmgr_free(scene->textures_manager, local_id);
    if (ok) free(t);
    return ok;
}

ShaderID Tropic_newShader(TropicID engine_id, const Shader* proto)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    ShaderID shader_id;
    if (!self || !scene) return 0;

    Shader *shader = (Shader*)malloc(sizeof(Shader));
    if (!shader) return 0;
    if (proto) memcpy(shader, proto, sizeof(Shader));
    else memset(shader, 0, sizeof(Shader));

    Handle h = idmgr_alloc(scene->shaders_manager, shader);
    if (h == 0) {
        free(shader);
        return 0;
    }

    shader_id = Tropic_makeShaderID(scene->id, h);
    shader->id = shader_id;
    return shader_id;
}

Shader* Tropic_getShader(TropicID engine_id, ShaderID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromShaderID(id));
    Handle local_id = Tropic_getLocalHandleFromShaderID(id);
    if (!self || !scene || local_id == 0) return NULL;
    return (Shader*)idmgr_get(scene->shaders_manager, local_id);
}

bool Tropic_freeShader(TropicID engine_id, ShaderID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromShaderID(id));
    Handle local_id = Tropic_getLocalHandleFromShaderID(id);
    if (!self || !scene || local_id == 0) return false;

    Shader *shader = (Shader*)idmgr_get(scene->shaders_manager, local_id);
    if (!shader) return false;

    shader_destroy(shader);
    bool ok = idmgr_free(scene->shaders_manager, local_id);
    if (ok) free(shader);
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
    if (self->state.music_path) {
        free(self->state.music_path);
        self->state.music_path = NULL;
    }

    if (self->window) {
        glfwDestroyWindow(self->window);
        self->window = NULL;
        glfwTerminate();
    }
}

void Tropic_enableVSync(TropicID engine_id, bool enable)
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self || !self->window) return;
    glfwSwapInterval(enable ? 1 : 0);
}

void Tropic_enableFpsOverlay(TropicID engine_id, bool enable)
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self) return;
    self->fps_overlay_enabled = enable;
}
