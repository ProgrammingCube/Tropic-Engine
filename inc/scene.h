#ifndef SCENE_H
#define SCENE_H
#include <vector.h>
#include "object.h"
//#include "camera.h"
#include "handles.h"

typedef struct sScene
{
    char *name;
    vector( ObjectID ) entities;                    // global vector of all entity handles in the game
    vector( CameraID ) cameras;
    CameraID active_camera;
    // Add more fields as needed for your scene
    void (*on_enter)(struct sScene* self);
    void (*on_update)(struct sScene* self, float delta_time);
    void (*on_render)(struct sScene* self);
    void (*on_exit)(struct sScene* self);
} Scene;

#endif /* SCENE_H */