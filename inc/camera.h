#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>
#include "handles.h"

typedef struct sTropicCamera
{
    CameraID id;
    bool active;
    vec3 position;
    vec3 up;
    vec3 target;

    float fov;
    float roll;
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

#endif