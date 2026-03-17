#ifndef CAMERA_H
#define CAMERA_H

#include <stdint.h>
#include <cglm/cglm.h>
#include "handles.h"

typedef struct sScene Scene;

/* Whether follow offsets are in world space or rotate with the object. */
typedef enum eTropicFollowSpace
{
    FOLLOW_WORLD_SPACE, /* offsets are absolute world-space vectors */
    FOLLOW_LOCAL_SPACE, /* offsets rotate with the object's orientation */
} TropicFollowSpace;

/*
 * Configuration for binding a camera to follow an object.
 * camera_offset: position of the camera relative to the object's origin.
 * target_offset: look-at point relative to the object's origin.
 * space:         whether offsets are world-space or object-local-space.
 */
typedef struct sTropicFollowConfig
{
    vec3 camera_offset;
    vec3 target_offset;
    TropicFollowSpace space;
} TropicFollowConfig;

typedef enum eTropicCameraSpinFlags
{
    TROPIC_CAMERA_SPIN_FLAG_NONE = 0,
    TROPIC_CAMERA_SPIN_FLAG_PERSIST_FOLLOW_OFFSET = 1u << 0,
} TropicCameraSpinFlags;

typedef struct sTropicCameraSpinState
{
    bool active;
    vec3 axis;
    vec3 start_position;
    vec3 start_target;
    vec3 start_up;
    float degrees;
    float duration_seconds;
    float elapsed_seconds;
    uint32_t flags;
} TropicCameraSpinState;

typedef struct sTropicCamera
{
    Scene* _scene_ptr;
    CameraID id;
    bool active;
    vec3 position;
    vec3 up;
    vec3 target;

    float fov;
    float roll;

    /* Follow binding — set via Tropic_bindCameraToObject. 0 = unbound. */
    ObjectID follow_object_id;
    TropicFollowConfig follow_cfg;
    TropicCameraSpinState spin;
} TropicCamera;

CameraID Tropic_newCamera( TropicID engine_id,
                           vec3 position,
                           vec3 up,
                           vec3 target,
                           float fov,
                           float roll
                        );
bool Tropic_setCamera( TropicID engine_id, CameraID camera_id );
TropicCamera* Tropic_getCamera( TropicID engine_id, CameraID camera_id );
TropicCamera* Tropic_getActiveCamera( TropicID engine_id );
CameraID Tropic_getActiveCameraId( TropicID engine_id );
bool Tropic_lookAtObjectById( TropicID engine, ObjectID object_id );

SceneID Tropic_getCameraScene( TropicID engine_id, CameraID camera_id );

/* helper functions for camera manipulation */
bool Tropic_setCameraFOV( TropicID engine_id, CameraID camera_id, float fov );
float Tropic_getCameraFOV( TropicID engine_id, CameraID camera_id );

bool Tropic_setCameraPosition( TropicID engine_id, CameraID camera_id, vec3 position );
void Tropic_getCameraPosition( TropicID engine_id, CameraID camera_id, vec3 *out_position );

bool Tropic_setCameraTarget( TropicID engine_id, CameraID camera_id, vec3 target );
void Tropic_getCameraTarget( TropicID engine_id, CameraID camera_id, vec3 *out_target );

bool Tropic_setCameraUp( TropicID engine_id, CameraID camera_id, vec3 up );
void Tropic_getCameraUp( TropicID engine_id, CameraID camera_id, vec3 *out_up );

bool Tropic_setCameraRoll( TropicID engine_id, CameraID camera_id, float roll );
float Tropic_getCameraRoll( TropicID engine_id, CameraID camera_id );

bool Tropic_followObjectById(TropicID engine_id,
    CameraID camera_id,
    ObjectID object_id,
    vec3 camera_offset,
    vec3 target_offset);

/*
 * Bind a camera to follow an object automatically in Tropic_Render.
 * Pass object_id = 0 to unbind, or call Tropic_unbindCamera.
 * config may be NULL to use default world-space zero offsets.
 */
bool Tropic_bindCameraToObject( TropicID engine_id,
                                CameraID camera_id,
                                ObjectID object_id,
                                const TropicFollowConfig* config );

/* Clear a camera's follow binding. */
bool Tropic_unbindCamera( TropicID engine_id, CameraID camera_id );

/*
 * Apply the follow binding for a camera, computing its position and target
 * from the bound object's current transform. Called automatically by
 * Tropic_Render for the active camera; can also be called manually.
 */
bool Tropic_updateCameraFollow( TropicID engine_id, CameraID camera_id );
bool Tropic_spinCamera( TropicID engine_id,
                        CameraID camera_id,
                        vec3 axis,
                        float degrees,
                        float duration_seconds );
bool Tropic_spinCameraEx( TropicID engine_id,
                          CameraID camera_id,
                          vec3 axis,
                          float degrees,
                          float duration_seconds,
                          uint32_t flags );
bool Tropic_applyCameraSpin( TropicID engine_id, CameraID camera_id );

#endif