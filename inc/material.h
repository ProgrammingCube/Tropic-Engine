#ifndef MATERIAL_H
#define MATERIAL_H

#include <stdbool.h>
#include <stdint.h>
#include "handles.h"

typedef struct sScene Scene;
typedef struct sShader Shader;
typedef struct sTropicCamera TropicCamera;
typedef struct sObject Object;
typedef struct sTropicMaterial TropicMaterial;

typedef void (*TropicMaterialRenderCallback)(TropicID engine_id,
                                             Scene *scene,
                                             Object *object,
                                             TropicMaterial *material,
                                             ShaderID shader_id,
                                             const TropicCamera *camera);

struct sTropicMaterial
{
    MaterialID id;
    MeshID mesh_id;
    ShaderID shader_id;
    TropicMaterialRenderCallback render_callback;
    void *user;
};

MaterialID Tropic_newMaterial(TropicID engine_id, const TropicMaterial* proto);
MaterialID Tropic_createMaterial(TropicID engine_id,
                                 MeshID mesh_id,
                                 ShaderID shader_id,
                                 TropicMaterialRenderCallback render_callback,
                                 void *user);
TropicMaterial* Tropic_getMaterial(TropicID engine_id, MaterialID id);
bool Tropic_freeMaterial(TropicID engine_id, MaterialID id);

bool Tropic_setObjectMaterial(TropicID engine_id, ObjectID object_id, MaterialID material_id);
MaterialID Tropic_getObjectMaterial(TropicID engine_id, ObjectID object_id);

#endif /* MATERIAL_H */
