#ifndef METRICS_VIEW_H
#define METRICS_VIEW_H

#include "metrics.h"

#include <QtWidgets>

class MetricsView : public QWidget
{
	Q_OBJECT
public:
	MetricsView();

	void update(const MetricsClock::duration& avgDuration, const QSize& size);
	void setCameraSwitch();
	void setCameraError();

private:
	QLabel* m_avgTimePerFrame = nullptr;
};

#endif
