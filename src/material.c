#include "tropic.h"

#include <stdlib.h>
#include <string.h>

MaterialID Tropic_newMaterial(TropicID engine_id, const TropicMaterial* proto)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getCurrentScenePtr(self);
    TropicMaterial *material;
    MaterialID material_id;
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

    material_id = Tropic_makeMaterialID(scene->id, h);
    material->id = material_id;
    return material_id;
}

MaterialID Tropic_createMaterial(TropicID engine_id,
                                 MeshID mesh_id,
                                 ShaderID shader_id,
                                 TropicMaterialRenderCallback render_callback,
                                 void *user)
{
    TropicMaterial material = {0};

    material.mesh_id = mesh_id;
    material.shader_id = shader_id;
    material.render_callback = render_callback;
    material.user = user;

    return Tropic_newMaterial(engine_id, &material);
}

TropicMaterial* Tropic_getMaterial(TropicID engine_id, MaterialID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromMaterialID(id));
    Handle local_id = Tropic_getLocalHandleFromMaterialID(id);
    if (!self || !scene || local_id == 0) return NULL;
    return (TropicMaterial*)idmgr_get(scene->materials_manager, local_id);
}

bool Tropic_freeMaterial(TropicID engine_id, MaterialID id)
{
    Tropic *self = Tropic_getById(engine_id);
    Scene *scene = Tropic_getSceneByID(engine_id, Tropic_getSceneIDFromMaterialID(id));
    Handle local_id = Tropic_getLocalHandleFromMaterialID(id);
    TropicMaterial *material;
    bool ok;

    if (!self || !scene || local_id == 0) return false;

    material = (TropicMaterial*)idmgr_get(scene->materials_manager, local_id);
    if (!material) return false;

    ok = idmgr_free(scene->materials_manager, local_id);
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
