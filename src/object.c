
#include "tropic.h"
#include "object.h"

#include <math.h>
#include <string.h>

void Tropic_releaseObjectPayload(void *payload)
{
    Object *object = (Object*)payload;
    if (!object) return;

    if (object->current_collision_ids) {
        vector_free(object->current_collision_ids);
        object->current_collision_ids = NULL;
    }
    if (object->previous_collision_ids) {
        vector_free(object->previous_collision_ids);
        object->previous_collision_ids = NULL;
    }

    free(object);
}

static void _Tropic_setDefaultColliderHalfExtents(Object *object)
{
    if (!object) return;
    object->collider.half_extents[0] = fabsf(object->scale[0]);
    object->collider.half_extents[1] = fabsf(object->scale[1]);
    object->collider.half_extents[2] = fabsf(object->scale[2]);
}

static bool _Tropic_eventPhaseMatches(TropicEventTriggerMode trigger_mode,
                                      TropicCollisionPhase collision_phase)
{
    switch (trigger_mode)
    {
    case TROPIC_EVENT_TRIGGER_STAY:
        return collision_phase == TROPIC_COLLISION_STAY;
    case TROPIC_EVENT_TRIGGER_EXIT:
        return collision_phase == TROPIC_COLLISION_EXIT;
    case TROPIC_EVENT_TRIGGER_ENTER:
    default:
        return collision_phase == TROPIC_COLLISION_ENTER;
    }
}

static float _Tropic_getEventDurationSeconds(const TropicEventSpec *event_spec)
{
    if (!event_spec) return 0.0f;
    if (event_spec->duration_seconds > 0.0f) return event_spec->duration_seconds;
    if (event_spec->speed > 0.0f) return fabsf(event_spec->degrees) / event_spec->speed;
    return 0.0f;
}

// perhaps change to Tropic_addObject and have a separate, true, Tropic_newObject that adds a generic object?
// add _scene_id to Object struct so we can easily query which scene an object belongs to without having to search through all scenes?
ObjectID Tropic_newObject( TropicID engine, const Object* proto)
{
    Tropic *self = Tropic_getById( engine );
    Scene *scene = Tropic_getCurrentScenePtr( self );
    if (!self || !scene) return 0;
    Object *o = (Object*)malloc(sizeof(Object));
    if (!o) return 0;
    if (proto) memcpy(o, proto, sizeof(Object));
    else memset(o, 0, sizeof(Object));

    /* ensure sensible defaults */
    o->current_collision_ids = NULL;
    o->previous_collision_ids = NULL;

    if (o->type == 0) o->type = TYPE_GENERIC;
    o->active = true;

    if (o->collider.type == TROPIC_COLLIDER_NONE) {
        o->collider.type = TROPIC_COLLIDER_AABB;
    }
    if (o->collider.half_extents[0] == 0.0f &&
        o->collider.half_extents[1] == 0.0f &&
        o->collider.half_extents[2] == 0.0f) {
        _Tropic_setDefaultColliderHalfExtents(o);
    }

    if (o->body.ground_friction <= 0.0f) o->body.ground_friction = 18.0f;
    if (o->body.air_friction <= 0.0f) o->body.air_friction = 4.0f;

    switch (o->type)
    {
    case TYPE_PLATFORM:
        o->collider.enabled = true;
        o->collider.flags |= TROPIC_COLLIDER_FLAG_SOLID;
        break;
    case TYPE_SPIKE:
        o->collider.enabled = true;
        o->collider.flags |= TROPIC_COLLIDER_FLAG_HAZARD;
        break;
    case TYPE_JUMPPAD:
        o->collider.enabled = true;
        o->collider.flags |= TROPIC_COLLIDER_FLAG_TRIGGER;
        break;
    case TYPE_EVENT:
        o->collider.enabled = true;
        o->collider.flags |= TROPIC_COLLIDER_FLAG_TRIGGER;
        break;
    default:
        break;
    }

    Handle local_id = idmgr_alloc(scene->objects_manager, o);
    if (local_id == 0) { free(o); return 0; }
    o->id = Tropic_makeObjectID(scene->id, local_id);

    if (o->id != 0)
    {
        vector_push_back(scene->entities, o->id );
        o->_scene_ptr = scene;
    }

    return o->id;
}

Object* Tropic_getObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    SceneID scene_id = Tropic_getSceneIDFromObjectID(id);
    Handle local_id = Tropic_getLocalHandleFromObjectID(id);
    Scene *scene = Tropic_getSceneByID(engine, scene_id);
    if (!self || !scene || local_id == 0) return NULL;
    return (Object*)idmgr_get(scene->objects_manager, local_id);
}

bool Tropic_freeObject( TropicID engine, ObjectID id)
{
    Tropic *self = Tropic_getById( engine );
    SceneID scene_id = Tropic_getSceneIDFromObjectID(id);
    Handle local_id = Tropic_getLocalHandleFromObjectID(id);
    Scene *scene = Tropic_getSceneByID(engine, scene_id);
    if (!self || !scene || local_id == 0) return false;
    Object *o = (Object*)idmgr_get(scene->objects_manager, local_id);
    if (!o) return false;
    bool ok = idmgr_free(scene->objects_manager, local_id);
    if (ok) Tropic_releaseObjectPayload(o);
    return ok;
}

SceneID Tropic_getObjectScene( ObjectID id )
{
    return Tropic_getSceneIDFromObjectID(id);
}

TropicID Tropic_getObjectEngine( ObjectID id )
{
    SceneID scene_id = Tropic_getSceneIDFromObjectID(id);
    Tropic *engine = Tropic_getActiveEnginePtr();
    if (!engine) return 0;

    Scene *scene = Tropic_getSceneByID(Tropic_getByPtr(engine), scene_id);
    if (!scene || !scene->_engine_ptr) return 0;
    return Tropic_getByPtr(scene->_engine_ptr);
}

ObjectID Tropic_findFirstObjectOfType( TropicID engine_id, ObjectType type )
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return 0;

    for (size_t i = 0; i < vector_size(scene->entities); i++) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        if (object && object->type == type) {
            return object->id;
        }
    }
    return 0;
}

ObjectID Tropic_findObjectByUid( TropicID engine_id, const char *uid )
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);

    if (!self || !scene || !uid || uid[0] == '\0') return 0;

    for (size_t i = 0; i < vector_size(scene->entities); i++) {
        Object *object = Tropic_getObject(engine_id, scene->entities[i]);
        if (object && strcmp(object->uid, uid) == 0) {
            return object->id;
        }
    }

    return 0;
}

bool Tropic_setObjectPosition( TropicID engine_id, ObjectID id, vec3 position )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_copy(position, o->pos);
    return true;
}

bool Tropic_setObjectRotation( TropicID engine_id, ObjectID id, vec3 rotation )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_copy(rotation, o->rot);
    return true;
}

bool Tropic_setObjectScale( TropicID engine_id, ObjectID id, vec3 scale )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_copy(scale, o->scale);
    _Tropic_setDefaultColliderHalfExtents(o);
    return true;
}

bool Tropic_setObjectCollisionCallback(TropicID engine_id,
                                       ObjectID id,
                                       TropicCollisionCallback callback,
                                       void *user_data)
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    o->collision_callback = callback;
    o->collision_user_data = user_data;
    return true;
}

bool Tropic_getObjectPosition( TropicID engine_id, ObjectID id, vec3 out_position )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o || !out_position) return false;
    glm_vec3_copy(o->pos, out_position);
    return true;
}

bool Tropic_getObjectRotation( TropicID engine_id, ObjectID id, vec3 out_rotation )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o || !out_rotation) return false;
    glm_vec3_copy(o->rot, out_rotation);
    return true;
}

bool Tropic_getObjectScale( TropicID engine_id, ObjectID id, vec3 out_scale )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o || !out_scale) return false;
    glm_vec3_copy(o->scale, out_scale);
    return true;
}

bool Tropic_translateObject( TropicID engine_id, ObjectID id, vec3 translation )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_add(o->pos, translation, o->pos);
    return true;
}

bool Tropic_rotateObject( TropicID engine_id, ObjectID id, vec3 rotation )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_add(o->rot, rotation, o->rot);
    return true;
}

bool Tropic_scaleObject( TropicID engine_id, ObjectID id, vec3 scale )
{
    Object *o = Tropic_getObject(engine_id, id);
    if (!o) return false;
    glm_vec3_mul(o->scale, scale, o->scale);
    glm_vec3_mul(o->collider.half_extents, scale, o->collider.half_extents);
    o->collider.half_extents[0] = fabsf(o->collider.half_extents[0]);
    o->collider.half_extents[1] = fabsf(o->collider.half_extents[1]);
    o->collider.half_extents[2] = fabsf(o->collider.half_extents[2]);
    return true;
}

bool Tropic_shouldTriggerObjectEvent( TropicID engine_id,
                                      ObjectID event_object_id,
                                      const TropicCollisionEvent *collision_event )
{
    Object *object = Tropic_getObject(engine_id, event_object_id);

    if (!object || object->type != TYPE_EVENT || !collision_event) return false;
    if (!_Tropic_eventPhaseMatches(object->event.trigger_mode, collision_event->phase)) return false;
    if (object->event.trigger_once && object->event.has_fired) return false;

    if (object->event.trigger_once) {
        object->event.has_fired = true;
    }

    return true;
}

bool Tropic_executeObjectBuiltinEvent( TropicID engine_id, ObjectID event_object_id )
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    Object *object = Tropic_getObject(engine_id, event_object_id);
    float duration_seconds;

    if (!self || !scene || !object || object->type != TYPE_EVENT) return false;

    duration_seconds = _Tropic_getEventDurationSeconds(&object->event);

    switch (object->event.action_type)
    {
    case TROPIC_EVENT_ACTION_GRAVITY_SET:
        return Tropic_setGravity(engine_id, scene->id, object->event.gravity);
    case TROPIC_EVENT_ACTION_GRAVITY_FLIP:
        return Tropic_invertGravity(engine_id, scene->id);
    case TROPIC_EVENT_ACTION_WORLD_SPIN:
    {
        ObjectID pivot_object_id = Tropic_findObjectByUid(engine_id, object->event.target_uid);
        if (pivot_object_id == 0) return false;
        return Tropic_spinWorldAroundObject(engine_id,
                                            pivot_object_id,
                                            object->event.axis,
                                            object->event.degrees,
                                            duration_seconds);
    }
    case TROPIC_EVENT_ACTION_CAMERA_SPIN:
    {
        CameraID camera_id = Tropic_getActiveCameraId(engine_id);
        if (camera_id == 0) return false;
        return Tropic_spinCamera(engine_id,
                                 camera_id,
                                 object->event.axis,
                                 object->event.degrees,
                                 duration_seconds);
    }
    case TROPIC_EVENT_ACTION_NONE:
    case TROPIC_EVENT_ACTION_CUSTOM:
    default:
        return false;
    }
}
