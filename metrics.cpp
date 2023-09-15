#include "metrics.h"

#include <algorithm>

static const auto infoExpirationTime = std::chrono::seconds(1);

FrameTimeInfo::FrameTimeInfo()
	: duration(MetricsClock::duration::zero())
	, timestamp(MetricsClock::duration::zero())
{}

Metrics::Metrics()
{
	_cameraError = false;
	_cameraSwitch = false;
}

void Metrics::onFrameProcessed(const FrameTimeInfo& info)
{
	std::lock_guard<std::mutex> locker(m_mutex);
	auto removedBeginIter = std::remove_if(
		m_frameTimeInfoList.begin(), 
		m_frameTimeInfoList.end(),
		[&info](const FrameTimeInfo& oldInfo) { 
			return ((info.timestamp - oldInfo.timestamp) > infoExpirationTime); 
		}
	);
	m_frameTimeInfoList.erase(removedBeginIter, m_frameTimeInfoList.end());

	m_frameTimeInfoList.push_back(info);
}

bool Metrics::hasCameraError() const
{
	return _cameraError;
}

void Metrics::setCameraError(bool hasError)
{
	_cameraError = hasError;
}

bool Metrics::isCameraSwitch() const
{
	return _cameraSwitch;
}

void Metrics::setCameraSwitch(bool cameraSwitch)
{
	_cameraSwitch = cameraSwitch;
}

MetricsClock::duration Metrics::avgTimePerFrame() const
{
	std::lock_guard<std::mutex> locker(m_mutex);
	auto sum = MetricsClock::duration::zero();
	for (auto& info : m_frameTimeInfoList) {
		sum += info.duration;
	}

	return sum / std::max<size_t>(m_frameTimeInfoList.size(), 1);
}

QSize Metrics::lastFrameSize() const
{
	if (m_frameTimeInfoList.empty()) {
		return {};
	}
	return m_frameTimeInfoList.back().size;
}
