#include "sdk_library_handler.h"

// Linked rather than loaded. The only way on macOS: the system never unloads
// the SDK library, which has Objective-C in it, so dlopen() would gain nothing.
// On Windows the alternative to sdk_library_handler_win.cpp, see
// EFFECTS_SDK_LINK: tsvb.dll then has to be there for the sample to start at all.
class SDKLibraryHandler::Impl
{ };

SDKLibraryHandler::SDKLibraryHandler():
	_impl(new Impl)
{
}

SDKLibraryHandler::~SDKLibraryHandler()
{
}

SDKUniquePtr<tsvb::ISDKFactory> SDKLibraryHandler::createSDKFactory()
{
	return SDKUniquePtr<tsvb::ISDKFactory>(tsvb::createSDKFactory());
}

bool SDKLibraryHandler::isValid() const
{
	return true;
}
