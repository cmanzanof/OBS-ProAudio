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

#include "../../obs-internal.h"

/* ProAudio: bus outputs are only implemented on Windows for now.  The portable
 * layer still remembers the chosen devices, so a profile written there keeps
 * its routing; it is simply not played on this platform. */

bool audio_bus_outputs_available(void)
{
	return false;
}

struct audio_bus_output *audio_bus_output_create(size_t bus, const char *device_id)
{
	UNUSED_PARAMETER(bus);
	UNUSED_PARAMETER(device_id);
	return NULL;
}

void audio_bus_output_destroy(struct audio_bus_output *output)
{
	UNUSED_PARAMETER(output);
}
