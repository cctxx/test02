#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/VersionControl/VCPlugin.h"
#include <map>

class EditorUserSettings : public Object
{
public:	
	REGISTER_DERIVED_CLASS (EditorUserSettings, Object)
	DECLARE_OBJECT_SERIALIZE(EditorUSerSettings)

	typedef std::map<UnityStr,UnityStr> ConfigValueMap;
	
	bool HasConfigValue(const UnityStr& name) const;
	UnityStr GetConfigValue(const UnityStr& name) const;
	const ConfigValueMap& GetConfigValues() const { return m_ConfigValues; }
	void SetConfigValue(const UnityStr& name, const UnityStr& vccf);
	
	GET_SET_DIRTY(bool, VCAutomaticAdd, m_VCAutomaticAdd);
	GET_SET_DIRTY(bool, VCWorkOffline, m_VCWorkOffline);
	GET_SET_DIRTY(bool, VCDebugCmd, m_VCDebugCmd);
	GET_SET_DIRTY(bool, VCDebugCom, m_VCDebugCom);
	GET_SET_DIRTY(bool, VCDebugOut, m_VCDebugOut);

	EditorUserSettings (MemLabelId label, ObjectCreationMode mode);
	void Reset ();
	
	char const* GetName () const { return "Editor User Settings"; };
	
private:

	// The values set for the fields 
	ConfigValueMap m_ConfigValues;
	
	bool m_VCAutomaticAdd;
	bool m_VCWorkOffline;
	bool m_VCDebugCmd;
	bool m_VCDebugCom;
	bool m_VCDebugOut;
};

EditorUserSettings& GetEditorUserSettings();
void SetEditorUserSettings(EditorUserSettings* settings);
