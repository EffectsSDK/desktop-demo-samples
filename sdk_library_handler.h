#ifndef DEMO_APP_SRC_LIBRARY_LOADER_H
#define DEMO_APP_SRC_LIBRARY_LOADER_H

#include <cstddef>
#include <vb_sdk/sdk_factory.h>

#include <memory>

class BGLibraryHandler
{
public:
	BGLibraryHandler();
	~BGLibraryHandler();

	bool isValid() const;

	tsvb::ISDKFactory* createSDKFactory();

private:
	class Impl;
	std::unique_ptr<Impl> _impl;
};

#endif
