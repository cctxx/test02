#pragma once
#include <string>
#include <vector>

enum Severity {
	kSevData = 0,      // Debug messages
	kSevVerbose = 1,    // Verbose messages
	kSevInfo = 2,       // Info from vc system
	kSevWarning = 3,    // Warnings from vc system
	kSevError = 4,      // Errors from vc system
	kSevCommand = 64,   // Commands from vc system
	kSevEOR = 128,      // End of response message
	kSevDelim = 129,    // Delimiter message
	kSevProgress = 130  // Alive signal and optional progress status
};

enum MessageArea
{
	kMAGeneral = 1,
	kMASystem = 2,         // Error on local system e.g. cannot create dir
	kMAPlugin = 4,         // Error in protocol while communicating with the plugin executable
	kMAConfig = 8,         // Error in configuration e.g. config value not set
	kMAConnect = 16,       // Error during connect to remote server
	kMAProtocol = 32,      // Error in protocol while communicating with server
	kMARemote = 64,        // Error on remote server
	kMAInvalid = 128,      // Must alway be the last entry
	kMAAll = 0xFFFFFFFF
};

std::string MessageAreaToString(MessageArea ma);

enum VCCommandName
{
	kVCCInvalid = 0,
	kVCCShutdown,
	kVCCAdd,
	kVCCChangeDescription,
	kVCCChangeMove,
	kVCCChangeStatus,
	kVCCChanges,
	kVCCCheckout,
	kVCCConfig,
	kVCCDeleteChanges,
	kVCCDelete,
	kVCCDownload,
	kVCCExit,
	kVCCGetLatest,
	kVCCIncomingChangeAssets,
	kVCCIncoming,
	kVCCLock,
	kVCCLogin,
	kVCCMove,
	kVCCQueryConfigParameters,
	kVCCResolve,
	kVCCRevertChanges,
	kVCCRevert,
	kVCCStatus,
	kVCCSubmit,
	kVCCUnlock,
	kVCCOnline,
	kVCCOffline,
	kVCCEnableCommand,
	kVCCDisableCommand,
	kVCCEnableAllCommands,
	kVCCDisableAllCommands,
	KVCCInvalidTail
};

std::string VCCommandNameToString(VCCommandName n);
VCCommandName StringToVCCommandName(const std::string& str);

class VCMessage
{	
public:
	VCMessage();
	VCMessage(Severity s, const std::string& msg, MessageArea a);
	Severity severity;
	std::string message;
	MessageArea area;
};

typedef std::vector<VCMessage> VCMessages;

// Return true if any errors has been logged
bool LogMessages(const VCMessages& msgs); // must be run on main thread
bool ContainsErrors(const VCMessages& msgs);
