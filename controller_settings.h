#ifndef SAMPLE_QML_CONTROLLER_SETTINGS_H
#define SAMPLE_QML_CONTROLLER_SETTINGS_H

#include <QObject>
#include <QSettings>

class SampleController;

class ControllerSettings : public QObject
{
	Q_OBJECT

public:
	explicit ControllerSettings(QObject* parent = nullptr);

	void load(SampleController& controller);
	void watch(SampleController* controller);

private slots:
	void saveNotified();

private:
	void loadProperties(QObject& object);
	void watchProperties(QObject* object);

private:
	QSettings _settings;
};

#endif
