#ifndef SAMPLE_QML_CAMERA_CONTROLLER_H
#define SAMPLE_QML_CAMERA_CONTROLLER_H

#include "capture_loop.h"

#include <QByteArray>
#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QSize>
#include <QStringList>
#include <QTimer>
#include <QVideoSink>
#include <QtQml/qqmlregistration.h>

#include <memory>

class QMediaDevices;
class VideoFilter;

namespace camera {
class Capturer;
}

class CameraController : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Owned by SampleController.")

	Q_PROPERTY(QStringList cameras READ cameras NOTIFY camerasChanged)
	Q_PROPERTY(int cameraIndex READ cameraIndex WRITE setCameraIndex NOTIFY cameraIndexChanged STORED false)
	Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId NOTIFY cameraIndexChanged)
	Q_PROPERTY(QSize resolution READ resolution WRITE setResolution NOTIFY resolutionChanged)
	Q_PROPERTY(int frameRate READ frameRate WRITE setFrameRate NOTIFY frameRateChanged)
	Q_PROPERTY(QList<int> frameRates READ frameRates NOTIFY frameRatesChanged)
	Q_PROPERTY(int activeFrameRate READ activeFrameRate NOTIFY frameRatesChanged)
	Q_PROPERTY(QVideoSink* videoSink READ videoSink WRITE setVideoSink NOTIFY videoSinkChanged STORED false)

	Q_PROPERTY(bool cameraAvailable READ cameraAvailable NOTIFY cameraAvailableChanged)
	Q_PROPERTY(double fps READ fps NOTIFY metricsChanged)
	Q_PROPERTY(double frameTimeMs READ frameTimeMs NOTIFY metricsChanged)
	Q_PROPERTY(QSize frameSize READ frameSize NOTIFY frameSizeChanged)

public:
	explicit CameraController(VideoFilter* filter, QObject* parent = nullptr);
	~CameraController() override;

	QStringList cameras() const;

	int cameraIndex() const;
	void setCameraIndex(int index);
	QString cameraId() const;
	void setCameraId(const QString& id);

	QSize resolution() const;
	void setResolution(QSize resolution);

	int frameRate() const;
	void setFrameRate(int fps);
	QList<int> frameRates() const;
	int activeFrameRate() const;

	QVideoSink* videoSink() const;
	void setVideoSink(QVideoSink* sink);

	bool cameraAvailable() const;
	double fps() const;
	double frameTimeMs() const;
	QSize frameSize() const;

	void start();

signals:
	void camerasChanged();
	void cameraIndexChanged();
	void resolutionChanged();
	void frameRateChanged();
	void frameRatesChanged();
	void videoSinkChanged();
	void cameraAvailableChanged();
	void metricsChanged();
	void frameSizeChanged();

	void errorOccurred(const QString& message);

private:
	void updateCameras();
	QByteArray currentCameraId() const;
	void updateFrameRates();
	void applyFormat();
	void applyCamera();
	void closeCamera();
	void updateMetrics();

private:
	std::unique_ptr<camera::Capturer> _capturer;
	CaptureLoop _loop;
	QMediaDevices* _mediaDevices = nullptr;
	bool _started = false;
	bool _accessRequested = false;

	QStringList _cameras;
	QList<QByteArray> _cameraIds;
	int _cameraIndex = 0;
	QByteArray _preferredCameraId;
	QSize _resolution = QSize(1280, 720);
	int _frameRate = 30;
	QList<int> _frameRates;
	QPointer<QVideoSink> _videoSink;

	bool _cameraAvailable = false;
	double _fps = 0;
	double _frameTimeMs = 0;
	QSize _frameSize;

	QTimer _metricsTimer;
	QElapsedTimer _metricsClock;
};

#endif
