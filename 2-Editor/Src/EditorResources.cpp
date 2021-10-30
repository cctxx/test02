#include "UnityPrefix.h"
#include "EditorResources.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Editor/Src/Utility/ObjectImages.h"
#include "Editor/Src/LicenseInfo.h"

static EditorResources* s_EditorResources = NULL;

const char* EditorResources::kDefaultAssets = "DefaultAssets/";
const char* EditorResources::kLightSkinPath = "Builtin Skins/Generated/LightSkin.guiskin";
const char* EditorResources::kDarkSkinPath = "Builtin Skins/Generated/DarkSkin.guiskin";
const char* EditorResources::kLightSkinSourcePath = "Builtin Skins/LightSkin/";
const char* EditorResources::kDarkSkinSourcePath = "Builtin Skins/DarkSkin/";
const char* EditorResources::kFontsPath = "Fonts/";
const char* EditorResources::kBrushesPath = "Brushes/";
const char* EditorResources::kIconsPath = "Icons/";
const char* EditorResources::kGeneratedIconsPath = "Icons/Generated/";
const char* EditorResources::kFolderIconName = "Folder Icon";
const char* EditorResources::kEmptyFolderIconName = "FolderEmpty Icon";


EditorResources::EditorResources ()
{
	m_SkinIdx = EditorPrefs::GetInt ("UserSkin", 1);
}

int EditorResources::GetSkinIdx ()
{
	if (!LicenseInfo::Flag (lf_pro_version))
		return 0;
	
	return m_SkinIdx;
}

void EditorResources::SetSkinIdx (int skinIdx)
{
	m_SkinIdx = skinIdx;
	EditorPrefs::SetInt ("UserSkin", m_SkinIdx);
	FlushCachedObjectImages ();
}

EditorResources &GetEditorResources ()
{
	if (!s_EditorResources)
		s_EditorResources = new EditorResources ();
	return *s_EditorResources;
}
