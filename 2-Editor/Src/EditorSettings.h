/*
 *  EditorSettings.h
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-06-03.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *
 */

#pragma once

#include "Runtime/BaseClasses/GameManager.h"
#include "Editor/Src/LicenseInfo.h"
#include "Editor/Src/AssetServer/ASController.h"

#define ExternalVersionControlHiddenMetaFiles "Hidden Meta Files"
#define ExternalVersionControlAutoDetect "Auto detect"
#define ExternalVersionControlVisibleMetaFiles "Visible Meta Files"
#define ExternalVersionControlAssetServer "Asset Server"

class EditorSettings : public Object
{
public:
	enum SerializationMode { kMixed = 0, kForceBinary = 1, kForceText = 2 };
#if ENABLE_SPRITES
	enum BehaviorMode { kMode3D = 0, kMode2D = 1 };
	enum SpritePackerMode { kSPOff = 0, kSPBuild = 1, kSPOn = 2 };
#endif

	REGISTER_DERIVED_CLASS (EditorSettings, Object)
	DECLARE_OBJECT_SERIALIZE(EditorSettings)
	
	std::string GetExternalVersionControlSupport() const
	{
		return m_ExternalVersionControlSupport;
	}

	void SetExternalVersionControlSupport(const std::string& value);
	bool GetVersionControlRequiresMetaFiles() const;
	
	int GetSerializationMode() { return m_SerializationMode; }
	void SetSerializationMode(int value);
	
	// If set to true web crossdomain security is anabled in editor mode. This does not affect the standalone or web player
	void SetWebSecurityEmulationEnabled(bool enabled);
	bool GetWebSecurityEmulationEnabled() const { return m_WebSecurityEmulationEnabled; }
	// Set the host uri to use to set the site of origin
	void SetWebSecurityEmulationHostUrl(const std::string &url);
	const UnityStr &GetWebSecurityEmulationHostUrl() const { return m_WebSecurityEmulationHostUrl; }

	EditorSettings (MemLabelId label, ObjectCreationMode mode);
	void Reset ();
	
	char const* GetName () const { return "Editor Settings"; };
	
#if ENABLE_SPRITES
	GET_SET_COMPARE_DIRTY(int, DefaultBehaviorMode, m_DefaultBehaviorMode);
	GET_SET_COMPARE_DIRTY(int, SpritePackerMode, m_SpritePackerMode);
#endif

private:
	UnityStr m_ExternalVersionControlSupport;
	int m_SerializationMode;

	int m_WebSecurityEmulationEnabled;
	UnityStr m_WebSecurityEmulationHostUrl;

#if ENABLE_SPRITES
	int m_DefaultBehaviorMode;
	int m_SpritePackerMode;
#endif
};

EditorSettings& GetEditorSettings();
EditorSettings* GetEditorSettingsPtr();
void SetEditorSettings(EditorSettings* settings);
