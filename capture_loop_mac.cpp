#include "capture_loop.h"
#include "video_filter.h"
#include "camera/camera_capturer.h"
#include "mac/pixel_buffer_video_frame.h"

#include <utility>

QVideoFrame CaptureLoop::filterFrame(std::unique_ptr<camera::IFrame> captured)
{
	QVideoFrame frame;
	if (_filter) {
		if (PixelBufferPtr processed = _filter->process(captured->pixelBuffer())) {
			frame = _pixelBufferBridge.pixelBufferToVideoFrame(std::move(processed));
		}
	}
	if (!frame.isValid()) {
		frame = _pixelBufferBridge.pixelBufferToVideoFrame(retainPixelBuffer(captured->pixelBuffer()));
	}
	return frame;
}
