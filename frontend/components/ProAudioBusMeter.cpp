/******************************************************************************
    ProAudio Studio - small level meter for an audio bus.

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

#include "ProAudioBusMeter.hpp"

#include <QPainter>
#include <QTimer>

#include <algorithm>

#include "moc_ProAudioBusMeter.cpp"

/* Same range as the rest of the bus widgets. */
static constexpr float METER_MIN_DB = -60.0f;

/* Fall back speeds, in dB per redraw (about 30 redraws per second). */
static constexpr float MAGNITUDE_FALL_DB = 3.0f;
static constexpr float PEAK_FALL_DB = 8.0f;
static constexpr float PEAK_HOLD_FALL_DB = 1.5f;

static inline float deflectionForDb(float db)
{
	if (db <= METER_MIN_DB)
		return 0.0f;

	return std::min(1.0f, (db - METER_MIN_DB) / -METER_MIN_DB);
}

ProAudioBusMeter::ProAudioBusMeter(size_t bus, QWidget *parent) : QWidget(parent)
{
	setAttribute(Qt::WA_OpaquePaintEvent, true);
	setAttribute(Qt::WA_TransparentForMouseEvents);
	setFocusPolicy(Qt::NoFocus);

	meter = obs_volmeter_create(OBS_FADER_LOG);
	obs_volmeter_add_callback(meter, levelsUpdated, this);
	obs_volmeter_attach_bus(meter, bus);

	timer = new QTimer(this);
	timer->setTimerType(Qt::CoarseTimer);
	connect(timer, &QTimer::timeout, this, &ProAudioBusMeter::refreshLevels);
	timer->start(33);
}

ProAudioBusMeter::~ProAudioBusMeter()
{
	/* This detaches the bus callback, which also waits for any callback in
	 * flight to return. */
	obs_volmeter_remove_callback(meter, levelsUpdated, this);
	obs_volmeter_detach_source(meter);
}

QSize ProAudioBusMeter::sizeHint() const
{
	return QSize(90, 14);
}

QSize ProAudioBusMeter::minimumSizeHint() const
{
	return QSize(48, 10);
}

void ProAudioBusMeter::levelsUpdated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS],
				     const float inputPeak[MAX_AUDIO_CHANNELS])
{
	ProAudioBusMeter *self = static_cast<ProAudioBusMeter *>(data);

	UNUSED_PARAMETER(inputPeak);

	self->publishLevels(magnitude, peak);
}

void ProAudioBusMeter::publishLevels(const float magnitude[MAX_AUDIO_CHANNELS],
				     const float peak[MAX_AUDIO_CHANNELS])
{
	float loudestMagnitude = -INFINITY;
	float loudestPeak = -INFINITY;

	for (int channel = 0; channel < MAX_AUDIO_CHANNELS; channel++) {
		loudestMagnitude = std::max(loudestMagnitude, magnitude[channel]);
		loudestPeak = std::max(loudestPeak, peak[channel]);
	}

	/* Called from the audio thread: only publish the values. */
	levelMagnitude.store(loudestMagnitude, std::memory_order_relaxed);
	levelPeak.store(loudestPeak, std::memory_order_relaxed);
}

void ProAudioBusMeter::refreshLevels()
{
	const float magnitude = levelMagnitude.load(std::memory_order_relaxed);
	const float peak = levelPeak.load(std::memory_order_relaxed);

	displayMagnitude = std::max(magnitude, displayMagnitude - MAGNITUDE_FALL_DB);
	displayPeak = std::max(peak, displayPeak - PEAK_FALL_DB);
	peakHold = std::max(peak, peakHold - PEAK_HOLD_FALL_DB);

	update();
}

void ProAudioBusMeter::paintEvent(QPaintEvent *event)
{
	UNUSED_PARAMETER(event);

	QPainter painter(this);
	QRect track = rect().adjusted(0, 0, -1, -1);

	painter.fillRect(track, QColor(0x1e, 0x1e, 0x1e));

	const int width = track.width();
	if (width <= 1 || track.height() <= 2)
		return;

	const int left = track.left();
	const int magnitudeX = left + (int)(deflectionForDb(displayMagnitude) * (width - 1));
	const int peakX = left + (int)(deflectionForDb(displayPeak) * (width - 1));
	const int holdX = left + (int)(deflectionForDb(peakHold) * (width - 1));
	const int innerHeight = track.height() - 2;

	/* RMS on the lower half, sample peak on the upper half. */
	if (magnitudeX > left)
		painter.fillRect(QRect(left, track.top() + 1 + innerHeight / 2, magnitudeX - left,
				       innerHeight - innerHeight / 2),
				 QColor(0x4c, 0xd9, 0x4c));

	if (peakX > left)
		painter.fillRect(QRect(left, track.top() + 1, peakX - left, innerHeight / 2), QColor(0xd9, 0xd9, 0x4c));

	/* Peak hold line. */
	if (holdX > left)
		painter.fillRect(QRect(holdX, track.top() + 1, 1, innerHeight), QColor(0xf0, 0xf0, 0xf0));
}
