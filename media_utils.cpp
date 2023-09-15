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
