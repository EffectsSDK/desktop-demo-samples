#include "controller_settings.h"

#include "sample_controller.h"

#include <QCoreApplication>
#include <QDir>
#include <QLoggingCategory>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QStandardPaths>

namespace {

Q_LOGGING_CATEGORY(settingsLog, "sample.settings")

QList<QMetaProperty> storedProperties(const QMetaObject& meta)
{
	QList<QMetaProperty> properties;
	for (int i = meta.propertyOffset(); i < meta.propertyCount(); ++i) {
		const QMetaProperty property = meta.property(i);
		if (property.isWritable() && property.hasNotifySignal() && property.isStored()) {
			properties << property;
		}
	}
	return properties;
}

QVariant toStored(const QMetaProperty& property, const QVariant& value)
{
	if (property.isEnumType()) {
		return QString::fromLatin1(property.enumerator().valueToKey(value.toInt()));
	}
	if (value.metaType() == QMetaType::fromType<QUrl>()) {
		return value.toUrl().toString();
	}
	return value;
}

QVariant fromStored(const QMetaProperty& property, const QVariant& stored)
{
	if (property.isEnumType()) {
		bool ok = false;
		const int value = property.enumerator().keyToValue(stored.toString().toLatin1().constData(), &ok);
		return ok ? QVariant(value) : QVariant();
	}

	QVariant value = stored;
	return value.convert(property.metaType()) ? value : QVariant();
}

QString settingsFilePath()
{
	const QString fileName = QCoreApplication::applicationName() + QStringLiteral(".ini");

#ifdef Q_OS_MACOS
	return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation)).filePath(fileName);
#else
	return QDir(QCoreApplication::applicationDirPath()).filePath(fileName);
#endif
}

}

ControllerSettings::ControllerSettings(QObject* parent)
	: QObject(parent)
	, _settings(settingsFilePath(), QSettings::IniFormat)
{ }

void ControllerSettings::load(SampleController& controller)
{
	const QMetaObject::Connection errors = connect(
		&controller, &SampleController::errorOccurred,
		this, [](const QString& message) {
			qCWarning(settingsLog) << "Not restored:" << message;
		}
	);

	loadProperties(controller);
	loadProperties(*controller.camera());

	disconnect(errors);
}

void ControllerSettings::watch(SampleController* controller)
{
	watchProperties(controller);
	watchProperties(controller->camera());
}

void ControllerSettings::loadProperties(QObject& object)
{
	for (const QMetaProperty& property : storedProperties(*object.metaObject())) {
		const QString name = QString::fromLatin1(property.name());
		if (!_settings.contains(name)) {
			continue;
		}

		const QVariant stored = _settings.value(name);
		const QVariant value = fromStored(property, stored);
		if (!value.isValid() || !property.write(&object, value)) {
			qCWarning(settingsLog) << "Ignored" << name << "=" << stored;
		}
	}
}

void ControllerSettings::watchProperties(QObject* object)
{
	const QMetaMethod slot = metaObject()->method(metaObject()->indexOfSlot("saveNotified()"));

	for (const QMetaProperty& property : storedProperties(*object->metaObject())) {
		connect(object, property.notifySignal(), this, slot, Qt::UniqueConnection);
	}
}

void ControllerSettings::saveNotified()
{
	QObject* object = sender();
	const int signal = senderSignalIndex();
	if ((nullptr == object) || (signal < 0)) {
		return;
	}

	for (const QMetaProperty& property : storedProperties(*object->metaObject())) {
		if (property.notifySignalIndex() == signal) {
			_settings.setValue(
				QString::fromLatin1(property.name()),
				toStored(property, property.read(object))
			);
		}
	}
}
