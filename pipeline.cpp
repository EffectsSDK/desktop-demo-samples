#include "pipeline.h"

#include <opencv2/opencv.hpp>

Pipeline::Pipeline(QObject* parent)
	: QObject(parent)
	, _stopRequested(false)
	, _openDeviceRequested(true)
	, _deviceIndex(0)
	, _frameWidth(1280)
	, _frameHeight(720)
{
#ifdef  Q_OS_WINDOWS
	bool notDefined = qgetenv("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS").isEmpty();
	if (notDefined) {
		// MS Media Foundation hardware transforms in some cases significantly increases camera initialization time.
		qputenv("OPENCV_VIDEOIO_MSMF_ENABLE_HW_TRANSFORMS", "0");
	}
#endif
}

Pipeline::~Pipeline()
{
	_stopRequested = true;
	if (_loopThread.joinable()) {
		_loopThread.join();
	}
}

void Pipeline::setDeviceIndex(int index)
{
	if (index == _deviceIndex) {
		return;
	}

	_deviceIndex = index;
	_openDeviceRequested = true;
}

void Pipeline::setDeviceIndexAndFrameSize(int index, int width, int height)
{
	if (index == _deviceIndex && _frameWidth == width && _frameHeight == height) {
		return;
	}

	_deviceIndex = index;
	_frameWidth = width;
	_frameHeight = height;
	_openDeviceRequested = true;
}

void Pipeline::frameSize(int& width, int& height)
{
	width = _frameWidth;
	height = _frameHeight;
}

void Pipeline::trySetFrameSize(int width, int height)
{
	if (_frameWidth == width && _frameHeight == height) {
		return;
	}

	_frameWidth = width;
	_frameHeight = height;
	_openDeviceRequested = true;
}

void Pipeline::start()
{
	if (_started) {
		return;
	}

	_loopThread = std::thread([this] { runLoop(); });
	_started = true;
}

VideoFilter* Pipeline::videoFilter()
{
	return &m_videoFilter;
}

Metrics* Pipeline::metrics()
{
	return &m_metrics;
}

void Pipeline::runLoop()
{
	cv::Mat readMat;
	cv::VideoCapture capturer;
	QImage cameraFrame;

	while (!_stopRequested) {
		if (_openDeviceRequested.exchange(false)) {
			m_metrics.setCameraSwitch(true);
			capturer.release();
			capturer.open(_deviceIndex);			
			#if (CV_MAJOR_VERSION >= 3)
			capturer.set(cv::CAP_PROP_FRAME_WIDTH, _frameWidth);
			capturer.set(cv::CAP_PROP_FRAME_HEIGHT, _frameHeight);
			#else
			capturer.set(CV_CAP_PROP_FRAME_WIDTH, _frameWidth);
			capturer.set(CV_CAP_PROP_FRAME_HEIGHT, _frameHeight);
			#endif

			m_metrics.setCameraSwitch(false);
			m_metrics.setCameraError(false);
		}

		if (!capturer.read(readMat)) {
			m_metrics.setCameraError(true);
			continue;
		}
		m_metrics.setCameraError(false);

		if ((cameraFrame.width() != readMat.cols) || (cameraFrame.height() != readMat.rows)) {
			cameraFrame = QImage(readMat.cols, readMat.rows, QImage::Format_ARGB32);
		}
		cv::Mat cameraFrameMat(
			cameraFrame.height(),
			cameraFrame.width(),
			CV_8UC4,
			cameraFrame.bits(),
			cameraFrame.bytesPerLine()
		);
		cv::cvtColor(readMat, cameraFrameMat, cv::COLOR_BGR2BGRA);
		auto replaceBeginTime = MetricsClock::now();
		QImage result = m_videoFilter.replaceBG(cameraFrame);
		auto replaceEndTime = MetricsClock::now();

		FrameTimeInfo frameTimeInfo;
		frameTimeInfo.duration = (replaceEndTime - replaceBeginTime);
		frameTimeInfo.timestamp = replaceEndTime;
		frameTimeInfo.size = !result.isNull() ? result.size() : cameraFrame.size();
		m_metrics.onFrameProcessed(frameTimeInfo);

		if (!result.isNull()) {
			emit frameAvailable(result);
		}
		else {
			emit frameAvailable(cameraFrame);
		}
	}

	capturer.release();
}