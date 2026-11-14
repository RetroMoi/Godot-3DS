/*************************************************************************/
/*  audio_server_3ds.h                                                   */
/*************************************************************************/
#ifndef AUDIO_SERVER_3DS_H
#define AUDIO_SERVER_3DS_H

#ifdef _3DS

#include "servers/audio/audio_server_sw.h"

extern "C" {
#include <3ds/types.h>
#include <3ds/ndsp/ndsp.h>
#include <3ds/ndsp/channel.h>
}

// Sound effects mixed by the 3DS's DSP instead of the CPU.
//
// AudioServerSW mixes every voice in software, on the CPU core the game runs on. The DSP has
// 24 hardware channels that resample, apply volume/pan and loop by themselves. Here each voice
// (SamplePlayer, SamplePlayer2D...) plays on its own DSP channel: no CPU mixing for effects.
// Streams (music) still go through AudioServerSW's software mix, which the audio driver plays
// on DSP channel 1.
//
// Samples are copied once into linear memory (the DSP can only read that), on first play.
// PCM8/PCM16 play in hardware; IMA-ADPCM samples (compressed .smp imports) fall back to the
// software mixer.
class AudioServer3DS : public AudioServerSW {

	enum {
		DRIVER_CHANNEL = 1, // AudioDriver3ds: the software mix
		CHANNEL_COUNT = 24,
	};

	struct HWSample {
		void *data; // linear memory
		int frames;
		int bytes_per_frame;
		u16 format; // NDSP_FORMAT_*
	};
	Map<RID, HWSample> hw_samples;

	struct HWVoice {
		RID sample;
		int channel; // -1: not playing on the DSP
		RID sw_voice; // software fallback (ADPCM), created on demand
		bool on_sw;
		float volume, pan, depth, height;
		int mix_rate;
		FilterType filter_type;
		float filter_cutoff, filter_resonance, filter_gain;
		float chorus, reverb;
		ReverbRoomType reverb_type;
		bool positional;
		ndspWaveBuf wave[2]; // intro + loop
		uint64_t start_tick;
	};
	mutable RID_Owner<HWVoice> hw_voice_owner;
	HWVoice *channel_voice[CHANNEL_COUNT];

	const HWSample *_get_hw_sample(RID p_sample);
	void _drop_hw_sample(RID p_sample);
	int _alloc_channel();
	void _release_channel(HWVoice *p_voice);
	void _apply_mix(HWVoice *p_voice);
	void _ensure_sw_voice(HWVoice *p_voice);
	bool _is_playing(const HWVoice *p_voice) const;

public:
	virtual void sample_set_data(RID p_sample, const DVector<uint8_t> &p_buffer);

	virtual RID voice_create();
	virtual void voice_play(RID p_voice, RID p_sample);
	virtual void voice_set_volume(RID p_voice, float p_volume);
	virtual void voice_set_pan(RID p_voice, float p_pan, float p_depth = 0, float height = 0);
	virtual void voice_set_filter(RID p_voice, FilterType p_type, float p_cutoff, float p_resonance, float p_gain = 0);
	virtual void voice_set_chorus(RID p_voice, float p_chorus);
	virtual void voice_set_reverb(RID p_voice, ReverbRoomType p_room_type, float p_reverb);
	virtual void voice_set_mix_rate(RID p_voice, int p_mix_rate);
	virtual void voice_set_positional(RID p_voice, bool p_positional);

	virtual float voice_get_volume(RID p_voice) const;
	virtual float voice_get_pan(RID p_voice) const;
	virtual float voice_get_pan_depth(RID p_voice) const;
	virtual float voice_get_pan_height(RID p_voice) const;
	virtual FilterType voice_get_filter_type(RID p_voice) const;
	virtual float voice_get_filter_cutoff(RID p_voice) const;
	virtual float voice_get_filter_resonance(RID p_voice) const;
	virtual float voice_get_chorus(RID p_voice) const;
	virtual ReverbRoomType voice_get_reverb_type(RID p_voice) const;
	virtual float voice_get_reverb(RID p_voice) const;
	virtual int voice_get_mix_rate(RID p_voice) const;
	virtual bool voice_is_positional(RID p_voice) const;

	virtual void voice_stop(RID p_voice);
	virtual bool voice_is_active(RID p_voice) const;

	virtual void set_fx_global_volume_scale(float p_volume);
	virtual void set_event_voice_global_volume_scale(float p_volume);

	virtual void free(RID p_id);

	AudioServer3DS(SampleManagerSW *p_sample_manager);
	~AudioServer3DS();
};

#endif
#endif
