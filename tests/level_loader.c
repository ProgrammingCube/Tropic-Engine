#include "level_loader.h"
#include "tropic.h"
#include "beat_grid_runtime.h"
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

static void parse_track_placement(cJSON *json_object, TropicTrackPlacement *out_placement)
{
    cJSON *placement;
    cJSON *space;

    if (!out_placement) return;

    memset(out_placement, 0, sizeof(*out_placement));
    out_placement->space = TROPIC_PLACEMENT_SPACE_WORLD;

    if (!json_object) return;

    placement = cJSON_GetObjectItemCaseSensitive(json_object, "placement");
    if (!placement || !cJSON_IsObject(placement)) {
        return;
    }

    space = cJSON_GetObjectItemCaseSensitive(placement, "space");
    if (cJSON_IsString(space) && space->valuestring && strcmp(space->valuestring, "world") == 0) {
        out_placement->space = TROPIC_PLACEMENT_SPACE_WORLD;
    } else {
        out_placement->space = TROPIC_PLACEMENT_SPACE_TRACK;
    }

    out_placement->time.beat = (int32_t)json_number_or_default(placement, "beat", 0.0);
    out_placement->time.substep = (int32_t)json_number_or_default(placement, "substep", 0.0);
    out_placement->track_x = (float)json_number_or_default(placement,
                                                           "track_x",
                                                           json_number_or_default(placement, "x", 0.0));
    out_placement->track_y = (float)json_number_or_default(placement,
                                                           "track_y",
                                                           json_number_or_default(placement, "y", 0.0));
    out_placement->length_beats = (float)json_number_or_default(placement, "length_beats", 0.0);

    {
        cJSON *snap_x = cJSON_GetObjectItemCaseSensitive(placement, "snap_x");
        cJSON *snap_y = cJSON_GetObjectItemCaseSensitive(placement, "snap_y");

        out_placement->snap_x = cJSON_IsBool(snap_x) ? cJSON_IsTrue(snap_x) : true;
        out_placement->snap_y = cJSON_IsBool(snap_y) ? cJSON_IsTrue(snap_y) : true;
    }
}

static void parse_beat_grid_metadata(cJSON *metadata, TropicBeatGridSettings *out_settings)
{
    cJSON *origin;
    cJSON *right;
    cJSON *up;
    cJSON *forward;

    if (!out_settings) return;

    Tropic_setDefaultBeatGridSettings(out_settings);
    if (!metadata) return;

    out_settings->bpm = (float)json_number_or_default(metadata, "bpm", out_settings->bpm);
    out_settings->music_offset_seconds = (float)json_number_or_default(metadata,
                                                                       "music_offset_seconds",
                                                                       out_settings->music_offset_seconds);
    out_settings->subdivisions_per_beat = (int32_t)json_number_or_default(metadata,
                                                                          "subdivisions_per_beat",
                                                                          out_settings->subdivisions_per_beat);
    out_settings->units_per_beat = (float)json_number_or_default(metadata,
                                                                 "units_per_beat",
                                                                 out_settings->units_per_beat);
    out_settings->snap_unit_x = (float)json_number_or_default(metadata,
                                                              "snap_unit_x",
                                                              out_settings->snap_unit_x);
    out_settings->snap_unit_y = (float)json_number_or_default(metadata,
                                                              "snap_unit_y",
                                                              out_settings->snap_unit_y);

    origin = cJSON_GetObjectItemCaseSensitive(metadata, "track_origin");
    if (!origin) origin = cJSON_GetObjectItemCaseSensitive(metadata, "origin");
    right = cJSON_GetObjectItemCaseSensitive(metadata, "track_right");
    if (!right) right = cJSON_GetObjectItemCaseSensitive(metadata, "right");
    up = cJSON_GetObjectItemCaseSensitive(metadata, "track_up");
    if (!up) up = cJSON_GetObjectItemCaseSensitive(metadata, "up");
    forward = cJSON_GetObjectItemCaseSensitive(metadata, "track_forward");
    if (!forward) forward = cJSON_GetObjectItemCaseSensitive(metadata, "forward");

    parse_vec3_or_default(origin, out_settings->origin,
                          out_settings->origin[0], out_settings->origin[1], out_settings->origin[2]);
    parse_vec3_or_default(right, out_settings->initial_right,
                          out_settings->initial_right[0], out_settings->initial_right[1], out_settings->initial_right[2]);
    parse_vec3_or_default(up, out_settings->initial_up,
                          out_settings->initial_up[0], out_settings->initial_up[1], out_settings->initial_up[2]);
    parse_vec3_or_default(forward, out_settings->initial_forward,
                          out_settings->initial_forward[0], out_settings->initial_forward[1], out_settings->initial_forward[2]);

    if (out_settings->subdivisions_per_beat <= 0) out_settings->subdivisions_per_beat = 1;
    if (out_settings->units_per_beat == 0.0f) out_settings->units_per_beat = 1.0f;
}

static bool parse_track_anchors(cJSON *json,
                                TropicTrackAnchor **out_anchors,
                                size_t *out_count)
{
    cJSON *anchors = cJSON_GetObjectItemCaseSensitive(json, "track_anchors");

    if (!out_anchors || !out_count) return false;

    *out_anchors = NULL;
    *out_count = 0;

    if (!anchors || !cJSON_IsArray(anchors)) {
        return true;
    }

    *out_count = (size_t)cJSON_GetArraySize(anchors);
    if (*out_count == 0) {
        return true;
    }

    *out_anchors = (TropicTrackAnchor*)calloc(*out_count, sizeof(TropicTrackAnchor));
    if (!*out_anchors) return false;

    for (size_t i = 0; i < *out_count; ++i) {
        cJSON *anchor = cJSON_GetArrayItem(anchors, (int)i);
        cJSON *axis = anchor ? cJSON_GetObjectItemCaseSensitive(anchor, "local_axis") : NULL;
        cJSON *pivot = anchor ? cJSON_GetObjectItemCaseSensitive(anchor, "pivot") : NULL;

        if (!cJSON_IsObject(anchor)) return false;

        (*out_anchors)[i].start_time.beat = (int32_t)json_number_or_default(anchor, "beat", 0.0);
        (*out_anchors)[i].start_time.substep = (int32_t)json_number_or_default(anchor, "substep", 0.0);
        (*out_anchors)[i].pivot_x = (float)json_number_or_default(anchor,
                                                                  "pivot_x",
                                                                  json_number_or_default(pivot, "x", 0.0));
        (*out_anchors)[i].pivot_y = (float)json_number_or_default(anchor,
                                                                  "pivot_y",
                                                                  json_number_or_default(pivot, "y", 0.0));
        (*out_anchors)[i].pivot_beat = (float)json_number_or_default(anchor,
                                                                     "pivot_beat",
                                                                     json_number_or_default(pivot, "beat", 0.0));
        parse_vec3_or_default(axis, (*out_anchors)[i].local_axis, 0.0f, 0.0f, 1.0f);
        (*out_anchors)[i].degrees = (float)json_number_or_default(anchor, "degrees", 0.0);
    }

    return true;
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
    out_spec->placement.space = TROPIC_PLACEMENT_SPACE_WORLD;
    out_spec->placement.snap_x = true;
    out_spec->placement.snap_y = true;
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
    parse_track_placement(json_object, &out_spec->placement);

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

        if (!music && metadata) {
            music = cJSON_GetObjectItemCaseSensitive(metadata, "music");
        }

        spec->game_title = cJSON_IsString(title) && title->valuestring ? strdup(title->valuestring) : strdup("Untitled Game");
        spec->level_name = cJSON_IsString(name) && name->valuestring ? strdup(name->valuestring) : strdup("Untitled Level");
        spec->music_path = cJSON_IsString(music) && music->valuestring ? strdup(music->valuestring) : strdup("");
        spec->play_speed = cJSON_IsNumber(speed) ? speed->valuedouble : 0.0;
        parse_beat_grid_metadata(metadata, &spec->beat_grid);
    }

    if (!spec->game_title || !spec->level_name || !spec->music_path) goto parse_failed;
    if (!parse_track_anchors(json, &spec->track_anchors, &spec->track_anchor_count)) goto parse_failed;
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
        Scene *scene = Tropic_getCurrentScene(engine);
        if (state) {
            if (state->game_title) free(state->game_title);
            if (state->level_name) free(state->level_name);
            if (state->music_path) free(state->music_path);
            state->game_title = spec->game_title ? strdup(spec->game_title) : strdup("Untitled Game");
            state->level_name = spec->level_name ? strdup(spec->level_name) : strdup("Untitled Level");
            state->music_path = spec->music_path ? strdup(spec->music_path) : strdup("");
            state->play_speed = (float)spec->play_speed;
        }
        if (scene) {
            memcpy(&scene->beat_grid, &spec->beat_grid, sizeof(scene->beat_grid));
            glm_vec3_copy(scene->beat_grid.origin, scene->base_track_frame.origin);
            glm_vec3_copy(scene->beat_grid.initial_right, scene->base_track_frame.right);
            glm_vec3_copy(scene->beat_grid.initial_up, scene->base_track_frame.up);
            glm_vec3_copy(scene->beat_grid.initial_forward, scene->base_track_frame.forward);
            glm_vec3_copy(scene->base_track_frame.origin, scene->current_track_frame.origin);
            glm_vec3_copy(scene->base_track_frame.right, scene->current_track_frame.right);
            glm_vec3_copy(scene->base_track_frame.up, scene->current_track_frame.up);
            glm_vec3_copy(scene->base_track_frame.forward, scene->current_track_frame.forward);
            if (scene->track_anchors) {
                vector_free(scene->track_anchors);
                scene->track_anchors = NULL;
            }
            for (size_t i = 0; i < spec->track_anchor_count; ++i) {
                vector_push_back(scene->track_anchors, spec->track_anchors[i]);
            }
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
    if (spec->track_anchors) free(spec->track_anchors);
    if (spec->platforms) free(spec->platforms);
    if (spec->spikes) free(spec->spikes);
    if (spec->jumppads) free(spec->jumppads);
    if (spec->events) free(spec->events);
    free(spec);
}