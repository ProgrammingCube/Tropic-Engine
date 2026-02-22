#include "level_loader.h"
#include "tropic.h"
#include <string.h>
#include <stdlib.h>

/* Map textual type names from level files to engine ObjectType enum. */
static ObjectType string_to_type(const char* s)
{
    if (!s) return TYPE_GENERIC;
    if (strcmp(s, "platform") == 0) return TYPE_PLATFORM;
    if (strcmp(s, "spike") == 0) return TYPE_SPIKE;
    if (strcmp(s, "jumppad") == 0) return TYPE_JUMPPAD;
    if (strcmp(s, "cube") == 0) return TYPE_CUBE;
    if (strcmp(s, "square") == 0) return TYPE_SQUARE;
    if (strcmp(s, "mesh") == 0) return TYPE_MESH;
    if (strcmp(s, "sphere") == 0) return TYPE_SPHERE;
    if (strcmp(s, "particle") == 0) return TYPE_PARTICLE;
    return TYPE_GENERIC;
}

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

LevelSpec* parseLevel(const char* path, int* out_num_objects)
{
    char *raw = raw_json_to_str( path );
    if ( !raw ) return NULL;

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

        spec->game_title = title && title->valuestring ? strdup(title->valuestring) : strdup("Untitled Game");
        spec->level_name = name && name->valuestring ? strdup(name->valuestring) : strdup("Untitled Level");
        spec->play_speed = speed ? speed->valuedouble : 0.0;
    } else {
        spec->game_title = strdup("Untitled Game");
        spec->level_name = strdup("Untitled Level");
        spec->play_speed = 0.0;
    }

    /* parse platforms */
    cJSON *platforms = cJSON_GetObjectItemCaseSensitive(json, "platforms");
    if (platforms && cJSON_IsArray(platforms)) {
        spec->platform_count = cJSON_GetArraySize(platforms);
        spec->platforms = (ObjectSpec*)malloc(spec->platform_count * sizeof(ObjectSpec));
        if (!spec->platforms) {
            level_free(spec);
            cJSON_Delete(json);
            return NULL;
        }
        for (size_t i = 0; i < spec->platform_count; i++) {
            cJSON *obj = cJSON_GetArrayItem(platforms, i);
            if (!obj) continue;

            cJSON *pos = cJSON_GetObjectItemCaseSensitive(obj, "pos");
            if (!pos) pos = cJSON_GetObjectItemCaseSensitive(obj, "position");
            cJSON *scale = cJSON_GetObjectItemCaseSensitive(obj, "scale");
            cJSON *rot = cJSON_GetObjectItemCaseSensitive(obj, "rot");
            if (!rot) rot = cJSON_GetObjectItemCaseSensitive(obj, "rotation");

            strncpy(spec->platforms[i].type, "platform", sizeof(spec->platforms[i].type));
            spec->platforms[i].type[sizeof(spec->platforms[i].type) - 1] = '\0';

            cJSON *tmp = NULL;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "x") : NULL;
            spec->platforms[i].position[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "y") : NULL;
            spec->platforms[i].position[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "z") : NULL;
            spec->platforms[i].position[2] = tmp ? tmp->valuedouble : 0.0;

            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "x") : NULL;
            spec->platforms[i].scale[0] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "y") : NULL;
            spec->platforms[i].scale[1] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "z") : NULL;
            spec->platforms[i].scale[2] = tmp ? tmp->valuedouble : 1.0;

            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "x") : NULL;
            spec->platforms[i].rotation[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "y") : NULL;
            spec->platforms[i].rotation[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "z") : NULL;
            spec->platforms[i].rotation[2] = tmp ? tmp->valuedouble : 0.0;
        }
    } else {
        spec->platform_count = 0;
        spec->platforms = NULL;
    }

    /* parse spikes */
    cJSON *spikes = cJSON_GetObjectItemCaseSensitive(json, "spikes");
    if (spikes && cJSON_IsArray(spikes)) {
        spec->spikes_count = cJSON_GetArraySize(spikes);
        spec->spikes = (ObjectSpec*)malloc(spec->spikes_count * sizeof(ObjectSpec));
        if (!spec->spikes) {
            level_free(spec);
            cJSON_Delete(json);
            return NULL;
        }
        for (size_t i = 0; i < spec->spikes_count; i++) {
            cJSON *obj = cJSON_GetArrayItem(spikes, i);
            if (!obj) continue;
            cJSON *pos = cJSON_GetObjectItemCaseSensitive(obj, "pos");
            if (!pos) pos = cJSON_GetObjectItemCaseSensitive(obj, "position");
            cJSON *scale = cJSON_GetObjectItemCaseSensitive(obj, "scale");
            cJSON *rot = cJSON_GetObjectItemCaseSensitive(obj, "rot");
            if (!rot) rot = cJSON_GetObjectItemCaseSensitive(obj, "rotation");

            strncpy(spec->spikes[i].type, "spike", sizeof(spec->spikes[i].type));
            spec->spikes[i].type[sizeof(spec->spikes[i].type) - 1] = '\0';

            cJSON *tmp = NULL;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "x") : NULL;
            spec->spikes[i].position[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "y") : NULL;
            spec->spikes[i].position[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "z") : NULL;
            spec->spikes[i].position[2] = tmp ? tmp->valuedouble : 0.0;

            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "x") : NULL;
            spec->spikes[i].scale[0] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "y") : NULL;
            spec->spikes[i].scale[1] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "z") : NULL;
            spec->spikes[i].scale[2] = tmp ? tmp->valuedouble : 1.0;

            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "x") : NULL;
            spec->spikes[i].rotation[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "y") : NULL;
            spec->spikes[i].rotation[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "z") : NULL;
            spec->spikes[i].rotation[2] = tmp ? tmp->valuedouble : 0.0;
        }
    } else {
        spec->spikes_count = 0;
        spec->spikes = NULL;
    }

    /* parse jumppads */
    cJSON *jumppads = cJSON_GetObjectItemCaseSensitive(json, "jumppads");
    if (jumppads && cJSON_IsArray(jumppads)) {
        spec->jumppads_count = cJSON_GetArraySize(jumppads);
        spec->jumppads = (ObjectSpec*)malloc(spec->jumppads_count * sizeof(ObjectSpec));
        if (!spec->jumppads) {
            level_free(spec);
            cJSON_Delete(json);
            return NULL;
        }
        for (size_t i = 0; i < spec->jumppads_count; i++) {
            cJSON *obj = cJSON_GetArrayItem(jumppads, i);
            if (!obj) continue;
            cJSON *pos = cJSON_GetObjectItemCaseSensitive(obj, "pos");
            if (!pos) pos = cJSON_GetObjectItemCaseSensitive(obj, "position");
            cJSON *scale = cJSON_GetObjectItemCaseSensitive(obj, "scale");
            cJSON *rot = cJSON_GetObjectItemCaseSensitive(obj, "rot");
            if (!rot) rot = cJSON_GetObjectItemCaseSensitive(obj, "rotation");

            strncpy(spec->jumppads[i].type, "jumppad", sizeof(spec->jumppads[i].type));
            spec->jumppads[i].type[sizeof(spec->jumppads[i].type) - 1] = '\0';

            cJSON *tmp = NULL;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "x") : NULL;
            spec->jumppads[i].position[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "y") : NULL;
            spec->jumppads[i].position[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = pos ? cJSON_GetObjectItemCaseSensitive(pos, "z") : NULL;
            spec->jumppads[i].position[2] = tmp ? tmp->valuedouble : 0.0;

            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "x") : NULL;
            spec->jumppads[i].scale[0] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "y") : NULL;
            spec->jumppads[i].scale[1] = tmp ? tmp->valuedouble : 1.0;
            tmp = scale ? cJSON_GetObjectItemCaseSensitive(scale, "z") : NULL;
            spec->jumppads[i].scale[2] = tmp ? tmp->valuedouble : 1.0;

            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "x") : NULL;
            spec->jumppads[i].rotation[0] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "y") : NULL;
            spec->jumppads[i].rotation[1] = tmp ? tmp->valuedouble : 0.0;
            tmp = rot ? cJSON_GetObjectItemCaseSensitive(rot, "z") : NULL;
            spec->jumppads[i].rotation[2] = tmp ? tmp->valuedouble : 0.0;
        }
    } else {
        spec->jumppads_count = 0;
        spec->jumppads = NULL;
    }

    return spec;
}

ObjectSpec* levelspec_to_objects(LevelSpec* spec, TropicID engine, int* out_num_objects)
{
    if (!spec) {
        if (out_num_objects) *out_num_objects = 0;
        return NULL;
    }

    /* apply metadata to engine game state if available */
    if (engine) {
        TropicGameState* state = Tropic_getGameState(engine);
        if (state) {
            if (state->game_title) free(state->game_title);
            if (state->level_name) free(state->level_name);
            state->game_title = spec->game_title ? strdup(spec->game_title) : strdup("Untitled Game");
            state->level_name = spec->level_name ? strdup(spec->level_name) : strdup("Untitled Level");
            state->play_speed = (float)spec->play_speed;
        }
    }

    size_t total = spec->platform_count + spec->spikes_count + spec->jumppads_count;
    if (total == 0) {
        if (out_num_objects) *out_num_objects = 0;
        return NULL;
    }

    ObjectSpec* arr = (ObjectSpec*)malloc(total * sizeof(ObjectSpec));
    if (!arr) {
        if (out_num_objects) *out_num_objects = 0;
        return NULL;
    }

    size_t idx = 0;
    for (size_t i = 0; i < spec->platform_count; i++) {
        strncpy(arr[idx].type, spec->platforms[i].type, sizeof(arr[idx].type));
        arr[idx].type[sizeof(arr[idx].type) - 1] = '\0';
        arr[idx].type_code = string_to_type(spec->platforms[i].type);
        memcpy(arr[idx].position, spec->platforms[i].position, sizeof(vec3));
        memcpy(arr[idx].scale, spec->platforms[i].scale, sizeof(vec3));
        memcpy(arr[idx].rotation, spec->platforms[i].rotation, sizeof(vec3));
        idx++;
    }

    for (size_t i = 0; i < spec->spikes_count; i++) {
        strncpy(arr[idx].type, spec->spikes[i].type, sizeof(arr[idx].type));
        arr[idx].type[sizeof(arr[idx].type) - 1] = '\0';
        arr[idx].type_code = string_to_type(spec->spikes[i].type);
        memcpy(arr[idx].position, spec->spikes[i].position, sizeof(vec3));
        memcpy(arr[idx].scale, spec->spikes[i].scale, sizeof(vec3));
        memcpy(arr[idx].rotation, spec->spikes[i].rotation, sizeof(vec3));
        idx++;
    }

    for (size_t i = 0; i < spec->jumppads_count; i++) {
        strncpy(arr[idx].type, spec->jumppads[i].type, sizeof(arr[idx].type));
        arr[idx].type[sizeof(arr[idx].type) - 1] = '\0';
        arr[idx].type_code = string_to_type(spec->jumppads[i].type);
        memcpy(arr[idx].position, spec->jumppads[i].position, sizeof(vec3));
        memcpy(arr[idx].scale, spec->jumppads[i].scale, sizeof(vec3));
        memcpy(arr[idx].rotation, spec->jumppads[i].rotation, sizeof(vec3));
        idx++;
    }

    if (out_num_objects) *out_num_objects = (int)total;
    return arr;
}

void level_free(LevelSpec *spec)
{
    if (!spec) return;
    if (spec->game_title) free(spec->game_title);
    if (spec->level_name) free(spec->level_name);
    if (spec->platforms) free(spec->platforms);
    if (spec->spikes) free(spec->spikes);
    if (spec->jumppads) free(spec->jumppads);
    free(spec);
}