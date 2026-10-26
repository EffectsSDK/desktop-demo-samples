#include "camera_capturer.h"
#include "frame_mailbox.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <wrl/client.h>

#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <iterator>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <utility>

using Microsoft::WRL::ComPtr;

namespace {

constexpr DWORD videoStream = DWORD(MF_SOURCE_READER_FIRST_VIDEO_STREAM);

constexpr auto firstReadTimeout = std::chrono::seconds(1);
constexpr auto stopTimeout = std::chrono::seconds(2);

std::wstring utf16(const std::string& text)
{
	const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0);
	if (length <= 0) return std::wstring();
	std::wstring result(size_t(length), L'\0');
	MultiByteToWideChar(CP_UTF8, 0, text.data(), int(text.size()), result.data(), length);
	return result;
}

std::string utf8(const std::wstring& text)
{
	const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), nullptr, 0, nullptr, nullptr);
	if (length <= 0) return std::string();
	std::string result(size_t(length), '\0');
	WideCharToMultiByte(CP_UTF8, 0, text.data(), int(text.size()), result.data(), length, nullptr, nullptr);
	return result;
}

std::string systemMessage(HRESULT hr)
{
	wchar_t* text = nullptr;
	DWORD length = FormatMessageW(
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		nullptr,
		DWORD(hr),
		MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
		reinterpret_cast<wchar_t*>(&text),
		0,
		nullptr
	);
	const std::unique_ptr<wchar_t, decltype(&LocalFree)> owner(text, &LocalFree);
	if (!text) return std::string();

	while (length > 0 && (text[length - 1] == L'\n' || text[length - 1] == L'\r' || text[length - 1] == L' ')) {
		--length;
	}
	return utf8(std::wstring(text, length));
}

std::string describe(HRESULT hr, const char* what)
{
	if (hr == E_ACCESSDENIED) {
		return "Camera access is denied. Allow desktop apps to use the camera in Windows Privacy & security settings.";
	}
	if (hr == MF_E_HW_MFT_FAILED_START_STREAMING) {
		return "The camera did not start. Another application may be using it.";
	}
	if (hr == MF_E_VIDEO_RECORDING_DEVICE_PREEMPTED) {
		return "Another application took over the camera.";
	}
	if (hr == MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED || hr == HRESULT_FROM_WIN32(ERROR_DEVICE_REMOVED)) {
		return "The camera was disconnected.";
	}
	if (hr == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND) || hr == HRESULT_FROM_WIN32(ERROR_PATH_NOT_FOUND)
			|| hr == HRESULT_FROM_WIN32(ERROR_DEVICE_NOT_CONNECTED) || hr == HRESULT_FROM_WIN32(ERROR_NOT_FOUND)) {
		return "The selected camera is not connected.";
	}
	char code[32];
	std::snprintf(code, sizeof(code), " (error 0x%08lX)", static_cast<unsigned long>(hr));
	const std::string message = systemMessage(hr);
	return what + std::string(code) + (message.empty() ? "." : ": " + message);
}

void check(HRESULT hr, const char* what)
{
	if (FAILED(hr)) {
		throw camera::Error(describe(hr, what));
	}
}

void logWarning(const std::string& message)
{
	OutputDebugStringA(("[camera_mf] " + message + "\n").c_str());
}

camera::Timestamp winTimeToCameraTime(LONGLONG winTimestamp)
{
	using namespace std::chrono;
	return duration_cast<camera::Timestamp>(nanoseconds(winTimestamp * 100));
}

HRESULT mediaFoundationStatus = MF_E_PLATFORM_NOT_INITIALIZED;

void checkMediaFoundation()
{
	if (mediaFoundationStatus == E_NOTIMPL) {
		throw camera::Error("Media Foundation is not available. On an N edition of Windows, install the Media Feature Pack.");
	}
	check(mediaFoundationStatus, "Media Foundation is not started");
}

ComPtr<IMFMediaSource> openSource(const std::string& uniqueId)
{
	constexpr const char* what = "The camera cannot be opened";
	ComPtr<IMFAttributes> attributes;
	check(MFCreateAttributes(&attributes, 2), what);
	check(attributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID), what);
	check(attributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, utf16(uniqueId).c_str()), what);
	ComPtr<IMFMediaSource> source;
	check(MFCreateDeviceSource(attributes.Get(), &source), what);
	return source;
}

ComPtr<IMFSourceReader> createReader(IMFMediaSource* source, IMFSourceReaderCallback* callback)
{
	constexpr const char* what = "The camera cannot be read";
	ComPtr<IMFAttributes> attributes;
	check(MFCreateAttributes(&attributes, 3), what);
	check(attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, FALSE), what);
	check(attributes->SetUINT32(MF_SOURCE_READER_ENABLE_ADVANCED_VIDEO_PROCESSING, TRUE), what);
	if (callback) {
		check(attributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK, callback), what);
	}
	ComPtr<IMFSourceReader> reader;
	check(MFCreateSourceReaderFromMediaSource(source, attributes.Get(), &reader), what);
	check(reader->SetStreamSelection(DWORD(MF_SOURCE_READER_ALL_STREAMS), FALSE), what);
	check(reader->SetStreamSelection(videoStream, TRUE), what);
	return reader;
}

struct Ratio
{
	UINT32 num = 0;
	UINT32 den = 0;

	double value() const
	{
		return den ? double(num) / den : 0.0;
	}

	int rounded() const
	{
		return int(std::lround(value()));
	}
};

bool operator==(Ratio a, Ratio b)
{
	return a.num == b.num && a.den == b.den;
}

struct Size
{
	UINT32 width = 0;
	UINT32 height = 0;
};

bool operator==(Size a, Size b)
{
	return a.width == b.width && a.height == b.height;
}

struct CameraFormat
{
	DWORD index = 0;
	GUID subtype = GUID_NULL;
	Size size;
	Ratio rate;
	Ratio minRate;
	Ratio maxRate;
};

int subtypeRank(const GUID& subtype)
{
	const GUID ranked[] = {
		MFVideoFormat_NV12,
		MFVideoFormat_YUY2,
		MFVideoFormat_UYVY,
		MFVideoFormat_I420,
		MFVideoFormat_IYUV,
		MFVideoFormat_YV12,
		MFVideoFormat_MJPG,
		MFVideoFormat_RGB32,
		MFVideoFormat_RGB24
	};
	for (int i = 0; i < int(std::size(ranked)); ++i) {
		if (subtype == ranked[i]) return i;
	}
	return -1;
}

std::vector<CameraFormat> readFormats(IMFSourceReader* reader)
{
	std::vector<CameraFormat> formats;
	for (DWORD index = 0;; ++index) {
		ComPtr<IMFMediaType> type;
		if (FAILED(reader->GetNativeMediaType(videoStream, index, &type))) break;

		CameraFormat format;
		format.index = index;
		if (FAILED(type->GetGUID(MF_MT_SUBTYPE, &format.subtype)) || subtypeRank(format.subtype) < 0) continue;
		if (FAILED(MFGetAttributeSize(type.Get(), MF_MT_FRAME_SIZE, &format.size.width, &format.size.height))
				|| !format.size.width || !format.size.height) {
			continue;
		}
		MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE, &format.rate.num, &format.rate.den);
		format.minRate = format.rate;
		format.maxRate = format.rate;
		MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE_RANGE_MIN, &format.minRate.num, &format.minRate.den);
		MFGetAttributeRatio(type.Get(), MF_MT_FRAME_RATE_RANGE_MAX, &format.maxRate.num, &format.maxRate.den);
		formats.push_back(format);
	}
	return formats;
}

Size nearestSize(const std::vector<CameraFormat>& formats, int width, int height)
{
	const auto distance = [width, height](Size size) {
		const bool sameAspect = int64_t(size.width) * height == int64_t(size.height) * width;
		const int64_t area = int64_t(size.width) * size.height;
		return std::make_pair(!sameAspect, std::llabs(area - int64_t(width) * height));
	};
	Size nearest;
	for (const CameraFormat& format : formats) {
		if (nearest.width == 0 || distance(format.size) < distance(nearest)) {
			nearest = format.size;
		}
	}
	return nearest;
}

constexpr int commonFrameRates[] = { 5, 10, 15, 20, 24, 25, 30, 50, 60, 90, 120 };

bool inRange(const CameraFormat& format, int fps)
{
	return format.rate.rounded() == fps
		|| (format.minRate.rounded() <= fps && fps <= format.maxRate.rounded());
}

std::vector<int> ratesOf(const std::vector<CameraFormat>& formats, Size size)
{
	std::set<int> rates;
	for (const CameraFormat& format : formats) {
		if (!(format.size == size)) continue;
		for (int rate : commonFrameRates) {
			if (inRange(format, rate)) rates.insert(rate);
		}
		for (const Ratio& rate : { format.rate, format.maxRate }) {
			if (rate.rounded() > 0) rates.insert(rate.rounded());
		}
	}
	return std::vector<int>(rates.begin(), rates.end());
}

struct Choice
{
	DWORD index = 0;
	Ratio rate;
};

bool operator==(const Choice& a, const Choice& b)
{
	return a.index == b.index && a.rate == b.rate;
}

std::optional<Choice> chooseFormat(const std::vector<CameraFormat>& formats, int width, int height, int fps)
{
	const Size size = nearestSize(formats, width, height);
	const int rate = camera::nearestFrameRate(ratesOf(formats, size), fps);
	const CameraFormat* best = nullptr;
	for (const CameraFormat& format : formats) {
		if (!(format.size == size) || (rate > 0 && !inRange(format, rate))) continue;
		if (!best || subtypeRank(format.subtype) < subtypeRank(best->subtype)) {
			best = &format;
		}
	}
	if (!best) return std::nullopt;

	Choice choice{ best->index, best->rate };
	if (rate > 0 && best->rate.rounded() != rate) {
		choice.rate = Ratio{ UINT32(rate), 1 };
		if (best->maxRate.value() < choice.rate.value()) choice.rate = best->maxRate;
		if (choice.rate.value() < best->minRate.value()) choice.rate = best->minRate;
	}
	return choice;
}

struct OutputFormat
{
	Size size;
	LONG stride = 0;
	camera::ColorRange range = camera::ColorRange::video;
	camera::ColorMatrix matrix = camera::ColorMatrix::bt601;
};

OutputFormat outputFormatOf(IMFMediaType* type)
{
	OutputFormat format;
	MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &format.size.width, &format.size.height);
	UINT32 value = 0;
	if (SUCCEEDED(type->GetUINT32(MF_MT_DEFAULT_STRIDE, &value))) {
		format.stride = LONG(value);
	}
	if (SUCCEEDED(type->GetUINT32(MF_MT_VIDEO_NOMINAL_RANGE, &value)) && value == MFNominalRange_0_255) {
		format.range = camera::ColorRange::full;
	}
	if (SUCCEEDED(type->GetUINT32(MF_MT_YUV_MATRIX, &value)) && value == MFVideoTransferMatrix_BT709) {
		format.matrix = camera::ColorMatrix::bt709;
	}
	return format;
}

OutputFormat applyFormat(IMFSourceReader* reader, const Choice& choice)
{
	constexpr const char* what = "The camera format cannot be set";
	ComPtr<IMFMediaType> native;
	check(reader->GetNativeMediaType(videoStream, choice.index, &native), what);
	ComPtr<IMFMediaType> input;
	check(MFCreateMediaType(&input), what);
	check(native->CopyAllItems(input.Get()), what);
	if (choice.rate.den) {
		check(MFSetAttributeRatio(input.Get(), MF_MT_FRAME_RATE, choice.rate.num, choice.rate.den), what);
	}
	const HRESULT nativeStatus = reader->SetCurrentMediaType(videoStream, nullptr, input.Get());
	if (FAILED(nativeStatus)) {
		logWarning(describe(nativeStatus, "the camera keeps its own format"));
	}

	Size size;
	check(MFGetAttributeSize(native.Get(), MF_MT_FRAME_SIZE, &size.width, &size.height), what);
	ComPtr<IMFMediaType> output;
	check(MFCreateMediaType(&output), what);
	check(output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), what);
	check(output->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_NV12), what);
	check(MFSetAttributeSize(output.Get(), MF_MT_FRAME_SIZE, size.width, size.height), what);
	if (choice.rate.den && SUCCEEDED(nativeStatus)) {
		check(MFSetAttributeRatio(output.Get(), MF_MT_FRAME_RATE, choice.rate.num, choice.rate.den), what);
	}
	Ratio pixelAspect;
	if (SUCCEEDED(MFGetAttributeRatio(native.Get(), MF_MT_PIXEL_ASPECT_RATIO, &pixelAspect.num, &pixelAspect.den))) {
		check(MFSetAttributeRatio(output.Get(), MF_MT_PIXEL_ASPECT_RATIO, pixelAspect.num, pixelAspect.den), what);
	}
	check(output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive), what);
	check(reader->SetCurrentMediaType(videoStream, nullptr, output.Get()), what);

	ComPtr<IMFMediaType> current;
	check(reader->GetCurrentMediaType(videoStream, &current), what);
	return outputFormatOf(current.Get());
}

class Frame : public camera::IFrame
{
public:
	static std::unique_ptr<Frame> lock(IMFSample* sample, const OutputFormat& format, camera::Timestamp timestamp)
	{
		const UINT32 width = format.size.width;
		const UINT32 height = format.size.height;
		if (!width || !height) return nullptr;
		ComPtr<IMFMediaBuffer> buffer;
		if (FAILED(sample->ConvertToContiguousBuffer(&buffer))) return nullptr;

		std::unique_ptr<Frame> frame(new Frame(sample, timestamp));
		BYTE* data = nullptr;
		LONG pitch = 0;
		std::optional<DWORD> length;
		ComPtr<IMF2DBuffer2> buffer2d2;
		ComPtr<IMF2DBuffer> buffer2d;
		if (SUCCEEDED(buffer.As(&buffer2d2))) {
			BYTE* start = nullptr;
			DWORD bufferLength = 0;
			if (FAILED(buffer2d2->Lock2DSize(MF2DBuffer_LockFlags_Read, &data, &pitch, &start, &bufferLength))) {
				return nullptr;
			}
			frame->_buffer2d = buffer2d2;
			length = bufferLength - DWORD(data - start);
		}
		else if (SUCCEEDED(buffer.As(&buffer2d))) {
			if (FAILED(buffer2d->Lock2D(&data, &pitch))) return nullptr;
			frame->_buffer2d = buffer2d;
		}
		else {
			DWORD currentLength = 0;
			if (FAILED(buffer->Lock(&data, nullptr, &currentLength))) return nullptr;
			frame->_buffer = buffer;
			pitch = format.stride ? format.stride : LONG(width);
			length = currentLength;
		}

		const UINT32 uvHeight = (height + 1) / 2;
		if (!data || pitch < LONG(width)) return nullptr;
		if (length && *length < uint64_t(pitch) * (height + uvHeight - 1) + width) return nullptr;

		camera::Nv12Image& image = frame->_image;
		image.width = int(width);
		image.height = int(height);
		image.y = data;
		image.yBytesPerLine = int(pitch);
		image.uv = data + size_t(pitch) * height;
		image.uvBytesPerLine = int(pitch);
		image.range = format.range;
		image.matrix = format.matrix;
		return frame;
	}

	~Frame() override
	{
		if (_buffer2d) {
			_buffer2d->Unlock2D();
		}
		else if (_buffer) {
			_buffer->Unlock();
		}
	}

	std::optional<camera::Timestamp> timestamp() const override
	{
		return _timestamp;
	}

	const camera::Nv12Image& image() const override
	{
		return _image;
	}

private:
	Frame(IMFSample* sample, camera::Timestamp timestamp)
		: _sample(sample),
		  _timestamp(timestamp)
	{ }

	ComPtr<IMFSample> _sample;
	ComPtr<IMF2DBuffer> _buffer2d;
	ComPtr<IMFMediaBuffer> _buffer;
	camera::Nv12Image _image;
	const camera::Timestamp _timestamp;
};

class ReaderCallback final : public IMFSourceReaderCallback
{
public:
	explicit ReaderCallback(std::shared_ptr<camera::FrameMailbox> mailbox)
		: _mailbox(std::move(mailbox))
	{ }

	ReaderCallback(const ReaderCallback&) = delete;
	ReaderCallback& operator=(const ReaderCallback&) = delete;

	STDMETHODIMP QueryInterface(REFIID iid, void** object) override
	{
		if (!object) return E_POINTER;
		if (iid == __uuidof(IUnknown) || iid == __uuidof(IMFSourceReaderCallback)) {
			*object = static_cast<IMFSourceReaderCallback*>(this);
			AddRef();
			return S_OK;
		}
		*object = nullptr;
		return E_NOINTERFACE;
	}

	STDMETHODIMP_(ULONG) AddRef() override
	{
		return ++_refCount;
	}

	STDMETHODIMP_(ULONG) Release() override
	{
		const ULONG count = --_refCount;
		if (count == 0) delete this;
		return count;
	}

	void start(ComPtr<IMFSourceReader> reader, const OutputFormat& format)
	{
		_format = format;
		_stopRequested = false;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_reader = reader;
			_running = true;
		}
		requestFrame(reader.Get());
	}

	std::future<void> stop()
	{
		std::future<void> stopped;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if (!_running) {
				std::promise<void> ended;
				ended.set_value();
				return ended.get_future();
			}
			_stopped.emplace();
			stopped = _stopped->get_future();
		}
		_stopRequested = true;
		return stopped;
	}

	std::future<void> flush()
	{
		std::future<void> flushed;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_flushed.emplace();
			flushed = _flushed->get_future();
		}
		const ComPtr<IMFSourceReader> reader = links().reader;
		if (!reader || FAILED(reader->Flush(videoStream))) {
			fulfil(_flushed);
		}
		return flushed;
	}

	void detach()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		_reader.Reset();
		_mailbox.reset();
	}

	STDMETHODIMP OnReadSample(HRESULT status, DWORD, DWORD flags, LONGLONG timestamp, IMFSample* sample) override
	{
		if (_stopRequested) {
			end();
			return S_OK;
		}
		const Links links = this->links();
		if (!links.mailbox) {
			end();
			return S_OK;
		}

		if (FAILED(status)) {
			fail(describe(status, "The camera failed"));
			return S_OK;
		}
		if (flags & MF_SOURCE_READERF_ERROR) {
			fail("The camera failed.");
			return S_OK;
		}
		if (flags & MF_SOURCE_READERF_ENDOFSTREAM) {
			fail("The camera stopped sending frames.");
			return S_OK;
		}

		if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED) {
			ComPtr<IMFMediaType> type;
			if (links.reader && SUCCEEDED(links.reader->GetCurrentMediaType(videoStream, &type))) {
				_format = outputFormatOf(type.Get());
			}
		}

		if (sample) {
			if (auto frame = Frame::lock(sample, _format, winTimeToCameraTime(timestamp))) {
				links.mailbox->post(std::move(frame));
			}
		}
		requestFrame(links.reader.Get());
		return S_OK;
	}

	STDMETHODIMP OnFlush(DWORD) override
	{
		fulfil(_flushed);
		return S_OK;
	}

	STDMETHODIMP OnEvent(DWORD, IMFMediaEvent* event) override
	{
		MediaEventType type = MEUnknown;
		if (!event || FAILED(event->GetType(&type))) return S_OK;

		const std::shared_ptr<camera::FrameMailbox> mailbox = links().mailbox;
		if (!mailbox) return S_OK;

		if (type == MEVideoCaptureDeviceRemoved) {
			mailbox->fail(describe(MF_E_VIDEO_RECORDING_DEVICE_INVALIDATED, ""));
		}
		else if (type == MEVideoCaptureDevicePreempted) {
			mailbox->fail(describe(MF_E_VIDEO_RECORDING_DEVICE_PREEMPTED, ""));
		}
		else if (type == MEError) {
			HRESULT status = S_OK;
			if (SUCCEEDED(event->GetStatus(&status)) && FAILED(status)) {
				mailbox->fail(describe(status, "The camera failed"));
			}
		}
		return S_OK;
	}

private:
	~ReaderCallback() = default;

	struct Links
	{
		ComPtr<IMFSourceReader> reader;
		std::shared_ptr<camera::FrameMailbox> mailbox;
	};

	Links links()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return { _reader, _mailbox };
	}

	void requestFrame(IMFSourceReader* reader)
	{
		if (_stopRequested || !reader) {
			end();
			return;
		}
		const HRESULT hr = reader->ReadSample(videoStream, 0, nullptr, nullptr, nullptr, nullptr);
		if (FAILED(hr)) {
			fail(describe(hr, "The camera cannot be read"));
		}
	}

	void fail(std::string message)
	{
		if (const std::shared_ptr<camera::FrameMailbox> mailbox = links().mailbox) {
			mailbox->fail(std::move(message));
		}
		end();
	}

	void end()
	{
		std::optional<std::promise<void>> stopped;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			_running = false;
			stopped = std::exchange(_stopped, std::nullopt);
		}
		if (stopped) {
			stopped->set_value();
		}
	}

	void fulfil(std::optional<std::promise<void>>& slot)
	{
		std::optional<std::promise<void>> promise;
		{
			std::lock_guard<std::mutex> lock(_mutex);
			promise = std::exchange(slot, std::nullopt);
		}
		if (promise) {
			promise->set_value();
		}
	}

private:
	std::atomic<ULONG> _refCount{ 1 };
	OutputFormat _format;
	std::atomic<bool> _stopRequested{ false };

	std::mutex _mutex;
	ComPtr<IMFSourceReader> _reader;
	std::shared_ptr<camera::FrameMailbox> _mailbox;
	bool _running = false;
	std::optional<std::promise<void>> _stopped;
	std::optional<std::promise<void>> _flushed;
};

}

namespace camera {

class Capturer::Impl
{
public:
	explicit Impl(const std::string& uniqueId)
		: _uniqueId(uniqueId)
	{ }
	
	~Impl()
	{
		if (_callback) {
			if (!stopReading()) {
				logWarning("the camera did not stop reading.");
			}
			_callback->detach();
		}
		_reader.Reset();
		if (_source) {
			_source->Shutdown();
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
		if (!_reader || _mailbox->error()) return;
		const std::optional<Choice> choice = chooseFormat(_formats, width, height, fps);
		if (!choice || *choice == _choice) return;

		if (!stopReading() || !flushReading()) {
			_mailbox->fail("The camera did not stop to change its format.");
			return;
		}
		OutputFormat output;
		try {
			output = applyFormat(_reader.Get(), *choice);
			_choice = *choice;
		}
		catch (const Error& error) {
			logWarning(std::string("the format was rejected: ") + error.what());
			try {
				output = applyFormat(_reader.Get(), _choice);
			}
			catch (const Error& restoreError) {
				_mailbox->fail(restoreError.what());
				return;
			}
		}
		_callback->start(_reader, output);
	}

	std::unique_ptr<IFrame> read(std::chrono::milliseconds timeout)
	{
		return _mailbox->read(timeout);
	}

	void initialize(int width, int height, int fps)
	{
		checkMediaFoundation();
		_source = openSource(_uniqueId);
		_callback.Attach(new ReaderCallback(_mailbox));
		_reader = createReader(_source.Get(), _callback.Get());
		_formats = readFormats(_reader.Get());
		const auto choice = chooseFormat(_formats, width, height, fps);
		if (!choice) {
			throw Error("The camera offers no video format the sample can take.");
		}
		const OutputFormat output = applyFormat(_reader.Get(), *choice);
		_choice = *choice;

		_callback->start(_reader, output);
		throwIfNotStarted();
	}

private:
	void throwIfNotStarted()
	{
		_mailbox->waitForFrame(firstReadTimeout);
	}

	bool stopReading()
	{
		return _callback->stop().wait_for(stopTimeout) != std::future_status::timeout;
	}

	bool flushReading()
	{
		return _callback->flush().wait_for(stopTimeout) != std::future_status::timeout;
	}

private:
	const std::string _uniqueId;
	const std::shared_ptr<FrameMailbox> _mailbox = std::make_shared<FrameMailbox>();
	ComPtr<IMFMediaSource> _source;
	ComPtr<ReaderCallback> _callback;
	ComPtr<IMFSourceReader> _reader;
	std::vector<CameraFormat> _formats;
	Choice _choice;
};

std::vector<int> frameRates(const std::string& uniqueId, int width, int height)
{
	if (FAILED(mediaFoundationStatus)) return {};

	ComPtr<IMFMediaSource> source;
	std::vector<CameraFormat> formats;
	try {
		source = openSource(uniqueId);
		formats = readFormats(createReader(source.Get(), nullptr).Get());
	}
	catch (const Error&) {
	}
	if (source) {
		source->Shutdown();
	}
	return ratesOf(formats, nearestSize(formats, width, height));
}

MediaFoundation::MediaFoundation()
{
	mediaFoundationStatus = MFStartup(MF_VERSION, MFSTARTUP_LITE);
}

MediaFoundation::~MediaFoundation()
{
	if (SUCCEEDED(mediaFoundationStatus)) {
		MFShutdown();
	}
	mediaFoundationStatus = MF_E_PLATFORM_NOT_INITIALIZED;
}

Capturer::Capturer(const std::string& uniqueId, int width, int height, int fps)
	: _impl(std::make_unique<Impl>(uniqueId))
{
	_impl->initialize(width, height, fps);
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
