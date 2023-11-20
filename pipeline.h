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
	void setMediaPath(const std::string& path);

	void getFrameSize(int& width, int& height);
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
	std::mutex _mutex;
	int _deviceIndex;
	std::string _mediaPath;
	int _frameWidth;
	int _frameHeight;

	VideoFilter m_videoFilter;
	Metrics m_metrics;

	std::atomic<bool> _stopRequested;
	std::thread _loopThread;
};

#endif // CAPTURE_H
