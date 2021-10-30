#pragma once

#if UNITY_OSX
	#ifdef __OBJC__
		@class NSTask;
		@class NSFileHandle;
		@class NSPipe;
		@class NSMutableData;
	#else
		typedef struct objc_object NSTask;
		typedef struct objc_object NSFileHandle;
		typedef struct objc_object NSPipe;
		typedef struct objc_object NSMutableData;
	#endif
#endif

#if UNITY_WIN
#include "PlatformDependent/Win/WinUtils.h"
#endif

enum ExternalProcessState {
	EPSTATE_NotRunning,
	EPSTATE_TimeoutReading,
	EPSTATE_TimeoutWriting,
	EPSTATE_BrokenPipe
};


class ExternalProcessException
{
public:
	ExternalProcessException(ExternalProcessState st, const std::string& msg = "") : m_State(st), m_Msg(msg) {}
	const std::string& Message() const throw() { return m_Msg; }
	const char* what() const throw() {  return m_Msg.c_str(); } // standard
private:
	const ExternalProcessState m_State;
	const std::string m_Msg;
};


class ExternalProcess
{
public:
	ExternalProcess(const std::string& app, const std::vector<std::string>& arguments);
	~ExternalProcess();
	bool Launch();
	bool IsRunning();
	bool Write(const std::string& data);
	std::string ReadLine();
	std::string PeekLine();
	void Shutdown();
	void SetReadTimeout(double secs);
	double GetReadTimeout();
	void SetWriteTimeout(double secs);
	double GetWriteTimeout();
private:
	void Cleanup();

	bool m_LineBufferValid;
	std::string m_LineBuffer;
	const std::string m_ApplicationPath;
	const std::vector<std::string> m_Arguments;
	double m_ReadTimeout;
	double m_WriteTimeout;

#if UNITY_OSX
	NSFileHandle* m_ReadHandle;
	NSFileHandle* m_WriteHandle;
	NSPipe* m_PipeIn;
	NSPipe* m_PipeOut;
	NSTask* m_Task;
	NSMutableData* m_Buffer;
#elif UNITY_WIN
	PROCESS_INFORMATION m_Task;

	OVERLAPPED m_Overlapped;
	HANDLE m_NamedPipe;
	HANDLE m_Event; // Read or Write event

	// Main task writes to m_Task_stdin_wr and child task reads from m_Task_stdin_rd
	winutils::AutoHandle m_Task_stdin_rd;
	winutils::AutoHandle m_Task_stdin_wr;

	// Main task reads m_Task_stdout_rd and child task writes to m_Task_stdout_wr
	winutils::AutoHandle m_Task_stdout_rd;
	winutils::AutoHandle m_Task_stdout_wr;

	std::string m_Buffer;
#endif
};
