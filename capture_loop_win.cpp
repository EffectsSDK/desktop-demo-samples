#include "capture_loop.h"
#include "video_filter.h"
#include "camera/camera_capturer.h"

#include <QAbstractVideoBuffer>
#include <QVideoFrameFormat>

#include <memory>
#include <utility>

namespace {

QVideoFrameFormat nv12Format(int width, int height, bool fullRange, bool bt709)
{
	QVideoFrameFormat format(QSize(width, height), QVideoFrameFormat::Format_NV12);
	format.setColorRange(fullRange ? QVideoFrameFormat::ColorRange_Full : QVideoFrameFormat::ColorRange_Video);
	format.setColorSpace(bt709 ? QVideoFrameFormat::ColorSpace_BT709 : QVideoFrameFormat::ColorSpace_BT601);
	return format;
}

QAbstractVideoBuffer::MapData nv12MapData(uchar* y, int yBytesPerLine, uchar* uv, int uvBytesPerLine, int height)
{
	QAbstractVideoBuffer::MapData data;
	data.planeCount = 2;
	data.data[0] = y;
	data.bytesPerLine[0] = yBytesPerLine;
	data.dataSize[0] = yBytesPerLine * height;
	data.data[1] = uv;
	data.bytesPerLine[1] = uvBytesPerLine;
	data.dataSize[1] = uvBytesPerLine * ((height + 1) / 2);
	return data;
}

class CameraBuffer : public QAbstractVideoBuffer
{
public:
	explicit CameraBuffer(std::unique_ptr<camera::IFrame> frame)
		: _frame(std::move(frame))
	{ }

	QVideoFrameFormat format() const override
	{
		const camera::Nv12Image& image = _frame->image();
		return nv12Format(
			image.width,
			image.height,
			image.range == camera::ColorRange::full,
			image.matrix == camera::ColorMatrix::bt709
		);
	}

	MapData map(QVideoFrame::MapMode mode) override
	{
		if (mode != QVideoFrame::ReadOnly) {
			return {};
		}
		const camera::Nv12Image& image = _frame->image();
		return nv12MapData(
			const_cast<uchar*>(image.y),
			image.yBytesPerLine,
			const_cast<uchar*>(image.uv),
			image.uvBytesPerLine,
			image.height
		);
	}

private:
	const std::unique_ptr<camera::IFrame> _frame;
};

// A frame of the SDK, locked while Qt maps it.
class SDKBuffer : public QAbstractVideoBuffer
{
public:
	SDKBuffer(std::shared_ptr<SDKLibraryHandler> sdkHandler, SDKUniquePtr<tsvb::IFrame> frame)
		: _sdkHandler(std::move(sdkHandler))
		, _frame(std::move(frame))
	{ }

	QVideoFrameFormat format() const override
	{
		return nv12Format(
			int(_frame->width()),
			int(_frame->height()),
			_frame->colorRange() == tsvb::colorRangeFull,
			_frame->colorMatrix() == tsvb::colorMatrixBT709
		);
	}

	MapData map(QVideoFrame::MapMode mode) override
	{
		if (mode != QVideoFrame::ReadOnly) {
			return {};
		}
		_lockedData.reset(_frame->lock(tsvb::read));
		if (nullptr == _lockedData) {
			return {};
		}
		return nv12MapData(
			static_cast<uchar*>(_lockedData->dataPointer(0)),
			int(_lockedData->bytesPerLine(0)),
			static_cast<uchar*>(_lockedData->dataPointer(1)),
			int(_lockedData->bytesPerLine(1)),
			int(_frame->height())
		);
	}

	void unmap() override
	{
		_lockedData = nullptr;
	}

private:
	// First, so that the library outlives the frame.
	std::shared_ptr<SDKLibraryHandler> _sdkHandler;
	SDKUniquePtr<tsvb::IFrame> _frame;
	SDKUniquePtr<tsvb::ILockedFrameData> _lockedData;
};

}

QVideoFrame CaptureLoop::filterFrame(std::unique_ptr<camera::IFrame> captured)
{
	if (_filter) {
		SDKUniquePtr<tsvb::IFrame> processed = _filter->process(captured->image());
		if (processed && (tsvb::nv12 == processed->frameFormat())) {
			return QVideoFrame(std::make_unique<SDKBuffer>(_filter->sdkHandler(), std::move(processed)));
		}
	}
	return QVideoFrame(std::make_unique<CameraBuffer>(std::move(captured)));
}
