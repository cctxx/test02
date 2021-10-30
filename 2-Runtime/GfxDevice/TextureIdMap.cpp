#include "UnityPrefix.h"
#include "TextureIdMap.h"


void TextureIdMap::Initialize()
{
	if(!s_Inited)
	{
		s_Textures.set_empty_key(TextureID(-1));
		s_Textures.set_deleted_key(TextureID(-2));

		s_Inited = true;
	}
}

void TextureIdMap::Uninitialize()
{
	if (!s_Inited)
		return;

	//@TODO: make sure all textures are cleaned up properly and re-enabled the assert
	//#if 0 // was !UNITY_EDITOR
	//Assert(s_Textures.empty());
	//#endif

	// Note, do not set s_Inited to false; would make it impossible to reload GfxDevice
	// since the hash map object stays the same but we can't set the empty key again.
}

TextureIdMap::TextureMap	TextureIdMap::s_Textures;
bool						TextureIdMap::s_Inited = false;
