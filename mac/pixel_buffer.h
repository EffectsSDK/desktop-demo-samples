#ifndef SAMPLE_QML_PIXEL_BUFFER_H
#define SAMPLE_QML_PIXEL_BUFFER_H

#include <CoreVideo/CoreVideo.h>

#include <memory>
#include <type_traits>

struct PixelBufferReleaser
{
	void operator()(CVPixelBufferRef buffer) const
	{
		CVPixelBufferRelease(buffer);
	}
};

using PixelBufferPtr = std::unique_ptr<
	std::remove_pointer_t<CVPixelBufferRef>, 
	PixelBufferReleaser
>;

inline PixelBufferPtr retainPixelBuffer(CVPixelBufferRef buffer)
{
	return PixelBufferPtr(buffer ? CVPixelBufferRetain(buffer) : nullptr);
}

#endif
