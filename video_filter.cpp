#include "video_filter.h"

#ifdef __APPLE__
#include <vb_sdk/apple/compat_frame.h>
#include <vb_sdk/apple/compat_frame_factory.h>
#endif

VideoFilter::VideoFilter()
	: _handler(std::make_shared<SDKLibraryHandler>())
{
	if (!_handler->isValid()) {
		return;
	}

	SDKUniquePtr<tsvb::ISDKFactory> factory = _handler->createSDKFactory();
	if (nullptr == factory) {
		return;
	}

	SDKUniquePtr<tsvb::IAuthResult> authResult(factory->auth("CUSTOMER_ID", nullptr, nullptr));
	if ((nullptr == authResult) || (tsvb::AuthStatus::active != authResult->status())) {
		return;
	}

	SDKUniquePtr<tsvb::IFrameFactory> frameFactory(factory->createFrameFactory());
	SDKUniquePtr<tsvb::IPipeline> pipeline(factory->createPipeline());
	if ((nullptr == frameFactory) || (nullptr == pipeline)) {
		return;
	}

#ifdef __APPLE__
	if (!frameFactory->getInterface(tsvb::InterfaceTypeID::appleCompatFrameFactory)) {
		return;
	}
#endif

	_frameFactory = std::move(frameFactory);
	_pipeline = std::move(pipeline);
}

VideoFilter::~VideoFilter() = default;

bool VideoFilter::isValid() const
{
	return (nullptr != _pipeline);
}

#ifndef __APPLE__
SDKUniquePtr<tsvb::IFrame> VideoFilter::process(const camera::Nv12Image& image)
{
	if (!isValid()) {
		return nullptr;
	}
	// Referenced, not copied: the frame lives no longer than this call.
	SDKUniquePtr<tsvb::IFrame> input(_frameFactory->createNV12(
		const_cast<std::uint8_t*>(image.y),
		image.yBytesPerLine,
		const_cast<std::uint8_t*>(image.uv),
		image.uvBytesPerLine,
		image.width,
		image.height,
		false
	));
	if (nullptr == input) {
		return nullptr;
	}
	input->setColorRange((image.range == camera::ColorRange::full)
		? tsvb::colorRangeFull : tsvb::colorRangeVideo);
	input->setColorMatrix((image.matrix == camera::ColorMatrix::bt709)
		? tsvb::colorMatrixBT709 : tsvb::colorMatrixBT601);

	return SDKUniquePtr<tsvb::IFrame>(_pipeline->process(input.get(), nullptr));
}
#endif

#ifdef __APPLE__
PixelBufferPtr VideoFilter::process(CVPixelBufferRef pixels)
{
	if (!isValid() || !pixels) {
		return nullptr;
	}

	auto* appleFactory = reinterpret_cast<tsvb::IAppleCompatFrameFactory*>(
		_frameFactory->getInterface(tsvb::InterfaceTypeID::appleCompatFrameFactory));
	if (!appleFactory) {
		return nullptr;
	}

	// AVFoundation's buffers are Metal-compatible. The input has to outlive
	// both SDK frames.
	PixelBufferPtr retainedInput = retainPixelBuffer(pixels);
	SDKUniquePtr<tsvb::IFrame> input(
		appleFactory->createFrameWithCVPixelBuffer(retainedInput.get(), true));
	if (!input) {
		return nullptr;
	}
	SDKUniquePtr<tsvb::IFrame> output(_pipeline->process(input.get(), nullptr));
	if (!output) {
		// Includes noFeaturesEnabled: the caller displays the camera buffer.
		return nullptr;
	}
	auto* appleFrame = reinterpret_cast<tsvb::IAppleCompatFrame*>(
		output->getInterface(tsvb::InterfaceTypeID::appleCompatFrame));
	if (!appleFrame) {
		return nullptr;
	}

	// The IFrame owns the buffer toCVPixelBuffer() returns: retain it before the
	// IFrame goes. After this call the IFrame cannot go to the pipeline again.
	return retainPixelBuffer(appleFrame->toCVPixelBuffer());
}
#endif

std::shared_ptr<SDKLibraryHandler> VideoFilter::sdkHandler() const
{
	return _handler;
}

tsvb::IPipeline& VideoFilter::pipeline()
{
	return *_pipeline;
}

tsvb::IFrameFactory& VideoFilter::frameFactory()
{
	return *_frameFactory;
}
