#ifndef TOMSKSOFT_VB_INCLUDE_FRAME_FACTORY_H
#define TOMSKSOFT_VB_INCLUDE_FRAME_FACTORY_H

#include "frame.h"

namespace tsvb {

class IFrameFactory : public IRelease
{
public:
	virtual IFrame* createBGRA(
		void* data,
		unsigned int bytesPerLine,
		unsigned int width,
		unsigned int height,
		bool makeCopy
	) = 0;


	virtual IFrame* createNV12(
		void* yData,
		unsigned int yBytesPerLine,
		void* uvData,
		unsigned int uvBytesPerLine,
		unsigned int width,
		unsigned int height,
		bool makeCopy
	) = 0;

	virtual IFrame* loadImage(const char* utf8FilePath) = 0;
};

}

#endif
