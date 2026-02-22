#ifndef TROPIC_H
#define TROPIC_H

#include <vector.h>
#include <cjson/cJSON.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
//#include "tropic_datatypes.h"
//#include "tropic_utils.h"
#include "tropic_gamestate.h"
#include "object.h"
#include "scene.h"
#include "handles.h"
#include "id_manager.h"
#include "mesh.h"
#include "texture.h"

typedef struct sTropic
{

    TropicGameState state;
    Scene* current_scene;

    /* Resource pools */
    IDManager* objects;
    IDManager* meshes;
    IDManager* textures;

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
bool Tropic_parseLevel(TropicID engine, const char* level_path);
void Tropic_LoadObjects( TropicID engine, ObjectSpec* objects, int num_objects );
int Tropic_getNumObjectsInScene( TropicID engine );
int Tropic_getNumObjectsByType( TropicID engine, ObjectType type );

/* Object pool APIs */
ObjectID Tropic_newObject(Tropic* self, const Object* proto);
Object*  Tropic_getObject(Tropic* self, ObjectID id);
bool      Tropic_freeObject(Tropic* self, ObjectID id);

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