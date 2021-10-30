#ifndef EDITORRESOURCES_H
#define EDITORRESOURCES_H

class EditorResources 
{
 public:
	EditorResources ();

	static const char* kLightSkinPath;
	static const char* kDarkSkinPath;
	static const char* kLightSkinSourcePath;
	static const char* kDarkSkinSourcePath;
	static const char* kFontsPath;
	static const char* kBrushesPath;
	static const char* kIconsPath;
	static const char* kGeneratedIconsPath;
	static const char* kDefaultAssets;
	static const char* kFolderIconName;
	static const char* kEmptyFolderIconName;


	// 0 if normal skin
	// 1 if dark skin
	int GetSkinIdx ();
	void SetSkinIdx (int skinIdx);
 private:
	int m_SkinIdx;
};



EditorResources &GetEditorResources ();

#endif
