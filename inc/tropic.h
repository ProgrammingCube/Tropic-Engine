#ifndef TROPIC_H
#define TROPIC_H

#include <vector.h>
#include <cjson/cJSON.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GL/freeglut.h>
#include "tropic_gamestate.h"
#include "camera.h"
#include "object.h"
#include "scene.h"
#include "handles.h"
#include "id_manager.h"
#include "mesh.h"
#include "texture.h"
#include "shader.h"

// probably should move at least objects and cameras to scenes
// refactor Scene to be SceneID vector and have Scene*'s be in the memory pool
typedef struct sTropic
{
    GLFWwindow* window;
    /* change to pointer */
    TropicGameState state;
    //Scene* current_scene;
    SceneID current_scene;
    vector( SceneID ) scenes;

    /* Resource pools */
    IDManager* scene_manager;

    //Renderer* renderer;
    // Add more fields as needed
} Tropic;


/* Engine handle APIs */
TropicID Tropic_create(void);
Tropic*  Tropic_getById(TropicID id);
TropicID Tropic_getByPtr(Tropic* ptr);
bool     Tropic_destroy(TropicID id);
TropicGameState* Tropic_getGameState( TropicID id );

TropicWindowID* Tropic_getWindow( TropicID engine_id );

/* Core lifecycle */
TropicWindowID* Tropic_CreateWindow( TropicID engine_id, int width, int height, const char* title, bool fullscreen );
int Tropic_Update( TropicID engine_id );
void Tropic_Render( TropicID engine_id );

void* Tropic_parseLevel(TropicID engine, const char* level_path, int* out_num_objects );
void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects );
int Tropic_getNumObjectsInScene( TropicID engine );
int Tropic_getNumObjectsByType( TropicID engine, ObjectType type );

/* Current scene resolver */
Scene* Tropic_getCurrentScenePtr( Tropic* self );

/* Mesh pool APIs */
MeshID   Tropic_newMesh(TropicID engine_id, const Mesh* proto);
Mesh*    Tropic_getMesh(TropicID engine_id, MeshID id);
bool     Tropic_freeMesh(TropicID engine_id, MeshID id);

/* Texture pool APIs */
TextureID Tropic_newTexture(TropicID engine_id, const Texture* proto);
Texture*  Tropic_getTexture(TropicID engine_id, TextureID id);
bool      Tropic_freeTexture(TropicID engine_id, TextureID id);

/* Shader pool APIs */
ShaderID Tropic_newShader(TropicID engine_id, const Shader* proto);
Shader*  Tropic_getShader(TropicID engine_id, ShaderID id);
bool     Tropic_freeShader(TropicID engine_id, ShaderID id);

void Tropic_update(Tropic* self, float delta_time);
void Tropic_render(Tropic* self);

void Tropic_cleanup(Tropic* self);

#endif /* TROPIC_H */
