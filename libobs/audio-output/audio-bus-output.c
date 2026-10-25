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

#include "../obs-internal.h"

/* ProAudio: plays an auxiliary bus (A..G) on a playback device.
 *
 * Up to here libobs only knew how to write audio to a device for source
 * monitoring, with one stream per monitored source, so a bus could not leave
 * the machine.  This module keeps one output per bus: the device chosen by
 * the user survives audio resets, and the streams are created again whenever
 * the audio system is reinitialized, so changing the sample rate or the
 * channel setup does not silently kill the outputs.
 *
 * Bus 0 (the master mix) cannot be routed, because it is the mix that feeds
 * recording and streaming and, like in vMix, it is not a device output. */

struct bus_output {
	char *device_id;
	struct audio_bus_output *impl;
};

static struct bus_output bus_outputs[MAX_AUDIO_MIXES];
static pthread_mutex_t bus_outputs_mutex = PTHREAD_MUTEX_INITIALIZER;

static bool valid_bus(size_t bus)
{
	return bus > 0 && bus < MAX_AUDIO_MIXES;
}

static void close_output(size_t bus)
{
	struct bus_output *output = &bus_outputs[bus];

	if (output->impl) {
		audio_bus_output_destroy(output->impl);
		output->impl = NULL;
	}
}

static bool open_output(size_t bus)
{
	struct bus_output *output = &bus_outputs[bus];

	if (!audio_bus_outputs_available())
		return false;

	output->impl = audio_bus_output_create(bus, output->device_id);
	return output->impl != NULL;
}

bool obs_audio_bus_outputs_available(void)
{
	return audio_bus_outputs_available();
}

bool obs_audio_set_bus_device(size_t bus, const char *device_id)
{
	bool success = true;
	bool enabled;

	if (!valid_bus(bus))
		return false;

	pthread_mutex_lock(&bus_outputs_mutex);

	/* Always close first: the device may be the same one, but the stream has
	 * to be reopened to pick up a format change. */
	close_output(bus);

	struct bus_output *output = &bus_outputs[bus];
	bfree(output->device_id);
	output->device_id = (device_id && *device_id) ? bstrdup(device_id) : NULL;
	enabled = output->device_id != NULL;

	/* Creating the stream needs a running audio output.  Before it exists
	 * the id is only remembered, and audio_bus_outputs_reload() opens it. */
	if (enabled && obs->audio.audio) {
		success = open_output(bus);

		if (success)
			blog(LOG_INFO, "ProAudio: bus %d output on '%s'", (int)bus, output->device_id);
		else
			blog(LOG_WARNING, "ProAudio: could not open '%s' as the output of bus %d",
			     output->device_id, (int)bus);
	}

	pthread_mutex_unlock(&bus_outputs_mutex);

	return enabled ? success : true;
}

const char *obs_audio_get_bus_device(size_t bus)
{
	if (!valid_bus(bus))
		return NULL;

	return bus_outputs[bus].device_id;
}

/* Called after the audio output has been created: reopens every bus output
 * with the new sample rate and channel setup. */
void audio_bus_outputs_reload(void)
{
	if (!obs->audio.audio)
		return;

	pthread_mutex_lock(&bus_outputs_mutex);

	for (size_t bus = 1; bus < MAX_AUDIO_MIXES; bus++) {
		close_output(bus);

		if (bus_outputs[bus].device_id && !open_output(bus))
			blog(LOG_WARNING, "ProAudio: bus %d output could not be reopened on '%s'", (int)bus,
			     bus_outputs[bus].device_id);
	}

	pthread_mutex_unlock(&bus_outputs_mutex);
}

/* Called before the audio output is closed: every stream has to drop its raw
 * audio callback while the audio system is still alive. */
void audio_bus_outputs_free(void)
{
	pthread_mutex_lock(&bus_outputs_mutex);

	for (size_t bus = 1; bus < MAX_AUDIO_MIXES; bus++)
		close_output(bus);

	pthread_mutex_unlock(&bus_outputs_mutex);
}

/* Called when obs shuts down for good. */
void audio_bus_outputs_cleanup(void)
{
	pthread_mutex_lock(&bus_outputs_mutex);

	for (size_t bus = 1; bus < MAX_AUDIO_MIXES; bus++) {
		close_output(bus);
		bfree(bus_outputs[bus].device_id);
		bus_outputs[bus].device_id = NULL;
	}

	pthread_mutex_unlock(&bus_outputs_mutex);
}
