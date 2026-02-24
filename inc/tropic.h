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

// probably should move at least objects and cameras to scenes
// refactor Scene to be SceneID vector and have Scene*'s be in the memory pool
typedef struct sTropic
{
    GLFWwindow* window;
    /* change to pointer */
    TropicGameState state;
    Scene* current_scene;

    /* Resource pools */

    //Renderer* renderer;
    // Add more fields as needed
} Tropic;


/* Engine handle APIs */
TropicID Tropic_create(void);
Tropic*  Tropic_getById(TropicID id);
TropicID Tropic_getByPtr(Tropic* ptr);
bool     Tropic_destroy(TropicID id);
TropicGameState* Tropic_getGameState( TropicID id );

/* Core lifecycle */
TropicWindowID* Tropic_CreateWindow( TropicID engine_id, int width, int height, const char* title, bool fullscreen );
int Tropic_Update( TropicID engine_id );
void Tropic_Render( TropicID engine_id );

void* Tropic_parseLevel(TropicID engine, const char* level_path, int* out_num_objects );
void Tropic_loadObjects( TropicID engine, ObjectSpec* objects, int num_objects );
int Tropic_getNumObjectsInScene( TropicID engine );
int Tropic_getNumObjectsByType( TropicID engine, ObjectType type );

/* Object pool APIs */
ObjectID Tropic_newObject(TropicID engine_id, const Object* proto);
Object*  Tropic_getObject( TropicID engine_id, ObjectID id);
bool      Tropic_freeObject( TropicID engine_id, ObjectID id);

/* Mesh pool APIs */
MeshID   Tropic_newMesh(Tropic* self, const Mesh* proto);
Mesh*    Tropic_getMesh(Tropic* self, MeshID id);
bool     Tropic_freeMesh(Tropic* self, MeshID id);

/* Texture pool APIs */
TextureID Tropic_newTexture(Tropic* self, const Texture* proto);
Texture*  Tropic_getTexture(Tropic* self, TextureID id);
bool      Tropic_freeTexture(Tropic* self, TextureID id);

void Tropic_update(Tropic* self, float delta_time);
void Tropic_render(Tropic* self);

void Tropic_cleanup(Tropic* self);

#endif /* TROPIC_H */
