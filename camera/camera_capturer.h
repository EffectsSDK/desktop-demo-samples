#ifndef SAMPLE_QML_CAMERA_CAPTURER_H
#define SAMPLE_QML_CAMERA_CAPTURER_H

#include <chrono>
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef __APPLE__
#include <CoreVideo/CoreVideo.h>
#else
#include <cstdint>
#endif

namespace camera {

using Timestamp = std::chrono::nanoseconds;

class Error : public std::runtime_error
{
public:
	using std::runtime_error::runtime_error;
};

#ifdef _WIN32
class MediaFoundation
{
public:
	MediaFoundation();
	~MediaFoundation();
	MediaFoundation(const MediaFoundation&) = delete;
	MediaFoundation& operator=(const MediaFoundation&) = delete;
};
#endif

#ifndef __APPLE__
enum class ColorRange
{
	video,
	full
};

enum class ColorMatrix
{
	bt601,
	bt709
};

struct Nv12Image
{
	int width = 0;
	int height = 0;
	const std::uint8_t* y = nullptr;
	int yBytesPerLine = 0;
	const std::uint8_t* uv = nullptr;
	int uvBytesPerLine = 0;
	ColorRange range = ColorRange::video;
	ColorMatrix matrix = ColorMatrix::bt601;
};
#endif

class IFrame
{
public:
	virtual ~IFrame() = default;

	virtual std::optional<Timestamp> timestamp() const = 0;

#ifdef __APPLE__
	virtual CVPixelBufferRef pixelBuffer() const = 0;
#else
	virtual const Nv12Image& image() const = 0;
#endif
};

std::vector<int> frameRates(const std::string& uniqueId, int width, int height);

inline int nearestFrameRate(const std::vector<int>& rates, int fps)
{
	int nearest = 0;
	for (int rate : rates) {
		if (nearest == 0 || std::abs(rate - fps) < std::abs(nearest - fps)) {
			nearest = rate;
		}
	}
	return nearest;
}

class Capturer
{
public:
	Capturer(const std::string& uniqueId, int width, int height, int fps);
	~Capturer();
	Capturer(const Capturer&) = delete;
	Capturer& operator=(const Capturer&) = delete;

	const std::string& uniqueId() const;

	void setFormat(int width, int height, int fps);

	std::unique_ptr<IFrame> read(std::chrono::milliseconds timeout);

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

}

#endif
