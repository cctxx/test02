#pragma once
#include <string>
#include <set>
#include <vector>
#include "Runtime/Utilities/GUID.h"
#include "VCPluginProtocol.h"

// Metadata flags for a VCConfigField
enum Flags 
{
	kNoneField = 0,
	kRequiredField = 1,
	kPasswordField = 2,
};

// A vector because we want to preserve order
class VCConfigField;
typedef std::vector<VCConfigField> VCConfigFields;

class VCPlugin;
typedef std::map<std::string,VCPlugin> VCPlugins;


/** 
 This class specifies configuration fields that a plugin process has informed the VCPlugin class
 that it has 
 See VCPlugin::NegotiateTraits().
 */
class VCConfigField
{
public:
	VCConfigField() {} 
	VCConfigField(const std::string& name, const std::string& label, const std::string& desc, const std::string& defaultValue, int flags);
	
	const std::string& GetName() const { return m_Name; }
	const std::string& GetLabel() const { return m_Label; }
	const std::string& GetDescription() const { return m_Description; }
	const std::string& GetDefaultValue() const { return m_DefaultValue; }
	bool IsRequired() const { return m_Flags & kRequiredField; }
	bool IsPassword() const { return m_Flags & kPasswordField; }
	int GetFlags() const { return m_Flags; }
	UnityGUID m_guid;
private:
	std::string m_Name;
	std::string m_Label;
	std::string m_Description;
	std::string m_DefaultValue;
	int m_Flags;
};

enum VCCommandContext
{
	kVCCommandContextGlobal = 1
};
	
struct VCCustomCommand
{
	std::string name;
	std::string label;
	VCCommandContext context;
};
typedef std::vector<VCCustomCommand> VCCustomCommands;

/** 
 This class can scan for plugins in a folder and manage them ie. negotiating
 config variables that they need send to them and launching and subprocess
 running the plugin. 
 
 Once the plugin process has been started all communication are done through
 the returned VCPluginSession instance.
 */
class VCPlugin
{
public:
	typedef std::map<int, std::string> OverlayIconStateMap;

	struct Traits
	{
		// e.g. Perforce cannot be used without network
		bool requiresNetwork;
		bool enablesCheckout;
		bool enablesLocking;
		bool enablesRevertUnchanged;
		bool enablesVersioningFolders;
		bool enablesChangelists;
		bool enablesGetLatestOnChangeSetSubset; // and not just on entire changesets
		bool enablesConflictHandlingByPlugin;
		
		// The plugin will report a list of names to use
		// for config options in the gui.
		VCConfigFields configFields;
		void Reset()
		{
			requiresNetwork = false;
			enablesCheckout = false;
			enablesLocking = false;
			enablesRevertUnchanged = false;
			enablesVersioningFolders = false;
			enablesChangelists = false;
			enablesGetLatestOnChangeSetSubset = false;
			enablesConflictHandlingByPlugin = false;
			configFields.clear();
		}
	};
	
	VCPlugin(const std::string& path, const std::string& name);
	const std::string& GetName() const { return m_Name; }
	const std::string& GetPath() const { return m_Path; }
	const VCConfigFields& GetConfigFields() const { return m_Traits.configFields; }
	bool RequiresNetwork() const { return m_Traits.requiresNetwork; }
	const Traits& GetTraits() const { return m_Traits; }
	const OverlayIconStateMap& GetOverlayIconStateMap() const { return m_OverlayIconForState; }
	const VCCustomCommands& GetCustomCommands() const { return m_CustomCommands; }
	const bool IsNull() const { return m_Name.empty(); }
	
	// This will spawn the plugin temporarily, negotiate and the shut it down immediately.
	// This same negotiation is done when Launch(..) is called.
	VCMessages NegotiateTraits();
	
	static VCPlugin& CreatePlugin(const std::string& name);

	VCPluginSession* Launch();

	static void ScanAvailablePlugins(VCPlugins& plugins);
	
	UnityGUID m_guid;
	
private:


	// Path to the plugin executable
	std::string m_Path;

	// The name presented in the GUI as derived from 
	// the path name of the plugin e.g. PerforcePlugin.so
	// becomes Perforce.
	std::string m_Name;
	
	// Overlay icon paths for a state
	OverlayIconStateMap m_OverlayIconForState;

	// Custom commands that the plugin provides
	VCCustomCommands m_CustomCommands;

	// This structure is filled upon request
	// by spawning the plugin and requesting traits
	// from it.
	Traits m_Traits;
	bool m_GotTraits;
};
