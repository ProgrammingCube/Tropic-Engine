#include "tropic.h"
#include "camera.h"

#include <stdlib.h>
#include <string.h>

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