#include "sdk_library_handler.h"

#include <Windows.h>

class SDKLibraryHandler::Impl
{
	HMODULE _handle = nullptr;
	tsvb::pfnCreateSDKFactory _createSDKFactory = nullptr;

public:
	Impl()
	{
		_handle = ::LoadLibraryA("tsvb");
		if (nullptr == _handle) {
			return;
		}

		_createSDKFactory = reinterpret_cast<tsvb::pfnCreateSDKFactory>(
			GetProcAddress(_handle, "createSDKFactory")
		);
	}

	~Impl()
	{
		_createSDKFactory = nullptr;
		if (nullptr != _handle) {
			FreeLibrary(_handle);
			_handle = nullptr;
		}
	}

	bool isValid() const
	{
		return
			(nullptr != _handle) &&
			(nullptr != _createSDKFactory);
	}

	tsvb::ISDKFactory* createFactory()
	{
		if (isValid()) {
			return _createSDKFactory();
		}

		return nullptr;
	}
};

SDKLibraryHandler::SDKLibraryHandler():
	_impl(new Impl)
{ }

SDKLibraryHandler::~SDKLibraryHandler()
{ }

SDKUniquePtr<tsvb::ISDKFactory> SDKLibraryHandler::createSDKFactory()
{
	return SDKUniquePtr<tsvb::ISDKFactory>(_impl->createFactory());
}

bool SDKLibraryHandler::isValid() const
{
	return _impl->isValid();
}