#ifndef SCENE_H
#define SCENE_H
#include <vector.h>
#include "object.h"
#include "handles.h"

typedef struct sScene
{
    char *name;
    SceneID id;
    vector( ObjectID ) entities;                    // global vector of all entity handles in the game
    vector( CameraID ) cameras;
    CameraID active_camera;
    // Add more fields as needed for your scene
    void (*on_enter)(struct sScene* self);
    void (*on_update)(struct sScene* self, float delta_time);
    void (*on_render)(struct sScene* self);
    void (*on_exit)(struct sScene* self);

    /* resource pools */
    IDManager* cameras_manager;
    IDManager* objects_manager;
    IDManager* meshes_manager;
    IDManager* textures_manager;
} Scene;

SceneID Tropic_createScene( TropicID engine_id, const char* name );
Scene*  Tropic_getSceneByID( TropicID engine_id, SceneID scene_id );
Scene*  Tropic_getCurrentScene( TropicID engine_id );
SceneID Tropic_getCurrentSceneID( TropicID engine_id );
bool    Tropic_setCurrentScene( TropicID engine_id, SceneID scene_id );
bool    Tropic_freeScene( TropicID engine_id, SceneID scene_id );

#endif /* SCENE_H */