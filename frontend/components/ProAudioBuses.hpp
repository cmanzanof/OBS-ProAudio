/******************************************************************************
    ProAudio Studio - audio bus helpers shared by the frontend widgets.

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

#include <QChar>
#include <QString>

#include <cmath>

#include <OBSApp.hpp>

/* ProAudio: mixer 0 is the master mix and mixes 1..MAX_AUDIO_MIXES-1 are the
 * auxiliary buses A..G, which is how vMix labels them. */
static inline QString proAudioBusShortName(size_t bus)
{
	return bus == 0 ? QString("M") : QString(QChar('A' + (int)bus - 1));
}

static inline QString proAudioBusName(size_t bus)
{
	return bus == 0 ? QTStr("Basic.ProAudio.Bus.Master")
			: QTStr("Basic.ProAudio.Bus.Letter").arg(QChar('A' + (int)bus - 1));
}

/* ProAudio: the bus faders work in tenths of a dB, from silence up to +6 dB,
 * and expose their value through these helpers so that every place that shows
 * a bus level agrees on the scale. */
#define PROAUDIO_FADER_MIN_DB10 (-600)
#define PROAUDIO_FADER_MAX_DB10 (60)
#define PROAUDIO_FADER_UNITY_DB10 (0)

static inline float proAudioFaderToVolume(int deflection)
{
	if (deflection <= PROAUDIO_FADER_MIN_DB10)
		return 0.0f;

	return powf(10.0f, (float)deflection / 200.0f);
}

static inline int proAudioVolumeToFader(float volume)
{
	if (volume <= 0.0f)
		return PROAUDIO_FADER_MIN_DB10;

	int deflection = (int)lroundf(200.0f * log10f(volume));

	if (deflection < PROAUDIO_FADER_MIN_DB10)
		deflection = PROAUDIO_FADER_MIN_DB10;
	else if (deflection > PROAUDIO_FADER_MAX_DB10)
		deflection = PROAUDIO_FADER_MAX_DB10;

	return deflection;
}

static inline QString proAudioFaderText(int deflection)
{
	if (deflection <= PROAUDIO_FADER_MIN_DB10)
		return QStringLiteral("-\u221e dB");

	return QString::number(deflection / 10.0, 'f', 1) + QStringLiteral(" dB");
}
