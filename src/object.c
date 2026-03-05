
#include "tropic.h"
#include "object.h"

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
    if (o->type == 0) o->type = TYPE_GENERIC;
    o->active = true;

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
    if (ok) free(o);
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
    return true;
}