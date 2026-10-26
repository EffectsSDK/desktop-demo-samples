#ifndef SAMPLE_QML_VIDEO_FILTER_H
#define SAMPLE_QML_VIDEO_FILTER_H

#include "sdk_library_handler.h"

#include <memory>

#ifdef __APPLE__
#include "mac/pixel_buffer.h"
#else
#include "camera/camera_capturer.h"
#endif

class VideoFilter
{
public:
	VideoFilter();
	~VideoFilter();

	bool isValid() const;

#ifdef __APPLE__
	PixelBufferPtr process(CVPixelBufferRef pixels);
#else
	// NV12 of the image's colors; null also when no effect is enabled.
	SDKUniquePtr<tsvb::IFrame> process(const camera::Nv12Image& image);
#endif

	// Held by whatever keeps a frame from process() after this filter is gone.
	std::shared_ptr<SDKLibraryHandler> sdkHandler() const;

	tsvb::IPipeline& pipeline();
	tsvb::IFrameFactory& frameFactory();

private:
	// First, so that the library outlives everything below.
	std::shared_ptr<SDKLibraryHandler> _handler;
	SDKUniquePtr<tsvb::IFrameFactory> _frameFactory;
	SDKUniquePtr<tsvb::IPipeline> _pipeline;
};

#endif
