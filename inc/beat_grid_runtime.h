#ifndef TROPIC_BEAT_GRID_RUNTIME_H
#define TROPIC_BEAT_GRID_RUNTIME_H

#include <stdbool.h>
#include <stddef.h>
#include "beat_grid.h"

void Tropic_setDefaultBeatGridSettings(TropicBeatGridSettings *settings);
void Tropic_setDefaultTrackFrame(TropicTrackFrame *frame);

float Tropic_getBeatTimeAsFloat(const TropicBeatTime *time,
								int subdivisions_per_beat);

bool Tropic_buildTrackFrameForPlacement(const TropicBeatGridSettings *settings,
								const TropicTrackFrame *base_frame,
								const TropicTrackAnchor *anchors,
								size_t anchor_count,
								const TropicTrackPlacement *placement,
								TropicTrackFrame *out_frame);

bool Tropic_resolveTrackPlacementPosition(const TropicBeatGridSettings *settings,
										  const TropicTrackFrame *frame,
										  const TropicTrackPlacement *placement,
										  vec3 out_position);

#endif /* TROPIC_BEAT_GRID_RUNTIME_H */