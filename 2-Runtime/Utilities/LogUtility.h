#pragma once

#include "LogAssert.h"

// Nested log with automatic indentation. NESTED_LOG(name,fmt,...) prints to the console
// and indents further log calls. Log indentation is decreased again when current scope ends.

class NestedLogOutput
{
public:
	NestedLogOutput(const char* name, const std::string& msg)
	{
		printf_console("%s: %*c%s\n", name, s_LogDepth, ' ', msg.c_str());
		s_LogDepth += 4;
	}
	~NestedLogOutput()
	{
		s_LogDepth -= 4;
	}
private:
	static int s_LogDepth;
};

#if !UNITY_RELEASE
#define NESTED_LOG(name,...) NestedLogOutput _nested_log_##__LINE__ (name,Format(__VA_ARGS__))
#else
#define NESTED_LOG(name,...)
#endif
