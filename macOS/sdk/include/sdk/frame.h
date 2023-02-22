#ifndef TOMSKSOFT_VB_INCLUDE_FRAME_H
#define TOMSKSOFT_VB_INCLUDE_FRAME_H

#include "irelease.h"

namespace tsvb {

enum FrameFormat
{
	bgra32 = 1,
	rgba32 = 2,
	nv12 = 3
};

enum FrameLock
{
	read = 1,
	write = 2,
	readWrite = (read | write)
};


class ILockedFrameData : public IRelease
{
public:
	/// For packed formats use planarIndex = 0
	virtual unsigned int bytesPerLine(int planarIndex) const = 0;
	virtual void* dataPointer(int planarIndex) = 0;
};

class IFrame : public IRelease
{
public:
	virtual int frameFormat() const = 0;

	virtual unsigned int width() const = 0;
	virtual unsigned int height() const = 0;

	virtual ILockedFrameData* lock(int frameLock) = 0;
};

}

#endif
