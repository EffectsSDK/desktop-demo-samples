#ifndef SAMPLE_QML_PIXEL_BUFFER_VIDEO_FRAME_H
#define SAMPLE_QML_PIXEL_BUFFER_VIDEO_FRAME_H

#include "pixel_buffer.h"

#include <QVideoFrame>

#include <memory>

class CurrentMetalTextureCache;

class PixelBufferBridge
{
public:
	PixelBufferBridge();

	QVideoFrame pixelBufferToVideoFrame(PixelBufferPtr pixels);

private:
	std::shared_ptr<CurrentMetalTextureCache> _currentCache;
};

#endif
