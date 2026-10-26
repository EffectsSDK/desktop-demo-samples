#ifndef SAMPLE_QML_CAMERA_FRAME_MAILBOX_H
#define SAMPLE_QML_CAMERA_FRAME_MAILBOX_H

#include "camera_capturer.h"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>

namespace camera {

class FrameMailbox
{
public:
	void post(std::unique_ptr<IFrame> frame)
	{
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if (_error) return;
			std::swap(_latest, frame);
		}
		_condVar.notify_one();
	}

	void fail(std::string message)
	{
		{
			std::lock_guard<std::mutex> lock(_mutex);
			if (_error) return;
			_error = std::move(message);
		}
		_condVar.notify_all();
	}

	std::optional<std::string> error()
	{
		std::lock_guard<std::mutex> lock(_mutex);
		return _error;
	}

	std::unique_ptr<IFrame> read(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_condVar.wait_for(lock, timeout, [this] { return _latest || _error; });
		if (_error) throw Error(*_error);
		return std::exchange(_latest, nullptr);
	}

	void waitForFrame(std::chrono::milliseconds timeout)
	{
		std::unique_lock<std::mutex> lock(_mutex);
		_condVar.wait_for(lock, timeout, [this] { return _latest || _error; });
		if (_error) throw Error(*_error);
	}

private:
	std::mutex _mutex;
	std::condition_variable _condVar;
	std::unique_ptr<IFrame> _latest;
	std::optional<std::string> _error;
};

}

#endif
