#pragma once
#include "Editor/Src/VersionControl/VCTask.h"

//
// This is called when version control EditorSettings has change the active vc type e.g.
// disabled -> perforce.
// The connect button in the editor inspector also calls this.
// It is also called when the version configuration EditorUserSettings in of the active
// vc is changed e.g. password set.
//
class UpdateSettingsTask : public VCTask
{
public:
	UpdateSettingsTask(const string& pluginName, const EditorUserSettings::ConfigValueMap& m);

	void Execute();

	// In main thread
	virtual void Done ();

private:
	void IntegrityFix();
	std::string m_PluginName;
	EditorUserSettings::ConfigValueMap m_Settings;
};
