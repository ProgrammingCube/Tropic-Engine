#ifndef OBJECT_H
#define OBJECT_H

#include <stdint.h>
#include <stdbool.h>
#include "tropic_datatypes.h"

typedef enum eObjectType
{
    TYPE_GENERIC,
    TYPE_PLAYER,
    TYPE_PLATFORM,
    TYPE_JUMPPAD,
    TYPE_SQUARE
} ObjectType;

typedef struct sObject
{
    uint32_t id;
    ObjectType type;
    bool active;
    Position pos;
    Scale scale;
    Rotation rot;
} Object;

#endif /* OBJECT_H */