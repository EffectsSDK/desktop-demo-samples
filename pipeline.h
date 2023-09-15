#ifndef PIPELINE_H
#define PIPELINE_H

#include "video_filter.h"
#include "metrics.h"

#include <QObject>
#include <QImage>

#include <atomic>
#include <thread>

class Pipeline : public QObject 
{
	Q_OBJECT
public:
	explicit Pipeline(QObject* parent = nullptr);
	~Pipeline() override;

	void setDeviceIndex(int index);
	void setDeviceIndexAndFrameSize(int index, int width, int height);

	void frameSize(int& width, int& height);
	void trySetFrameSize(int width, int height);

	void start();

	VideoFilter* videoFilter();
	Metrics* metrics();

signals:
	void frameAvailable(const QImage& frame);

private:
	void runLoop();

private:
	bool _started = false;

	std::atomic<bool> _openDeviceRequested;
	std::atomic<int> _deviceIndex;
	std::atomic<int> _frameWidth;
	std::atomic<int> _frameHeight;

	VideoFilter m_videoFilter;
	Metrics m_metrics;

	std::atomic<bool> _stopRequested;
	std::thread _loopThread;
};

#endif // CAPTURE_H
