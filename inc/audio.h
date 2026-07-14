#ifndef AUDIO_H
#define	AUDIO_H

#include <stdbool.h>
#include "handles.h"

bool Tropic_AudioSystemInit(TropicID engine_id);
bool Tropic_AudioSystemShutdown(TropicID engine_id);

bool Tropic_LoadMusic(TropicID engine_id, const char* path);
void Tropic_UnloadMusic(TropicID engine_id);

bool Tropic_PlayMusic(TropicID engine_id);
bool Tropic_PauseMusic(TropicID engine_id);
bool Tropic_StopMusic(TropicID engine_id);

bool Tropic_IsMusicPlaying(TropicID engine_id);
bool Tropic_SeekMusicSeconds(TropicID engine_id, double seconds);
double Tropic_GetMusicTimeSeconds(TropicID engine_id);
double Tropic_GetMusicLengthSeconds(TropicID engine_id);
float Tropic_GetMusicBeat(TropicID engine_id, float bpm, float music_offset_seconds);

void Tropic_SetVolume(TropicID engine_id, float volume);

#endif // !AUDIO_H