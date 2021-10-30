#include "UnityPrefix.h"
#include "VCPlugin.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/Word.h"

using std::string;

static const char COMMAND_PREFIX = 'c';
static const char VERBOSE_PREFIX = 'v';
static const char DATA_PREFIX = 'o';
static const char ERROR_PREFIX = 'e';
static const char WARNING_PREFIX = 'w';
static const char INFO_PREFIX = 'i';
static const char EOR_PREFIX = 'r';   // end of response delimiter
static const char DELIM_PREFIX = 'd'; // general delimiter
static const char PROGRESS_PREFIX = 'p';

VCPluginException::VCPluginException(const VCMessages& m) : m_Messages(m)
{}

VCPluginException::VCPluginException(const VCMessage& m)
{
	m_Messages.push_back(m);
}

VCPluginException::VCPluginException(Severity s, const std::string& message, MessageArea a)
{
	VCMessage m;
	m.severity = s;
	m.area = a;
	m.message = message;
	m_Messages.push_back(m);
}

const char* VCPluginException::what() const throw() 
{ 
	if (m_Messages.empty()) return "";
	return m_Messages.back().message.c_str(); 
}

const VCMessages& VCPluginException::GetMessages() const 
{
	return m_Messages;
}

VCPluginSession::VCPluginSession(const std::string& name) :
	m_Proc(0),
	m_MessageOffsetForLastCheckpoint(0),
	m_ProgressListener(NULL)
{

	
}

VCPluginSession::VCPluginSession(ExternalProcess* p) :
	m_Proc(p),
	m_MessageOffsetForLastCheckpoint(0),
	m_ProgressListener(NULL)
{

}

VCPluginSession::~VCPluginSession()
{
	Kill();
	ClearMessages();
}

void VCPluginSession::Reset(ExternalProcess* p)
{
	Kill();
	ClearMessages();
	m_Proc = p;
}

bool VCPluginSession::IsRunning()
{
	return m_Proc != NULL && m_Proc->IsRunning();
}

void VCPluginSession::Kill()
{
	if (m_Proc == NULL) return;
	if (m_Proc->IsRunning())
	{
		double to = m_Proc->GetReadTimeout();
		m_Proc->SetReadTimeout(1.0); // wait a sec for the plugin to shutdown
		try
		{
			SendCommand("shutdown");
			m_Proc->ReadLine(); // read ack
		}
		catch (ExternalProcessException &ex)
		{
			// ignore
		}
		m_Proc->SetReadTimeout(to);
	}
	UNITY_DELETE(m_Proc, kMemVersionControl);
	m_Proc = NULL;
}

bool VCPluginSession::KillIfAreaError(int areas)
{
	VCMessages::const_iterator i = m_Messages.begin();
	i += m_MessageOffsetForLastCheckpoint;
	
	for ( ; i != m_Messages.end(); ++i)
	{
		if (i->area & areas)
		{
			Kill();
			return true;
		}
	}
	return false;
}

void VCPluginSession::ThrowOnAreaError(int areas)
{
	m_ThrowOnAreaError = areas;
}

void VCPluginSession::SendCommand(const string& name)
{
	// WarningString(string("Issuing command ") + name + " " + param);
	WriteLine(Format("%c:%s", COMMAND_PREFIX, name.c_str()));
}

void VCPluginSession::SendCommand(const string& name,
								  const string& param1)
{
	// WarningString(string("Issuing command ") + name + " " + param);
	WriteLine(Format("%c:%s %s", COMMAND_PREFIX, name.c_str(), param1.c_str()));
}

void VCPluginSession::SendCommand(const string& name,
								  const string& param1,
								  const string& param2)
{
	// WarningString(string("Issuing command ") + name + " " + param);
	WriteLine(Format("%c:%s %s %s", COMMAND_PREFIX, name.c_str(), param1.c_str(), param2.c_str()));
}

void VCPluginSession::SendCommand(const string& name,
								  const string& param1,
								  const string& param2,
								  const string& param3)
{
	// WarningString(string("Issuing command ") + name + " " + param);
	WriteLine(Format("%c:%s %s %s %s", COMMAND_PREFIX, name.c_str(), 
				 param1.c_str(), param2.c_str(), param3.c_str()));
}

static void ParseMessage(const string& line, VCMessage& msg)
{
	// All statements read from the plugin have the format: 
	// <severity><area>:<message>
	//
	// Where: 
	// <severity> is one of 'e:'. 'w:' or 'o:' meaning error,warning,ok
	// <area> is one of the int value of the MessageArea enum
	// <message> is the message itself
	//
	string::size_type len = line.length();
	
	if (len < 3)
	{
		msg.severity = kSevError;
		msg.area = kMAPlugin;
		msg.message = "Read invalid statement from revision control plugin";
		return;
	}
	
	char msgType = line[0];
	switch (msgType) {
		case COMMAND_PREFIX:
			msg.severity = kSevCommand;
			break;
		case ERROR_PREFIX:
			msg.severity = kSevError;
			break;
		case WARNING_PREFIX:
			msg.severity = kSevWarning;
			break;
		case INFO_PREFIX:
			msg.severity = kSevInfo;
			break;
		case DATA_PREFIX:
			msg.severity = kSevData;
			break;
		case VERBOSE_PREFIX:
			msg.severity = kSevVerbose;
			break;
		case EOR_PREFIX:
			msg.severity = kSevEOR;
			break;
		case DELIM_PREFIX:
			msg.severity = kSevDelim;
			break;
		case PROGRESS_PREFIX:
			msg.severity = kSevProgress;
			break;
		default:
			msg.severity = kSevError;
			msg.area = kMAPlugin;
			msg.message = "Read invalid message type from revision control plugin";
			return;
			break;
	}
	
	string::size_type i = line.find(':');
	if (i == string::npos) 
	{
		// cannot be the second char according to format above
		msg.severity = kSevError;
		msg.area = kMAPlugin;
		msg.message = "No message delimiter from revision control plugin";
		return;
	}
	MessageArea area = kMAGeneral;
	if (i != 1)
		area = static_cast<MessageArea>(StringToInt(line.substr(1, i - 1)));

	msg.area = area;
	
	if (msg.area < kMAGeneral || msg.area >= kMAInvalid)
	{
		msg.severity = kSevError;
		msg.area = kMAPlugin;
		msg.message = "Read invalid area from revision control plugin";
		return;
	}
	
	if (i+1 == len)
		msg.message.clear();
	else 
		msg.message = line.substr(i+1);

}

void escapeString(string& target)
{
	replace_string(target, "\\", "\\\\", 0);
	replace_string(target, "\n", "\\n", 0);
}


void unescapeString(string& target)
{
	string::size_type len = target.length();
	std::string::size_type n1 = 0;
	std::string::size_type n2 = 0;
	
	while ( n1 < len && (n2 = target.find('\\', n1)) != std::string::npos &&
		   n2+1 < len )
	{
		char c = target[n2+1];
		if ( c == '\\' )
		{
			target.replace(n2, 2, "\\");
			len--;
		}
		else if ( c == 'n')
		{
			target.replace(n2, 2, "\n");
			len--;
		}
		n1 = n2 + 1;
	}
}

double VCPluginSession::GetReadTimeout() const
{
	return m_Proc->GetReadTimeout();
}

void VCPluginSession::SetReadTimeout(double secs)
{
	m_Proc->SetReadTimeout(secs);
}

void VCPluginSession::SkipLinesUntilEndResponse()
{
	VCMessage msg;
	ExternalProcess& p = *m_Proc;
	string line = p.PeekLine();
	while ( line.empty() || line[0] != EOR_PREFIX) // read until reaching end of response
	{
		p.ReadLine();
		unescapeString(line);
		ParseMessage(line, msg);
		m_Messages.push_back(msg);
		line = p.PeekLine();
	}
	p.ReadLine();
}

// Reads a line from the process and filters error lines
string VCPluginSession::ReadLine(bool setCheckpoint)
{
	VCMessage msg;
	PeekLine(setCheckpoint, msg);
	m_Proc->ReadLine();
	if (msg.severity == kSevEOR)
	{
		const VCMessages& msgs = GetMessages(true);
		if (msgs.empty())
			throw VCPluginException(kSevError, "Premature end of version control response", kMAPlugin);
		else
			throw VCPluginException(GetMessages(true));
	}
	return msg.message;
}

string VCPluginSession::PeekLine(bool setCheckpoint)
{
	VCMessage msg;
	PeekLine(setCheckpoint, msg);
	return msg.message;
}

void VCPluginSession::PeekLine(bool setCheckpoint, VCMessage& msg)
{
	if (setCheckpoint)
		MessageCheckpoint();

	ExternalProcess& p = *m_Proc;
	
	while (true)
	{		
		string l = p.PeekLine();
		unescapeString(l);
		ParseMessage(l, msg);

		if ( msg.severity == kSevError && 
			((msg.area & m_ThrowOnAreaError) || msg.area == kMAPlugin) )
		{
			SkipLinesUntilEndResponse();
			throw VCPluginException(GetMessages(true)); // abort
		}

		AddMessage(msg);

		if (msg.severity == kSevData || msg.severity == kSevEOR || msg.severity == kSevDelim)
		{
			return;
		}
		else if (msg.severity == kSevProgress && m_ProgressListener)
		{
			string::size_type i1 = msg.message.find(' ');
			string::size_type i2 = msg.message.find(' ', i1 == string::npos ? 0 : i1 + 1);
			string progressMessage = (i2+1) >= msg.message.length() ? string() : msg.message.substr(i2+1);
			if (i1 != string::npos)
			{
				int pctDone = StringToInt(msg.message.substr(0, i1));
				int timeSpent = StringToInt(msg.message.substr(i1+1, i2));
				m_ProgressListener->OnProgress(pctDone, timeSpent, progressMessage);
			}
		}
		
		// Remove the just peeked line because it was not a plain message
		// but an error or something. Then re-peek.
		p.ReadLine();
	}
}

void VCPluginSession::Write(const string& msg)
{
	string tmp(msg);
	escapeString(tmp);
	m_Proc->Write(tmp);
}

void VCPluginSession::WriteLine(const string& msg)
{
	string tmp(msg);
	escapeString(tmp);
	m_Proc->Write(tmp + "\n");
}

void VCPluginSession::ClearMessages()
{
	m_Messages.clear();
	MessageCheckpoint();
}

void VCPluginSession::AddMessage(Severity s, const std::string& message, MessageArea a)
{
	VCMessage m;
	m.severity = s;
	m.area = a;
	m.message = message;
	AddMessage(m);
}

void VCPluginSession::AddMessage(const VCMessage& m)
{
	m_Messages.push_back(m);
}

const VCMessages& VCPluginSession::GetMessages(bool sinceLastCheckpoint) const
{
	static VCMessages checkpointMessages;
	if (!sinceLastCheckpoint)
		return m_Messages;
	
	checkpointMessages.clear();
	size_t sz = m_Messages.size();

	if (m_MessageOffsetForLastCheckpoint >= sz)
		return checkpointMessages;
	
	VCMessages::const_iterator i = m_Messages.begin();
	i += m_MessageOffsetForLastCheckpoint;
	checkpointMessages.reserve(sz - m_MessageOffsetForLastCheckpoint);
	for ( ; i != m_Messages.end(); ++i)
		checkpointMessages.push_back(*i);
	
	return checkpointMessages;
}

/*
VCMessage VCPluginSession::GetFirstErrorSinceCheckpoint()
{
	VCMessages::const_iterator i = m_Messages.begin();
	i += m_MessageOffsetForLastCheckpoint;
	
	for ( ; i != m_Messages.end(); ++i)
	{
		if (i->severity == kSevError)
		{
			return *i;
		}
	}
	return VCMessage();	
}
*/

void VCPluginSession::MessageCheckpoint()
{
	m_MessageOffsetForLastCheckpoint = m_Messages.size();
}
