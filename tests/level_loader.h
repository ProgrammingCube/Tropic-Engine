#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H

#include <stdio.h>
#include <cjson/cJSON.h>
#include "tropic.h"
#include "object.h"

typedef struct sLevelSpec
{
    char *game_title;
    char *level_name;
    double play_speed;
    size_t platform_count;
    ObjectSpec *platforms;
    size_t spikes_count;
    ObjectSpec *spikes;
    size_t jumppads_count;
    ObjectSpec *jumppads;
} LevelSpec;

LevelSpec* parseLevel(const char* path, int* out_num_objects);

/* Convert a parsed LevelSpec into a flat array of engine-agnostic ObjectSpec.
 * This will also apply the level metadata into the engine game state when
 * a valid `engine` id is provided. The returned array is malloc'd and must
 * be freed by the caller (Tropic_loadObjects currently frees it).
 */
ObjectSpec* levelspec_to_objects(LevelSpec* spec, TropicID engine, int* out_num_objects);

void level_free(LevelSpec *spec);

#endif /* LEVEL_LOADER_H */