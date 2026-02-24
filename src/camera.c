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
    Handle h = idmgr_alloc( scene->cameras_manager, c );
    if ( h == 0 ) { free( c ); return 0; }
    c->id = ( CameraID )h;
    if ( c->id != 0 )
    {
        vector_push_back( scene->cameras, c->id );
        //self->current_scene->active_camera = c->id;
        // set up camera parameters here
    }
    return ( CameraID )h;
}

bool Tropic_setCamera( TropicID engine_id, CameraID camera_id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScene( engine_id );
    if ( !self || !scene ) return false;
    TropicCamera *c = ( TropicCamera* )idmgr_get( scene->cameras_manager, camera_id );
    if ( !c ) return false;
    scene->active_camera = camera_id;
    return true;
}

TropicCamera* Tropic_getCamera( TropicID engine_id, CameraID id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return NULL;
    return ( TropicCamera* )idmgr_get( scene->cameras_manager, id );
}

TropicCamera* Tropic_getActiveCamera( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return NULL;
    return ( TropicCamera* )idmgr_get( scene->cameras_manager, scene->active_camera );
}

CameraID Tropic_getActiveCameraId( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if ( !self || !scene ) return 0;
    return scene->active_camera;
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