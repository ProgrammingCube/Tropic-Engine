#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "tropic_datatypes.h"
#include "mesh.h"

typedef enum eObjectType
{
    TYPE_GENERIC,
    TYPE_CUBE,
    TYPE_PLATFORM,
    TYPE_SPIKE,
    /* TYPE_PLAYER, */
    TYPE_JUMPPAD,
    TYPE_SQUARE,
    TYPE_MESH,
    TYPE_SPHERE,
    TYPE_PARTICLE,
} ObjectType;

typedef struct sObjectSpec
{
    char type[16]; // e.g. "platform", "spike", "jumppad"
    /* Engine-friendly enum for object type. Filled by level conversion code so
     * the engine does not need to parse string names. */
    ObjectType type_code;
    vec3 position;
    vec3 scale;
    vec3 rotation;
} ObjectSpec;

typedef struct sObject
{
    ObjectID id;
    MeshID mesh_id;
    ShaderID shader_id;
    ObjectType type;
    bool active;
    Position pos;
    Scale scale;
    Rotation rot;
    Mesh mesh;
} Object;

ObjectID Tropic_newObject(TropicID engine_id, const Object* proto);
Object*  Tropic_getObject( TropicID engine_id, ObjectID id);
bool     Tropic_freeObject( TropicID engine_id, ObjectID id);

ObjectID Tropic_findFirstObjectOfType( TropicID engine_id, ObjectType type );

bool Tropic_translateObject( TropicID engine_id, ObjectID id, vec3 translation );
bool Tropic_rotateObject( TropicID engine_id, ObjectID id, vec3 rotation );
bool Tropic_scaleObject( TropicID engine_id, ObjectID id, vec3 scale );

#endif /* OBJECT_H */