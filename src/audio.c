#include "audio.h"

void Tropic_AudioSystemInit(TropicID engine_id)
{
	(void)engine_id;
}

void Tropic_AudioSystemShutdown(TropicID engine_id)
{
	(void)engine_id;
}

void Tropic_GetAudioDuration(TropicID engine_id, AudioID audio_id, double* out_duration)
{
	(void)engine_id;
	(void)audio_id;
	if (out_duration) {
		*out_duration = 0.0;
	}
}

void Tropic_SetVolume(TropicID engine_id, float volume)
{
	(void)engine_id;
	(void)volume;
}