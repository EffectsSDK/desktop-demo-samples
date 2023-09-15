#include "sdk_library_handler.h"

#include <dlfcn.h>

#include <string>
#include <vector>

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>

static std::string getBundleFrameworkPath()
{
	CFBundleRef appBundle = CFBundleGetMainBundle();

	CFURLRef bundleUrlRef = CFBundleCopyBundleURL(appBundle);
	CFURLRef frameworkUrlRef = CFBundleCopyPrivateFrameworksURL(appBundle);
	CFStringRef frameworkPathRef = CFURLCopyPath(frameworkUrlRef);
	CFURLRef frameworkFullUrlRef = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, bundleUrlRef, frameworkPathRef, true);
	CFStringRef frameworkFullPathRef = CFURLCopyFileSystemPath(frameworkFullUrlRef, kCFURLPOSIXPathStyle);
	
	CFIndex length = CFStringGetLength(frameworkFullPathRef);
	CFIndex byteSize = CFStringGetMaximumSizeForEncoding(length, kCFStringEncodingASCII);
	
	std::string result;
	try {
		std::vector<char> strData(byteSize + 1, '\0');
		bool ok = CFStringGetCString(frameworkFullPathRef, strData.data(), strData.size(), kCFStringEncodingASCII);
		if (ok) {
			result = std::string(strData.data());
		}
	}
	catch(...) { }
	
	CFRelease(frameworkFullPathRef);
	CFRelease(frameworkFullUrlRef);
	CFRelease(frameworkPathRef);
	CFRelease(frameworkUrlRef);
	CFRelease(bundleUrlRef);
	
	return result;
}

#else

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

#endif

static std::string getLibName()
{
	const std::string name = "libtsvb";

#ifdef __APPLE__
	const std::string ext = ".dylib";
#else
	const std::string ext = ".so";
#endif

	return name + ext;
}

class BGLibraryHandler::Impl
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

BGLibraryHandler::BGLibraryHandler():
	_impl(new Impl)
{
#ifdef __APPLE__
	const std::string path = getBundleFrameworkPath() + "/" + getLibName();
#else
	const std::string absolutePath = getCurrentExecutableDir() + "/" + getLibName();
	bool exists = (access(absolutePath.c_str(), 0) == 0);
	const std::string path = exists ? absolutePath : getLibName();
#endif

	_impl->load(path.c_str());
}

BGLibraryHandler::~BGLibraryHandler()
{
}

tsvb::ISDKFactory* BGLibraryHandler::createSDKFactory()
{
	if ((nullptr != _impl) && _impl->isValid()) {
		return _impl->createFactory();
	}
	return nullptr;
}

bool BGLibraryHandler::isValid() const
{
	return _impl->isValid();
}
