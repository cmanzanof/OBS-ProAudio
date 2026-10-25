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

#pragma once

#include <obs.hpp>

#include <QWidget>

#include <atomic>
#include <cmath>

class QTimer;

/* ProAudio: the mixer dock shows a meter per source, but there was no way to
 * see the level of a bus.  This widget listens to the raw mix of a bus through
 * the volume meter API and draws it on a compact bar. */
class ProAudioBusMeter : public QWidget {
	Q_OBJECT

public:
	explicit ProAudioBusMeter(size_t bus, QWidget *parent = nullptr);
	~ProAudioBusMeter() override;

	QSize sizeHint() const override;
	QSize minimumSizeHint() const override;

protected:
	void paintEvent(QPaintEvent *event) override;

private:
	static void levelsUpdated(void *data, const float magnitude[MAX_AUDIO_CHANNELS],
				  const float peak[MAX_AUDIO_CHANNELS], const float inputPeak[MAX_AUDIO_CHANNELS]);

	void publishLevels(const float magnitude[MAX_AUDIO_CHANNELS], const float peak[MAX_AUDIO_CHANNELS]);
	void refreshLevels();

	OBSVolMeter meter;
	QTimer *timer = nullptr;

	/* Written from the audio thread, read from the GUI thread. */
	std::atomic<float> levelMagnitude{-INFINITY};
	std::atomic<float> levelPeak{-INFINITY};

	float displayMagnitude = -INFINITY;
	float displayPeak = -INFINITY;
	float peakHold = -INFINITY;
};
