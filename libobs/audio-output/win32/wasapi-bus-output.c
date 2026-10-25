/******************************************************************************
    Copyright (C) 2026 ProAudio Studio contributors

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

#include "../../util/platform.h"
#include "../../obs-internal.h"

#include <windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>

#ifndef KSAUDIO_SPEAKER_2POINT1
#define KSAUDIO_SPEAKER_2POINT1 (KSAUDIO_SPEAKER_STEREO | SPEAKER_LOW_FREQUENCY)
#endif

#ifndef KSAUDIO_SPEAKER_4POINT1
#define KSAUDIO_SPEAKER_4POINT1 (KSAUDIO_SPEAKER_SURROUND | SPEAKER_LOW_FREQUENCY)
#endif

#define safe_release(ptr)                          \
	do {                                       \
		if (ptr) {                         \
			ptr->lpVtbl->Release(ptr); \
		}                                  \
	} while (false)

#define ACTUALLY_DEFINE_GUID(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
	EXTERN_C const GUID DECLSPEC_SELECTANY name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

ACTUALLY_DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69,
		     0x2E);
ACTUALLY_DEFINE_GUID(IID_IMMDeviceEnumerator, 0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17,
		     0xE6);
ACTUALLY_DEFINE_GUID(IID_IAudioClient, 0x1CB9AD4C, 0xDBFA, 0x4C32, 0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2);
ACTUALLY_DEFINE_GUID(IID_IAudioRenderClient, 0xF294ACFC, 0x3146, 0x4483, 0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60,
		     0xE2);
ACTUALLY_DEFINE_GUID(KSDATAFORMAT_SUBTYPE_PCM, 0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B,
		     0x71);
ACTUALLY_DEFINE_GUID(KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, 0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38,
		     0x9B, 0x71);

/* ProAudio: plays one auxiliary bus on a WASAPI playback device.
 *
 * The audio is not resampled here: the output is registered as a raw audio
 * callback on the mix with the exact format of the device, so libobs converts
 * the mix (float, planar, its own sample rate and channel layout) into the
 * interleaved bytes the device expects, and this file only has to move the
 * block into the render buffer.  That keeps the timing of the bus tied to the
 * audio thread instead of adding another thread per bus. */

struct audio_bus_output {
	size_t bus;

	IAudioClient *client;
	IAudioRenderClient *render;

	enum audio_format format;
	enum speaker_layout speakers;
	uint32_t sample_rate;
	uint32_t buffer_frames;
	size_t frame_size;

	/* Set when WASAPI fails: the block is dropped instead of retrying (and
	 * logging) on every audio tick.  Selecting the device again reopens it. */
	bool failed;
};

/* Every message of this file is about one bus, so the helper adds the bus
 * letter instead of repeating it in every string. */
static void do_log(int level, const struct audio_bus_output *output, const char *format, ...)
{
	char message[512];
	va_list args;

	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);

	blog(level, "ProAudio: bus %c output: %s", (char)('A' + (int)output->bus - 1), message);
}

static bool map_format(const WAVEFORMATEX *wfex, enum audio_format *format)
{
	const bool extensible = wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE;
	const GUID *sub = extensible ? &((const WAVEFORMATEXTENSIBLE *)wfex)->SubFormat : NULL;
	const WORD bits = wfex->wBitsPerSample;

	/* A WAVE_FORMAT_EXTENSIBLE PCM format may store fewer valid bits than
	 * the container has (24 in 32, for example), which libobs cannot
	 * produce, so it has to be rejected instead of played as 32 bit. */
	if (extensible && sub && IsEqualGUID(sub, &KSDATAFORMAT_SUBTYPE_PCM)) {
		if (((const WAVEFORMATEXTENSIBLE *)wfex)->Samples.wValidBitsPerSample != 0 &&
		    ((const WAVEFORMATEXTENSIBLE *)wfex)->Samples.wValidBitsPerSample != bits)
			return false;
	}

	if ((!sub && wfex->wFormatTag == WAVE_FORMAT_IEEE_FLOAT) ||
	    (sub && IsEqualGUID(sub, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT))) {
		if (bits != 32)
			return false;

		*format = AUDIO_FORMAT_FLOAT;
		return true;
	}

	if ((!sub && wfex->wFormatTag == WAVE_FORMAT_PCM) || (sub && IsEqualGUID(sub, &KSDATAFORMAT_SUBTYPE_PCM))) {
		switch (bits) {
		case 16:
			*format = AUDIO_FORMAT_16BIT;
			return true;
		case 32:
			*format = AUDIO_FORMAT_32BIT;
			return true;
		}
	}

	return false;
}

static enum speaker_layout map_speakers(const WAVEFORMATEX *wfex)
{
	DWORD mask = 0;

	if (wfex->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
		mask = ((const WAVEFORMATEXTENSIBLE *)wfex)->dwChannelMask;

	switch (mask) {
	case KSAUDIO_SPEAKER_2POINT1:
		return SPEAKERS_2POINT1;
	case KSAUDIO_SPEAKER_SURROUND:
		return SPEAKERS_4POINT0;
	case KSAUDIO_SPEAKER_4POINT1:
		return SPEAKERS_4POINT1;
	case KSAUDIO_SPEAKER_5POINT1:
		return SPEAKERS_5POINT1;
	case KSAUDIO_SPEAKER_7POINT1:
		return SPEAKERS_7POINT1;
	}

	switch (wfex->nChannels) {
	case 1:
		return SPEAKERS_MONO;
	case 2:
		return SPEAKERS_STEREO;
	case 3:
		return SPEAKERS_2POINT1;
	case 4:
		return SPEAKERS_4POINT0;
	case 5:
		return SPEAKERS_4POINT1;
	case 6:
		return SPEAKERS_5POINT1;
	case 8:
		return SPEAKERS_7POINT1;
	}

	return SPEAKERS_UNKNOWN;
}

static void on_bus_audio(void *param, size_t mix_idx, struct audio_data *data);

static bool bus_output_open_device(struct audio_bus_output *output, const char *device_id)
{
	IMMDeviceEnumerator *immde = NULL;
	IMMDevice *device = NULL;
	WAVEFORMATEX *wfex = NULL;
	struct audio_convert_info conversion;
	HRESULT hr;
	bool success = false;

	hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator, (void **)&immde);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not create the device enumerator: %08lX", (unsigned long)hr);
		return false;
	}

	if (!device_id || strcmp(device_id, "default") == 0) {
		hr = immde->lpVtbl->GetDefaultAudioEndpoint(immde, eRender, eConsole, &device);
	} else {
		wchar_t w_id[512];

		os_utf8_to_wcs(device_id, 0, w_id, 512);
		hr = immde->lpVtbl->GetDevice(immde, w_id, &device);
	}

	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not get the device: %08lX", (unsigned long)hr);
		goto fail;
	}

	hr = device->lpVtbl->Activate(device, &IID_IAudioClient, CLSCTX_ALL, NULL, (void **)&output->client);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not activate the device: %08lX", (unsigned long)hr);
		goto fail;
	}

	hr = output->client->lpVtbl->GetMixFormat(output->client, &wfex);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not get the device format: %08lX", (unsigned long)hr);
		goto fail;
	}

	if (!map_format(wfex, &output->format)) {
		do_log(LOG_WARNING, output,
		       "the device uses an unsupported format (tag %d, %d bits, %d channels)",
		       (int)wfex->wFormatTag, (int)wfex->wBitsPerSample, (int)wfex->nChannels);
		goto fail;
	}

	output->speakers = map_speakers(wfex);
	if (output->speakers == SPEAKERS_UNKNOWN) {
		do_log(LOG_WARNING, output, "the device uses an unsupported channel layout (%d channels)",
		       (int)wfex->nChannels);
		goto fail;
	}

	/* Shared mode wants the exact mix format, so the callback is registered
	 * with it and libobs does the conversion. */
	hr = output->client->lpVtbl->Initialize(output->client, AUDCLNT_SHAREMODE_SHARED, 0, 10000000, 0, wfex, NULL);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not initialize the device: %08lX", (unsigned long)hr);
		goto fail;
	}

	hr = output->client->lpVtbl->GetBufferSize(output->client, &output->buffer_frames);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not get the device buffer size: %08lX", (unsigned long)hr);
		goto fail;
	}

	hr = output->client->lpVtbl->GetService(output->client, &IID_IAudioRenderClient, (void **)&output->render);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not get the render client: %08lX", (unsigned long)hr);
		goto fail;
	}

	hr = output->client->lpVtbl->Start(output->client);
	if (FAILED(hr)) {
		do_log(LOG_WARNING, output, "could not start the device: %08lX", (unsigned long)hr);
		goto fail;
	}

	output->sample_rate = (uint32_t)wfex->nSamplesPerSec;
	output->frame_size = wfex->nBlockAlign;

	conversion.samples_per_sec = output->sample_rate;
	conversion.format = output->format;
	conversion.speakers = output->speakers;
	conversion.allow_clipping = false;

	if (!audio_output_connect(obs->audio.audio, output->bus, &conversion, on_bus_audio, output)) {
		do_log(LOG_WARNING, output, "could not register the mix callback");
		goto fail;
	}

	success = true;

fail:
	safe_release(device);
	safe_release(immde);
	if (wfex)
		CoTaskMemFree(wfex);
	return success;
}

static void on_bus_audio(void *param, size_t mix_idx, struct audio_data *data)
{
	struct audio_bus_output *output = param;
	UINT32 pad = 0;
	UINT32 frames = (UINT32)data->frames;
	BYTE *buffer;

	UNUSED_PARAMETER(mix_idx);

	if (!frames || output->failed || !output->client || !output->render)
		return;

	if (FAILED(output->client->lpVtbl->GetCurrentPadding(output->client, &pad)))
		goto fail;

	/* The device is behind (or the format changed under us): dropping the
	 * block keeps the audio thread from blocking, which would stall every
	 * other bus and the program mix with it. */
	if (pad + frames > output->buffer_frames)
		return;

	if (FAILED(output->render->lpVtbl->GetBuffer(output->render, frames, &buffer)))
		goto fail;

	memcpy(buffer, data->data[0], (size_t)frames * output->frame_size);

	if (FAILED(output->render->lpVtbl->ReleaseBuffer(output->render, frames, 0)))
		goto fail;

	return;

fail:
	output->failed = true;
	do_log(LOG_WARNING, output, "stopped after an error, select the device again to reopen it");
}

bool audio_bus_outputs_available(void)
{
	return true;
}

struct audio_bus_output *audio_bus_output_create(size_t bus, const char *device_id)
{
	struct audio_bus_output *output = bzalloc(sizeof(*output));

	output->bus = bus;

	if (!bus_output_open_device(output, device_id)) {
		audio_bus_output_destroy(output);
		return NULL;
	}

	return output;
}

void audio_bus_output_destroy(struct audio_bus_output *output)
{
	if (!output)
		return;

	/* Undo audio_output_connect() first: it takes the audio output mutex,
	 * which means the audio thread is no longer inside the callback when it
	 * returns, so the WASAPI objects can be released safely. */
	if (obs->audio.audio)
		audio_output_disconnect(obs->audio.audio, output->bus, on_bus_audio, output);

	if (output->client)
		output->client->lpVtbl->Stop(output->client);

	safe_release(output->render);
	safe_release(output->client);
	bfree(output);
}
