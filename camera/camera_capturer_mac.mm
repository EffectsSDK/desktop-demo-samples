#include "camera_capturer.h"
#include "frame_mailbox.h"

#include "../mac/pixel_buffer.h"

#import <AVFoundation/AVFoundation.h>
#import <Foundation/Foundation.h>

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <set>
#include <utility>

namespace {

class Frame : public camera::IFrame
{
public:
	Frame(CVPixelBufferRef pixelBuffer, std::optional<camera::Timestamp> timestamp)
		: _pixelBuffer(retainPixelBuffer(pixelBuffer))
		, _timestamp(timestamp)
	{ }

	std::optional<camera::Timestamp> timestamp() const override
	{
		return _timestamp;
	}

	CVPixelBufferRef pixelBuffer() const override
	{
		return _pixelBuffer.get();
	}

private:
	PixelBufferPtr _pixelBuffer;
	std::optional<camera::Timestamp> _timestamp;
};

struct BeginConfiguration
{
	BeginConfiguration(const BeginConfiguration&) = delete;

	explicit BeginConfiguration(AVCaptureSession* session_)
		: session(session_)
	{
		[session beginConfiguration];
	}

	~BeginConfiguration()
	{
		commit();
	}

	void commit()
	{
		if (isOpen) {
			[session commitConfiguration];
			isOpen = false;
		}
	}

	AVCaptureSession* session;
	bool isOpen = true;

	BeginConfiguration& operator=(const BeginConfiguration&) = delete;
};

std::string utf8(NSString* text)
{
	if (nil == text) return std::string();
	const char* rawUtf8 = [text UTF8String];
	return std::string(rawUtf8 ? rawUtf8 : "");
}

struct DeviceLock
{
	DeviceLock(const DeviceLock&) = delete;

	explicit DeviceLock(AVCaptureDevice* device_)
		: device(device_)
	{
		NSError* error = nil;
		isLocked = [device lockForConfiguration:&error];
		if (!isLocked) {
			message = utf8(error.localizedDescription ?: @"The camera cannot be configured.");
		}
	}

	~DeviceLock()
	{
		if (isLocked) [device unlockForConfiguration];
	}

	AVCaptureDevice* device;
	bool isLocked = false;
	std::string message;

	DeviceLock& operator=(const DeviceLock&) = delete;
};

AVCaptureDevice* findDevice(const std::string& uniqueId)
{
	NSString* wantedId = [NSString stringWithUTF8String:uniqueId.c_str()];
	return wantedId ? [AVCaptureDevice deviceWithUniqueID:wantedId] : nil;
}

CMVideoDimensions dimensionsOf(AVCaptureDeviceFormat* format)
{
	return CMVideoFormatDescriptionGetDimensions(format.formatDescription);
}

bool operator==(CMVideoDimensions a, CMVideoDimensions b)
{
	return a.width == b.width && a.height == b.height;
}

CMVideoDimensions nearestSize(AVCaptureDevice* device, int width, int height)
{
	const auto distance = [width, height](CMVideoDimensions size) {
		const bool sameAspect = int64_t(size.width) * height == int64_t(size.height) * width;
		const int64_t area = int64_t(size.width) * size.height;
		return std::make_pair(!sameAspect, std::llabs(area - int64_t(width) * height));
	};
	CMVideoDimensions nearest{ 0, 0 };
	for (AVCaptureDeviceFormat* format in device.formats) {
		const CMVideoDimensions size = dimensionsOf(format);
		if (nearest.width == 0 || distance(size) < distance(nearest)) {
			nearest = size;
		}
	}
	return nearest;
}

constexpr int commonFrameRates[] = { 5, 10, 15, 20, 24, 25, 30, 50, 60, 90, 120 };

bool inRange(AVFrameRateRange* range, int fps)
{
	return std::lround(range.minFrameRate) <= fps && fps <= std::lround(range.maxFrameRate);
}

std::vector<int> ratesOf(AVCaptureDevice* device, CMVideoDimensions size)
{
	std::set<int> rates;
	for (AVCaptureDeviceFormat* format in device.formats) {
		if (!(dimensionsOf(format) == size)) continue;
		for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
			for (int rate : commonFrameRates) {
				if (inRange(range, rate)) rates.insert(rate);
			}
			if (const int maxRate = int(std::lround(range.maxFrameRate)); maxRate > 0) {
				rates.insert(maxRate);
			}
		}
	}
	return std::vector<int>(rates.begin(), rates.end());
}

struct Format
{
	AVCaptureDeviceFormat* format = nil;
	CMTime frameDuration = kCMTimeInvalid;
};

Format chooseFormat(AVCaptureDevice* device, int width, int height, int fps)
{
	const CMVideoDimensions size = nearestSize(device, width, height);
	const int rate = camera::nearestFrameRate(ratesOf(device, size), fps);
	for (AVCaptureDeviceFormat* format in device.formats) {
		if (!(dimensionsOf(format) == size)) continue;
		for (AVFrameRateRange* range in format.videoSupportedFrameRateRanges) {
			if (!inRange(range, rate)) continue;
			CMTime frameDuration = CMTimeMake(1, rate);
			if (CMTimeCompare(frameDuration, range.minFrameDuration) < 0) {
				frameDuration = range.minFrameDuration;
			}
			if (CMTimeCompare(frameDuration, range.maxFrameDuration) > 0) {
				frameDuration = range.maxFrameDuration;
			}
			return { format, frameDuration };
		}
	}
	return {};
}

void applyFormat(AVCaptureDevice* device, const Format& format)
{
	device.activeFormat = format.format;
	device.activeVideoMinFrameDuration = format.frameDuration;
	device.activeVideoMaxFrameDuration = format.frameDuration;
}

}

@interface SampleCameraDelegate : NSObject <AVCaptureVideoDataOutputSampleBufferDelegate>
{
	std::shared_ptr<camera::FrameMailbox> _mailbox;
}
- (instancetype)initWithMailbox:(std::shared_ptr<camera::FrameMailbox>)mailbox;
@end

@implementation SampleCameraDelegate
- (instancetype)initWithMailbox:(std::shared_ptr<camera::FrameMailbox>)mailbox
{
	self = [super init];
	if (self) {
		_mailbox = std::move(mailbox);
	}
	return self;
}

- (void)captureOutput:(AVCaptureOutput*)output
		didOutputSampleBuffer:(CMSampleBufferRef)sampleBuffer
		fromConnection:(AVCaptureConnection*)connection
{
	(void)connection;
	@autoreleasepool {
		CVPixelBufferRef pixels = CMSampleBufferGetImageBuffer(sampleBuffer);
		if (!pixels) return;

		const CMTime time = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
		std::optional<camera::Timestamp> timestamp;
		if (CMTIME_IS_NUMERIC(time)) {
			timestamp = camera::Timestamp(
				CMTimeConvertScale(time, 1000000000, kCMTimeRoundingMethod_Default).value);
		}
		_mailbox->post(std::make_unique<Frame>(pixels, timestamp));
	}
}
@end

namespace camera {

class Capturer::Impl
{
public:
	explicit Impl(const std::string& uniqueId)
		: _uniqueId(uniqueId)
	{ }

	~Impl()
	{
		@autoreleasepool {
			[_output setSampleBufferDelegate:nil queue:nullptr];
			[_session stopRunning];
			NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
			if (_runtimeErrorObserver) [center removeObserver:_runtimeErrorObserver];
			if (_interruptionObserver) [center removeObserver:_interruptionObserver];
			if (_disconnectObserver) [center removeObserver:_disconnectObserver];
			waitUnfinishedWork();
		}
	}

	Impl(const Impl&) = delete;
	Impl& operator=(const Impl&) = delete;

	const std::string& uniqueId() const
	{
		return _uniqueId;
	}

	void setFormat(int width, int height, int fps)
	{
		@autoreleasepool {
			const Format format = chooseFormat(_device, width, height, fps);
			if (!format.format) return;
			DeviceLock lock(_device);
			if (!lock.isLocked) {
				NSLog(@"Camera format cannot be changed: %s", lock.message.c_str());
				return;
			}
			BeginConfiguration configuration(_session);
			@try {
				applyFormat(_device, format);
			}
			@catch (NSException* exception) {
				NSLog(@"Camera format was rejected: %@", exception.reason);
			}
		}
	}

	std::unique_ptr<IFrame> read(std::chrono::milliseconds timeout)
	{
		return _mailbox->read(timeout);
	}

	void start(int width, int height, int fps)
	{
		switch ([AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeVideo]) {
		case AVAuthorizationStatusAuthorized:
			break;
		case AVAuthorizationStatusNotDetermined:
			throw Error("Camera access has not been granted yet.");
		default:
			throw Error("Camera access is denied or restricted. Enable it in macOS Privacy settings.");
		}

		AVCaptureDevice* device = findDevice(_uniqueId);
		if (!device) {
			throw Error("The selected camera is not connected.");
		}

		NSError* error = nil;
		AVCaptureDeviceInput* input =
			[AVCaptureDeviceInput deviceInputWithDevice:device error:&error];
		if (!input) {
			throw Error(error ? utf8(error.localizedDescription) : "The selected camera cannot be opened.");
		}

		_device = device;
		_session = [[AVCaptureSession alloc] init];
		_output = [[AVCaptureVideoDataOutput alloc] init];
		_queue = dispatch_queue_create("com.effectssdk.sample.camera", DISPATCH_QUEUE_SERIAL);
		_delegate = [[SampleCameraDelegate alloc] initWithMailbox:_mailbox];

		AVCaptureSession* session = _session;
		AVCaptureVideoDataOutput* output = _output;

		DeviceLock lock(device);
		if (!lock.isLocked) {
			throw Error(lock.message);
		}
		BeginConfiguration configuration(session);
		if (![session canAddInput:input]) {
			throw Error("AVFoundation cannot add the selected camera input.");
		}
		[session addInput:input];
		if (![session canAddOutput:output]) {
			throw Error("AVFoundation cannot add a video output for this camera.");
		}
		[session addOutput:output];
		if (const Format format = chooseFormat(device, width, height, fps); format.format) {
			applyFormat(device, format);
		}
		output.videoSettings = @{
			(__bridge NSString*)kCVPixelBufferPixelFormatTypeKey: @(kCVPixelFormatType_32BGRA)
		};
		output.alwaysDiscardsLateVideoFrames = YES;
		[output setSampleBufferDelegate:_delegate queue:_queue];
		configuration.commit();

		const auto mailbox = _mailbox;
		NSNotificationCenter* center = NSNotificationCenter.defaultCenter;
		_runtimeErrorObserver = [center addObserverForName:AVCaptureSessionRuntimeErrorNotification
			object:session queue:nil usingBlock:^(NSNotification* notification) {
				NSError* runtimeError = notification.userInfo[AVCaptureSessionErrorKey];
				mailbox->fail(utf8(runtimeError.localizedDescription ?: @"Camera session failed."));
			}];
		_interruptionObserver = [center addObserverForName:AVCaptureSessionWasInterruptedNotification
			object:session queue:nil usingBlock:^(NSNotification*) {
				mailbox->fail("Camera session was interrupted.");
			}];
		_disconnectObserver = [center addObserverForName:AVCaptureDeviceWasDisconnectedNotification
			object:device queue:nil usingBlock:^(NSNotification*) {
				mailbox->fail("The camera was disconnected.");
			}];

		[session startRunning];
		throwIfNotStarted();
	}

private:
	void throwIfNotStarted()
	{
		if (!_session.running) {
			_mailbox->fail("AVFoundation could not start the camera session.");
		}
		if (auto failure = _mailbox->error()) {
			throw Error(*failure);
		}
	}

	void waitUnfinishedWork()
	{
		if (_queue) dispatch_sync(_queue, ^{});
	}

private:
	const std::string _uniqueId;
	const std::shared_ptr<FrameMailbox> _mailbox = std::make_shared<FrameMailbox>();
	AVCaptureDevice* _device = nil;
	AVCaptureSession* _session = nil;
	AVCaptureVideoDataOutput* _output = nil;
	SampleCameraDelegate* _delegate = nil;
	dispatch_queue_t _queue = nullptr;
	id _runtimeErrorObserver = nil;
	id _interruptionObserver = nil;
	id _disconnectObserver = nil;
};

std::vector<int> frameRates(const std::string& uniqueId, int width, int height)
{
	@autoreleasepool {
		AVCaptureDevice* device = findDevice(uniqueId);
		if (!device) return {};
		return ratesOf(device, nearestSize(device, width, height));
	}
}

Capturer::Capturer(const std::string& uniqueId, int width, int height, int fps)
	: _impl(std::make_unique<Impl>(uniqueId))
{
	@autoreleasepool {
		@try {
			_impl->start(width, height, fps);
		}
		@catch (NSException* exception) {
			throw Error(utf8(exception.reason ?: exception.name));
		}
	}
}

Capturer::~Capturer() = default;

const std::string& Capturer::uniqueId() const
{
	return _impl->uniqueId();
}

void Capturer::setFormat(int width, int height, int fps)
{
	_impl->setFormat(width, height, fps);
}

std::unique_ptr<IFrame> Capturer::read(std::chrono::milliseconds timeout)
{
	return _impl->read(timeout);
}

}
