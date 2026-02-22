#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "tropic_datatypes.h"

typedef struct sObjectSpec
{
    char type[16]; // e.g. "platform", "spike", "jumppad"
    vec3 position;
    vec3 scale;
    vec3 rotation;
} ObjectSpec;

typedef enum eObjectType
{
    TYPE_GENERIC,
    TYPE_SPIKE,
    TYPE_PLAYER,
    TYPE_PLATFORM,
    TYPE_JUMPPAD,
    TYPE_SQUARE
} ObjectType;

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