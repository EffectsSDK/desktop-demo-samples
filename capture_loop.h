#ifndef SAMPLE_QML_CAPTURE_LOOP_H
#define SAMPLE_QML_CAPTURE_LOOP_H

#include <QObject>
#include <QVideoFrame>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <thread>

#ifdef __APPLE__
#include "mac/pixel_buffer_video_frame.h"
#endif

class VideoFilter;

namespace camera {
class Capturer;
class IFrame;
}

class CaptureLoop : public QObject
{
	Q_OBJECT

public:
	explicit CaptureLoop(QObject* parent = nullptr);
	~CaptureLoop() override;

	void setVideoFilter(VideoFilter* filter);

	void start(camera::Capturer& capturer);
	void stop();
	bool isRunning() const;

	struct Counters
	{
		int captured = 0;
		std::chrono::nanoseconds processingTime{ 0 };
	};

	Counters takeCounters();

signals:
	void frameReady(const QVideoFrame& frame);
	void availableChanged(bool available);
	void errorOccurred(const QString& message);

private:
	void run(camera::Capturer& capturer);
	QVideoFrame filterFrame(std::unique_ptr<camera::IFrame> captured);
	void setAvailable(bool available);
	void publishFrame(const QVideoFrame& frame);

private:
	std::atomic<bool> _running = false;
	std::atomic<bool> _stopRequested = false;
	std::atomic<bool> _available = false;
	std::mutex _countersMutex;
	Counters _counters;
	std::mutex _frameMutex;
	QVideoFrame _pendingFrame;
	bool _deliveryPending = false;

	VideoFilter* _filter = nullptr;
#ifdef __APPLE__
	PixelBufferBridge _pixelBufferBridge;
#endif

	std::thread _thread;
};

#endif
