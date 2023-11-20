#ifndef MEDIA_UTILS_H
#define MEDIA_UTILS_H

#include <QString>
#include <QList>

QStringList availableCameras();

#ifdef Q_OS_LINUX
std::string findDevicePathByName(const QString& name);
#else
int findDeviceIndexByName(const QString& name);
#endif

#endif 
