#include "capture_loop.h"
#include "camera/camera_capturer.h"

#include <optional>
#include <utility>

namespace {
constexpr auto cameraFrameTimeout = std::chrono::milliseconds(200);
constexpr auto cameraStallTimeout = std::chrono::seconds(3);
}

CaptureLoop::CaptureLoop(QObject* parent)
	: QObject(parent)
{ }

CaptureLoop::~CaptureLoop()
{
	stop();
}

void CaptureLoop::setVideoFilter(VideoFilter* filter)
{
	_filter = filter;
}

void CaptureLoop::start(camera::Capturer& capturer)
{
	stop();
	_stopRequested = false;
	_running = true;
	_thread = std::thread([this, &capturer] { run(capturer); });
}

void CaptureLoop::stop()
{
	_stopRequested = true;
	if (_thread.joinable()) {
		_thread.join();
	}
	std::lock_guard<std::mutex> lock(_frameMutex);
	_pendingFrame = QVideoFrame();
}

bool CaptureLoop::isRunning() const
{
	return _running;
}

void CaptureLoop::run(camera::Capturer& capturer)
{
	using namespace std::chrono;
	QString failure;
	auto lastFrameAt = steady_clock::now();
	while (!_stopRequested) {
		std::unique_ptr<camera::IFrame> captured;
		try {
			captured = capturer.read(cameraFrameTimeout);
		}
		catch (const camera::Error& error) {
			failure = QString::fromUtf8(error.what());
			break;
		}
		if (!captured) {
			if (steady_clock::now() - lastFrameAt > cameraStallTimeout) {
				failure = QStringLiteral("The camera stopped sending frames.");
				break;
			}
			continue;
		}
		lastFrameAt = steady_clock::now();
		setAvailable(true);

		const std::optional<camera::Timestamp> timestamp = captured->timestamp();
		const auto processingStarted = steady_clock::now();
		QVideoFrame frame = filterFrame(std::move(captured));
		const auto processingTime = steady_clock::now() - processingStarted;
		if (!frame.isValid() || _stopRequested) {
			continue;
		}
		if (timestamp) {
			frame.setStartTime(duration_cast<microseconds>(*timestamp).count());
		}
		{
			std::lock_guard<std::mutex> lock(_countersMutex);
			++_counters.captured;
			_counters.processingTime += processingTime;
		}
		publishFrame(frame);
	}
	_running = false;
	if (!failure.isEmpty()) {
		emit errorOccurred(failure);
	}
	setAvailable(false);
}

CaptureLoop::Counters CaptureLoop::takeCounters()
{
	std::lock_guard<std::mutex> lock(_countersMutex);
	return std::exchange(_counters, Counters());
}

void CaptureLoop::setAvailable(bool available)
{
	if (_available.exchange(available) != available) {
		emit availableChanged(available);
	}
}

void CaptureLoop::publishFrame(const QVideoFrame& frame)
{
	{
		std::lock_guard<std::mutex> lock(_frameMutex);
		_pendingFrame = frame;
		if (_deliveryPending) {
			return;
		}
		_deliveryPending = true;
	}
	QMetaObject::invokeMethod(this, [this] {
		QVideoFrame latest;
		{
			std::lock_guard<std::mutex> lock(_frameMutex);
			latest = std::exchange(_pendingFrame, QVideoFrame());
			_deliveryPending = false;
		}
		if (latest.isValid()) {
			emit frameReady(latest);
		}
	}, Qt::QueuedConnection);
}
