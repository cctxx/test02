#include "UnityPrefix.h"
#include "EditorBuildSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/BitSetSerialization.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/GUID.h"
#include "GUIDPersistentManager.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"

static EditorBuildSettings* gSingleton = NULL; 

EditorBuildSettings::EditorBuildSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_ReadOldBuildSettings = true;
}

EditorBuildSettings::~EditorBuildSettings ()
{
	if (gSingleton == this)
		gSingleton = NULL;
}

void EditorBuildSettings::ReadOldBuildSettings ()
{
	// Only try to read from old Unity 2.1 plist if we haven't setup new scenes yet.
	if( !m_Scenes.empty() )
		return;

	InputString plist;
	if( !ReadStringFromFile( &plist, "Library/BuildPlayer.prefs" ) )
		return;

	// We're interested in those bits:
	//
	//	<key>PathNames</key>
	//	<array>
	//		<string>Assets/blahblah.unity</string>
	//		...
	//	</array>
	//
	//	<key>Enabled</key>
	//	<array>
	//		<true/>
	//		<false/>
	//		...
	//	</array>

	// Read path names
	std::vector<InputString> pathNames;

	{
		size_t pathsStart = plist.find("<key>PathNames</key>");
		if( pathsStart == std::string::npos )
			return;
		size_t pathsEnd = plist.find("</array>", pathsStart);
		if( pathsEnd == std::string::npos )
			return;

		size_t idx = pathsStart;
		const InputString kStringStart("<string>");
		const InputString kStringEnd("</string>");
		while( idx < pathsEnd )
		{
			// opening string tag
			idx = plist.find(kStringStart, idx);
			if( idx == InputString::npos || idx >= pathsEnd )
				break; // finished already
			idx += kStringStart.size();
			// closing string tag
			size_t idx2 = plist.find(kStringEnd, idx);
			if( idx2 == InputString::npos || idx2 > pathsEnd || idx2 == idx )
				return; // no end tag? something is wrong

			pathNames.push_back( plist.substr( idx, idx2 - idx ) );

			idx = idx2 + kStringEnd.size();
		}
	}

	// add scene infos from the pathnames we read
	m_Scenes.reserve( pathNames.size() );
	for( size_t i = 0; i < pathNames.size(); ++i )
	{
		Scene scene;
		scene.enabled = true;
		scene.path = pathNames[i].c_str();
		m_Scenes.push_back( scene );
	}

	// read enabled flags
	{
		const InputString kEnabledStart("<key>Enabled</key>");
		size_t enabledStart = plist.find(kEnabledStart);
		if( enabledStart == InputString::npos )
			return;
		enabledStart += kEnabledStart.size();
		const InputString kArrayStart("<array>");
		enabledStart = plist.find(kArrayStart, enabledStart);
		if( enabledStart == InputString::npos )
			return;
		enabledStart += kArrayStart.size();

		size_t enabledEnd = plist.find("</array>", enabledStart);
		if( enabledEnd == InputString::npos )
			return;

		size_t idx = enabledStart;
		size_t sceneIndex = 0;
		while( idx < enabledEnd && sceneIndex < m_Scenes.size() )
		{
			// start of next tag
			idx = plist.find('<', idx);
			if( idx == InputString::npos || idx >= enabledEnd )
				break; // finished already
			idx += 1;
			// end of next tag
			size_t idx2 = plist.find("/>", idx);
			if( idx2 == InputString::npos || idx2 >= enabledEnd || idx2 == idx )
				return; // no end tag? something is wrong

			std::string val = plist.substr( idx, idx2 - idx ).c_str();
			ToLowerInplace(val);
			if( val == "true" )
			{
				m_Scenes[sceneIndex].enabled = true;
			}
			else if( val == "false" )
			{
				m_Scenes[sceneIndex].enabled = false;
			}
			else
			{
				// some other value? something is wrong
				return;
			}

			idx = idx2 + 2;
			++sceneIndex;
		}
	}
}


template<class TransferClass>
void EditorBuildSettings::Scene::Transfer (TransferClass& transfer)
{
	TRANSFER(enabled);
	transfer.Align();
	TRANSFER(path);
}

template<class TransferFunc>
void EditorBuildSettings::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);
	
	transfer.SetVersion (2);
	
	TRANSFER (m_Scenes);
	
	m_ReadOldBuildSettings = transfer.IsOldVersion (1);
}

void EditorBuildSettings::AwakeFromLoad (AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	if (m_ReadOldBuildSettings)
		ReadOldBuildSettings();
}

void EditorBuildSettings::SetScenes (const Scenes& scenes)
{
	if (m_Scenes != scenes)
	{
		m_Scenes = scenes;
		SetDirty();
	}
}

IMPLEMENT_CLASS (EditorBuildSettings)
IMPLEMENT_OBJECT_SERIALIZE (EditorBuildSettings)

void SetEditorBuildSettings(EditorBuildSettings* settings)
{
	gSingleton = settings;
}

EditorBuildSettings& GetEditorBuildSettings()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}
