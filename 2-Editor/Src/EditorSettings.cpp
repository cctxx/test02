/*
 *  EditorSettings.cpp
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-06-03.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *
 */

#include "UnityPrefix.h"
#include "Editor/Src/EditorSettings.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "AssetServer/ASCache.h"
#include "Editor/Src/Application.h"
#include "Runtime/Mono/MonoManager.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/VersionControl/VCProvider.h"

static EditorSettings* gSingleton = NULL; 

static const char* kDefaultWebSecurityEmulationURL = "http://www.mydomain.com/mygame.unity3d";

EditorSettings::EditorSettings (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

EditorSettings::~EditorSettings ()
{
	if (gSingleton ==  this)
		gSingleton = NULL;
}

void EditorSettings::Reset ()
{
	Super::Reset();
	m_ExternalVersionControlSupport = LicenseInfo::Flag (lf_pro_version) ? 
		ExternalVersionControlAutoDetect : ExternalVersionControlHiddenMetaFiles;

	m_SerializationMode = kMixed;
	m_WebSecurityEmulationEnabled = false;
	m_WebSecurityEmulationHostUrl = kDefaultWebSecurityEmulationURL;

#if ENABLE_SPRITES
	m_DefaultBehaviorMode = kMode3D;
	m_SpritePackerMode = kSPOff;
#endif
}

void EditorSettings::SetExternalVersionControlSupport(const string& value)
{
	if (m_ExternalVersionControlSupport != value)
	{
		m_ExternalVersionControlSupport=value;

		// Clear asset server error about meta files not being enabled.
		if (value != ExternalVersionControlHiddenMetaFiles)
			RemoveErrorWithIdentifierFromConsole (GetEditorSettings().GetInstanceID());

		SetDirty();
	}
}

bool EditorSettings::GetVersionControlRequiresMetaFiles () const 
{ 
	return m_ExternalVersionControlSupport != ExternalVersionControlAutoDetect 
		&& m_ExternalVersionControlSupport != ExternalVersionControlHiddenMetaFiles;
}

void EditorSettings::SetSerializationMode (int value)
{
	if (m_SerializationMode != value)
	{
		// We allow switch from binary or to text
		bool canSwitch = m_SerializationMode == kForceBinary || value == kForceText;
		if (GetMonoManager().HasCompileErrors () && !canSwitch)
		{
			ErrorString ("Can't switch serialization mode to binary or from text while compile errors are present. Please fix the scripts and try again.");
			return;
		}

		m_SerializationMode = value;

		SetDirty ();

		if (m_SerializationMode != kMixed)
			AssetInterface::Get().RewriteAllSerializedFiles();
	}
}

void EditorSettings::SetWebSecurityEmulationEnabled(bool enabled) 
{ 
	m_WebSecurityEmulationEnabled = enabled; 
	SetDirty(); 
}

void EditorSettings::SetWebSecurityEmulationHostUrl(const std::string &url) 
{ 
	m_WebSecurityEmulationHostUrl = url; 
	SetDirty(); 
}

template<class TransferFunc>
void EditorSettings::Transfer (TransferFunc& transfer)
{
	Super::Transfer(transfer);	
	
	transfer.SetVersion (3);

	if (transfer.IsOldVersion(1))
	{
		enum OldExternalVersionControlSupport { kAutoDetect = -1, kDisabled = 0, kGeneric = 1, kSubversion = 2, kPerforce = 3, kAssetServer = 4 };

		// Convert from old enum representation to new string
		// representation
		int oldValue;
		transfer.Transfer(oldValue, "m_ExternalVersionControlSupport");

		switch (static_cast<OldExternalVersionControlSupport>(oldValue)) {
			case kAutoDetect:
				m_ExternalVersionControlSupport = ExternalVersionControlAutoDetect;
				break;
			case kDisabled:
				m_ExternalVersionControlSupport = ExternalVersionControlHiddenMetaFiles;
				break;
			case kGeneric:
				m_ExternalVersionControlSupport = ExternalVersionControlVisibleMetaFiles;
				break;
			case kSubversion:
				m_ExternalVersionControlSupport = "Subversion";
				break;
			case kPerforce:
				m_ExternalVersionControlSupport = "Perforce";
				break;
			case kAssetServer:
				m_ExternalVersionControlSupport = ExternalVersionControlAssetServer;
				break;
			default:
				m_ExternalVersionControlSupport = ExternalVersionControlHiddenMetaFiles;
				break;
		}
	}
	else if (transfer.IsOldVersion(2))
	{
		TRANSFER (m_ExternalVersionControlSupport);
		
		if( m_ExternalVersionControlSupport.compare("Disabled") == 0 )
			m_ExternalVersionControlSupport = ExternalVersionControlHiddenMetaFiles;
		else if( m_ExternalVersionControlSupport.compare("Meta Files") == 0 )
			m_ExternalVersionControlSupport = ExternalVersionControlVisibleMetaFiles;
	}
	else
	{
		TRANSFER (m_ExternalVersionControlSupport);
	}
		
	TRANSFER (m_SerializationMode);
	TRANSFER (m_WebSecurityEmulationEnabled);
	TRANSFER (m_WebSecurityEmulationHostUrl);
	transfer.Align();

#if ENABLE_SPRITES
	TRANSFER (m_DefaultBehaviorMode);
	TRANSFER (m_SpritePackerMode);
#endif
}

IMPLEMENT_CLASS (EditorSettings)
IMPLEMENT_OBJECT_SERIALIZE (EditorSettings)

void SetEditorSettings(EditorSettings* settings)
{
	gSingleton = settings;
}

EditorSettings& GetEditorSettings()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}

EditorSettings* GetEditorSettingsPtr()
{
	return gSingleton;
}

