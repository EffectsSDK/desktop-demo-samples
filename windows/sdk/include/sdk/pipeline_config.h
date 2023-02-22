#ifndef TOMSKSOFT_VB_INCLUDE_PIPELINE_CONFIG_H
#define TOMSKSOFT_VB_INCLUDE_PIPELINE_CONFIG_H

#include "irelease.h"

namespace tsvb {

enum Backend : int
{
	CPU = 1,
	GPU = 2
};

class IPipelineConfiguration : public IRelease
{
public:
	virtual void setBackend(int backend) = 0;
	virtual int getBackend() const = 0;
};

}

#endif
