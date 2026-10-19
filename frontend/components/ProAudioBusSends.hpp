/******************************************************************************
    ProAudio Studio - per source send levels for the master mix and the buses.

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

#include <array>

class QLabel;
class QSlider;

/* ProAudio: one fader per bus.  The send is applied post-fader, after the
 * source volume, and only reaches the mixes the source is routed to. */
class ProAudioBusSends : public QDialog {
	Q_OBJECT

public:
	ProAudioBusSends(obs_source_t *source, QWidget *parent = nullptr);

protected:
	void done(int result) override;

private:
	OBSSource source;
	std::array<float, MAX_AUDIO_MIXES> initialSends{};
	std::array<QSlider *, MAX_AUDIO_MIXES> sliders{};

	void setBusSend(size_t bus, float volume);
	void resetSends();
};
