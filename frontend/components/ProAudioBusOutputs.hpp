/******************************************************************************
    ProAudio Studio - bus outputs, i.e. one playback device per auxiliary bus.

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

#pragma once

#include <QString>

class QComboBox;

/* ProAudio: plays the buses A..G on playback devices, like the physical
 * outputs of a video switcher.  The device of every bus is stored in the
 * profile, so a show keeps its routing between sessions. */
bool proAudioBusOutputsAvailable();

/* Restores the saved device of every bus and opens the streams. */
void proAudioLoadBusOutputs();

/* Fills a combo with "no output", the default device and every playback
 * device, selecting the one saved for the bus. */
void proAudioFillBusOutputCombo(QComboBox *combo, size_t bus);

/* Routes a bus to a device (empty id turns the output off) and saves it in the
 * current profile. */
void proAudioApplyBusOutput(size_t bus, const QString &id, const QString &name);
