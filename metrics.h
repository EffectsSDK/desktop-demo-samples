#ifndef METRICS_H
#define METRICS_H

#include <chrono>
#include <list>
#include <mutex>
#include <QSize>

using MetricsClock = std::chrono::steady_clock;

struct FrameTimeInfo
{
	FrameTimeInfo();

	MetricsClock::duration duration;
	MetricsClock::time_point timestamp;
	QSize size;
};

class Metrics
{
public:
	explicit Metrics();
	~Metrics() = default;

	void onFrameProcessed(const FrameTimeInfo& info);

	bool hasCameraError() const;
	void setCameraError(bool hasError);

	bool isCameraSwitch() const;
	void setCameraSwitch(bool cameraSwitch);

	MetricsClock::duration avgTimePerFrame() const;

	QSize lastFrameSize() const;

private:
	mutable std::mutex m_mutex;
	std::list<FrameTimeInfo> m_frameTimeInfoList;
	std::atomic<bool> _cameraError;
	std::atomic<bool> _cameraSwitch;

};

#endif
