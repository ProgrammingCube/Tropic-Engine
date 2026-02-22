#ifndef LEVEL_LOADER_H
#define LEVEL_LOADER_H
#include "object.h"
#include <stdio.h>
#include <cjson/cJSON.h>

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

ObjectSpec* parseLevel(const char* path, int* out_num_objects);

#endif /* LEVEL_LOADER_H */