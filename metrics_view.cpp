#include "metrics_view.h"

MetricsView::MetricsView()
{
	auto palette = this->palette();
	QColor bgColor = Qt::black;
	bgColor.setAlphaF(0.5);
	palette.setColor(QPalette::Window, bgColor);
	this->setPalette(palette);
	setAutoFillBackground(true);

	m_avgTimePerFrame = new QLabel(this);
	QFont font = m_avgTimePerFrame->font();
	font.setPointSizeF(14);
	m_avgTimePerFrame->setFont(font);
	palette = m_avgTimePerFrame->palette();
	palette.setColor(QPalette::WindowText, Qt::white);
	m_avgTimePerFrame->setPalette(palette);

	auto layout = new QVBoxLayout(this);
	layout->setContentsMargins(5, 3, 5, 3);
	layout->addWidget(m_avgTimePerFrame);
}

void MetricsView::update(const MetricsClock::duration& avgDuration, const QSize& size)
{
	auto microsecondsPerFrame = 
		std::chrono::duration_cast<std::chrono::microseconds>(avgDuration);
	auto milisecondsPerFrame = double(microsecondsPerFrame.count()) / 1000;

	auto text = QString("%1 ms per frame").arg(milisecondsPerFrame, 0, 'g', 3);
	if (size.isValid()) {
		text += QString(", last frame size: %1x%2").arg(size.width()).arg(size.height());
	}
	m_avgTimePerFrame->setText(text);
}

void MetricsView::setCameraSwitch()
{
	m_avgTimePerFrame->setText("Switch camera");
}

void MetricsView::setCameraError()
{
	m_avgTimePerFrame->setText("Camera error");
}
