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

/* Core lifecycle */
bool Tropic_init(Tropic* self);
bool Tropic_parseLevel(Tropic* self, const char* level_path);

/* Object pool APIs */
ObjectID Tropic_newObject(Tropic* self, const Object* proto);
Object*  Tropic_getObject(Tropic* self, ObjectID id);
bool      Tropic_freeObject(Tropic* self, ObjectID id);

void Tropic_update(Tropic* self, float delta_time);
void Tropic_render(Tropic* self);

void Tropic_cleanup(Tropic* self);

#endif /* TROPIC_H */