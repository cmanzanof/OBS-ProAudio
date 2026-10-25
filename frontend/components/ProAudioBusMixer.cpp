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

#include "ProAudioBusMixer.hpp"

#include "ProAudioBusMeter.hpp"
#include "ProAudioBuses.hpp"

#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QDialogButtonBox>
#include <QFrame>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <functional>

#include "moc_ProAudioBusMixer.cpp"

ProAudioBusMixer::ProAudioBusMixer(QWidget *parent) : QDialog(parent)
{
	setWindowTitle(QTStr("Basic.ProAudio.BusMixer"));
	setMinimumWidth(460);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	QGridLayout *grid = new QGridLayout();

	grid->setHorizontalSpacing(12);
	grid->setColumnStretch(1, 1);

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		float volume = obs_audio_get_bus_volume(bus);

		initialBusVolumes[bus] = volume;

		/* The bus fader is already post source faders and post sends, so
		 * moving it does not disturb how the sources are mixed. */
		ProAudioBusMeter *meter = new ProAudioBusMeter(bus, this);
		QString busName = proAudioBusName(bus);

		meter->setToolTip(QTStr("Basic.ProAudio.BusMixer.MeterTooltip").arg(busName));

		QSlider *slider = addFaderRow(grid, (int)bus, proAudioBusShortName(bus), busName, volume, meter);

		connect(slider, &QSlider::valueChanged, this,
			[bus](int deflection) { obs_audio_set_bus_volume(bus, proAudioFaderToVolume(deflection)); });

		sliders[bus] = slider;
	}

	/* Separate the buses from the headphones, which is not a bus: it is the
	 * monitoring device. */
	QFrame *line = new QFrame(this);
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);
	grid->addWidget(line, (int)MAX_AUDIO_MIXES, 0, 1, 4);

	initialMonitorVolume = obs_audio_get_monitoring_volume();
	monitorSlider = addFaderRow(grid, (int)MAX_AUDIO_MIXES + 1, QTStr("Basic.ProAudio.BusMixer.Headphones"),
				    QTStr("Basic.ProAudio.BusMixer.HeadphonesTooltip"), initialMonitorVolume, nullptr);

	connect(monitorSlider, &QSlider::valueChanged, this,
		[](int deflection) { obs_audio_set_monitoring_volume(proAudioFaderToVolume(deflection)); });

	mainLayout->addLayout(grid);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	QPushButton *resetButton = buttons->addButton(QTStr("Basic.ProAudio.BusMixer.Reset"),
						      QDialogButtonBox::ResetRole);

	connect(resetButton, &QPushButton::clicked, this, [this]() { resetVolumes(); });
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	mainLayout->addWidget(buttons);
}

QSlider *ProAudioBusMixer::addFaderRow(QGridLayout *grid, int row, const QString &label, const QString &tooltip,
				       float volume, QWidget *meter)
{
	QLabel *name = new QLabel(label, this);
	name->setToolTip(tooltip);
	name->setMinimumWidth(80);

	QSlider *slider = new QSlider(Qt::Horizontal, this);
	slider->setRange(PROAUDIO_FADER_MIN_DB10, PROAUDIO_FADER_MAX_DB10);
	slider->setValue(proAudioVolumeToFader(volume));
	slider->setToolTip(tooltip);
	slider->setAccessibleName(tooltip);

	QLabel *value = new QLabel(proAudioFaderText(slider->value()), this);
	value->setMinimumWidth(64);
	value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

	connect(slider, &QSlider::valueChanged, value,
		[value](int deflection) { value->setText(proAudioFaderText(deflection)); });

	grid->addWidget(name, row, 0);
	grid->addWidget(slider, row, 1);
	if (meter)
		grid->addWidget(meter, row, 2);
	grid->addWidget(value, row, 3);

	return slider;
}

void ProAudioBusMixer::resetVolumes()
{
	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		/* Moving the fader applies the level through valueChanged(). */
		if (sliders[bus]->value() != PROAUDIO_FADER_UNITY_DB10)
			sliders[bus]->setValue(PROAUDIO_FADER_UNITY_DB10);
	}

	if (monitorSlider->value() != PROAUDIO_FADER_UNITY_DB10)
		monitorSlider->setValue(PROAUDIO_FADER_UNITY_DB10);
}

void ProAudioBusMixer::done(int result)
{
	/* The whole dialog is a single undo step. */
	std::array<float, MAX_AUDIO_MIXES> busVolumes{};
	float monitorVolume = obs_audio_get_monitoring_volume();
	bool changed = false;

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		busVolumes[bus] = obs_audio_get_bus_volume(bus);
		if (busVolumes[bus] != initialBusVolumes[bus])
			changed = true;
	}

	if (monitorVolume != initialMonitorVolume)
		changed = true;

	if (changed) {
		auto undo_redo = [](std::array<float, MAX_AUDIO_MIXES> busVolumes, float monitorVolume) {
			for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++)
				obs_audio_set_bus_volume(bus, busVolumes[bus]);

			obs_audio_set_monitoring_volume(monitorVolume);
		};

		OBSBasic::Get()->undo_s.add_action(QTStr("Undo.BusVolume.Change"),
						   std::bind(undo_redo, initialBusVolumes, initialMonitorVolume),
						   std::bind(undo_redo, busVolumes, monitorVolume), "", "");
	}

	QDialog::done(result);
}
