#pragma once
#include <string>
#include <vector>
#include <set>
#include <exception>
#include "Editor/Platform/Interface/ExternalProcess.h"
#include "VCMessage.h"

class VCPlugin;

class VCPluginException : public std::exception
{
public:
	VCPluginException(const VCMessages& m);
	VCPluginException(const VCMessage& m);
	virtual ~VCPluginException() throw() {};
	VCPluginException(Severity s, const std::string& message, MessageArea a);
	virtual const char* what() const throw();
	const VCMessages& GetMessages() const;
private:
	VCMessages m_Messages;
};


class VCProgressListener
{
public:
	virtual void OnProgress(int pctDone, int totalTimeSpent, const std::string& msg) = 0;
};


class VCPluginSession
{
public:
	VCPluginSession(const std::string& name);
	VCPluginSession(ExternalProcess* p = 0);
	~VCPluginSession();
	
	// Kill any existing process, clear messages and associate 
	// new process with this session
	void Reset(ExternalProcess* p);
	
	bool IsRunning();
	
	void SendCommand(const std::string& name);
	void SendCommand(const std::string& name, const std::string& param1);
	void SendCommand(const std::string& name, const std::string& param1, const std::string& param2);
	void SendCommand(const std::string& name, const std::string& param1, const std::string& param2, const std::string& param3);

	std::string ReadLine(bool setCheckpoint = false);
	std::string PeekLine(bool setCheckpoint = false);
	void PeekLine(bool setCheckpoint, VCMessage& msg);

	void Write(const std::string& msg);
	void WriteLine(const std::string& msg);

	void Kill(); // Kill the process if it is running.

	// Kill the process if there has been any messages with error in the areas since last checkpoint
	bool KillIfAreaError(int areas); 

	// Automatically throw an exception when an error message is detected in the areas specified
	void ThrowOnAreaError(int areas); 

	void ClearMessages();

	// Since reading a single line or a structure can perform many
	// reads which themself can have out-of-band error, warning messages
	// checkpoints are utilized. This way it is possible to receive
	// messages associated with a given protocol task by setting
	// the checkpoint before starting it and checking messages sinceLastCheckpoint
	// right after.
	const VCMessages& GetMessages(bool sinceLastCheckpoint = true) const;
	void MessageCheckpoint();
	void SkipLinesUntilEndResponse();
	
	double GetReadTimeout() const;
	void SetReadTimeout(double secs);
	void SetProgressListener(VCProgressListener* listener) { m_ProgressListener = listener; }
	
private:
	ExternalProcess* m_Proc;
	VCProgressListener* m_ProgressListener;
	
	VCMessages m_Messages;
	void AddMessage(Severity s, const std::string& message, MessageArea ma = kMAGeneral);
	void AddMessage(const VCMessage& m);
	
	size_t m_MessageOffsetForLastCheckpoint;

	int m_ThrowOnAreaError; // Bit field describing which areas that should kill the session when they have errors
	friend class VCPlugin; // Allow VCPlugin to set messages on this when creating the session
};

template <typename T>
VCPluginSession& operator<<(VCPluginSession& p, const std::vector<T>& v)
{
	p.WriteLine(IntToString(v.size()));
	for (typename std::vector<T>::const_iterator i = v.begin(); i != v.end(); ++i)
		p << *i;
	return p;
}

template <typename T>
VCPluginSession& operator<<(VCPluginSession& p, const std::set<T>& v)
{
	p.WriteLine(IntToString(v.size()));
	for (typename std::set<T>::const_iterator i = v.begin(); i != v.end(); ++i)
		p << *i;
	return p;
}

template <typename T>
VCPluginSession& operator>>(VCPluginSession& p, std::vector<T>& v)
{
	std::string line = p.ReadLine();
	int count = atoi(line.c_str());
	T t;
	if (count >= 0)
	{
		while (count--)
		{
			p >> t;
			v.push_back(t);
		}
	} 
	else 
	{
		// Newline delimited list
		VCMessage msg;
		p.PeekLine(false, msg);
		while (msg.severity != kSevDelim)
		{
			p >> t;
			v.push_back(t);
			p.PeekLine(false, msg);
		}
		p.ReadLine();
	}
	return p;
}

// When c++11 is mandated this should use functional partial templates instead
// of duplicating code
template <typename T>
VCPluginSession& operator>>(VCPluginSession& p, std::set<T>& v)
{
	std::string line = p.ReadLine();
	int count = atoi(line.c_str());
	T t;
	if (count >= 0)
	{
		while (count--)
		{
			p >> t;
			v.insert(t);
		}
	}
	else
	{
		// Newline delimited list
		VCMessage msg;
		p.PeekLine(false, msg);
		while (msg.severity != kSevDelim)
		{
			p >> t;
			v.insert(t);
			p.PeekLine(false, msg);
		}
		p.ReadLine();
	}
	return p;
}

