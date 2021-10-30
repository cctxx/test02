#include "UnityPrefix.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "AssetServer/ASCache.h"
#include "Editor/Src/Application.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/VersionControl/VCProvider.h"

static EditorUserSettings* gSingleton = NULL; 

static const char* kDefaultVCLogLevel = "info";

EditorUserSettings::EditorUserSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
, m_VCAutomaticAdd(true)
, m_VCWorkOffline(false)
, m_VCDebugCom(false)
, m_VCDebugCmd(false)
, m_VCDebugOut(false)
{
	m_ConfigValues.insert(make_pair("vcSharedLogLevel", kDefaultVCLogLevel));
}

EditorUserSettings::~EditorUserSettings ()
{
	if (gSingleton ==  this)
		gSingleton = NULL;
}

void EditorUserSettings::Reset ()
{
	Super::Reset();
	m_VCAutomaticAdd = true;
	m_VCDebugCmd = false;
	m_VCDebugCom = false;
	m_VCDebugOut = false;
	m_ConfigValues.clear();
	SetConfigValue("vcSharedLogLevel", kDefaultVCLogLevel);
}

bool EditorUserSettings::HasConfigValue(const UnityStr& name) const
{
	return m_ConfigValues.find(name) != m_ConfigValues.end();
}

UnityStr EditorUserSettings::GetConfigValue(const UnityStr& name) const
{
	ConfigValueMap::const_iterator i = m_ConfigValues.find(name);
	if (i == m_ConfigValues.end()) return "";
	return i->second;
}

void EditorUserSettings::SetConfigValue(const UnityStr& name, const UnityStr& value)
{
	std::pair<ConfigValueMap::iterator, bool> i = m_ConfigValues.insert(make_pair(name,value));

	// if not inserted
	if (!i.second)
	{
		if (i.first->second == value)
			return; // existing value already the same as incoming value
		i.first->second = value; // overwrite old entry
	} 

	SetDirty();

	// Disabled for now since initial setup will spawn lots of errors
	// in the console this way. It should really be set explicitly.
	/*
	// Setting starting with vc is version control by convention and 
	// updates should be sent notified to the version control subsystem
	if (name.length() > 2 && name.substr(0,2) == "vc")
		GetVCProvider().UpdateSettings();
	*/
}

template<class TransferFunc>
void EditorUserSettings::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);
	
	transfer.SetVersion (2);
	
	TRANSFER (m_ConfigValues);

	TRANSFER (m_VCAutomaticAdd);
	TRANSFER (m_VCDebugCom);
	TRANSFER (m_VCDebugCmd);
	TRANSFER (m_VCDebugOut);

	if (transfer.IsOldVersion(1))
	{
		UnityStr value;
		transfer.Transfer(value, "m_VCUserName");
		m_ConfigValues["vcUsername"] = value;
		transfer.Transfer(value, "m_VCPassword");
		m_ConfigValues["vcPassword"] = value;
		transfer.Transfer(value, "m_VCServer");
		m_ConfigValues["vcServer"] = value;
		transfer.Transfer(value, "m_VCWorkspace");
		m_ConfigValues["vcWorkspace"] = value;
	}
	
	transfer.Align();
}

IMPLEMENT_CLASS (EditorUserSettings)
IMPLEMENT_OBJECT_SERIALIZE (EditorUserSettings)

void SetEditorUserSettings(EditorUserSettings* settings)
{
	gSingleton = settings;
}

EditorUserSettings& GetEditorUserSettings()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}
