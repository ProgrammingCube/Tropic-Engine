#ifndef LEVEL_PARSER_H
#define LEVEL_PARSER_H

#include <stddef.h>

typedef struct sLevelSpec {
    char *game_title;
    char *level_name;
    double play_speed;
    /* Future: entity arrays, metadata, etc. */
} LevelSpec;

/* Parse a level file. Returns a newly-allocated LevelSpec on success, or NULL on failure. */
LevelSpec* level_parse_file(const char *path);

/* Free a LevelSpec returned by level_parse_file. */
void level_free(LevelSpec *spec);

#endif /* LEVEL_PARSER_H */
