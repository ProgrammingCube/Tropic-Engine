#ifndef LEVEL_PARSER_H
#define LEVEL_PARSER_H

#include <stddef.h>
#include <cglm/vec3.h>
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

/* Parse a level file. Returns a newly-allocated LevelSpec on success, or NULL on failure. */
LevelSpec* level_parse_file(const char *path);

/* Free a LevelSpec returned by level_parse_file. */
void level_free(LevelSpec *spec);

#endif /* LEVEL_PARSER_H */
