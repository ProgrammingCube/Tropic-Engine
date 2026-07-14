#include "beat_grid_runtime.h"

#include <cglm/cglm.h>
#include <math.h>
#include <string.h>

static void _BeatGrid_transformDirection(const TropicTrackFrame *frame,
									 const vec3 local_direction,
									 vec3 out_direction)
{
	vec3 right_component;
	vec3 up_component;
	vec3 forward_component;

	if (!frame || !local_direction || !out_direction) return;

	glm_vec3_scale(frame->right, local_direction[0], right_component);
	glm_vec3_scale(frame->up, local_direction[1], up_component);
	glm_vec3_scale(frame->forward, local_direction[2], forward_component);
	glm_vec3_copy(right_component, out_direction);
	glm_vec3_add(out_direction, up_component, out_direction);
	glm_vec3_add(out_direction, forward_component, out_direction);
}

static bool _BeatGrid_resolveAnchorAxis(const TropicTrackFrame *frame,
									const vec3 local_axis,
									vec3 out_axis)
{
	if (!frame || !local_axis || !out_axis) return false;

	_BeatGrid_transformDirection(frame, local_axis, out_axis);
	if (glm_vec3_norm2(out_axis) <= 0.000001f) return false;
	glm_vec3_normalize(out_axis);
	return true;
}

static void _BeatGrid_rotatePointAroundPivot(const vec3 point,
									 const vec3 pivot,
									 const vec3 axis,
									 float degrees,
									 vec3 out_point)
{
	mat4 rotation;
	vec3 relative;
	vec4 relative4;
	vec4 rotated4;

	if (!point || !pivot || !axis || !out_point) return;

	glm_rotate_make(rotation, glm_rad(degrees), axis);
	glm_vec3_sub(point, pivot, relative);
	relative4[0] = relative[0];
	relative4[1] = relative[1];
	relative4[2] = relative[2];
	relative4[3] = 1.0f;
	glm_mat4_mulv(rotation, relative4, rotated4);
	out_point[0] = pivot[0] + rotated4[0];
	out_point[1] = pivot[1] + rotated4[1];
	out_point[2] = pivot[2] + rotated4[2];
}

static void _BeatGrid_rotateDirection(const vec3 direction,
								  const vec3 axis,
								  float degrees,
								  vec3 out_direction)
{
	mat4 rotation;
	vec4 direction4;
	vec4 rotated4;

	if (!direction || !axis || !out_direction) return;

	glm_rotate_make(rotation, glm_rad(degrees), axis);
	direction4[0] = direction[0];
	direction4[1] = direction[1];
	direction4[2] = direction[2];
	direction4[3] = 0.0f;
	glm_mat4_mulv(rotation, direction4, rotated4);
	out_direction[0] = rotated4[0];
	out_direction[1] = rotated4[1];
	out_direction[2] = rotated4[2];
}

static bool _BeatGrid_shouldApplyAnchor(const TropicBeatGridSettings *settings,
									 const TropicTrackAnchor *anchor,
									 const TropicTrackPlacement *placement)
{
	float anchor_beat;
	float placement_beat;

	if (!settings || !anchor || !placement) return false;

	anchor_beat = Tropic_getBeatTimeAsFloat(&anchor->start_time, settings->subdivisions_per_beat);
	placement_beat = Tropic_getBeatTimeAsFloat(&placement->time, settings->subdivisions_per_beat);
	return placement_beat >= anchor_beat;
}

static bool _BeatGrid_applyAnchor(const TropicBeatGridSettings *settings,
								  const TropicTrackAnchor *anchor,
								  TropicTrackFrame *frame)
{
	vec3 pivot_world;
	vec3 world_axis;
	vec3 rotated_origin;
	vec3 rotated_right;
	vec3 rotated_up;
	vec3 rotated_forward;
	float pivot_forward_units;

	if (!settings || !anchor || !frame) return false;

	pivot_forward_units = anchor->pivot_beat * settings->units_per_beat;
	glm_vec3_copy(frame->origin, pivot_world);
	{
		vec3 right_component;
		vec3 up_component;
		vec3 forward_component;
		glm_vec3_scale(frame->right, anchor->pivot_x, right_component);
		glm_vec3_scale(frame->up, anchor->pivot_y, up_component);
		glm_vec3_scale(frame->forward, pivot_forward_units, forward_component);
		glm_vec3_add(pivot_world, right_component, pivot_world);
		glm_vec3_add(pivot_world, up_component, pivot_world);
		glm_vec3_add(pivot_world, forward_component, pivot_world);
	}

	if (!_BeatGrid_resolveAnchorAxis(frame, anchor->local_axis, world_axis)) return false;

	_BeatGrid_rotatePointAroundPivot(frame->origin, pivot_world, world_axis, anchor->degrees, rotated_origin);
	_BeatGrid_rotateDirection(frame->right, world_axis, anchor->degrees, rotated_right);
	_BeatGrid_rotateDirection(frame->up, world_axis, anchor->degrees, rotated_up);
	_BeatGrid_rotateDirection(frame->forward, world_axis, anchor->degrees, rotated_forward);

	glm_vec3_copy(rotated_origin, frame->origin);
	glm_vec3_copy(rotated_right, frame->right);
	glm_vec3_copy(rotated_up, frame->up);
	glm_vec3_copy(rotated_forward, frame->forward);
	return true;
}

void Tropic_setDefaultBeatGridSettings(TropicBeatGridSettings *settings)
{
	if (!settings) return;

	memset(settings, 0, sizeof(*settings));
	settings->bpm = 120.0f;
	settings->music_offset_seconds = 0.0f;
	settings->subdivisions_per_beat = 4;
	settings->units_per_beat = 1.0f;
	settings->snap_unit_x = 1.0f;
	settings->snap_unit_y = 1.0f;
	glm_vec3_zero(settings->origin);
	glm_vec3_copy((vec3){ 1.0f, 0.0f, 0.0f }, settings->initial_right);
	glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, settings->initial_up);
	glm_vec3_copy((vec3){ 0.0f, 0.0f, -1.0f }, settings->initial_forward);
}

void Tropic_setDefaultTrackFrame(TropicTrackFrame *frame)
{
	if (!frame) return;

	memset(frame, 0, sizeof(*frame));
	glm_vec3_zero(frame->origin);
	glm_vec3_copy((vec3){ 1.0f, 0.0f, 0.0f }, frame->right);
	glm_vec3_copy((vec3){ 0.0f, 1.0f, 0.0f }, frame->up);
	glm_vec3_copy((vec3){ 0.0f, 0.0f, -1.0f }, frame->forward);
}

float Tropic_getBeatTimeAsFloat(const TropicBeatTime *time,
								int subdivisions_per_beat)
{
	float beat_value;

	if (!time) return 0.0f;

	beat_value = (float)time->beat;
	if (subdivisions_per_beat > 0) {
		beat_value += (float)time->substep / (float)subdivisions_per_beat;
	}

	return beat_value;
}

bool Tropic_buildTrackFrameForPlacement(const TropicBeatGridSettings *settings,
								const TropicTrackFrame *base_frame,
								const TropicTrackAnchor *anchors,
								size_t anchor_count,
								const TropicTrackPlacement *placement,
								TropicTrackFrame *out_frame)
{
	if (!settings || !base_frame || !placement || !out_frame) return false;

	memcpy(out_frame, base_frame, sizeof(*out_frame));
	if (!anchors || anchor_count == 0 || placement->space != TROPIC_PLACEMENT_SPACE_TRACK) {
		return true;
	}

	for (size_t i = 0; i < anchor_count; ++i) {
		if (!_BeatGrid_shouldApplyAnchor(settings, &anchors[i], placement)) continue;
		if (!_BeatGrid_applyAnchor(settings, &anchors[i], out_frame)) return false;
	}

	return true;
}

bool Tropic_resolveTrackPlacementPosition(const TropicBeatGridSettings *settings,
										  const TropicTrackFrame *frame,
										  const TropicTrackPlacement *placement,
										  vec3 out_position)
{
	vec3 right_component;
	vec3 up_component;
	vec3 forward_component;
	float beat_units;

	if (!settings || !frame || !placement || !out_position) return false;
	if (placement->space != TROPIC_PLACEMENT_SPACE_TRACK) return false;

	beat_units = Tropic_getBeatTimeAsFloat(&placement->time,
										   settings->subdivisions_per_beat) *
				 settings->units_per_beat;

	glm_vec3_scale(frame->right, placement->track_x, right_component);
	glm_vec3_scale(frame->up, placement->track_y, up_component);
	glm_vec3_scale(frame->forward, beat_units, forward_component);

	glm_vec3_copy(frame->origin, out_position);
	glm_vec3_add(out_position, right_component, out_position);
	glm_vec3_add(out_position, up_component, out_position);
	glm_vec3_add(out_position, forward_component, out_position);
	return true;
}
