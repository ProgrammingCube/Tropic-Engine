#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include <vector.h>
#include "tropic_datatypes.h"
#include "material.h"
#include "mesh.h"
#include "beat_grid.h"

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
    TYPE_EVENT,
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

typedef enum eTropicCollisionPhase
{
    TROPIC_COLLISION_ENTER,
    TROPIC_COLLISION_STAY,
    TROPIC_COLLISION_EXIT,
} TropicCollisionPhase;

#define TROPIC_OBJECT_TYPE_NAME_MAX 16
#define TROPIC_OBJECT_UID_MAX 64
#define TROPIC_EVENT_NAME_MAX 64

typedef enum eTropicEventActionType
{
    TROPIC_EVENT_ACTION_NONE,
    TROPIC_EVENT_ACTION_GRAVITY_SET,
    TROPIC_EVENT_ACTION_GRAVITY_FLIP,
    TROPIC_EVENT_ACTION_WORLD_SPIN,
    TROPIC_EVENT_ACTION_CAMERA_SPIN,
    TROPIC_EVENT_ACTION_CUSTOM,
} TropicEventActionType;

typedef enum eTropicEventTriggerMode
{
    TROPIC_EVENT_TRIGGER_ENTER,
    TROPIC_EVENT_TRIGGER_STAY,
    TROPIC_EVENT_TRIGGER_EXIT,
} TropicEventTriggerMode;

typedef struct sTropicEventSpec
{
    TropicEventActionType action_type;
    TropicEventTriggerMode trigger_mode;
    bool trigger_once;
    bool has_fired;
    vec3 gravity;
    vec3 axis;
    float degrees;
    float speed;
    float duration_seconds;
    char target_uid[TROPIC_OBJECT_UID_MAX];
    char custom_function[TROPIC_EVENT_NAME_MAX];
} TropicEventSpec;

typedef struct sTropicCollisionEvent
{
    ObjectID self_id;
    ObjectID other_id;
    TropicCollisionPhase phase;
    float impact_speed;
} TropicCollisionEvent;

typedef void (*TropicCollisionCallback)(TropicID engine_id,
                                        const TropicCollisionEvent *event,
                                        void *user_data);

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
    char type[TROPIC_OBJECT_TYPE_NAME_MAX]; // e.g. "platform", "spike", "jumppad", "event"
    /* Engine-friendly enum for object type. Filled by level conversion code so
     * the engine does not need to parse string names. */
    ObjectType type_code;
    char uid[TROPIC_OBJECT_UID_MAX];
    vec3 position;
    vec3 scale;
    vec3 rotation;
    TropicTrackPlacement placement;
    TropicEventSpec event;
} ObjectSpec;

struct sObject
{
    Scene* _scene_ptr;
    ObjectID id;
    MaterialID material_id;
    ObjectType type;
    bool active;
    char uid[TROPIC_OBJECT_UID_MAX];
    Position pos;
    Scale scale;
    Rotation rot;
    TropicTrackPlacement placement;
    TropicEventSpec event;
    TropicCollider collider;
    TropicPhysicsBody body;
    TropicCollisionCallback collision_callback;
    void *collision_user_data;
    vector( ObjectID ) current_collision_ids;
    vector( ObjectID ) previous_collision_ids;
    Mesh mesh;
};

ObjectID Tropic_newObject(TropicID engine_id, const Object* proto);
Object*  Tropic_getObject( TropicID engine_id, ObjectID id);
bool     Tropic_freeObject( TropicID engine_id, ObjectID id);

SceneID  Tropic_getObjectScene( ObjectID id );
TropicID Tropic_getObjectEngine( ObjectID id );

ObjectID Tropic_findFirstObjectOfType( TropicID engine_id, ObjectType type );
ObjectID Tropic_findObjectByUid( TropicID engine_id, const char *uid );

bool Tropic_setObjectPosition(TropicID engine_id, ObjectID id, vec3 position);
bool Tropic_setObjectRotation(TropicID engine_id, ObjectID id, vec3 rotation);
bool Tropic_setObjectScale(TropicID engine_id, ObjectID id, vec3 scale);
bool Tropic_setObjectCollisionCallback(TropicID engine_id,
                                       ObjectID id,
                                       TropicCollisionCallback callback,
                                       void *user_data);

bool Tropic_getObjectPosition(TropicID engine_id, ObjectID id, vec3 position);
bool Tropic_getObjectRotation(TropicID engine_id, ObjectID id, vec3 rotation);
bool Tropic_getObjectScale(TropicID engine_id, ObjectID id, vec3 scale);


bool Tropic_translateObject( TropicID engine_id, ObjectID id, vec3 translation );
bool Tropic_rotateObject( TropicID engine_id, ObjectID id, vec3 rotation );
bool Tropic_scaleObject( TropicID engine_id, ObjectID id, vec3 scale );
bool Tropic_shouldTriggerObjectEvent( TropicID engine_id,
                                      ObjectID event_object_id,
                                      const TropicCollisionEvent *collision_event );
bool Tropic_executeObjectBuiltinEvent( TropicID engine_id, ObjectID event_object_id );

#endif /* OBJECT_H */