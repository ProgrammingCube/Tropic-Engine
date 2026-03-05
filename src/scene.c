#include "tropic.h"

#include <stdlib.h>
#include <string.h>

static void _Scene_freeMeshPayload(void *payload)
{
    Mesh *mesh = (Mesh*)payload;
    if (!mesh) return;
    if (mesh->vbo != 0) glDeleteBuffers(1, &mesh->vbo);
    if (mesh->ebo != 0) glDeleteBuffers(1, &mesh->ebo);
    if (mesh->vao != 0) glDeleteVertexArrays(1, &mesh->vao);
    free(mesh);
}

static void _Scene_freeShaderPayload(void *payload)
{
    Shader *shader = (Shader*)payload;
    if (!shader) return;
    shader_destroy(shader);
    free(shader);
}

static void _Scene_free( Scene* scene )
{
    if ( !scene ) return;

    if ( scene->objects_manager ) {
        idmgr_free_all( scene->objects_manager, free );
        scene->objects_manager = NULL;
    }
    if ( scene->meshes_manager ) {
        idmgr_free_all( scene->meshes_manager, _Scene_freeMeshPayload );
        scene->meshes_manager = NULL;
    }
    if ( scene->textures_manager ) {
        idmgr_free_all( scene->textures_manager, free );
        scene->textures_manager = NULL;
    }
    if ( scene->shaders_manager ) {
        idmgr_free_all( scene->shaders_manager, _Scene_freeShaderPayload );
        scene->shaders_manager = NULL;
    }
    if ( scene->cameras_manager ) {
        idmgr_free_all( scene->cameras_manager, free );
        scene->cameras_manager = NULL;
    }

    if ( scene->entities ) {
        vector_free( scene->entities );
        scene->entities = NULL;
    }
    if ( scene->cameras ) {
        vector_free( scene->cameras );
        scene->cameras = NULL;
    }

    if ( scene->name ) {
        free( scene->name );
        scene->name = NULL;
    }

    free( scene );
}

static void _Scene_removeIdFromVector( vector( SceneID )* scenes, SceneID id )
{
    if ( !scenes || !( *scenes ) ) return;
    size_t count = vector_size( *scenes );
    for ( size_t i = 0; i < count; ++i ) {
        if ( ( *scenes )[i] == id ) {
            vector_erase( *scenes, i );
            return;
        }
    }
}

SceneID Tropic_createScene( TropicID engine_id, const char* name )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self || !self->scene_manager ) return 0;

    Scene *scene = ( Scene* )malloc( sizeof( Scene ) );
    if ( !scene ) return 0;
    memset( scene, 0, sizeof( Scene ) );

    scene->name = strdup( name ? name : "Scene" );
    if ( !scene->name ) {
        free( scene );
        return 0;
    }

    scene->entities = NULL;
    scene->cameras = NULL;
    scene->active_camera = 0;

    scene->objects_manager = idmgr_create( 256 );
    scene->meshes_manager = idmgr_create( 128 );
    scene->textures_manager = idmgr_create( 128 );
    scene->cameras_manager = idmgr_create( 32 );
        scene->shaders_manager = idmgr_create( 64 );
        scene->default_platform_mesh = 0;
        scene->default_platform_shader = 0;
        scene->renderer_ready = false;
    if ( !scene->objects_manager ||
         !scene->meshes_manager ||
         !scene->textures_manager ||
            !scene->cameras_manager ||
            !scene->shaders_manager ) {
        _Scene_free( scene );
        return 0;
    }

    Handle h = idmgr_alloc( self->scene_manager, scene );
    if ( h == 0 ) {
        _Scene_free( scene );
        return 0;
    }

    scene->id = ( SceneID )h;
    scene->_engine_ptr = self;
    vector_push_back( self->scenes, scene->id );
    return scene->id;
}

Scene* Tropic_getSceneByID( TropicID engine_id, SceneID scene_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self || !self->scene_manager || scene_id == 0 ) return NULL;
    return ( Scene* )idmgr_get( self->scene_manager, scene_id );
}

Scene* Tropic_getCurrentScene( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self || self->current_scene == 0 ) return NULL;
    return Tropic_getSceneByID( engine_id, self->current_scene );
}

SceneID Tropic_getCurrentSceneID( TropicID engine_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self ) return 0;
    return self->current_scene;
}

bool Tropic_setCurrentScene( TropicID engine_id, SceneID scene_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self || !self->scene_manager || scene_id == 0 ) return false;

    Scene *next = ( Scene* )idmgr_get( self->scene_manager, scene_id );
    if ( !next ) return false;

    Scene *prev = NULL;
    if ( self->current_scene != 0 ) {
        prev = ( Scene* )idmgr_get( self->scene_manager, self->current_scene );
    }

    if ( prev && prev->on_exit ) prev->on_exit( prev );
    self->current_scene = scene_id;
    if ( next->on_enter ) next->on_enter( next );
    return true;
}

bool Tropic_freeScene( TropicID engine_id, SceneID scene_id )
{
    Tropic *self = Tropic_getById( engine_id );
    if ( !self || !self->scene_manager || scene_id == 0 ) return false;

    Scene *scene = ( Scene* )idmgr_get( self->scene_manager, scene_id );
    if ( !scene ) return false;

    bool was_current = ( self->current_scene == scene_id );
    if ( was_current && scene->on_exit ) scene->on_exit( scene );

    _Scene_removeIdFromVector( &self->scenes, scene_id );

    if ( !idmgr_free( self->scene_manager, scene_id ) ) return false;
    _Scene_free( scene );

    if ( was_current ) {
        self->current_scene = 0;
        if ( self->scenes && vector_size( self->scenes ) > 0 ) {
            SceneID replacement = self->scenes[0];
            (void)Tropic_setCurrentScene( engine_id, replacement );
        }
    }

    return true;
}