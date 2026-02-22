#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "tropic_datatypes.h"

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
    ObjectType type;
    bool active;
    Position pos;
    Scale scale;
    Rotation rot;
} Object;

#endif /* OBJECT_H */