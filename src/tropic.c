#include "tropic.h"
#include "primitives.h"

#include <cglm/cglm.h>
#include <vector.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Parsing moved to level_parser.{h,c}. Tropic only consumes LevelSpec. */

static bool _Tropic_fileExists(const char *path)
{
    if (!path) return false;
    FILE *file = fopen(path, "rb");
    if (!file) return false;
    fclose(file);
    return true;
}

static bool _Tropic_initPlatformMesh(TropicID engine_id, Scene *scene)
{
    if (!scene || scene->default_platform_mesh != 0) return true;

    Mesh mesh = {0};
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glGenBuffers(1, &mesh.ebo);

    if (mesh.vao == 0 || mesh.vbo == 0 || mesh.ebo == 0) {
        if (mesh.vbo != 0) glDeleteBuffers(1, &mesh.vbo);
        if (mesh.ebo != 0) glDeleteBuffers(1, &mesh.ebo);
        if (mesh.vao != 0) glDeleteVertexArrays(1, &mesh.vao);
        return false;
    }

    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_verticies), cube_verticies, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, CUBE_VERTEX_STRIDE, (void*)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    mesh.vbo_size = sizeof(cube_verticies);
    mesh.ebo_size = sizeof(cube_indices);
    mesh.vao_size = 1;

    MeshID mesh_id = Tropic_newMesh(engine_id, &mesh);
    if (mesh_id == 0) {
        glDeleteBuffers(1, &mesh.vbo);
        glDeleteBuffers(1, &mesh.ebo);
        glDeleteVertexArrays(1, &mesh.vao);
        return false;
    }

    scene->default_platform_mesh = mesh_id;
    return true;
}

static bool _Tropic_initPlatformShader(TropicID engine_id, Scene *scene)
{
    if (!scene || scene->default_platform_shader != 0) return true;

    static const char *vertex_candidates[] = {
        "assets/shaders/platform_normals.vert",
        "../assets/shaders/platform_normals.vert",
        "../../assets/shaders/platform_normals.vert",
    };
    static const char *fragment_candidates[] = {
        "assets/shaders/platform_normals.frag",
        "../assets/shaders/platform_normals.frag",
        "../../assets/shaders/platform_normals.frag",
    };

    for (size_t i = 0; i < sizeof(vertex_candidates) / sizeof(vertex_candidates[0]); i++) {
        if (!_Tropic_fileExists(vertex_candidates[i]) || !_Tropic_fileExists(fragment_candidates[i])) {
            continue;
        }

        Shader shader = {0};
        if (!shader_load_from_files(&shader, vertex_candidates[i], fragment_candidates[i])) {
            continue;
        }

        ShaderID shader_id = Tropic_newShader(engine_id, &shader);
        if (shader_id == 0) {
            shader_destroy(&shader);
            continue;
        }

        scene->default_platform_shader = shader_id;
        return true;
    }

    fprintf(stderr, "Failed to load platform shader files\n");
    return false;
}

static bool _Tropic_ensureRendererReady(TropicID engine_id, Tropic *self, Scene *scene)
{
    if (!self || !scene || !self->window) return false;
    if (scene->renderer_ready) return true;

    glfwMakeContextCurrent(self->window);

    if (!_Tropic_initPlatformMesh(engine_id, scene)) return false;
    if (!_Tropic_initPlatformShader(engine_id, scene)) return false;

    scene->renderer_ready = (scene->default_platform_mesh != 0 && scene->default_platform_shader != 0);
    return scene->renderer_ready;
}

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

TropicWindowID* Tropic_getWindow( TropicID engine_id )
{
    Tropic *self = Tropic_getById(engine_id);
    if (!self) return NULL;
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
    Scene *scene = Tropic_getCurrentScenePtr(self);

    /* Clear the screen */
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (!scene || !_Tropic_ensureRendererReady(engine_id, self, scene)) {
        glfwSwapBuffers(self->window);
        return;
    }

    TropicCamera *camera = Tropic_getActiveCamera(engine_id);
    if (!camera) {
        glfwSwapBuffers(self->window);
        return;
    }

    int width = 1;
    int height = 1;
    glfwGetFramebufferSize(self->window, &width, &height);
    if (height == 0) height = 1;

    mat4 view;
    mat4 projection;
    glm_lookat(camera->position, camera->target, camera->up, view);
    glm_perspective(glm_rad(camera->fov), (float)width / (float)height, 0.1f, 1000.0f, projection);

    vec3 light_pos = {20.0f, 35.0f, 20.0f};
    vec3 ambient_color = {0.2f, 0.2f, 0.2f};

    for (size_t i = 0; i < vector_size(scene->entities); i++) {
        ObjectID object_id = scene->entities[i];
        Object *object = Tropic_getObject(engine_id, object_id);
        if (!object || !object->active) continue;

        MeshID mesh_id = object->mesh_id;
        ShaderID shader_id = object->shader_id;
        vec3 object_color = {0.65f, 0.65f, 0.65f};

        if (object->type == TYPE_PLATFORM) {
            if (mesh_id == 0) mesh_id = scene->default_platform_mesh;
            if (shader_id == 0) shader_id = scene->default_platform_shader;
            object_color[0] = 0.35f;
            object_color[1] = 0.75f;
            object_color[2] = 0.45f;
        } else if (object->type == TYPE_GENERIC) {
            if (mesh_id == 0 || shader_id == 0) continue;
        } else {
            continue;
        }

        Mesh *mesh = Tropic_getMesh(engine_id, mesh_id);
        Shader *shader = Tropic_getShader(engine_id, shader_id);
        if (!mesh || !shader || mesh->vao == 0 || mesh->ebo_size == 0 || shader->program == 0) continue;

        shader_use(shader);

        mat4 model;
        glm_mat4_identity(model);
        glm_translate(model, object->pos);
        glm_rotate(model, glm_rad(object->rot[0]), (vec3){1.0f, 0.0f, 0.0f});
        glm_rotate(model, glm_rad(object->rot[1]), (vec3){0.0f, 1.0f, 0.0f});
        glm_rotate(model, glm_rad(object->rot[2]), (vec3){0.0f, 0.0f, 1.0f});
        glm_scale(model, object->scale);

        GLint model_loc = shader_get_uniform_location(shader, "model");
        GLint view_loc = shader_get_uniform_location(shader, "view");
        GLint projection_loc = shader_get_uniform_location(shader, "projection");
        GLint light_pos_loc = shader_get_uniform_location(shader, "lightPos");
        GLint object_color_loc = shader_get_uniform_location(shader, "objectColor");
        GLint ambient_color_loc = shader_get_uniform_location(shader, "ambientColor");

        if (model_loc >= 0) glUniformMatrix4fv(model_loc, 1, GL_FALSE, (const float*)model);
        if (view_loc >= 0) glUniformMatrix4fv(view_loc, 1, GL_FALSE, (const float*)view);
        if (projection_loc >= 0) glUniformMatrix4fv(projection_loc, 1, GL_FALSE, (const float*)projection);
        if (light_pos_loc >= 0) glUniform3fv(light_pos_loc, 1, light_pos);
        if (object_color_loc >= 0) glUniform3fv(object_color_loc, 1, object_color);
        if (ambient_color_loc >= 0) glUniform3fv(ambient_color_loc, 1, ambient_color);

        glBindVertexArray(mesh->vao);
        glDrawElements(GL_TRIANGLES, (GLsizei)(mesh->ebo_size / sizeof(GLuint)), GL_UNSIGNED_INT, 0);
    }

    glBindVertexArray(0);
    glUseProgram(0);

    glfwSwapBuffers(self->window);
}

void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects )
{
    Tropic *self = Tropic_getById( engine );
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !objects || num_objects <= 0) return;

    (void)_Tropic_ensureRendererReady(engine, self, scene);

    for (int i = 0; i < num_objects; i++) {
        Object proto = {0};
        proto.type = objects[i].type_code;
        memcpy(proto.pos, objects[i].position, sizeof(vec3));
        memcpy(proto.scale, objects[i].scale, sizeof(vec3));
        memcpy(proto.rot, objects[i].rotation, sizeof(vec3));

        if (scene && proto.type == TYPE_PLATFORM) {
            proto.mesh_id = scene->default_platform_mesh;
            proto.shader_id = scene->default_platform_shader;
        }

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

__attribute__((weak)) void* Tropic_parseLevel( TropicID engine,
                                                     const char* file,
                                                     int* out_num_objects
                                                    )
{
    (void)engine; (void)file; (void)out_num_objects;
    return NULL;
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
    if (m->vbo != 0) glDeleteBuffers(1, &m->vbo);
    if (m->ebo != 0) glDeleteBuffers(1, &m->ebo);
    if (m->vao != 0) glDeleteVertexArrays(1, &m->vao);
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

ShaderID Tropic_newShader(TropicID engine_id, const Shader* proto)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
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

    shader->id = (uint32_t)h;
    return (ShaderID)h;
}

Shader* Tropic_getShader(TropicID engine_id, ShaderID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return NULL;
    return (Shader*)idmgr_get(scene->shaders_manager, id);
}

bool Tropic_freeShader(TropicID engine_id, ShaderID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return false;

    Shader *shader = (Shader*)idmgr_get(scene->shaders_manager, id);
    if (!shader) return false;

    shader_destroy(shader);
    bool ok = idmgr_free(scene->shaders_manager, id);
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

    if (self->window) {
        glfwDestroyWindow(self->window);
        self->window = NULL;
        glfwTerminate();
    }

}
