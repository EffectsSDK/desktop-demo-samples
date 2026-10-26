#include "sdk_library_handler.h"

#include <dlfcn.h>

#include <string>

#include <libgen.h>
#include <linux/limits.h>
#include <unistd.h>

static std::string getCurrentExecutableDir()
{
	char path[PATH_MAX];
	ssize_t count = readlink("/proc/self/exe", path, sizeof(path));
	if (count < 1) {
		return std::string();
	}

	return std::string(dirname(path));
}

static std::string getLibName()
{
	return "libtsvb.so";
}

class SDKLibraryHandler::Impl
{
	void* _handle = nullptr;
	tsvb::pfnCreateSDKFactory _createFactory = nullptr;

public:
	Impl() = default;

	~Impl()
	{
		if (nullptr != _handle) {
			dlclose(_handle);
			_handle = nullptr;
		}
	}

	bool isValid() const
	{
		return
			(nullptr != _handle) &&
			(nullptr != _createFactory);
	}

	void load(const char* path)
	{
		void* handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
		if (nullptr == handle) {
			return;
		}

		_createFactory = reinterpret_cast<tsvb::pfnCreateSDKFactory>(
			dlsym(handle, "createSDKFactory")
		);

		if (nullptr != _createFactory) {
			_handle = handle;
		}
		else {
			dlclose(handle);
		}
	}

	tsvb::ISDKFactory* createFactory()
	{
		if (isValid()) {
			return _createFactory();
		}

		return nullptr;
	}
};

SDKLibraryHandler::SDKLibraryHandler():
	_impl(new Impl)
{
	const std::string absolutePath = getCurrentExecutableDir() + "/" + getLibName();
	bool exists = (access(absolutePath.c_str(), 0) == 0);
	const std::string path = exists ? absolutePath : getLibName();

	_impl->load(path.c_str());
}

SDKLibraryHandler::~SDKLibraryHandler()
{
}

SDKUniquePtr<tsvb::ISDKFactory> SDKLibraryHandler::createSDKFactory()
{
	if ((nullptr != _impl) && _impl->isValid()) {
		return SDKUniquePtr<tsvb::ISDKFactory>(_impl->createFactory());
	}
	return nullptr;
}

bool SDKLibraryHandler::isValid() const
{
	return _impl->isValid();
}
