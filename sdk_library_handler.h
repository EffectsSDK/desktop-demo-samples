#ifndef SAMPLE_QML_SDK_LIBRARY_HANDLER_H
#define SAMPLE_QML_SDK_LIBRARY_HANDLER_H

#include <cstddef>
#include <vb_sdk/sdk_factory.h>

#include <memory>

struct SDKReleaser
{
	void operator()(tsvb::IRelease* object) const
	{
		object->release();
	}
};

// An object the SDK created. The library that made it has to be loaded when it
// goes: whatever keeps one beyond the handler's life keeps the handler too.
template <typename T>
using SDKUniquePtr = std::unique_ptr<T, SDKReleaser>;

class SDKLibraryHandler
{
public:
	SDKLibraryHandler();
	~SDKLibraryHandler();

	bool isValid() const;

	SDKUniquePtr<tsvb::ISDKFactory> createSDKFactory();

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

#endif
