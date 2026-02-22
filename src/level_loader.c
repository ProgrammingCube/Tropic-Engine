#include "level_loader.h"

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

ObjectSpec* parseLevel(const char* path, int* out_num_objects)
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