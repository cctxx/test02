#pragma once

class IPlatformSupportModule
{
public:
	virtual const char* GetTargetName() = 0;
	virtual void        OnLoad() = 0;
	virtual void        OnUnload() = 0;
};
