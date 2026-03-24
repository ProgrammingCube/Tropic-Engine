#include "tropic.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

void Tropic_releaseObjectPayload(void *payload);

static bool _Tropic_getAxisAlignedRotationComponent(vec3 axis, int *out_index, float *out_sign);
static void _Tropic_rotatePointAroundPivot(vec3 point,
                                           vec3 pivot,
                                           vec3 axis,
                                           float degrees,
                                           vec3 out_point);
static void _Tropic_rotateWorldBasis(Scene *scene, vec3 axis, float degrees);
static void _Tropic_applyWorldSpinDelta(TropicID engine_id, Scene *scene, float delta_degrees);

static bool _Tropic_normalizeSpinAxis(vec3 axis, vec3 out_axis)
{
    if (!axis || !out_axis || glm_vec3_norm2(axis) <= 0.000001f) return false;
    glm_vec3_normalize_to(axis, out_axis);
    return true;
}

Scene* Tropic_getCurrentScenePtr( Tropic* self )
{
    if ( !self || !self->scene_manager || self->current_scene == 0 ) return NULL;
    return ( Scene* )idmgr_get( self->scene_manager, self->current_scene );
}

bool Tropic_setBackgroundColor(TropicID engine_id, vec4 color)
{
    Tropic* self = Tropic_getById(engine_id);
    Scene* scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return false;
    memcpy(scene->background_color, color, sizeof(vec4));
    return true;
}

bool Tropic_spinWorldAroundObject( TropicID engine_id,
                                   ObjectID pivot_object_id,
                                   vec3 axis,
                                   float degrees,
                                   float duration_seconds )
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    Object *pivot_object = Tropic_getObject(engine_id, pivot_object_id);
    vec3 normalized_axis;
    int rotation_axis = 0;
    float rotation_sign = 1.0f;

    if (!self || !scene || !pivot_object || !pivot_object->active) return false;
    if (fabsf(degrees) <= 0.000001f) {
        scene->world_spin.active = false;
        scene->world_spin.elapsed_seconds = 0.0f;
        scene->world_spin.duration_seconds = 0.0f;
        scene->world_spin.degrees = 0.0f;
        scene->world_spin.applied_degrees = 0.0f;
        return true;
    }
    if (!_Tropic_normalizeSpinAxis(axis, normalized_axis)) return false;
    if (!_Tropic_getAxisAlignedRotationComponent(normalized_axis, &rotation_axis, &rotation_sign)) return false;

    scene->world_spin.active = duration_seconds > 0.0f;
    scene->world_spin.pivot_object_id = pivot_object_id;
    glm_vec3_copy(pivot_object->pos, scene->world_spin.pivot_position);
    glm_vec3_copy(normalized_axis, scene->world_spin.axis);
    scene->world_spin.degrees = degrees;
    scene->world_spin.duration_seconds = duration_seconds > 0.0f ? duration_seconds : 0.0f;
    scene->world_spin.elapsed_seconds = 0.0f;
    scene->world_spin.applied_degrees = 0.0f;

    if (!scene->world_spin.active) {
        _Tropic_applyWorldSpinDelta(engine_id, scene, degrees);
        scene->world_spin.applied_degrees = degrees;
    }

    return true;
}

bool Tropic_invertGravity(TropicID engine_id, SceneID scene_id)
{
    Tropic* self = Tropic_getById(engine_id);
    Scene* scene = Tropic_getSceneByID(engine_id, scene_id);
    if (!self || !scene) return false;
    scene->gravity[0] = -scene->gravity[0];
    scene->gravity[1] = -scene->gravity[1];
    scene->gravity[2] = -scene->gravity[2];
    return true;
}

bool Tropic_setGravity(TropicID engine_id, SceneID scene_id, vec3 gravity)
{
    Tropic* self = Tropic_getById(engine_id);
    Scene* scene = Tropic_getSceneByID(engine_id, scene_id);
    if (!self || !scene) return false;
    memcpy(scene->gravity, gravity, sizeof(vec3));
    return true;
}

void Tropic_updateSceneAnimations(TropicID engine_id, float delta_time)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);

    if (!self || !scene || delta_time <= 0.0f) return;

    for (size_t i = 0; i < vector_size(scene->cameras); ++i) {
        TropicCamera *camera = Tropic_getCamera(engine_id, scene->cameras[i]);
        if (!camera || !camera->spin.active) continue;

        camera->spin.elapsed_seconds += delta_time;
        if (camera->spin.elapsed_seconds > camera->spin.duration_seconds) {
            camera->spin.elapsed_seconds = camera->spin.duration_seconds;
        }
    }

    if (scene->world_spin.active) {
        float current_degrees;
        float progress;

        scene->world_spin.elapsed_seconds += delta_time;
        if (scene->world_spin.elapsed_seconds > scene->world_spin.duration_seconds) {
            scene->world_spin.elapsed_seconds = scene->world_spin.duration_seconds;
        }

        progress = scene->world_spin.duration_seconds > 0.0f
            ? scene->world_spin.elapsed_seconds / scene->world_spin.duration_seconds
            : 1.0f;
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        current_degrees = scene->world_spin.degrees * progress;
        _Tropic_applyWorldSpinDelta(engine_id,
                                    scene,
                                    current_degrees - scene->world_spin.applied_degrees);
        scene->world_spin.applied_degrees = current_degrees;

        if (progress >= 1.0f) {
            scene->world_spin.active = false;
        }
    }
}
static bool _Tropic_getAxisAlignedRotationComponent(vec3 axis, int *out_index, float *out_sign)
{
    vec3 normalized_axis;
    float abs_x;
    float abs_y;
    float abs_z;

    if (!out_index || !out_sign) return false;
    if (!_Tropic_normalizeSpinAxis(axis, normalized_axis)) return false;

    abs_x = fabsf(normalized_axis[0]);
    abs_y = fabsf(normalized_axis[1]);
    abs_z = fabsf(normalized_axis[2]);

    if (abs_x >= 0.999f && abs_y <= 0.001f && abs_z <= 0.001f) {
        *out_index = 0;
        *out_sign = normalized_axis[0] >= 0.0f ? 1.0f : -1.0f;
        return true;
    }
    if (abs_y >= 0.999f && abs_x <= 0.001f && abs_z <= 0.001f) {
        *out_index = 1;
        *out_sign = normalized_axis[1] >= 0.0f ? 1.0f : -1.0f;
        return true;
    }
    if (abs_z >= 0.999f && abs_x <= 0.001f && abs_y <= 0.001f) {
        *out_index = 2;
        *out_sign = normalized_axis[2] >= 0.0f ? 1.0f : -1.0f;
        return true;
    }

    return false;
}

static void _Tropic_rotatePointAroundPivot(vec3 point,
                                           vec3 pivot,
                                           vec3 axis,
                                           float degrees,
                                           vec3 out_point)
{
    mat4 rotation;
    vec3 relative;
    vec4 relative4;
    vec4 rotated4;
    vec3 rotated;

    if (!point || !pivot || !axis || !out_point) return;

    glm_rotate_make(rotation, glm_rad(degrees), axis);
    glm_vec3_sub(point, pivot, relative);
    relative4[0] = relative[0];
    relative4[1] = relative[1];
    relative4[2] = relative[2];
    relative4[3] = 0.0f;
    glm_mat4_mulv(rotation, relative4, rotated4);
    glm_vec3(rotated4, rotated);
    glm_vec3_add(pivot, rotated, out_point);
}

static void _Tropic_rotateWorldBasis(Scene *scene, vec3 axis, float degrees)
{
    mat4 rotation4;
    mat3 rotation3;
    mat3 updated_basis;

    if (!scene || fabsf(degrees) <= 0.000001f) return;

    glm_rotate_make(rotation4, glm_rad(degrees), axis);
    for (int column = 0; column < 3; ++column) {
        for (int row = 0; row < 3; ++row) {
            rotation3[column][row] = rotation4[column][row];
        }
    }
    glm_mat3_mul(rotation3, scene->world_up, updated_basis);
    glm_mat3_copy(updated_basis, scene->world_up);
}

static void _Tropic_applyWorldSpinDelta(TropicID engine_id, Scene *scene, float delta_degrees)
{
    vec3 pivot;
    int rotation_axis = 0;
    float rotation_sign = 1.0f;

    if (!scene || fabsf(delta_degrees) <= 0.000001f) return;
    if (!_Tropic_getAxisAlignedRotationComponent(scene->world_spin.axis, &rotation_axis, &rotation_sign)) return;

    glm_vec3_copy(scene->world_spin.pivot_position, pivot);
    if (scene->world_spin.pivot_object_id != 0) {
        Object *pivot_object = Tropic_getObject(engine_id, scene->world_spin.pivot_object_id);
        if (pivot_object && pivot_object->active) {
            glm_vec3_copy(pivot_object->pos, pivot);
            glm_vec3_copy(pivot, scene->world_spin.pivot_position);
        }
    }

    for (size_t i = 0; i < vector_size(scene->entities); ++i) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        vec3 rotated_position;

        if (!object || !object->active || object->id == scene->world_spin.pivot_object_id) continue;

        _Tropic_rotatePointAroundPivot(object->pos,
                                       pivot,
                                       scene->world_spin.axis,
                                       delta_degrees,
                                       rotated_position);
        glm_vec3_copy(rotated_position, object->pos);
        object->rot[rotation_axis] += delta_degrees * rotation_sign;
    }

    _Tropic_rotateWorldBasis(scene, scene->world_spin.axis, delta_degrees);
}

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

static void _Scene_freeMaterialPayload(void *payload)
{
    TropicMaterial *material = (TropicMaterial*)payload;
    if (!material) return;
    free(material);
}

static void _Scene_free( Scene* scene )
{
    if ( !scene ) return;

    if ( scene->objects_manager ) {
        idmgr_free_all( scene->objects_manager, Tropic_releaseObjectPayload );
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
    if ( scene->materials_manager ) {
        idmgr_free_all( scene->materials_manager, _Scene_freeMaterialPayload );
        scene->materials_manager = NULL;
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
    glm_vec3_copy((vec3){ 0.2f, 0.2f, 0.2f }, scene->ambient_light_color);
    glm_vec3_copy((vec3){ 0.1f, 0.1f, 0.1f }, scene->background_color);
    glm_vec3_copy((vec3){ 0.0f, -18.0f, 0.0f }, scene->gravity);
    glm_mat3_identity(scene->world_up);

    scene->objects_manager = idmgr_create( 256 );
    scene->meshes_manager = idmgr_create( 128 );
    scene->textures_manager = idmgr_create( 128 );
    scene->cameras_manager = idmgr_create( 32 );
    scene->shaders_manager = idmgr_create( 64 );
    scene->materials_manager = idmgr_create( 64 );
    if ( !scene->objects_manager ||
         !scene->meshes_manager ||
         !scene->textures_manager ||
         !scene->cameras_manager ||
         !scene->shaders_manager ||
         !scene->materials_manager ) {
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