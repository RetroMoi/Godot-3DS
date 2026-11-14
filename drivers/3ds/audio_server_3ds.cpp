/*************************************************************************/
/*  audio_server_3ds.cpp                                                 */
/*************************************************************************/
#ifdef _3DS

#include "audio_server_3ds.h"

extern "C" {
#include <3ds/allocator/linear.h>
#include <3ds/services/dsp.h>
#include <3ds/svc.h>
}
#include <string.h>

/* SAMPLES */

const AudioServer3DS::HWSample *AudioServer3DS::_get_hw_sample(RID p_sample) {

	Map<RID, HWSample>::Element *E = hw_samples.find(p_sample);
	if (E)
		return &E->get();

	SampleFormat format = sample_get_format(p_sample);
	if (format != SAMPLE_FORMAT_PCM8 && format != SAMPLE_FORMAT_PCM16)
		return NULL; // IMA-ADPCM: software mixer

	bool stereo = sample_is_stereo(p_sample);
	int frames = sample_get_length(p_sample);
	const void *src = sample_get_data_ptr(p_sample);
	if (frames <= 0 || !src)
		return NULL;

	HWSample hs;
	int bytes_per_sample = format == SAMPLE_FORMAT_PCM8 ? 1 : 2;
	hs.bytes_per_frame = bytes_per_sample * (stereo ? 2 : 1);
	hs.frames = frames;
	hs.format = NDSP_CHANNELS(stereo ? 2 : 1) | NDSP_ENCODING(format == SAMPLE_FORMAT_PCM8 ? NDSP_ENCODING_PCM8 : NDSP_ENCODING_PCM16);
	int bytes = frames * hs.bytes_per_frame;
	hs.data = linearAlloc(bytes);
	if (!hs.data)
		return NULL; // out of linear memory: software mixer
	memcpy(hs.data, src, bytes);
	DSP_FlushDataCache(hs.data, bytes);
	return &hw_samples.insert(p_sample, hs)->get();
}

void AudioServer3DS::_drop_hw_sample(RID p_sample) {

	Map<RID, HWSample>::Element *E = hw_samples.find(p_sample);
	if (!E)
		return;
	// Stop the channels still reading it.
	for (int i = 0; i < CHANNEL_COUNT; i++) {
		HWVoice *v = channel_voice[i];
		if (v && v->sample == p_sample)
			_release_channel(v);
	}
	linearFree(E->get().data);
	hw_samples.erase(E);
}

void AudioServer3DS::sample_set_data(RID p_sample, const DVector<uint8_t> &p_buffer) {

	_drop_hw_sample(p_sample); // copied again on next play
	AudioServerSW::sample_set_data(p_sample, p_buffer);
}

/* CHANNELS */

static bool _waves_done(const ndspWaveBuf *p_wave) {
	// The wave buffers' status is set when they are queued (ndspChnIsPlaying only catches up a
	// DSP frame later): queued/playing until the last one is done. Unused ones stay FREE (0).
	for (int i = 0; i < 2; i++)
		if (p_wave[i].status == NDSP_WBUF_QUEUED || p_wave[i].status == NDSP_WBUF_PLAYING)
			return false;
	return true;
}

bool AudioServer3DS::_is_playing(const HWVoice *p_voice) const {

	return p_voice->channel >= 0 && channel_voice[p_voice->channel] == p_voice && !_waves_done(p_voice->wave);
}

int AudioServer3DS::_alloc_channel() {

	// A free channel (or one whose sound has ended), else the oldest sound is cut.
	int oldest = -1;
	uint64_t oldest_tick = 0;
	for (int i = 0; i < CHANNEL_COUNT; i++) {
		if (i == DRIVER_CHANNEL)
			continue;
		HWVoice *v = channel_voice[i];
		if (!v || _waves_done(v->wave)) {
			if (v)
				v->channel = -1;
			channel_voice[i] = NULL;
			return i;
		}
		if (oldest < 0 || v->start_tick < oldest_tick) {
			oldest = i;
			oldest_tick = v->start_tick;
		}
	}
	_release_channel(channel_voice[oldest]);
	return oldest;
}

void AudioServer3DS::_release_channel(HWVoice *p_voice) {

	if (p_voice->channel < 0)
		return;
	if (channel_voice[p_voice->channel] == p_voice) {
		ndspChnWaveBufClear(p_voice->channel);
		channel_voice[p_voice->channel] = NULL;
	}
	p_voice->channel = -1;
}

void AudioServer3DS::_apply_mix(HWVoice *p_voice) {

	if (p_voice->channel < 0)
		return;
	float vol = p_voice->volume * get_fx_global_volume_scale() * get_event_voice_global_volume_scale();
	float mix[12];
	memset(mix, 0, sizeof(mix));
	mix[0] = vol * MIN(1.0f, 1.0f - p_voice->pan); // front left
	mix[1] = vol * MIN(1.0f, 1.0f + p_voice->pan); // front right
	ndspChnSetMix(p_voice->channel, mix);
	ndspChnSetRate(p_voice->channel, p_voice->mix_rate);
}

void AudioServer3DS::_ensure_sw_voice(HWVoice *p_voice) {

	if (p_voice->sw_voice.is_valid())
		return;
	RID v = AudioServerSW::voice_create();
	p_voice->sw_voice = v;
	AudioServerSW::voice_set_volume(v, p_voice->volume);
	AudioServerSW::voice_set_pan(v, p_voice->pan, p_voice->depth, p_voice->height);
	AudioServerSW::voice_set_filter(v, p_voice->filter_type, p_voice->filter_cutoff, p_voice->filter_resonance, p_voice->filter_gain);
	AudioServerSW::voice_set_chorus(v, p_voice->chorus);
	AudioServerSW::voice_set_reverb(v, p_voice->reverb_type, p_voice->reverb);
	AudioServerSW::voice_set_positional(v, p_voice->positional);
}

/* VOICES */

RID AudioServer3DS::voice_create() {

	HWVoice *v = memnew(HWVoice);
	v->channel = -1;
	v->on_sw = false;
	v->volume = 1.0;
	v->pan = v->depth = v->height = 0;
	v->mix_rate = 44100;
	v->filter_type = FILTER_NONE;
	v->filter_cutoff = 0;
	v->filter_resonance = 0;
	v->filter_gain = 0;
	v->chorus = 0;
	v->reverb = 0;
	v->reverb_type = REVERB_SMALL;
	v->positional = false;
	v->start_tick = 0;
	return hw_voice_owner.make_rid(v);
}

void AudioServer3DS::voice_play(RID p_voice, RID p_sample) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);

	_release_channel(v);
	if (v->on_sw)
		AudioServerSW::voice_stop(v->sw_voice);
	v->sample = p_sample;
	v->mix_rate = sample_get_mix_rate(p_sample);

	const HWSample *hs = _get_hw_sample(p_sample);
	if (!hs) {
		_ensure_sw_voice(v);
		v->on_sw = true;
		AudioServerSW::voice_play(v->sw_voice, p_sample);
		return;
	}
	v->on_sw = false;

	int ch = _alloc_channel();
	v->channel = ch;
	v->start_tick = svcGetSystemTick();
	channel_voice[ch] = v;

	ndspChnReset(ch);
	ndspChnSetInterp(ch, NDSP_INTERP_LINEAR);
	ndspChnSetFormat(ch, hs->format);
	_apply_mix(v);

	// Forward loops: an intro buffer [0, loop_begin) and a looping one [loop_begin, loop_end).
	// (Ping-pong loops play forward.)
	int begin = 0, end = hs->frames;
	bool loop = sample_get_loop_format(p_sample) != SAMPLE_LOOP_NONE;
	if (loop) {
		begin = CLAMP(sample_get_loop_begin(p_sample), 0, hs->frames - 1);
		end = CLAMP(sample_get_loop_end(p_sample), begin + 1, hs->frames);
	}
	memset(v->wave, 0, sizeof(v->wave));
	uint8_t *data = (uint8_t *)hs->data;
	if (!loop || begin > 0) {
		v->wave[0].data_vaddr = data;
		v->wave[0].nsamples = loop ? begin : hs->frames;
		ndspChnWaveBufAdd(ch, &v->wave[0]);
	}
	if (loop) {
		v->wave[1].data_vaddr = data + begin * hs->bytes_per_frame;
		v->wave[1].nsamples = end - begin;
		v->wave[1].looping = true;
		ndspChnWaveBufAdd(ch, &v->wave[1]);
	}
}

void AudioServer3DS::voice_set_volume(RID p_voice, float p_volume) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->volume = p_volume;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_volume(v->sw_voice, p_volume);
	_apply_mix(v);
}

void AudioServer3DS::voice_set_pan(RID p_voice, float p_pan, float p_depth, float p_height) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->pan = p_pan;
	v->depth = p_depth;
	v->height = p_height;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_pan(v->sw_voice, p_pan, p_depth, p_height);
	_apply_mix(v);
}

void AudioServer3DS::voice_set_filter(RID p_voice, FilterType p_type, float p_cutoff, float p_resonance, float p_gain) {

	// The DSP has filters too, but SamplePlayer's filters are rarely used: hardware voices
	// play unfiltered; the setting is kept for the software fallback.
	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->filter_type = p_type;
	v->filter_cutoff = p_cutoff;
	v->filter_resonance = p_resonance;
	v->filter_gain = p_gain;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_filter(v->sw_voice, p_type, p_cutoff, p_resonance, p_gain);
}

void AudioServer3DS::voice_set_chorus(RID p_voice, float p_chorus) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->chorus = p_chorus;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_chorus(v->sw_voice, p_chorus);
}

void AudioServer3DS::voice_set_reverb(RID p_voice, ReverbRoomType p_room_type, float p_reverb) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->reverb_type = p_room_type;
	v->reverb = p_reverb;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_reverb(v->sw_voice, p_room_type, p_reverb);
}

void AudioServer3DS::voice_set_mix_rate(RID p_voice, int p_mix_rate) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->mix_rate = p_mix_rate;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_mix_rate(v->sw_voice, p_mix_rate);
	if (v->channel >= 0 && channel_voice[v->channel] == v)
		ndspChnSetRate(v->channel, p_mix_rate);
}

void AudioServer3DS::voice_set_positional(RID p_voice, bool p_positional) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	v->positional = p_positional;
	if (v->sw_voice.is_valid())
		AudioServerSW::voice_set_positional(v->sw_voice, p_positional);
}

#define GET_VOICE(m_ret)                        \
	const HWVoice *v = hw_voice_owner.get(p_voice); \
	ERR_FAIL_COND_V(!v, m_ret);

float AudioServer3DS::voice_get_volume(RID p_voice) const {
	GET_VOICE(0);
	return v->volume;
}
float AudioServer3DS::voice_get_pan(RID p_voice) const {
	GET_VOICE(0);
	return v->pan;
}
float AudioServer3DS::voice_get_pan_depth(RID p_voice) const {
	GET_VOICE(0);
	return v->depth;
}
float AudioServer3DS::voice_get_pan_height(RID p_voice) const {
	GET_VOICE(0);
	return v->height;
}
AudioServer::FilterType AudioServer3DS::voice_get_filter_type(RID p_voice) const {
	GET_VOICE(FILTER_NONE);
	return v->filter_type;
}
float AudioServer3DS::voice_get_filter_cutoff(RID p_voice) const {
	GET_VOICE(0);
	return v->filter_cutoff;
}
float AudioServer3DS::voice_get_filter_resonance(RID p_voice) const {
	GET_VOICE(0);
	return v->filter_resonance;
}
float AudioServer3DS::voice_get_chorus(RID p_voice) const {
	GET_VOICE(0);
	return v->chorus;
}
AudioServer::ReverbRoomType AudioServer3DS::voice_get_reverb_type(RID p_voice) const {
	GET_VOICE(REVERB_SMALL);
	return v->reverb_type;
}
float AudioServer3DS::voice_get_reverb(RID p_voice) const {
	GET_VOICE(0);
	return v->reverb;
}
int AudioServer3DS::voice_get_mix_rate(RID p_voice) const {
	GET_VOICE(0);
	return v->mix_rate;
}
bool AudioServer3DS::voice_is_positional(RID p_voice) const {
	GET_VOICE(false);
	return v->positional;
}

void AudioServer3DS::voice_stop(RID p_voice) {

	HWVoice *v = hw_voice_owner.get(p_voice);
	ERR_FAIL_COND(!v);
	_release_channel(v);
	if (v->on_sw)
		AudioServerSW::voice_stop(v->sw_voice);
	v->on_sw = false;
}

bool AudioServer3DS::voice_is_active(RID p_voice) const {

	GET_VOICE(false);
	if (v->on_sw)
		return AudioServerSW::voice_is_active(v->sw_voice);
	return _is_playing(v);
}

void AudioServer3DS::set_fx_global_volume_scale(float p_volume) {

	AudioServerSW::set_fx_global_volume_scale(p_volume);
	for (int i = 0; i < CHANNEL_COUNT; i++)
		if (channel_voice[i])
			_apply_mix(channel_voice[i]);
}

void AudioServer3DS::set_event_voice_global_volume_scale(float p_volume) {

	AudioServerSW::set_event_voice_global_volume_scale(p_volume);
	for (int i = 0; i < CHANNEL_COUNT; i++)
		if (channel_voice[i])
			_apply_mix(channel_voice[i]);
}

void AudioServer3DS::free(RID p_id) {

	if (hw_voice_owner.owns(p_id)) {
		HWVoice *v = hw_voice_owner.get(p_id);
		_release_channel(v);
		if (v->sw_voice.is_valid())
			AudioServerSW::free(v->sw_voice);
		hw_voice_owner.free(p_id);
		memdelete(v);
		return;
	}
	_drop_hw_sample(p_id); // no-op for anything that isn't a played sample
	AudioServerSW::free(p_id);
}

AudioServer3DS::AudioServer3DS(SampleManagerSW *p_sample_manager) :
		AudioServerSW(p_sample_manager) {

	for (int i = 0; i < CHANNEL_COUNT; i++)
		channel_voice[i] = NULL;
}

AudioServer3DS::~AudioServer3DS() {

	for (int i = 0; i < CHANNEL_COUNT; i++)
		if (channel_voice[i])
			_release_channel(channel_voice[i]);
	for (Map<RID, HWSample>::Element *E = hw_samples.front(); E; E = E->next())
		linearFree(E->get().data);
}

#endif
