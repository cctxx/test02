#include "UnityPrefix.h"
#include "Editor/Src/EditorUserSettings.h"
#include "VCMessage.h"
#include "VCProvider.h"
#include <iostream>

using namespace std;

string MessageAreaToString(MessageArea ma)
{
	switch (ma)
	{
		case kMAGeneral:	return "General"; 
		case kMASystem:		return "System"; 
		case kMAPlugin:		return "Plugin"; 
		case kMAConfig:		return "Config"; 
		case kMAConnect:	return "Connect"; 
		case kMAProtocol:	return "Protocol"; 
		case kMARemote:		return "Remote"; 
		case kMAInvalid:	return "Invalid"; 
		case kMAAll:		return "All"; 
		default:
			return "UnknownMessageArea"; 
	}
}

// This list must match order in header file
static const char* infos[] = {
	"<invalid>",
	"shutdown",
	"add", //
	"changeDescription", //
	"changeMove", //
	"changeStatus", //
	"changes", //
	"checkout", //  
	"pluginConfig", //
	"deleteChanges", //
	"delete", //
	"download", //
	"getLatest", //
	"incomingChangeAssets", //
	"incoming", //
	"lock", //
	"login", //
	"move", //
	"resolve", //
	"revertChanges", //
	"revert", //
	"status", //
	"submit", //
	"unlock", //
	"online", // From plugin to unity
	"offline", // From plugin to unity
	"enableCommand", // From plugin to unity
	"disableCommand", // From plugin to unity
	"enableAllCommands", // From plugin to unity
	"disableAllCommands", // From plugin to unity
	"<invalidTail>"
};

static const size_t infosSize = (sizeof(infos) / sizeof(infos[0])) - 1;

string VCCommandNameToString(VCCommandName n)
{
	if (n >= infosSize)
		return "<invalid>";
	return infos[n];
}

VCCommandName StringToVCCommandName(const string& str)
{
	for (size_t i = 0; i < infosSize; i++)
		if (str == infos[i])
			return static_cast<VCCommandName>(i);
	return kVCCInvalid;
}

bool ContainsErrors(const VCMessages& msgs)
{
	for (VCMessages::const_iterator i = msgs.begin(); i != msgs.end(); ++i)
	{
		if (i->severity == kSevError)
			return true;
	}
	return false;
}

static void PlainLog(const std::string& msg)
{
	DebugStringToFilePostprocessedStacktrace(msg.c_str(), "", "", 0, "", 0, kLog);
}

static bool LogMessage(const string& msg, Severity sev, Severity lowestWantedSeverity)
{
	string formattedMsg = string("Version Control: ") + msg;

	if (msg.empty())
	{
		// nothing to log
	}
	else if (sev == kSevError)
	{
		ErrorString(formattedMsg);
		return false;
	}
	else if (sev == kSevWarning && kSevWarning >= lowestWantedSeverity)
	{
		WarningString(formattedMsg);
	}
	else if (sev == kSevInfo && kSevInfo >= lowestWantedSeverity)
	{
		PlainLog(formattedMsg);
	}
	else if (sev == kSevVerbose && kSevVerbose >= lowestWantedSeverity)
	{
		PlainLog(formattedMsg);
	}
	return true;
}

bool LogMessages(const VCMessages& msgs)
{
	bool success = true;
	string logLevel = GetEditorUserSettings().GetConfigValue("vcSharedLogLevel");
	Severity lowestWantedSeverity = kSevError;
	if (logLevel == "verbose")
		lowestWantedSeverity = kSevVerbose;
	else if (logLevel == "info")
		lowestWantedSeverity = kSevInfo;
	else if (logLevel == "notice")
		lowestWantedSeverity = kSevWarning;
	else if (logLevel == "fatal")
		lowestWantedSeverity = kSevError;
	
	Severity lastSev = kSevData;
	string msg;
	
	for (VCMessages::const_iterator i = msgs.begin(); i != msgs.end(); ++i)
	{
		if (i->area >= kMAProtocol && GetVCProvider().GetOnlineState() != kOSOnline)
			continue; // Skip protocol messages as long as the plugin has not stated that it is online
		
		bool isLoggingSeverityType = i->severity == kSevError || i->severity == kSevWarning || i->severity == kSevInfo || i->severity == kSevVerbose;

		if (!isLoggingSeverityType)
			continue;
		
		if (i->severity == lastSev)
		{
			msg += "\n";
			msg += i->message;
		}
		else
		{
			success = success && LogMessage(msg, lastSev, lowestWantedSeverity);
			msg = i->message;
			lastSev = i->severity;
		}
	}
	
	// Check for remaining msg to be logged
	success = success && LogMessage(msg, lastSev, lowestWantedSeverity);
	
	return success;
}

VCMessage::VCMessage() : severity(kSevVerbose), area(kMAGeneral) 
{
}

VCMessage::VCMessage(Severity s, const std::string& msg, MessageArea a) 
	: severity(s), message(msg), area(a) 
{
}
