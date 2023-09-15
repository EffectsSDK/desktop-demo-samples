#ifndef FRAME_VIEW_H
#define FRAME_VIEW_H

#include <QtWidgets>

class FrameView : public QWidget 
{
	Q_OBJECT
public:
	explicit FrameView(QWidget* parent = nullptr);

	void present(const QImage& image);

protected:
	void paintEvent(QPaintEvent* event) override;

private:
	QImage _image;
};

#endif 
