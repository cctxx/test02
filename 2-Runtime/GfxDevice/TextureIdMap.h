#pragma once

#include "Configuration/UnityConfigure.h"

#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/LogAssert.h"

#include "Runtime/GfxDevice/GfxDeviceConfigure.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class
TextureIdMap
{
public:

	static void Initialize();
	static void Uninitialize();

	static void UpdateTexture(TextureID texid, intptr_t nativeTex);
	static void RemoveTexture(TextureID texid);

	static intptr_t QueryNativeTexture(TextureID texid);


private:

	struct TextureIDHashFunctor
	{
		inline size_t operator()(TextureID x) const
		{
			UInt32 a = x.m_ID;
			a = (a+0x7ed55d16) + (a<<12);
			a = (a^0xc761c23c) ^ (a>>19);
			a = (a+0x165667b1) + (a<<5);
			a = (a+0xd3a2646c) ^ (a<<9);
			a = (a+0xfd7046c5) + (a<<3);
			a = (a^0xb55a4f09) ^ (a>>16);
			return a;
		}
	};


	typedef pair<const TextureID, intptr_t> TextureIdToTexturePair;
	typedef dense_hash_map< TextureID, intptr_t, TextureIDHashFunctor, std::equal_to<TextureID>, STL_ALLOCATOR(kMemSTL, TextureIdToTexturePair) > TextureMap;

	static TextureMap	s_Textures;

	// some devices call OnCreate twice - easier to workaround here
	static bool			s_Inited;
};

inline void TextureIdMap::UpdateTexture(TextureID texid, intptr_t nativeTex)
{
	TextureMap::iterator it = s_Textures.find(texid);
	if(it != s_Textures.end())
		it->second = nativeTex;
	else
		s_Textures.insert(std::make_pair(texid, nativeTex));
}

inline void TextureIdMap::RemoveTexture(TextureID texid)
{
	s_Textures.erase(texid);
}

inline intptr_t TextureIdMap::QueryNativeTexture(TextureID texid)
{
	TextureMap::iterator it = s_Textures.find(texid);
	return it == s_Textures.end() ? 0 : it->second;
}
