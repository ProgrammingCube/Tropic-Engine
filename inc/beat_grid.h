#ifndef TROPIC_BEAT_GRID_H
#define TROPIC_BEAT_GRID_H

#include <stdbool.h>
#include <stdint.h>
#include <cglm/cglm.h>

typedef enum eTropicPlacementSpace
{
    TROPIC_PLACEMENT_SPACE_WORLD = 0,
    TROPIC_PLACEMENT_SPACE_TRACK = 1,
} TropicPlacementSpace;

typedef struct sTropicBeatTime
{
    int32_t beat;
    int32_t substep;
} TropicBeatTime;

/* Level-wide timing + spatial conversion settings.
 * This is not a lane system.
 * `track_x` and `track_y` are free-form coordinates in the track plane.
 */
typedef struct sTropicBeatGridSettings
{
    float bpm;
    float music_offset_seconds;
    int32_t subdivisions_per_beat;

    /* How much world distance one beat advances along track forward. */
    float units_per_beat;

    /* Optional editor snap spacing for free-form construction. */
    float snap_unit_x;
    float snap_unit_y;

    /* Initial construction frame for the level. */
    vec3 origin;
    vec3 initial_right;
    vec3 initial_up;
    vec3 initial_forward;
} TropicBeatGridSettings;

/* Authored placement for an object in beat-space.
 * `track_x` and `track_y` are free-form floats, not fixed lanes.
 */
typedef struct sTropicTrackPlacement
{
    TropicPlacementSpace space;

    TropicBeatTime time;

    float track_x;
    float track_y;

    bool snap_x;
    bool snap_y;

    /* Optional depth/extent in beat units for editor/runtime helpers. */
    float length_beats;
} TropicTrackPlacement;

/* Current scene-local construction/runtime frame. */
typedef struct sTropicTrackFrame
{
    vec3 origin;
    vec3 right;
    vec3 up;
    vec3 forward;
} TropicTrackFrame;

/* Optional authoring anchor:
 * from this beat onward, placement is interpreted in a rotated frame.
 * This is the clean answer for “rotate 90 degrees right at beat N”.
 */
typedef struct sTropicTrackAnchor
{
    TropicBeatTime start_time;

    /* Pivot expressed in track-space, not world-space. */
    float pivot_x;
    float pivot_y;
    float pivot_beat;

    /* Rotation expressed relative to the current frame. */
    vec3 local_axis;
    float degrees;
} TropicTrackAnchor;

#endif /* TROPIC_BEAT_GRID_H */