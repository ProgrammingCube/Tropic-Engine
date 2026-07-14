#include "audio.h"
#include "tropic.h"

bool Tropic_AudioSystemInit(TropicID engine_id)
{
	Tropic* engine = Tropic_getById( engine_id );
	if (!engine) return false;
	if (engine->audio_initialized) return true;
	
	ma_result result;
	result = ma_engine_init(NULL, &engine->audio_engine);
	if (result != MA_SUCCESS) return false;
	
	engine->audio_initialized = true;
	engine->music_loaded = false;
	engine->master_volume = 1.0f;
	ma_engine_set_volume(&engine->audio_engine, engine->master_volume);
	return true;
}

bool Tropic_AudioSystemShutdown(TropicID engine_id)
{
	Tropic* engine = Tropic_getById(engine_id);

	if (!engine || !engine->audio_initialized) return false;

	if (engine->music_loaded)
	{
		ma_sound_uninit(&engine->music_sound);
		engine->music_loaded = false;
	}

	ma_engine_uninit(&engine->audio_engine);
	engine->audio_initialized = false;
	return true;
}

bool Tropic_LoadMusic(TropicID engine_id, const char* path)
{
    Tropic* engine = Tropic_getById(engine_id);
    ma_result result;

    if (!engine || !path)
    {
        return false;
    }

    if (!engine->audio_initialized && !Tropic_AudioSystemInit(engine_id))
    {
        return false;
    }

    if (engine->music_loaded)
    {
        ma_sound_uninit(&engine->music_sound);
        engine->music_loaded = false;
    }

    result = ma_sound_init_from_file(&engine->audio_engine,
        path,
        MA_SOUND_FLAG_STREAM,
        NULL,
        NULL,
        &engine->music_sound);
    if (result != MA_SUCCESS)
    {
        return false;
    }

    ma_sound_set_looping(&engine->music_sound, MA_FALSE);
    ma_sound_set_volume(&engine->music_sound, 1.0f);
    engine->music_loaded = true;
    return true;
}

void Tropic_UnloadMusic(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->music_loaded)
    {
        return;
    }

    ma_sound_uninit(&engine->music_sound);
    engine->music_loaded = false;
}

bool Tropic_PlayMusic(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->music_loaded)
    {
        return false;
    }

    return ma_sound_start(&engine->music_sound) == MA_SUCCESS;
}

bool Tropic_PauseMusic(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->music_loaded)
    {
        return false;
    }

    return ma_sound_stop(&engine->music_sound) == MA_SUCCESS;
}

bool Tropic_StopMusic(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->music_loaded)
    {
        return false;
    }

    if (ma_sound_stop(&engine->music_sound) != MA_SUCCESS)
    {
        return false;
    }

    return ma_sound_seek_to_pcm_frame(&engine->music_sound, 0) == MA_SUCCESS;
}

bool Tropic_IsMusicPlaying(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->music_loaded)
    {
        return false;
    }

    return ma_sound_is_playing(&engine->music_sound) == MA_TRUE;
}

float Tropic_GetMusicBeat(TropicID engine_id, float bpm, float music_offset_seconds)
{
    double song_time_seconds;
    double beat;

    if (bpm <= 0.0f)
    {
        return 0.0f;
    }

    song_time_seconds = Tropic_GetMusicTimeSeconds(engine_id);
    beat = ((song_time_seconds - (double)music_offset_seconds) * (double)bpm) / 60.0;
    if (beat < 0.0)
    {
        beat = 0.0;
    }

    return (float)beat;
}

bool Tropic_SeekMusicSeconds(TropicID engine_id, double seconds)
{
    Tropic* engine = Tropic_getById(engine_id);
    ma_uint64 frame_index;
    ma_uint32 sample_rate;

    if (!engine || !engine->music_loaded)
    {
        return false;
    }

    if (seconds < 0.0)
    {
        seconds = 0.0;
    }

    sample_rate = ma_engine_get_sample_rate(&engine->audio_engine);
    if (sample_rate == 0)
    {
        return false;
    }

    frame_index = (ma_uint64)(seconds * (double)sample_rate);
    return ma_sound_seek_to_pcm_frame(&engine->music_sound, frame_index) == MA_SUCCESS;
}

double Tropic_GetMusicTimeSeconds(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);
    ma_uint64 cursor_frames;
    ma_uint32 sample_rate;

    if (!engine || !engine->music_loaded)
    {
        return 0.0;
    }

    sample_rate = ma_engine_get_sample_rate(&engine->audio_engine);
    if (sample_rate == 0)
    {
        return 0.0;
    }

    if (ma_sound_get_cursor_in_pcm_frames(&engine->music_sound, &cursor_frames) != MA_SUCCESS)
    {
        return 0.0;
    }

    return (double)cursor_frames / (double)sample_rate;
}

double Tropic_GetMusicLengthSeconds(TropicID engine_id)
{
    Tropic* engine = Tropic_getById(engine_id);
    ma_uint64 length_frames;
    ma_uint32 sample_rate;

    if (!engine || !engine->music_loaded)
    {
        return 0.0;
    }

    sample_rate = ma_engine_get_sample_rate(&engine->audio_engine);
    if (sample_rate == 0)
    {
        return 0.0;
    }

    if (ma_sound_get_length_in_pcm_frames(&engine->music_sound, &length_frames) != MA_SUCCESS)
    {
        return 0.0;
    }

    return (double)length_frames / (double)sample_rate;
}

void Tropic_SetVolume(TropicID engine_id, float volume)
{
    Tropic* engine = Tropic_getById(engine_id);

    if (!engine || !engine->audio_initialized)
    {
        return;
    }

    if (volume < 0.0f)
    {
        volume = 0.0f;
    }

    engine->master_volume = volume;
    ma_engine_set_volume(&engine->audio_engine, volume);
}