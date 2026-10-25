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

#include "ProAudioBusOutputs.hpp"

#include "ProAudioBuses.hpp"

#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QComboBox>

namespace {

/* The bus letter is part of the key (BusDeviceA, BusDeviceB...) so that the
 * profile stays readable and does not depend on the order of the buses. */
QByteArray busDeviceKey(size_t bus)
{
	return QStringLiteral("BusDevice%1").arg(proAudioBusShortName(bus)).toUtf8();
}

QByteArray busDeviceNameKey(size_t bus)
{
	return QStringLiteral("BusDeviceName%1").arg(proAudioBusShortName(bus)).toUtf8();
}

QString configString(config_t *config, const QByteArray &key)
{
	const char *value = config ? config_get_string(config, "ProAudio", key.constData()) : nullptr;

	return value ? QString::fromUtf8(value) : QString();
}

struct EnumData {
	QComboBox *combo;
	QString selected;
	bool found;
};

bool enumDevice(void *param, const char *name, const char *id)
{
	EnumData *data = static_cast<EnumData *>(param);
	QString deviceId = QString::fromUtf8(id);

	data->combo->addItem(QString::fromUtf8(name), deviceId);

	if (deviceId == data->selected)
		data->found = true;

	return true;
}

} // namespace

bool proAudioBusOutputsAvailable()
{
	return obs_audio_bus_outputs_available();
}

void proAudioLoadBusOutputs()
{
	OBSBasic *main = OBSBasic::Get();

	if (!main || !proAudioBusOutputsAvailable())
		return;

	config_t *config = main->Config();

	for (size_t bus = 1; bus < MAX_AUDIO_MIXES; bus++) {
		const QString id = configString(config, busDeviceKey(bus));
		QByteArray utf8 = id.toUtf8();

		/* Also clears a bus that was enabled by the previous profile. */
		obs_audio_set_bus_device(bus, id.isEmpty() ? nullptr : utf8.constData());
	}
}

void proAudioFillBusOutputCombo(QComboBox *combo, size_t bus)
{
	OBSBasic *main = OBSBasic::Get();
	config_t *config = main ? main->Config() : nullptr;

	const QString savedId = configString(config, busDeviceKey(bus));
	const QString savedName = configString(config, busDeviceNameKey(bus));

	combo->clear();
	combo->addItem(QTStr("Basic.ProAudio.BusOutput.None"), QString());
	combo->addItem(QTStr("Basic.Settings.Advanced.Audio.MonitoringDevice.Default"), QStringLiteral("default"));

	/* "default" always resolves, so it does not need an entry of its own. */
	EnumData data{combo, savedId, savedId.isEmpty() || savedId == QStringLiteral("default")};
	obs_enum_audio_monitoring_devices(enumDevice, &data);

	/* A device that was unplugged between sessions is still the device the
	 * bus is routed to, so it is shown instead of silently falling back to
	 * "no output". */
	if (!savedId.isEmpty() && !data.found) {
		QString label = savedName.isEmpty() ? savedId : savedName;
		combo->addItem(QTStr("Basic.ProAudio.BusOutput.Unavailable").arg(label), savedId);
	}

	int index = combo->findData(savedId);
	combo->setCurrentIndex(index >= 0 ? index : 0);
}

void proAudioApplyBusOutput(size_t bus, const QString &id, const QString &name)
{
	OBSBasic *main = OBSBasic::Get();
	config_t *config = main ? main->Config() : nullptr;
	const bool enabled = !id.isEmpty();
	QByteArray idUtf8 = id.toUtf8();

	bool success = obs_audio_set_bus_device(bus, enabled ? idUtf8.constData() : nullptr);

	if (config) {
		QByteArray idKey = busDeviceKey(bus);
		QByteArray nameKey = busDeviceNameKey(bus);

		if (enabled) {
			QByteArray nameUtf8 = name.toUtf8();

			config_set_string(config, "ProAudio", idKey.constData(), idUtf8.constData());
			config_set_string(config, "ProAudio", nameKey.constData(), nameUtf8.constData());
		} else {
			config_remove_value(config, "ProAudio", idKey.constData());
			config_remove_value(config, "ProAudio", nameKey.constData());
		}

		config_save_safe(config, "tmp", nullptr);
	}

	/* The routing is saved even when the stream could not be opened, so the
	 * user can, for example, plug the interface back in. */
	if (enabled && !success && main) {
		OBSMessageBox::warning(main, QTStr("Basic.ProAudio.BusOutput.ErrorTitle"),
				       QTStr("Basic.ProAudio.BusOutput.Error")
					       .arg(proAudioBusName(bus), name.isEmpty() ? id : name));
	}
}
