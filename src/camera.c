#include "tropic.h"
#include "camera.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static bool _Tropic_normalizeSpinAxis(vec3 axis, vec3 out_axis)
{
    if (!axis || !out_axis || glm_vec3_norm2(axis) <= 0.000001f) return false;
    glm_vec3_normalize_to(axis, out_axis);
    return true;
}

static void _Tropic_applyCameraOrbit(TropicCamera *camera,
                                     vec3 base_position,
                                     vec3 base_target,
                                     vec3 base_up,
                                     vec3 axis,
                                     float degrees)
{
    mat4 rotation;
    vec3 offset;
    vec4 offset4;
    vec4 rotated_offset4;
    vec4 up4;
    vec4 rotated_up4;
    vec3 rotated_offset;
    vec3 rotated_up;

    if (!camera) return;

    glm_rotate_make(rotation, glm_rad(degrees), axis);

    glm_vec3_sub(base_position, base_target, offset);
    offset4[0] = offset[0];
    offset4[1] = offset[1];
    offset4[2] = offset[2];
    offset4[3] = 0.0f;
    glm_mat4_mulv(rotation, offset4, rotated_offset4);
    glm_vec3(rotated_offset4, rotated_offset);

    up4[0] = base_up[0];
    up4[1] = base_up[1];
    up4[2] = base_up[2];
    up4[3] = 0.0f;
    glm_mat4_mulv(rotation, up4, rotated_up4);
    glm_vec3(rotated_up4, rotated_up);
    if (glm_vec3_norm2(rotated_up) > 0.000001f) {
        glm_vec3_normalize(rotated_up);
    }

    glm_vec3_add(base_target, rotated_offset, camera->position);
    glm_vec3_copy(base_target, camera->target);
    glm_vec3_copy(rotated_up, camera->up);
}

static bool _Tropic_storeFollowOffsetFromCamera(TropicID engine_id, TropicCamera *camera)
{
    Object *object;
    vec3 world_offset;

    if (!camera || camera->follow_object_id == 0) return false;

    object = Tropic_getObject(engine_id, camera->follow_object_id);
    if (!object || !object->active) return false;

    glm_vec3_sub(camera->position, object->pos, world_offset);

    if (camera->follow_cfg.space == FOLLOW_LOCAL_SPACE) {
        mat4 rotation;
        mat4 inverse_rotation;
        vec4 world_offset4;
        vec4 local_offset4;

        glm_mat4_identity(rotation);
        glm_rotate(rotation, glm_rad(object->rot[0]), (vec3){ 1.0f, 0.0f, 0.0f });
        glm_rotate(rotation, glm_rad(object->rot[1]), (vec3){ 0.0f, 1.0f, 0.0f });
        glm_rotate(rotation, glm_rad(object->rot[2]), (vec3){ 0.0f, 0.0f, 1.0f });
        glm_mat4_inv(rotation, inverse_rotation);

        world_offset4[0] = world_offset[0];
        world_offset4[1] = world_offset[1];
        world_offset4[2] = world_offset[2];
        world_offset4[3] = 0.0f;
        glm_mat4_mulv(inverse_rotation, world_offset4, local_offset4);
        glm_vec3(local_offset4, camera->follow_cfg.camera_offset);
    } else {
        glm_vec3_copy(world_offset, camera->follow_cfg.camera_offset);
    }

    return true;
}

CameraID Tropic_newCamera( TropicID engine_id,
                           vec3 position,
                           vec3 up,
                           vec3 target,
                           float fov,
                           float roll
                        )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return 0;
    TropicCamera *c = ( TropicCamera* )malloc( sizeof( TropicCamera ) );
    if ( !c ) return 0;
    // set up memory for TropicCamera
    memset( c, 0, sizeof( TropicCamera ) );
    c->active = true;
    glm_vec3_copy( position, c->position );
    glm_vec3_copy( up, c->up );
    glm_vec3_copy( target, c->target );
    c->fov = fov;
    c->roll = roll;

    // add to Handler
    Handle local_id = idmgr_alloc( scene->cameras_manager, c );
    if ( local_id == 0 ) { free( c ); return 0; }
    c->id = Tropic_makeCameraID(scene->id, local_id);
    if ( c->id != 0 )
    {
        vector_push_back( scene->cameras, c->id );
        //self->current_scene->active_camera = c->id;
        // set up camera parameters here
    }
    return c->id;
}

bool Tropic_setCamera( TropicID engine_id, CameraID camera_id )
{
    Tropic *self = Tropic_getById( engine_id );
    SceneID scene_id = Tropic_getSceneIDFromCameraID(camera_id);
    Handle local_id = Tropic_getLocalHandleFromCameraID(camera_id);
    Scene *scene = Tropic_getSceneByID( engine_id, scene_id );
    if ( !self || !scene || local_id == 0 ) return false;
    TropicCamera *c = ( TropicCamera* )idmgr_get( scene->cameras_manager, local_id );
    if ( !c ) return false;
    scene->active_camera = camera_id;
    return true;
}

TropicCamera* Tropic_getCamera( TropicID engine_id, CameraID id )
{
    Tropic *self = Tropic_getById( engine_id );
    SceneID scene_id = Tropic_getSceneIDFromCameraID(id);
    Handle local_id = Tropic_getLocalHandleFromCameraID(id);
    Scene *scene = Tropic_getSceneByID( engine_id, scene_id );
    if ( !self || !scene || local_id == 0 ) return NULL;
    return ( TropicCamera* )idmgr_get( scene->cameras_manager, local_id );
}

TropicCamera* Tropic_getActiveCamera( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return NULL;
    Handle local_id = Tropic_getLocalHandleFromCameraID(scene->active_camera);
    if (local_id == 0) return NULL;
    return ( TropicCamera* )idmgr_get( scene->cameras_manager, local_id );
}

CameraID Tropic_getActiveCameraId( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return 0;
    return scene->active_camera;
}

bool Tropic_lookAtObjectById( TropicID engine, ObjectID object_id )
{
    Tropic *self = Tropic_getById(engine);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene || object_id == 0) return false;

    Object *object = Tropic_getObject(engine, object_id);
    if (!object || !object->active) return false;

    TropicCamera *camera = Tropic_getActiveCamera(engine);
    if (!camera) return false;

    return Tropic_setCameraTarget(engine, camera->id, object->pos);
}

SceneID Tropic_getCameraScene( TropicID engine_id, CameraID camera_id )
{
    (void)engine_id;
    return Tropic_getSceneIDFromCameraID(camera_id);
}

bool Tropic_setCameraFOV( TropicID engine_id, CameraID camera_id, float fov )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    c->fov = fov;
    return true;
}

float Tropic_getCameraFOV( TropicID engine_id, CameraID camera_id )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return 0.0f;
    return c->fov;
}

bool Tropic_setCameraPosition( TropicID engine_id, CameraID camera_id, vec3 position )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    glm_vec3_copy( position, c->position );
    return true;
}

void Tropic_getCameraPosition( TropicID engine_id, CameraID camera_id, vec3 *out_position )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c || !out_position ) return;
    glm_vec3_copy( c->position, *out_position );
}

bool Tropic_setCameraTarget( TropicID engine_id, CameraID camera_id, vec3 target )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    glm_vec3_copy( target, c->target );
    return true;
}

void Tropic_getCameraTarget( TropicID engine_id, CameraID camera_id, vec3 *out_target )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c || !out_target ) return;
    glm_vec3_copy( c->target, *out_target );
}

bool Tropic_setCameraUp( TropicID engine_id, CameraID camera_id, vec3 up )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    glm_vec3_copy( up, c->up );
    return true;
}

void Tropic_getCameraUp( TropicID engine_id, CameraID camera_id, vec3 *out_up )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c || !out_up ) return;
    glm_vec3_copy( c->up, *out_up );
}

bool Tropic_setCameraRoll( TropicID engine_id, CameraID camera_id, float roll )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    c->roll = roll;
    return true;
}

float Tropic_getCameraRoll( TropicID engine_id, CameraID camera_id )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return 0.0f;
    return c->roll;
}

bool Tropic_followObjectById(TropicID engine_id,
    CameraID camera_id,
    ObjectID object_id,
    vec3 camera_offset,
    vec3 target_offset)
{
    TropicCamera* camera = Tropic_getCamera(engine_id, camera_id);
    Object* object = Tropic_getObject(engine_id, object_id);
    vec3 focus_point;
    vec3 camera_position;

    if (!camera || !object || !camera->active || !object->active)
        return false;

    glm_vec3_copy(object->pos, focus_point);
    glm_vec3_add(focus_point, target_offset, focus_point);

    glm_vec3_copy(object->pos, camera_position);
    glm_vec3_add(camera_position, camera_offset, camera_position);

    glm_vec3_copy(camera_position, camera->position);
    glm_vec3_copy(focus_point, camera->target);

    return true;
}

bool Tropic_spinCamera( TropicID engine_id,
                        CameraID camera_id,
                        vec3 axis,
                        float degrees,
                        float duration_seconds )
{
    return Tropic_spinCameraEx( engine_id,
                                camera_id,
                                axis,
                                degrees,
                                duration_seconds,
                                TROPIC_CAMERA_SPIN_FLAG_PERSIST_FOLLOW_OFFSET );
}

bool Tropic_spinCameraEx( TropicID engine_id,
                          CameraID camera_id,
                          vec3 axis,
                          float degrees,
                          float duration_seconds,
                          uint32_t flags )
{
    TropicCamera *camera = Tropic_getCamera( engine_id, camera_id );
    vec3 normalized_axis;

    if ( !camera ) return false;
    if ( fabsf( degrees ) <= 0.000001f ) {
        camera->spin.active = false;
        camera->spin.elapsed_seconds = 0.0f;
        camera->spin.duration_seconds = 0.0f;
        camera->spin.degrees = 0.0f;
        camera->spin.flags = TROPIC_CAMERA_SPIN_FLAG_NONE;
        return true;
    }
    if ( !_Tropic_normalizeSpinAxis( axis, normalized_axis ) ) return false;

    glm_vec3_copy( normalized_axis, camera->spin.axis );
    glm_vec3_copy( camera->position, camera->spin.start_position );
    glm_vec3_copy( camera->target, camera->spin.start_target );
    glm_vec3_copy( camera->up, camera->spin.start_up );
    camera->spin.degrees = degrees;
    camera->spin.elapsed_seconds = 0.0f;
    camera->spin.duration_seconds = duration_seconds > 0.0f ? duration_seconds : 0.0f;
    camera->spin.flags = flags;
    camera->spin.active = camera->spin.duration_seconds > 0.0f;

    if ( !camera->spin.active ) {
        _Tropic_applyCameraOrbit( camera,
                                  camera->spin.start_position,
                                  camera->spin.start_target,
                                  camera->spin.start_up,
                                  camera->spin.axis,
                                  camera->spin.degrees );
    }

    return true;
}

bool Tropic_applyCameraSpin( TropicID engine_id, CameraID camera_id )
{
    TropicCamera *camera = Tropic_getCamera( engine_id, camera_id );
    vec3 base_position;
    vec3 base_target;
    float progress;
    float degrees;

    if ( !camera || !camera->spin.active ) return false;

    progress = camera->spin.duration_seconds > 0.0f
        ? camera->spin.elapsed_seconds / camera->spin.duration_seconds
        : 1.0f;
    if ( progress < 0.0f ) progress = 0.0f;
    if ( progress > 1.0f ) progress = 1.0f;
    degrees = camera->spin.degrees * progress;

    if ( camera->follow_object_id != 0 ) {
        glm_vec3_copy( camera->position, base_position );
        glm_vec3_copy( camera->target, base_target );
    } else {
        glm_vec3_copy( camera->spin.start_position, base_position );
        glm_vec3_copy( camera->spin.start_target, base_target );
    }

    _Tropic_applyCameraOrbit( camera,
                              base_position,
                              base_target,
                              camera->spin.start_up,
                              camera->spin.axis,
                              degrees );

    if ( progress >= 1.0f ) {
        if ( camera->follow_object_id != 0 &&
             ( camera->spin.flags & TROPIC_CAMERA_SPIN_FLAG_PERSIST_FOLLOW_OFFSET ) != 0u ) {
            (void)_Tropic_storeFollowOffsetFromCamera( engine_id, camera );
        }
        camera->spin.active = false;
        camera->spin.elapsed_seconds = camera->spin.duration_seconds;
    }

    return true;
}

bool Tropic_bindCameraToObject( TropicID engine_id,
                                CameraID camera_id,
                                ObjectID object_id,
                                const TropicFollowConfig* config )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;

    c->follow_object_id = object_id;

    if ( object_id == 0 )
    {
        memset( &c->follow_cfg, 0, sizeof( TropicFollowConfig ) );
        return true;
    }

    if ( config )
    {
        glm_vec3_copy( config->camera_offset, c->follow_cfg.camera_offset );
        glm_vec3_copy( config->target_offset, c->follow_cfg.target_offset );
        c->follow_cfg.space = config->space;
    }
    else
    {
        memset( &c->follow_cfg, 0, sizeof( TropicFollowConfig ) );
    }

    return true;
}

bool Tropic_unbindCamera( TropicID engine_id, CameraID camera_id )
{
    TropicCamera *c = Tropic_getCamera( engine_id, camera_id );
    if ( !c ) return false;
    c->follow_object_id = 0;
    memset( &c->follow_cfg, 0, sizeof( TropicFollowConfig ) );
    return true;
}

bool Tropic_updateCameraFollow( TropicID engine_id, CameraID camera_id )
{
    TropicCamera *camera = Tropic_getCamera( engine_id, camera_id );
    if ( !camera || !camera->active || camera->follow_object_id == 0 )
        return false;

    Object *object = Tropic_getObject( engine_id, camera->follow_object_id );
    if ( !object || !object->active )
        return false;

    vec3 cam_offset;
    vec3 tgt_offset;
    glm_vec3_copy( camera->follow_cfg.camera_offset, cam_offset );
    glm_vec3_copy( camera->follow_cfg.target_offset, tgt_offset );

    if ( camera->follow_cfg.space == FOLLOW_LOCAL_SPACE )
    {
        /* Build rotation matrix from the object's XYZ Euler angles (degrees),
         * matching the same rotation order used in Tropic_Render. */
        mat4 rot;
        glm_mat4_identity( rot );
        glm_rotate( rot, glm_rad( object->rot[0] ), (vec3){ 1.0f, 0.0f, 0.0f } );
        glm_rotate( rot, glm_rad( object->rot[1] ), (vec3){ 0.0f, 1.0f, 0.0f } );
        glm_rotate( rot, glm_rad( object->rot[2] ), (vec3){ 0.0f, 0.0f, 1.0f } );

        vec4 cam_off4 = { cam_offset[0], cam_offset[1], cam_offset[2], 0.0f };
        vec4 tgt_off4 = { tgt_offset[0], tgt_offset[1], tgt_offset[2], 0.0f };
        vec4 cam_off4_rot, tgt_off4_rot;
        glm_mat4_mulv( rot, cam_off4, cam_off4_rot );
        glm_mat4_mulv( rot, tgt_off4, tgt_off4_rot );

        glm_vec3( cam_off4_rot, cam_offset );
        glm_vec3( tgt_off4_rot, tgt_offset );
    }

    glm_vec3_add( object->pos, cam_offset, camera->position );
    glm_vec3_add( object->pos, tgt_offset, camera->target );

    return true;
}