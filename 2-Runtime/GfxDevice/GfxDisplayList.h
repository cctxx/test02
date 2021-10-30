#pragma once

#include "Runtime/Threads/ThreadSharedObject.h"

class GfxDisplayList : public ThreadSharedObject
{
public:
	virtual void Call() = 0;
};
