#ifndef CAMERA_H
#define CAMERA_H

#include <cglm/cglm.h>
#include "tropic.h"
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

#endif