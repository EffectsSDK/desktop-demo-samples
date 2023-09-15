#include "sdk_library_handler.h"

#include <Windows.h>

class BGLibraryHandler::Impl
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

BGLibraryHandler::BGLibraryHandler():
	_impl(new Impl)
{ }

BGLibraryHandler::~BGLibraryHandler()
{ }

tsvb::ISDKFactory* BGLibraryHandler::createSDKFactory()
{
	return _impl->createFactory();
}

bool BGLibraryHandler::isValid() const
{
	return _impl->isValid();
}