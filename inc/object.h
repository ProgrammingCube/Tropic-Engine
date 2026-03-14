#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "tropic_datatypes.h"
#include "material.h"
#include "mesh.h"

typedef struct sScene Scene;
typedef struct sObject Object;

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

typedef enum eTropicColliderType
{
    TROPIC_COLLIDER_NONE,
    TROPIC_COLLIDER_AABB,
} TropicColliderType;

typedef enum eTropicColliderFlags
{
    TROPIC_COLLIDER_FLAG_NONE = 0,
    TROPIC_COLLIDER_FLAG_SOLID = 1u << 0,
    TROPIC_COLLIDER_FLAG_TRIGGER = 1u << 1,
    TROPIC_COLLIDER_FLAG_HAZARD = 1u << 2,
} TropicColliderFlags;

typedef struct sTropicCollider
{
    bool enabled;
    TropicColliderType type;
    uint32_t flags;
    vec3 offset;
    vec3 half_extents;
} TropicCollider;

typedef struct sTropicPhysicsBody
{
    bool enabled;
    bool is_static;
    bool is_grounded;
    float ground_friction;
    float air_friction;
    ObjectID support_object_id;
    ObjectID last_contact_object_id;
    vec3 velocity;
} TropicPhysicsBody;

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

struct sObject
{
    Scene* _scene_ptr;
    ObjectID id;
    MaterialID material_id;
    ObjectType type;
    bool active;
    Position pos;
    Scale scale;
    Rotation rot;
    TropicCollider collider;
    TropicPhysicsBody body;
    Mesh mesh;
};

ObjectID Tropic_newObject(TropicID engine_id, const Object* proto);
Object*  Tropic_getObject( TropicID engine_id, ObjectID id);
bool     Tropic_freeObject( TropicID engine_id, ObjectID id);

SceneID  Tropic_getObjectScene( ObjectID id );
TropicID Tropic_getObjectEngine( ObjectID id );

ObjectID Tropic_findFirstObjectOfType( TropicID engine_id, ObjectType type );

bool Tropic_setObjectPosition(TropicID engine_id, ObjectID id, vec3 position);
bool Tropic_setObjectRotation(TropicID engine_id, ObjectID id, vec3 rotation);
bool Tropic_setObjectScale(TropicID engine_id, ObjectID id, vec3 scale);

bool Tropic_getObjectPosition(TropicID engine_id, ObjectID id, vec3 position);
bool Tropic_getObjectRotation(TropicID engine_id, ObjectID id, vec3 rotation);
bool Tropic_getObjectScale(TropicID engine_id, ObjectID id, vec3 scale);


bool Tropic_translateObject( TropicID engine_id, ObjectID id, vec3 translation );
bool Tropic_rotateObject( TropicID engine_id, ObjectID id, vec3 rotation );
bool Tropic_scaleObject( TropicID engine_id, ObjectID id, vec3 scale );

#endif /* OBJECT_H */