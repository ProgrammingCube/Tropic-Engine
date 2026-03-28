#ifndef AUDIO_H
#define	AUDIO_H

#include "tropic.h"
#include "miniaudio.h"

void Tropic_AudioSystemInit(TropicID engine_id);
void Tropic_AudioSystemShutdown(TropicID engine_id);

void Tropic_GetAudioDuration(TropicID engine_id, AudioID audio_id, double* out_duration);

void Tropic_SetVolume(TropicID engine_id, float volume);

#endif // !AUDIO_H
