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

typedef struct sTropic
{

    TropicGameState state;
    Scene* current_scene;

    //Renderer* renderer;
    // Add more fields as needed
    
} Tropic;

bool Tropic_init(Tropic* self);
bool Tropic_parseLevel(Tropic* self, const char* level_path);

void Tropic_update(Tropic* self, float delta_time);
void Tropic_render(Tropic* self);
void Tropic_cleanup(Tropic* self);

#endif /* TROPIC_H */