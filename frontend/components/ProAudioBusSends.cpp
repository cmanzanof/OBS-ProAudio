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

#include "ProAudioBusSends.hpp"

#include "ProAudioBuses.hpp"

#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QDialogButtonBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <cmath>
#include <functional>

#include "moc_ProAudioBusSends.cpp"

ProAudioBusSends::ProAudioBusSends(obs_source_t *source_, QWidget *parent) : source(source_), QDialog(parent)
{
	setWindowTitle(QTStr("Basic.ProAudio.BusSends").arg(QT_UTF8(obs_source_get_name(source_))));
	setMinimumWidth(420);

	QVBoxLayout *mainLayout = new QVBoxLayout(this);
	QGridLayout *grid = new QGridLayout();

	grid->setHorizontalSpacing(12);
	grid->setColumnStretch(1, 1);

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		float send = obs_source_get_bus_send(source_, bus);
		QString busName = proAudioBusName(bus);

		initialSends[bus] = send;

		QLabel *name = new QLabel(proAudioBusShortName(bus), this);
		name->setToolTip(busName);
		name->setMinimumWidth(28);

		QSlider *slider = new QSlider(Qt::Horizontal, this);
		slider->setRange(PROAUDIO_FADER_MIN_DB10, PROAUDIO_FADER_MAX_DB10);
		slider->setValue(proAudioVolumeToFader(send));
		slider->setToolTip(QTStr("Basic.ProAudio.BusSends.Tooltip").arg(busName));
		slider->setAccessibleName(slider->toolTip());

		QLabel *value = new QLabel(proAudioFaderText(slider->value()), this);
		value->setMinimumWidth(64);
		value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

		connect(slider, &QSlider::valueChanged, this, [this, bus, value](int deflection) {
			value->setText(proAudioFaderText(deflection));
			setBusSend(bus, proAudioFaderToVolume(deflection));
		});

		grid->addWidget(name, (int)bus, 0);
		grid->addWidget(slider, (int)bus, 1);
		grid->addWidget(value, (int)bus, 2);

		sliders[bus] = slider;
	}

	mainLayout->addLayout(grid);

	QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
	QPushButton *resetButton = buttons->addButton(QTStr("Basic.ProAudio.BusSends.Reset"), QDialogButtonBox::ResetRole);

	connect(resetButton, &QPushButton::clicked, this, [this]() { resetSends(); });
	connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

	mainLayout->addWidget(buttons);
}

void ProAudioBusSends::setBusSend(size_t bus, float volume)
{
	obs_source_set_bus_send(source, bus, volume);
}

void ProAudioBusSends::resetSends()
{
	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		/* Moving the fader applies the send through valueChanged(). */
		if (sliders[bus]->value() != PROAUDIO_FADER_UNITY_DB10)
			sliders[bus]->setValue(PROAUDIO_FADER_UNITY_DB10);
	}
}

void ProAudioBusSends::done(int result)
{
	/* The whole dialog is a single undo step. */
	std::array<float, MAX_AUDIO_MIXES> sends{};
	bool changed = false;

	for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++) {
		sends[bus] = obs_source_get_bus_send(source, bus);
		if (sends[bus] != initialSends[bus])
			changed = true;
	}

	if (changed) {
		std::string uuid = obs_source_get_uuid(source);
		const char *name = obs_source_get_name(source);

		auto undo_redo = [](const std::string &uuid, std::array<float, MAX_AUDIO_MIXES> sends) {
			OBSSourceAutoRelease source = obs_get_source_by_uuid(uuid.c_str());

			if (!source)
				return;

			for (size_t bus = 0; bus < MAX_AUDIO_MIXES; bus++)
				obs_source_set_bus_send(source, bus, sends[bus]);
		};

		OBSBasic::Get()->undo_s.add_action(QTStr("Undo.BusSends.Change").arg(name),
						   std::bind(undo_redo, std::placeholders::_1, initialSends),
						   std::bind(undo_redo, std::placeholders::_1, sends), uuid, uuid);
	}

	QDialog::done(result);
}
