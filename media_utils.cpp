#include "media_utils.h"

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QCameraDevice>
#include <QMediaDevices>
#else
#include <QCameraInfo>
#endif

QStringList availableCameras()
{
	QStringList result;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
	for (const auto& deviceInfo : QMediaDevices::videoInputs()) {
		result << deviceInfo.description();
	}
#else
	for (const auto& deviceInfo : QCameraInfo::availableCameras()) {
		result << deviceInfo.description();
	}
#endif

	return result;
}

#ifdef Q_OS_LINUX
std::string findDevicePathByName(const QString& name)
{
	for (const auto& deviceInfo : QCameraInfo::availableCameras()) {
		if (deviceInfo.description() == name) {
			return deviceInfo.deviceName().toStdString();
		}
	}

	return std::string();
}
#else
int findDeviceIndexByName(const QString& name)
{
	int index = 0;
	for (auto& cameraName : availableCameras()) {
		if (name == cameraName) {
			return index;
		}
		index++;
	}

	return -1;
}
#endif