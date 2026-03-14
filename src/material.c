#include "tropic.h"

#include <stdlib.h>
#include <string.h>

MaterialID Tropic_newMaterial(TropicID engine_id, const TropicMaterial* proto)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    TropicMaterial *material;
    Handle h;

    if (!self || !scene) return 0;

    material = (TropicMaterial*)malloc(sizeof(TropicMaterial));
    if (!material) return 0;
    if (proto) memcpy(material, proto, sizeof(TropicMaterial));
    else memset(material, 0, sizeof(TropicMaterial));

    h = idmgr_alloc(scene->materials_manager, material);
    if (h == 0) {
        free(material);
        return 0;
    }

    material->id = (uint32_t)h;
    return (MaterialID)h;
}

TropicMaterial* Tropic_getMaterial(TropicID engine_id, MaterialID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    if (!self || !scene) return NULL;
    return (TropicMaterial*)idmgr_get(scene->materials_manager, id);
}

bool Tropic_freeMaterial(TropicID engine_id, MaterialID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    TropicMaterial *material;
    bool ok;

    if (!self || !scene) return false;

    material = (TropicMaterial*)idmgr_get(scene->materials_manager, id);
    if (!material) return false;

    ok = idmgr_free(scene->materials_manager, id);
    if (ok) free(material);
    return ok;
}

bool Tropic_setObjectMaterial(TropicID engine_id, ObjectID object_id, MaterialID material_id)
{
    Object *object = Tropic_getObject(engine_id, object_id);

    if (!object) return false;
    if (material_id != 0 && !Tropic_getMaterial(engine_id, material_id)) return false;

    object->material_id = material_id;
    return true;
}

MaterialID Tropic_getObjectMaterial(TropicID engine_id, ObjectID object_id)
{
    Object *object = Tropic_getObject(engine_id, object_id);
    if (!object) return 0;
    return object->material_id;
}
