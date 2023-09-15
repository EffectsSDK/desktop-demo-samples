#include "frame_view.h"

FrameView::FrameView(QWidget* parent)
	: QWidget(parent)
{ }

void FrameView::present(const QImage& image)
{
	_image = image;
	update();
}

void FrameView::paintEvent(QPaintEvent* event)
{
	QSize dstSize = _image.size().scaled(contentsRect().size(), Qt::KeepAspectRatio);
	QRect dstRect(QPoint(0, 0), dstSize);
	dstRect.moveCenter(contentsRect().center());

	QPainter painter(this);
	painter.setRenderHint(QPainter::SmoothPixmapTransform);
	painter.drawImage(dstRect, _image);
}