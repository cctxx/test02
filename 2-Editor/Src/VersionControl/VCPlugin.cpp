#include "UnityPrefix.h"
#include "VCPlugin.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Platform/Interface/ExternalProcess.h"


using std::set;
using std::string;

VCConfigField::VCConfigField(const std::string& name, const string& label, const std::string& desc, const string& defaultValue, int flags)
: m_Name(name), m_Label(label), m_Description(desc), m_DefaultValue(defaultValue), m_Flags(flags)
{

}

string ExtractFileName(const string& path)
{
	string::size_type i = path.find_last_of('/');
	if (i == string::npos || i == path.length() - 1) return "";
	i++; // skip the /

	string::size_type iend = path.find_last_of('.');
	if (iend < i || iend == string::npos) return path.substr(i);

	return path.substr(i, iend - i);
}

string GetPluginNameFromPath (const std::string& path)
{
	string filename = ExtractFileName(path);
	if (!EndsWith(filename, "Plugin"))
		return string();
	
	string pluginName = filename.substr(0, filename.length() - 6);
	return pluginName;
}



////@TODO: Review thread safety conventions....

static string GetVersionControlPluginFolder()
{
	return AppendPathName(AppendPathName(GetApplicationContentsPath(), "Tools"), "VersionControl");
}

VCPlugin::VCPlugin(const string& path, const string& name) : m_Path(path), m_Name(name), m_GotTraits(false)
{
}

void VCPlugin::ScanAvailablePlugins(VCPlugins& plugins)
{
	set<string> paths;
	GetFolderContentsAtPath(GetVersionControlPluginFolder(), paths);
	for (set<string>::const_iterator i = paths.begin(); i != paths.end(); ++i)
	{
		string filename = ExtractFileName(*i);
		if (EndsWith(filename, "Plugin"))
		{
			string name = GetPluginNameFromPath(*i);
			
			// TODO: Remove condition when subversion is to be visible again
			if (name != "Subversion")
				plugins.insert(make_pair(name, VCPlugin (*i, name)));
		}
	}
}
 
VCMessages VCPlugin::NegotiateTraits() 
{
	VCMessages errMsg;
	try 
	{
		VCPluginSession * p = Launch();
		errMsg = p->GetMessages();
		UNITY_DELETE(p, kMemVersionControl);
	}
	catch (VCPluginException& e) 
	{
		// Session is dead at this point
		errMsg = e.GetMessages();
	}
	return errMsg;
}

VCPlugin& VCPlugin::CreatePlugin(const std::string& name) 
{
	VCPlugins plugins;
	ScanAvailablePlugins (plugins);
	
	VCPlugins::iterator i = plugins.find(name);
	if (i == plugins.end() || i->second.IsNull())
		throw VCPluginException(kSevError, string("Unknown version control plugin: ") + name, kMASystem);
	
	VCPlugin* plugin = UNITY_NEW(VCPlugin, kMemVersionControl)(i->second.GetPath(), i->second.GetName());
	return *plugin;
}

VCPluginSession* VCPlugin::Launch() 
{
	VCPluginSession * session = UNITY_NEW(VCPluginSession, kMemVersionControl)();
	ExternalProcess * p = UNITY_NEW(ExternalProcess, kMemVersionControl)(GetPath(), std::vector<string>());
	session->ThrowOnAreaError(kMAAll); // No errors allowed in negotiations

	try
	{

		if (p == NULL)
		{
			session->AddMessage(kSevError, 
								string("Error launching version control plugin : ") + GetPath(),
								kMASystem);
			throw VCPluginException(session->GetMessages(true)); // abort
		}
		
		if (!p->Launch())
		{
			UNITY_DELETE(p, kMemVersionControl);
			session->AddMessage(kSevError, 
								string("Error launching version control plugin: ") + GetPath(),
								kMASystem);
			throw VCPluginException(session->GetMessages(true)); // abort
		}
		
		// The process is running. Associate it with the session.
		session->Reset(p);
		session->MessageCheckpoint();
	
		// Negotiate protocol version
		const char* kSupportedPluginVersions = "1 2"; // space separated
		session->SendCommand("pluginConfig", "pluginVersions", kSupportedPluginVersions);
		string selectedVersionStr = session->ReadLine();
		session->SkipLinesUntilEndResponse();
		int selectedVersion = StringToInt(selectedVersionStr);

		// version 1 == unity 4.2
		// version 2 == unity 4.3
		if (selectedVersion != 1 && selectedVersion != 2 )
		{
			session->AddMessage(kSevError, 
								string("Unsupported plugin version ") + selectedVersionStr,
								kMAPlugin);
			throw VCPluginException(session->GetMessages(true)); // abort
		}
		
		session->SendCommand("pluginConfig", "projectPath", File::GetCurrentDirectory());
		session->SkipLinesUntilEndResponse();

		// Read traits
		session->SendCommand("pluginConfig", "pluginTraits");
		int traitCount = StringToInt(session->ReadLine());
		
		const int kMaxTraitCount = 30;
		if (traitCount > kMaxTraitCount || traitCount < 0)
		{
			// throws
			session->SkipLinesUntilEndResponse();
			session->AddMessage(kSevError, 
								string("Version control plugin exceeded allowed fields: ") + IntToString(traitCount),
								kMAPlugin);
			throw VCPluginException(session->GetMessages(true)); // abort
		}
			
		m_Traits.Reset();
		m_CustomCommands.clear();

		string t;
		while (traitCount--)
		{
			t = session->ReadLine(); // field name
			if (t == "requiresNetwork")
				m_Traits.requiresNetwork = true;
			if (t == "enablesCheckout")
				m_Traits.enablesCheckout = true;
			if (t == "enablesLocking")
				m_Traits.enablesLocking = true;
			if (t == "enablesRevertUnchanged")
				m_Traits.enablesRevertUnchanged = true;
			if (t == "enablesVersioningFolders")
				m_Traits.enablesVersioningFolders = true;
			if (t == "enablesChangelists")
				m_Traits.enablesChangelists = true;
			if (t == "enablesGetLatestOnChangeSetSubset")
				m_Traits.enablesGetLatestOnChangeSetSubset = true;
			if (t == "enablesConflictHandlingByPlugin")
				m_Traits.enablesConflictHandlingByPlugin = true;
		}
		
		int cfgCount = StringToInt(session->ReadLine());
		
		if (cfgCount > kMaxTraitCount || cfgCount < 0)
		{
			// throws
			session->SkipLinesUntilEndResponse();
			session->AddMessage(kSevError, 
								string("Version control plugin exceeded allowed config fields : ") + IntToString(cfgCount),
								kMAPlugin);
			throw VCPluginException(session->GetMessages(true)); // abort
		}

		string name;
		string label;
		string desc;
		string defVal;
		int flags;

		VCConfigFields fields;
		fields.reserve(cfgCount);
		
		while (cfgCount--)
		{
			name = session->ReadLine(); // field name
			label = session->ReadLine(); // field label
			desc = session->ReadLine(); // field description
			defVal = session->ReadLine(); // field default value
			flags = StringToInt(session->ReadLine()); // field flags
			
			fields.push_back(VCConfigField(name, label, desc, defVal, flags));
		}

		m_OverlayIconForState.clear();
		VCMessage msg;
		session->PeekLine(false, msg);

		// TODO: use normal dispatch for commands
		if (msg.severity == kSevData && msg.message == "overlays")
		{
			// Custom overlay icons is a >4.2 feature
			session->ReadLine();
			int overlayCount = StringToInt(session->ReadLine());
			const int kMaxOverlayCount = 40;
			if (overlayCount > kMaxOverlayCount || overlayCount < 0)
			{
				// throws
				session->SkipLinesUntilEndResponse();
				session->AddMessage(kSevError,
									string("Version control plugin exceeded allowed overlay fields : ") + IntToString(overlayCount),
									kMAPlugin);
				throw VCPluginException(session->GetMessages(true)); // abort
			}
			
			string path;
			int state;
			const string pluginDir = AppendPathName(GetVersionControlPluginFolder(), m_Name);
			while (overlayCount--)
			{
				state = StringToInt(session->ReadLine());
				path = session->ReadLine();
				if (path != "default" && path != "blank")
					path = AppendPathName(pluginDir, path);
				m_OverlayIconForState[state] = path;
			}
		}

		session->PeekLine(false, msg);
		
		if (msg.severity == kSevData && msg.message == "customCommands")
		{
			// Custom global commands is a >4.2 feature
			session->ReadLine();
			int commandCount = StringToInt(session->ReadLine());
			const int kMaxCommandCount = 40;
			if (commandCount > kMaxCommandCount || commandCount < 0)
			{
				// throws
				session->SkipLinesUntilEndResponse();
				session->AddMessage(kSevError,
					string("Version control plugin exceeded allowed custom command fields : ") + IntToString(commandCount),
					kMAPlugin);
				throw VCPluginException(session->GetMessages(true)); // abort
			}

			string context;
			while (commandCount--)
			{
				name = session->ReadLine();
				label = session->ReadLine();
				context =session->ReadLine();
				if (context != "global")
				{
					session->AddMessage(kSevWarning, Format("Plugin %s had custom command with unknown context %s", m_Name.c_str(), context.c_str()));
					continue;
				}
				VCCustomCommand ci = { name, label, kVCCommandContextGlobal };
				m_CustomCommands.push_back(ci);
			}
		}

		session->SkipLinesUntilEndResponse();

		m_Traits.configFields.swap(fields);
	
		m_GotTraits = true;
	}
	catch (VCPluginException& e)
	{
		session->ThrowOnAreaError(0);
		UNITY_DELETE(session, kMemVersionControl);
		throw;
	}
	session->ThrowOnAreaError(0);
	return session;
}
