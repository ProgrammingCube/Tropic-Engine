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
    if (strcmp(s, "event") == 0) return TYPE_EVENT;
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

static char* sanitize_json_source(const char* raw)
{
    size_t length;
    char* without_comments;
    char* sanitized;
    size_t write_index = 0;
    bool in_string = false;
    bool escape = false;
    bool in_line_comment = false;
    bool in_block_comment = false;

    if (!raw) return NULL;

    length = strlen(raw);
    without_comments = (char*)malloc(length + 1);
    if (!without_comments) return NULL;

    for (size_t i = 0; i < length; ++i) {
        char ch = raw[i];
        char next = (i + 1 < length) ? raw[i + 1] : '\0';

        if (in_line_comment) {
            if (ch == '\n') {
                in_line_comment = false;
                without_comments[write_index++] = ch;
            }
            continue;
        }

        if (in_block_comment) {
            if (ch == '*' && next == '/') {
                in_block_comment = false;
                ++i;
            }
            continue;
        }

        if (!in_string && ch == '/' && next == '/') {
            in_line_comment = true;
            ++i;
            continue;
        }

        if (!in_string && ch == '/' && next == '*') {
            in_block_comment = true;
            ++i;
            continue;
        }

        without_comments[write_index++] = ch;

        if (in_string) {
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == '"') {
                in_string = false;
            }
        } else if (ch == '"') {
            in_string = true;
        }
    }
    without_comments[write_index] = '\0';

    sanitized = (char*)malloc(write_index + 1);
    if (!sanitized) {
        free(without_comments);
        return NULL;
    }

    in_string = false;
    escape = false;
    write_index = 0;
    for (size_t i = 0; without_comments[i] != '\0'; ++i) {
        char ch = without_comments[i];

        if (!in_string && ch == ',') {
            size_t j = i + 1;
            while (without_comments[j] == ' ' || without_comments[j] == '\t' ||
                   without_comments[j] == '\r' || without_comments[j] == '\n') {
                ++j;
            }
            if (without_comments[j] == ']' || without_comments[j] == '}') {
                continue;
            }
        }

        sanitized[write_index++] = ch;

        if (in_string) {
            if (escape) {
                escape = false;
            } else if (ch == '\\') {
                escape = true;
            } else if (ch == '"') {
                in_string = false;
            }
        } else if (ch == '"') {
            in_string = true;
        }
    }
    sanitized[write_index] = '\0';

    free(without_comments);
    return sanitized;
}

static TropicEventActionType string_to_event_action(const char* s)
{
    if (!s || s[0] == '\0') return TROPIC_EVENT_ACTION_GRAVITY_FLIP;
    if (strcmp(s, "gravity") == 0 || strcmp(s, "gravity_set") == 0) return TROPIC_EVENT_ACTION_GRAVITY_SET;
    if (strcmp(s, "gravity_flip") == 0) return TROPIC_EVENT_ACTION_GRAVITY_FLIP;
    if (strcmp(s, "world_spin") == 0) return TROPIC_EVENT_ACTION_WORLD_SPIN;
    if (strcmp(s, "camera_spin") == 0) return TROPIC_EVENT_ACTION_CAMERA_SPIN;
    if (strcmp(s, "custom") == 0) return TROPIC_EVENT_ACTION_CUSTOM;
    return TROPIC_EVENT_ACTION_NONE;
}

static TropicEventTriggerMode string_to_event_trigger(const char* s)
{
    if (!s || s[0] == '\0') return TROPIC_EVENT_TRIGGER_ENTER;
    if (strcmp(s, "stay") == 0) return TROPIC_EVENT_TRIGGER_STAY;
    if (strcmp(s, "exit") == 0) return TROPIC_EVENT_TRIGGER_EXIT;
    return TROPIC_EVENT_TRIGGER_ENTER;
}

static double json_number_or_default(cJSON *object, const char *key, double default_value)
{
    cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;
    return cJSON_IsNumber(item) ? item->valuedouble : default_value;
}

static bool copy_required_json_string(cJSON *object, const char *key, char *destination, size_t destination_size)
{
    cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;

    if (!destination || destination_size == 0) return false;
    destination[0] = '\0';
    if (!cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') return false;

    strncpy(destination, item->valuestring, destination_size);
    destination[destination_size - 1] = '\0';
    return true;
}

static bool copy_optional_json_string(cJSON *object, const char *key, char *destination, size_t destination_size)
{
    cJSON *item = object ? cJSON_GetObjectItemCaseSensitive(object, key) : NULL;

    if (!destination || destination_size == 0) return false;
    destination[0] = '\0';
    if (!cJSON_IsString(item) || !item->valuestring || item->valuestring[0] == '\0') return false;

    strncpy(destination, item->valuestring, destination_size);
    destination[destination_size - 1] = '\0';
    return true;
}

static void parse_vec3_or_default(cJSON *object, vec3 out_vec3, float x, float y, float z)
{
    out_vec3[0] = (float)json_number_or_default(object, "x", x);
    out_vec3[1] = (float)json_number_or_default(object, "y", y);
    out_vec3[2] = (float)json_number_or_default(object, "z", z);
}

static bool register_uid(const char *uid, char ***uid_registry, size_t *uid_count)
{
    char **expanded_registry;

    if (!uid || uid[0] == '\0' || !uid_registry || !uid_count) return false;

    for (size_t i = 0; i < *uid_count; ++i) {
        if (strcmp((*uid_registry)[i], uid) == 0) {
            return false;
        }
    }

    expanded_registry = (char**)realloc(*uid_registry, (*uid_count + 1) * sizeof(char*));
    if (!expanded_registry) return false;

    *uid_registry = expanded_registry;
    (*uid_registry)[*uid_count] = strdup(uid);
    if (!(*uid_registry)[*uid_count]) return false;

    (*uid_count)++;
    return true;
}

static void free_uid_registry(char **uid_registry, size_t uid_count)
{
    if (!uid_registry) return;

    for (size_t i = 0; i < uid_count; ++i) {
        free(uid_registry[i]);
    }

    free(uid_registry);
}

static bool parse_common_object(cJSON *json_object,
                                ObjectSpec *out_spec,
                                const char *type_name,
                                bool platform_style,
                                char ***uid_registry,
                                size_t *uid_count)
{
    cJSON *pos;
    cJSON *scale;
    cJSON *rot;

    if (!json_object || !out_spec || !type_name) return false;

    memset(out_spec, 0, sizeof(*out_spec));
    strncpy(out_spec->type, type_name, sizeof(out_spec->type));
    out_spec->type[sizeof(out_spec->type) - 1] = '\0';
    out_spec->event.trigger_mode = TROPIC_EVENT_TRIGGER_ENTER;
    out_spec->event.trigger_once = true;
    out_spec->event.axis[2] = 1.0f;

    if (!copy_required_json_string(json_object, "uid", out_spec->uid, sizeof(out_spec->uid))) {
        fprintf(stderr, "Level object of type '%s' is missing a required uid.\n", type_name);
        return false;
    }

    if (!register_uid(out_spec->uid, uid_registry, uid_count)) {
        fprintf(stderr, "Level object uid '%s' is duplicated or invalid.\n", out_spec->uid);
        return false;
    }

    pos = cJSON_GetObjectItemCaseSensitive(json_object, "pos");
    if (!pos) pos = cJSON_GetObjectItemCaseSensitive(json_object, "position");
    scale = cJSON_GetObjectItemCaseSensitive(json_object, "scale");
    rot = cJSON_GetObjectItemCaseSensitive(json_object, "rot");
    if (!rot) rot = cJSON_GetObjectItemCaseSensitive(json_object, "rotation");

    out_spec->position[0] = (float)json_number_or_default(pos, "x", 0.0);
    out_spec->position[1] = (float)json_number_or_default(pos, "y", 0.0);
    out_spec->position[2] = (float)json_number_or_default(pos, "z", 0.0);

    if (platform_style) {
        double scale_x = json_number_or_default(scale, "x", 1.0);
        double scale_y = json_number_or_default(scale, "y", 1.0);
        double scale_z = json_number_or_default(scale, "z", 1.0);
        double width = json_number_or_default(pos, "width", 2.0);
        double height = json_number_or_default(pos, "height", 2.0);
        double length = json_number_or_default(pos, "length", 2.0);

        out_spec->scale[0] = (float)(0.5 * width * scale_x);
        out_spec->scale[1] = (float)(0.5 * height * scale_y);
        out_spec->scale[2] = (float)(0.5 * length * scale_z);
    } else {
        out_spec->scale[0] = (float)json_number_or_default(scale, "x", 1.0);
        out_spec->scale[1] = (float)json_number_or_default(scale, "y", 1.0);
        out_spec->scale[2] = (float)json_number_or_default(scale, "z", 1.0);
    }

    out_spec->rotation[0] = (float)json_number_or_default(rot, "x", 0.0);
    out_spec->rotation[1] = (float)json_number_or_default(rot, "y", 0.0);
    out_spec->rotation[2] = (float)json_number_or_default(rot, "z", 0.0);

    return true;
}

static bool parse_event_object(cJSON *json_object,
                               ObjectSpec *out_spec,
                               char ***uid_registry,
                               size_t *uid_count)
{
    cJSON *gravity;
    cJSON *axis;
    cJSON *event_type;
    cJSON *trigger;
    cJSON *once;
    char target_uid[TROPIC_OBJECT_UID_MAX] = { 0 };

    if (!parse_common_object(json_object, out_spec, "event", false, uid_registry, uid_count)) {
        return false;
    }

    event_type = cJSON_GetObjectItemCaseSensitive(json_object, "event_type");
    trigger = cJSON_GetObjectItemCaseSensitive(json_object, "trigger");
    once = cJSON_GetObjectItemCaseSensitive(json_object, "once");
    gravity = cJSON_GetObjectItemCaseSensitive(json_object, "gravity");
    axis = cJSON_GetObjectItemCaseSensitive(json_object, "axis");

    out_spec->event.action_type = string_to_event_action(cJSON_IsString(event_type) ? event_type->valuestring : NULL);
    out_spec->event.trigger_mode = string_to_event_trigger(cJSON_IsString(trigger) ? trigger->valuestring : NULL);
    if (cJSON_IsBool(once)) {
        out_spec->event.trigger_once = cJSON_IsTrue(once);
    } else if (cJSON_IsNumber(once)) {
        out_spec->event.trigger_once = once->valuedouble != 0.0;
    }

    parse_vec3_or_default(gravity, out_spec->event.gravity, 0.0f, 0.0f, 0.0f);
    parse_vec3_or_default(axis, out_spec->event.axis, 0.0f, 0.0f, 1.0f);
    out_spec->event.degrees = (float)json_number_or_default(json_object, "degrees", 0.0);
    out_spec->event.speed = (float)json_number_or_default(json_object, "speed", 0.0);
    out_spec->event.duration_seconds = (float)json_number_or_default(json_object, "duration", 0.0);
    if (out_spec->event.duration_seconds <= 0.0f) {
        out_spec->event.duration_seconds = (float)json_number_or_default(json_object, "duration_seconds", 0.0);
    }

    if (!copy_optional_json_string(json_object, "target_uid", target_uid, sizeof(target_uid))) {
        (void)copy_optional_json_string(json_object, "pivot_uid", target_uid, sizeof(target_uid));
    }
    if (strcmp(target_uid, "self") == 0) {
        strncpy(out_spec->event.target_uid, out_spec->uid, sizeof(out_spec->event.target_uid));
        out_spec->event.target_uid[sizeof(out_spec->event.target_uid) - 1] = '\0';
    } else if (target_uid[0] != '\0') {
        strncpy(out_spec->event.target_uid, target_uid, sizeof(out_spec->event.target_uid));
        out_spec->event.target_uid[sizeof(out_spec->event.target_uid) - 1] = '\0';
    }

    if (!copy_optional_json_string(json_object, "function", out_spec->event.custom_function, sizeof(out_spec->event.custom_function))) {
        (void)copy_optional_json_string(json_object, "custom_function", out_spec->event.custom_function, sizeof(out_spec->event.custom_function));
    }

    if (out_spec->event.action_type == TROPIC_EVENT_ACTION_WORLD_SPIN && out_spec->event.target_uid[0] == '\0') {
        strncpy(out_spec->event.target_uid, out_spec->uid, sizeof(out_spec->event.target_uid));
        out_spec->event.target_uid[sizeof(out_spec->event.target_uid) - 1] = '\0';
    }

    return true;
}

static bool parse_object_group(cJSON *json,
                               const char *group_name,
                               const char *type_name,
                               bool platform_style,
                               bool event_group,
                               ObjectSpec **out_objects,
                               size_t *out_count,
                               char ***uid_registry,
                               size_t *uid_count)
{
    cJSON *group = cJSON_GetObjectItemCaseSensitive(json, group_name);

    if (!out_objects || !out_count) return false;

    *out_objects = NULL;
    *out_count = 0;

    if (!group || !cJSON_IsArray(group)) {
        return true;
    }

    *out_count = cJSON_GetArraySize(group);
    if (*out_count == 0) {
        return true;
    }

    *out_objects = (ObjectSpec*)calloc(*out_count, sizeof(ObjectSpec));
    if (!*out_objects) return false;

    for (size_t i = 0; i < *out_count; ++i) {
        cJSON *json_object = cJSON_GetArrayItem(group, (int)i);
        bool ok = event_group
            ? parse_event_object(json_object, &(*out_objects)[i], uid_registry, uid_count)
            : parse_common_object(json_object, &(*out_objects)[i], type_name, platform_style, uid_registry, uid_count);

        if (!ok) {
            return false;
        }
    }

    return true;
}

LevelSpec* parseLevel(const char* path, int* out_num_objects)
{
    char *raw = raw_json_to_str(path);
    char *sanitized = NULL;
    cJSON *json;
    LevelSpec *spec;
    char **uid_registry = NULL;
    size_t uid_count = 0;

    if (!raw) return NULL;

    sanitized = sanitize_json_source(raw);
    free(raw);
    if (!sanitized) return NULL;

    json = cJSON_Parse(sanitized);
    free(sanitized);
    if (!json) return NULL;

    spec = (LevelSpec*)calloc(1, sizeof(LevelSpec));
    if (!spec) {
        cJSON_Delete(json);
        return NULL;
    }

    {
        cJSON *metadata = cJSON_GetObjectItemCaseSensitive(json, "metadata");
        cJSON *title = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "game_title") : NULL;
        cJSON *name = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "level_name") : NULL;
        cJSON *music = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "music_path") : NULL;
        cJSON *speed = metadata ? cJSON_GetObjectItemCaseSensitive(metadata, "play_speed") : NULL;

        spec->game_title = cJSON_IsString(title) && title->valuestring ? strdup(title->valuestring) : strdup("Untitled Game");
        spec->level_name = cJSON_IsString(name) && name->valuestring ? strdup(name->valuestring) : strdup("Untitled Level");
        spec->music_path = cJSON_IsString(music) && music->valuestring ? strdup(music->valuestring) : strdup("");
        spec->play_speed = cJSON_IsNumber(speed) ? speed->valuedouble : 0.0;
    }

    if (!spec->game_title || !spec->level_name || !spec->music_path) goto parse_failed;
    if (!parse_object_group(json, "platforms", "platform", true, false, &spec->platforms, &spec->platform_count, &uid_registry, &uid_count)) goto parse_failed;
    if (!parse_object_group(json, "spikes", "spike", false, false, &spec->spikes, &spec->spikes_count, &uid_registry, &uid_count)) goto parse_failed;
    if (!parse_object_group(json, "jumppads", "jumppad", false, false, &spec->jumppads, &spec->jumppads_count, &uid_registry, &uid_count)) goto parse_failed;
    if (!parse_object_group(json, "events", "event", false, true, &spec->events, &spec->events_count, &uid_registry, &uid_count)) goto parse_failed;

    free_uid_registry(uid_registry, uid_count);
    cJSON_Delete(json);

    if (out_num_objects) {
        *out_num_objects = (int)(spec->platform_count + spec->spikes_count +
                                 spec->jumppads_count + spec->events_count);
    }

    return spec;

parse_failed:
    free_uid_registry(uid_registry, uid_count);
    cJSON_Delete(json);
    level_free(spec);
    return NULL;
}

ObjectSpec* levelspec_to_objects(LevelSpec* spec, TropicID engine, int* out_num_objects)
{
    if (!spec) {
        if (out_num_objects) *out_num_objects = 0;
        return NULL;
    }

    if (engine) {
        TropicGameState* state = Tropic_getGameState(engine);
        if (state) {
            if (state->game_title) free(state->game_title);
            if (state->level_name) free(state->level_name);
            if (state->music_path) free(state->music_path);
            state->game_title = spec->game_title ? strdup(spec->game_title) : strdup("Untitled Game");
            state->level_name = spec->level_name ? strdup(spec->level_name) : strdup("Untitled Level");
            state->music_path = spec->music_path ? strdup(spec->music_path) : strdup("");
            state->play_speed = (float)spec->play_speed;
        }
    }

    {
        size_t total = spec->platform_count + spec->spikes_count + spec->jumppads_count + spec->events_count;
        ObjectSpec* arr;
        size_t idx = 0;

        if (total == 0) {
            if (out_num_objects) *out_num_objects = 0;
            return NULL;
        }

        arr = (ObjectSpec*)malloc(total * sizeof(ObjectSpec));
        if (!arr) {
            if (out_num_objects) *out_num_objects = 0;
            return NULL;
        }

        for (size_t i = 0; i < spec->platform_count; ++i) {
            arr[idx] = spec->platforms[i];
            arr[idx].type_code = string_to_type(arr[idx].type);
            arr[idx].event.has_fired = false;
            idx++;
        }
        for (size_t i = 0; i < spec->spikes_count; ++i) {
            arr[idx] = spec->spikes[i];
            arr[idx].type_code = string_to_type(arr[idx].type);
            arr[idx].event.has_fired = false;
            idx++;
        }
        for (size_t i = 0; i < spec->jumppads_count; ++i) {
            arr[idx] = spec->jumppads[i];
            arr[idx].type_code = string_to_type(arr[idx].type);
            arr[idx].event.has_fired = false;
            idx++;
        }
        for (size_t i = 0; i < spec->events_count; ++i) {
            arr[idx] = spec->events[i];
            arr[idx].type_code = string_to_type(arr[idx].type);
            arr[idx].event.has_fired = false;
            idx++;
        }

        if (out_num_objects) *out_num_objects = (int)total;
        return arr;
    }
}

void level_free(LevelSpec *spec)
{
    if (!spec) return;
    if (spec->game_title) free(spec->game_title);
    if (spec->level_name) free(spec->level_name);
    if (spec->music_path) free(spec->music_path);
    if (spec->platforms) free(spec->platforms);
    if (spec->spikes) free(spec->spikes);
    if (spec->jumppads) free(spec->jumppads);
    if (spec->events) free(spec->events);
    free(spec);
}