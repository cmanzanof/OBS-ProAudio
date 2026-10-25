/******************************************************************************
    ProAudio Studio - master faders of the buses and of the headphones.

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

#include <QDialog>
#include <QString>

#include <array>

class QGridLayout;
class QSlider;
class QWidget;

/* ProAudio: the "outputs" section of a vMix style mixer, i.e. the master gain
 * of every bus plus the volume of the headphones.  Both live in the engine, so
 * they can be changed from anywhere without touching a single source. */
class ProAudioBusMixer : public QDialog {
	Q_OBJECT

public:
	ProAudioBusMixer(QWidget *parent = nullptr);

protected:
	void done(int result) override;

private:
	std::array<float, MAX_AUDIO_MIXES> initialBusVolumes{};
	float initialMonitorVolume = 1.0f;
	std::array<QSlider *, MAX_AUDIO_MIXES> sliders{};
	QSlider *monitorSlider = nullptr;

	QSlider *addFaderRow(QGridLayout *grid, int row, const QString &label, const QString &tooltip, float volume,
			     QWidget *meter);
	void addDeviceCombo(QGridLayout *grid, int row, size_t bus, const QString &busName);
	void resetVolumes();
};
