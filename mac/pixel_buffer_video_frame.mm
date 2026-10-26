#include "pixel_buffer_video_frame.h"

#include <QtMultimedia/private/qhwvideobuffer_p.h>
#include <rhi/qrhi.h>
#include <rhi/qrhi_platform.h>
#include <QThread>

#import <CoreVideo/CVMetalTexture.h>
#import <CoreVideo/CVMetalTextureCache.h>
#import <Metal/Metal.h>
#include <QVideoFrameFormat>

#include <array>
#include <limits>
#include <mutex>
#include <vector>
#include <utility>

namespace {

QVideoFrameFormat bufferFormat(CVPixelBufferRef pixels)
{
	if (!pixels) {
		return {};
	}
	const size_t width = CVPixelBufferGetWidth(pixels);
	const size_t height = CVPixelBufferGetHeight(pixels);
	if (!width || !height || width > size_t(std::numeric_limits<int>::max())
			|| height > size_t(std::numeric_limits<int>::max())) {
		return {};
	}
	QVideoFrameFormat::PixelFormat pixelFormat = QVideoFrameFormat::Format_Invalid;
	const OSType nativeFormat = CVPixelBufferGetPixelFormatType(pixels);
	switch (nativeFormat) {
	case kCVPixelFormatType_32BGRA:
		pixelFormat = QVideoFrameFormat::Format_BGRA8888;
		break;
	case kCVPixelFormatType_32RGBA:
		pixelFormat = QVideoFrameFormat::Format_RGBA8888;
		break;
	case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
	case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
		if ((width % 2) || (height % 2) || CVPixelBufferGetPlaneCount(pixels) != 2) {
			return {};
		}
		pixelFormat = QVideoFrameFormat::Format_NV12;
		break;
	default:
		return {};
	}
	QVideoFrameFormat result(QSize(int(width), int(height)), pixelFormat);
	if (pixelFormat == QVideoFrameFormat::Format_NV12) {
		result.setColorRange(nativeFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
			? QVideoFrameFormat::ColorRange_Full : QVideoFrameFormat::ColorRange_Video);
		CFTypeRef matrix = CVBufferCopyAttachment(pixels, kCVImageBufferYCbCrMatrixKey, nullptr);
		const bool bt709 = matrix && CFEqual(matrix, kCVImageBufferYCbCrMatrix_ITU_R_709_2);
		if (matrix) CFRelease(matrix);
		result.setColorSpace(bt709 ? QVideoFrameFormat::ColorSpace_BT709 : QVideoFrameFormat::ColorSpace_BT601);
	}
	return result;
}

struct MetalTextureReleaser
{
	void operator()(CVMetalTextureRef texture) const { CFRelease(texture); }
};
using MetalTexturePtr = std::unique_ptr<
	std::remove_pointer_t<CVMetalTextureRef>, MetalTextureReleaser>;

class MetalTextureCache
{
public:
	explicit MetalTextureCache(id<MTLDevice> device) : _device(device)
	{
		const CVReturn status = CVMetalTextureCacheCreate(kCFAllocatorDefault,
			nullptr, device, nullptr, &_cache);
		if (status != kCVReturnSuccess && _cache) {
			CFRelease(_cache);
			_cache = nullptr;
		}
	}
	~MetalTextureCache()
	{
		if (_cache) {
			CFRelease(_cache);
		}
	}
	MetalTextureCache(const MetalTextureCache&) = delete;
	MetalTextureCache& operator=(const MetalTextureCache&) = delete;

	id<MTLDevice> device() const { return _device; }
	bool isValid() const { return _cache != nullptr; }

	MetalTexturePtr createTexture(CVPixelBufferRef pixels, MTLPixelFormat format,
		size_t width, size_t height, size_t plane)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		CVMetalTextureRef texture = nullptr;
		const CVReturn status = CVMetalTextureCacheCreateTextureFromImage(kCFAllocatorDefault,
			_cache, pixels, nullptr, format, width, height, plane, &texture);
		MetalTexturePtr result(texture);
		if (status != kCVReturnSuccess || !texture || !CVMetalTextureGetTexture(texture)) {
			return {};
		}
		return result;
	}

private:
	id<MTLDevice> _device;
	CVMetalTextureCacheRef _cache = nullptr;
	std::mutex _mutex;
};

}

class CurrentMetalTextureCache
{
public:
	std::shared_ptr<MetalTextureCache> forDevice(id<MTLDevice> device)
	{
		std::lock_guard<std::mutex> lock(_mutex);
		if (!_cache || _cache->device() != device) {
			auto cache = std::make_shared<MetalTextureCache>(device);
			if (!cache->isValid()) {
				return {};
			}
			_cache = std::move(cache);
		}
		return _cache;
	}

private:
	std::mutex _mutex;
	std::shared_ptr<MetalTextureCache> _cache;
};

namespace {

struct MetalBinding
{
	std::shared_ptr<MetalTextureCache> cache;
	std::array<MetalTexturePtr, 2> planes;
	std::array<bool, 2> attempted{};
};

class PixelBufferVideoBuffer final : public QHwVideoBuffer
{
public:
	PixelBufferVideoBuffer(std::shared_ptr<CurrentMetalTextureCache> currentCache, PixelBufferPtr pixels)
		: QHwVideoBuffer(QVideoFrame::RhiTextureHandle),
		  _currentCache(std::move(currentCache)),
		  _pixels(std::move(pixels)),
		  _format(bufferFormat(_pixels.get()))
	{ }

	~PixelBufferVideoBuffer() override
	{
		unmap();
	}
	QVideoFrameFormat format() const override { return _format; }

	quint64 textureHandle(QRhi& rhi, int plane) override
	{
		@autoreleasepool {
			std::lock_guard<std::mutex> lock(_accessMutex);
			if (rhi.backend() != QRhi::Metal) {
				return 0;
			}
			if (QThread::currentThread() != rhi.thread() || _locked || plane < 0
					|| plane >= _format.planeCount()) {
				return 0;
			}
			const auto* handles = static_cast<const QRhiMetalNativeHandles*>(rhi.nativeHandles());
			id<MTLDevice> device = handles ? (id<MTLDevice>)handles->dev : nil;
			if (!device) {
				return 0;
			}
			auto cache = _currentCache->forDevice(device);
			if (!cache) {
				return 0;
			}
			MetalBinding* binding = nullptr;
			for (auto& candidate : _metalBindings) {
				if (candidate.cache == cache) {
					binding = &candidate;
					break;
				}
			}
			if (!binding) {
				_metalBindings.emplace_back();
				binding = &_metalBindings.back();
				binding->cache = std::move(cache);
			}
			if (binding->attempted[plane]) {
				const auto texture = binding->planes[plane].get();
				return texture ? reinterpret_cast<quint64>(
					(__bridge void*)CVMetalTextureGetTexture(texture)) : 0;
			}
			binding->attempted[plane] = true;
			MTLPixelFormat metalFormat = MTLPixelFormatInvalid;
			switch (_format.pixelFormat()) {
			case QVideoFrameFormat::Format_BGRA8888:
				metalFormat = MTLPixelFormatBGRA8Unorm;
				break;
			case QVideoFrameFormat::Format_RGBA8888:
				metalFormat = MTLPixelFormatRGBA8Unorm;
				break;
			case QVideoFrameFormat::Format_NV12:
				metalFormat = plane == 0 ? MTLPixelFormatR8Unorm : MTLPixelFormatRG8Unorm;
				break;
			default:
				return 0;
			}
			const bool planar = CVPixelBufferIsPlanar(_pixels.get());
			const size_t width = planar ? CVPixelBufferGetWidthOfPlane(_pixels.get(), plane)
				: CVPixelBufferGetWidth(_pixels.get());
			const size_t height = planar ? CVPixelBufferGetHeightOfPlane(_pixels.get(), plane)
				: CVPixelBufferGetHeight(_pixels.get());
			binding->planes[plane] = binding->cache->createTexture(
				_pixels.get(), metalFormat, width, height, size_t(plane));
			const auto texture = binding->planes[plane].get();
			return texture ? reinterpret_cast<quint64>(
				(__bridge void*)CVMetalTextureGetTexture(texture)) : 0;
		}
	}

	MapData map(QVideoFrame::MapMode mode) override
	{
		std::lock_guard<std::mutex> lock(_accessMutex);
		if (_locked || mode == QVideoFrame::NotMapped || !_format.isValid()) {
			return {};
		}
		_lockFlags = (mode == QVideoFrame::ReadOnly) ? kCVPixelBufferLock_ReadOnly : 0;
		if (CVPixelBufferLockBaseAddress(_pixels.get(), _lockFlags) != kCVReturnSuccess) {
			return {};
		}
		_locked = true;
		const bool planar = CVPixelBufferIsPlanar(_pixels.get());
		const size_t planeCount = planar ? CVPixelBufferGetPlaneCount(_pixels.get()) : 1;
		MapData result;
		if (!planeCount || planeCount > 4) {
			unlockPixels();
			return {};
		}
		for (size_t plane = 0; plane < planeCount; ++plane) {
			const size_t stride = planar
				? CVPixelBufferGetBytesPerRowOfPlane(_pixels.get(), plane)
				: CVPixelBufferGetBytesPerRow(_pixels.get());
			const size_t height = planar
				? CVPixelBufferGetHeightOfPlane(_pixels.get(), plane)
				: CVPixelBufferGetHeight(_pixels.get());
			void* data = planar
				? CVPixelBufferGetBaseAddressOfPlane(_pixels.get(), plane)
				: CVPixelBufferGetBaseAddress(_pixels.get());
			const size_t limit = size_t(std::numeric_limits<int>::max());
			if (!data || !stride || !height || stride > limit || height > limit / stride) {
				unlockPixels();
				return {};
			}
			result.data[plane] = static_cast<uchar*>(data);
			result.bytesPerLine[plane] = int(stride);
			result.dataSize[plane] = int(stride * height);
		}
		result.planeCount = int(planeCount);
		return result;
	}

	void unmap() override
	{
		std::lock_guard<std::mutex> lock(_accessMutex);
		unlockPixels();
	}

private:
	void unlockPixels()
	{
		if (_locked) {
			CVPixelBufferUnlockBaseAddress(_pixels.get(), _lockFlags);
			_locked = false;
		}
	}

private:
	const std::shared_ptr<CurrentMetalTextureCache> _currentCache;
	PixelBufferPtr _pixels;
	QVideoFrameFormat _format;
	CVPixelBufferLockFlags _lockFlags = 0;
	bool _locked = false;
	std::mutex _accessMutex;
	std::vector<MetalBinding> _metalBindings;
};

} 

PixelBufferBridge::PixelBufferBridge()
	: _currentCache(std::make_shared<CurrentMetalTextureCache>())
{ }

QVideoFrame PixelBufferBridge::pixelBufferToVideoFrame(PixelBufferPtr pixels)
{
	if (!bufferFormat(pixels.get()).isValid()) {
		return {};
	}
	return QVideoFrame(std::make_unique<PixelBufferVideoBuffer>(_currentCache, std::move(pixels)));
}
