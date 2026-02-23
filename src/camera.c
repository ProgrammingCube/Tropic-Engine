#include "camera.h"

CameraID Tropic_newCamera( TropicID engine_id,
                           vec3 position,
                           vec3 up,
                           vec3 target,
                           float fov,
                           float roll
                        )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return 0;
    TropicCamera *c = ( TropicCamera* )malloc( sizeof( TropicCamera ) );
    if ( !c ) return 0;
    // set up memory for TropicCamera
    memset( c, 0, sizeof( TropicCamera ) );
    c->active = true;

    // add to Handler
    Handle h = idmgr_alloc( self->cameras, c );
    if ( h == 0 ) { free( c ); return 0; }
    c->id = ( CameraID )h;
    if ( c->id != 0 )
    {
        vector_push_back( self->current_scene->cameras, c->id );
        //self->current_scene->active_camera = c->id;
        // set up camera parameters here
    }
    return ( CameraID )h;
}

bool Tropic_setCamera( TropicID engine_id, CameraID camera_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return false;
    self->current_scene->active_camera = camera_id;
    return true;
}

TropicCamera* Tropic_getCamera( TropicID engine_id, CameraID id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return NULL;
    return ( TropicCamera* )idmgr_get( self->cameras, id );
}

TropicCamera* Tropic_getActiveCamera( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return NULL;
    return ( TropicCamera* )idmgr_get( self->cameras, self->current_scene->active_camera );
}