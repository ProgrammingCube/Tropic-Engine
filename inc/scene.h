#ifndef SCENE_H
#define SCENE_H
#include <cglm/cglm.h>
#include <vector.h>
#include "object.h"
#include "handles.h"

typedef struct sTropic Tropic;

typedef struct sTropicWorldSpinState
{
    bool active;
    ObjectID pivot_object_id;
    vec3 pivot_position;
    vec3 axis;
    float degrees;
    float duration_seconds;
    float elapsed_seconds;
    float applied_degrees;
} TropicWorldSpinState;

typedef struct sScene
{
    char *name;
    Tropic* _engine_ptr;
    SceneID id;
    vector( ObjectID ) entities;                    // global vector of all entity handles in the game
    vector( CameraID ) cameras;
    CameraID active_camera;
	
    vec3 ambient_light_color;
	vec4 background_color;

    vec3 gravity;
    mat3 world_up; /* 3x3 matrix representing the world's basis, used for orienting objects and cameras. */
    TropicWorldSpinState world_spin;

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
    IDManager* shaders_manager;
    IDManager* materials_manager;
} Scene;

SceneID  Tropic_createScene( TropicID engine_id, const char* name );
Scene*   Tropic_getSceneByID( TropicID engine_id, SceneID scene_id );
Scene*   Tropic_getCurrentScene( TropicID engine_id );
SceneID  Tropic_getCurrentSceneID( TropicID engine_id );
bool     Tropic_setCurrentScene( TropicID engine_id, SceneID scene_id );
bool     Tropic_freeScene( TropicID engine_id, SceneID scene_id );
Scene*   Tropic_getCurrentScenePtr( Tropic* self );
bool     Tropic_setBackgroundColor( TropicID engine_id, vec4 color );
bool     Tropic_spinWorldAroundObject( TropicID engine_id,
                                       ObjectID pivot_object_id,
                                       vec3 axis,
                                       float degrees,
                                       float duration_seconds );
bool     Tropic_invertGravity( TropicID engine_id, SceneID scene_id );
bool     Tropic_setGravity( TropicID engine_id, SceneID scene_id, vec3 gravity );
void     Tropic_updateSceneAnimations( TropicID engine_id, float delta_time );

#endif /* SCENE_H */