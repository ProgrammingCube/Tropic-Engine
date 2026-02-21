#include "../inc/level_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <cjson/cJSON.h>

static char* raw_json_to_str(const char* file_name)
{
    char* tmp_buffer;
    long file_size;
    FILE* file = fopen(file_name, "r");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    file_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    tmp_buffer = (char*)malloc(file_size + 1);
    if (tmp_buffer == NULL) { fclose(file); return NULL; }
    fread(tmp_buffer, 1, file_size, file);
    tmp_buffer[file_size] = '\0';
    fclose(file);
    return tmp_buffer;
}

static char* my_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *d = (char*)malloc(n);
    if (!d) return NULL;
    memcpy(d, s, n);
    return d;
}

LevelSpec* level_parse_file(const char *path)
{
    char *raw = raw_json_to_str(path);
    if (!raw) return NULL;

    cJSON *json = cJSON_Parse(raw);
    free(raw);
    if (!json) return NULL;

    LevelSpec *spec = (LevelSpec*)malloc(sizeof(LevelSpec));
    if (!spec) { cJSON_Delete(json); return NULL; }
    spec->game_title = NULL;
    spec->level_name = NULL;
    spec->play_speed = 0.0;

    cJSON *metadata = cJSON_GetObjectItemCaseSensitive(json, "metadata");
    if (metadata) {
        cJSON *title = cJSON_GetObjectItemCaseSensitive(metadata, "game_title");
        cJSON *name = cJSON_GetObjectItemCaseSensitive(metadata, "level_name");
        cJSON *speed = cJSON_GetObjectItemCaseSensitive(metadata, "play_speed");

        spec->game_title = title && title->valuestring ? my_strdup(title->valuestring) : my_strdup("Untitled Game");
        spec->level_name = name && name->valuestring ? my_strdup(name->valuestring) : my_strdup("Untitled Level");
        spec->play_speed = speed ? speed->valuedouble : 0.0;
    } else {
        spec->game_title = my_strdup("Untitled Game");
        spec->level_name = my_strdup("Untitled Level");
        spec->play_speed = 0.0;
    }

    cJSON_Delete(json);
    return spec;
}

void level_free(LevelSpec *spec)
{
    if (!spec) return;
    if (spec->game_title) free(spec->game_title);
    if (spec->level_name) free(spec->level_name);
    free(spec);
}
