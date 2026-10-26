#include "camera_controller.h"

#include "camera/camera_capturer.h"

#include <QCameraDevice>
#include <QCoreApplication>
#include <QMediaDevices>
#include <QPermissions>

#include <utility>

namespace {
constexpr int metricsUpdateIntervalMs = 1000;
}

CameraController::CameraController(VideoFilter* filter, QObject* parent)
	: QObject(parent)
	, _mediaDevices(new QMediaDevices(this))
{
	_loop.setVideoFilter(filter);

	connect(_mediaDevices, &QMediaDevices::videoInputsChanged, this, &CameraController::updateCameras);

	connect(&_loop, &CaptureLoop::errorOccurred, this, [this](const QString& message) {
		if (_loop.isRunning()) {
			return;
		}
		closeCamera();
		emit errorOccurred(message);
	});

	connect(&_loop, &CaptureLoop::availableChanged, this, [this](bool available) {
		if (_cameraAvailable == available) {
			return;
		}
		_cameraAvailable = available;
		if (!available) {
			_fps = 0;
			_frameTimeMs = 0;
			emit metricsChanged();
		}
		emit cameraAvailableChanged();
	});

	connect(&_loop, &CaptureLoop::frameReady, this, [this](const QVideoFrame& frame) {
		if (nullptr != _videoSink) {
			_videoSink->setVideoFrame(frame);
		}
		if (_frameSize != frame.size()) {
			_frameSize = frame.size();
			emit frameSizeChanged();
		}
	});

	_metricsTimer.setInterval(metricsUpdateIntervalMs);
	connect(&_metricsTimer, &QTimer::timeout, this, &CameraController::updateMetrics);

	updateCameras();
}

CameraController::~CameraController()
{
	_loop.stop();
}

QStringList CameraController::cameras() const
{
	return _cameras;
}

int CameraController::cameraIndex() const
{
	return _cameraIndex;
}

QString CameraController::cameraId() const
{
	if (!_preferredCameraId.isEmpty()) {
		return QString::fromUtf8(_preferredCameraId);
	}
	return QString::fromUtf8(currentCameraId());
}

void CameraController::setCameraId(const QString& id)
{
	_preferredCameraId = id.toUtf8();

	const int index = _cameraIds.indexOf(_preferredCameraId);
	if (index >= 0) {
		_cameraIndex = index;
		updateFrameRates();
		applyCamera();
	}
	emit cameraIndexChanged();
}

void CameraController::setCameraIndex(int index)
{
	if ((index >= 0) && (index < _cameras.size())) {
		_preferredCameraId = _cameraIds[index];
		_cameraIndex = index;
		updateFrameRates();
		applyCamera();
	}
	emit cameraIndexChanged();
}

QSize CameraController::resolution() const
{
	return _resolution;
}

void CameraController::setResolution(QSize resolution)
{
	if (resolution != _resolution) {
		_resolution = resolution;
		updateFrameRates();
		applyFormat();
	}
	emit resolutionChanged();
}

int CameraController::frameRate() const
{
	return _frameRate;
}

void CameraController::setFrameRate(int fps)
{
	if ((fps > 0) && (fps != _frameRate)) {
		_frameRate = fps;
		applyFormat();
		emit frameRatesChanged();
	}
	emit frameRateChanged();
}

QList<int> CameraController::frameRates() const
{
	return _frameRates;
}

int CameraController::activeFrameRate() const
{
	return camera::nearestFrameRate(
		std::vector<int>(_frameRates.begin(), _frameRates.end()), _frameRate);
}

QVideoSink* CameraController::videoSink() const
{
	return _videoSink;
}

void CameraController::setVideoSink(QVideoSink* sink)
{
	if (sink == _videoSink) {
		return;
	}

	_videoSink = sink;
	emit videoSinkChanged();
}

bool CameraController::cameraAvailable() const
{
	return _cameraAvailable;
}

double CameraController::fps() const
{
	return _fps;
}

double CameraController::frameTimeMs() const
{
	return _frameTimeMs;
}

QSize CameraController::frameSize() const
{
	return _frameSize;
}

void CameraController::start()
{
	_started = true;
	applyCamera();

	_loop.takeCounters();
	_metricsClock.start();
	_metricsTimer.start();
}

void CameraController::updateCameras()
{
	const QByteArray currentId = currentCameraId();

	_cameras.clear();
	_cameraIds.clear();
	for (const QCameraDevice& device : QMediaDevices::videoInputs()) {
		_cameras << device.description();
		_cameraIds << device.id();
	}

	int index = _cameraIds.indexOf(_preferredCameraId);
	if (index < 0) {
		index = _cameraIds.indexOf(currentId);
	}
	const int newIndex = (index >= 0) ? index : 0;
	const bool indexChanged = (newIndex != _cameraIndex);
	_cameraIndex = newIndex;

	emit camerasChanged();
	if (indexChanged) {
		emit cameraIndexChanged();
	}
	updateFrameRates();
	applyCamera();
}

QByteArray CameraController::currentCameraId() const
{
	return (_cameraIndex < _cameraIds.size()) ? _cameraIds[_cameraIndex] : QByteArray();
}

void CameraController::updateFrameRates()
{
	const QByteArray id = currentCameraId();
	const std::vector<int> rates = id.isEmpty()
		? std::vector<int>()
		: camera::frameRates(id.toStdString(), _resolution.width(), _resolution.height());
	const QList<int> frameRates(rates.begin(), rates.end());
	if (frameRates != _frameRates) {
		_frameRates = frameRates;
		emit frameRatesChanged();
	}
}

void CameraController::applyFormat()
{
	if (_capturer && _loop.isRunning()) {
		_capturer->setFormat(_resolution.width(), _resolution.height(), _frameRate);
	}
}

void CameraController::applyCamera()
{
	if (!_started) {
		return;
	}

	const QByteArray id = currentCameraId();
	if (id.isEmpty()) {
		closeCamera();
		return;
	}
	if (_capturer && _loop.isRunning() && (_capturer->uniqueId() == id.toStdString())) {
		return;
	}

	if (qApp->checkPermission(QCameraPermission()) == Qt::PermissionStatus::Undetermined) {
		if (!_accessRequested) {
			_accessRequested = true;
			qApp->requestPermission(QCameraPermission(), this, [this](const QPermission&) {
				updateFrameRates();
				applyCamera();
			});
		}
		return;
	}

	if (_capturer && !_loop.isRunning()) {
		closeCamera();
	}

	std::unique_ptr<camera::Capturer> next;
	try {
		next = std::make_unique<camera::Capturer>(
			id.toStdString(), _resolution.width(), _resolution.height(), _frameRate);
	}
	catch (const camera::Error& error) {
		if (_capturer && _loop.isRunning()) {
			const int index = _cameraIds.indexOf(QByteArray::fromStdString(_capturer->uniqueId()));
			if (index >= 0) {
				_cameraIndex = index;
				updateFrameRates();
				emit cameraIndexChanged();
			}
		}
		emit errorOccurred(QString::fromUtf8(error.what()));
		return;
	}

	_loop.stop();
	_capturer = std::move(next);
	_loop.start(*_capturer);
}

void CameraController::closeCamera()
{
	_loop.stop();
	_capturer.reset();
}

void CameraController::updateMetrics()
{
	const qint64 elapsedMs = _metricsClock.restart();
	const CaptureLoop::Counters counters = _loop.takeCounters();

	_fps = (elapsedMs > 0)
		? (double(counters.captured) * 1000.0 / double(elapsedMs))
		: 0.0;
	_frameTimeMs = (counters.captured > 0)
		? (std::chrono::duration<double, std::milli>(counters.processingTime).count() / counters.captured)
		: 0.0;
	emit metricsChanged();
}
