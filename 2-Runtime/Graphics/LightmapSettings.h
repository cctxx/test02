#pragma once

#include "Runtime/Math/Color.h"
#include "Texture2D.h"
#include "Runtime/BaseClasses/GameManager.h"
#include "Runtime/Utilities/NonCopyable.h"
#include "Runtime/Utilities/triple.h"
#if UNITY_EDITOR
#include "Editor/Src/LightmapEditorSettings.h"
#endif
#include "Runtime/Camera/LightProbes.h"

class LightmapData
{
public:
	PPtr<Texture2D> m_Lightmap;
	PPtr<Texture2D> m_IndirectLightmap;
	PPtr<Texture2D> m_ThirdLightmap;

	DECLARE_SERIALIZE(LightmapData)
};

struct LightmapDataMono {
	ScriptingObjectPtr m_Lightmap;
	ScriptingObjectPtr m_IndirectLightmap;
};
void LightmapDataToMono (const LightmapData &src, LightmapDataMono &dest);
void LightmapDataToCpp (LightmapDataMono &src, LightmapData &dest);

class LightmapSettings : public LevelGameManager
{
public:
	REGISTER_DERIVED_CLASS (LightmapSettings, LevelGameManager)
	DECLARE_OBJECT_SERIALIZE(LightmapSettings)

	// Lightmap index is stored as UInt8,
	// 2 indices are reserved.
	static const int kMaxLightmaps = 254;

	LightmapSettings(MemLabelId label, ObjectCreationMode mode);

	typedef triple<TextureID> TextureTriple;
	TextureTriple GetLightmapTexture (UInt32 index) const
	{
		if (index < m_LightmapTextureCount)
			return m_LightmapTextures[index];
		else
			return triple<TextureID>(TextureID(),TextureID(), TextureID());
	}

	const std::vector<LightmapData>& GetLightmaps () { return m_Lightmaps; }
	void ClearLightmaps();
	void SetLightmaps (const std::vector<LightmapData>& data);
	void AppendLightmaps (const std::vector<LightmapData>& data);
	
	void SetBakedColorSpace(ColorSpace colorSpace) { m_BakedColorSpace = (int)colorSpace; }
	ColorSpace GetBakedColorSpace() { return (ColorSpace)m_BakedColorSpace; }

	enum LightmapsMode {
		kSingleLightmapsMode = 0,
		kDualLightmapsMode = 1,
		kDirectionalLightmapsMode = 2,
		kRNMMode = 3,
		kLightmapsModeCount // keep this last
	};

	static const char* const kLightmapsModeNames[kLightmapsModeCount];

	GET_SET(int, LightmapsMode, m_LightmapsMode);
	bool GetUseDualLightmapsInForward () const;

	#if UNITY_EDITOR
	LightmapEditorSettings& GetLightmapEditorSettings() { return m_LightmapEditorSettings; };
	#endif

	void AwakeFromLoad(AwakeFromLoadMode mode);

	void SetLightProbes (LightProbes* lightProbes);
	LightProbes* GetLightProbes ();
	
private:
	void Rebuild();

private:
	TextureTriple* m_LightmapTextures;
	int  m_LightmapTextureCount;
	
	PPtr<LightProbes> m_LightProbes;

	std::vector<LightmapData> m_Lightmaps;
	int		m_LightmapsMode;		// LightmapsMode
	int		m_BakedColorSpace;
	bool	m_UseDualLightmapsInForward;

	#if UNITY_EDITOR
	LightmapEditorSettings m_LightmapEditorSettings;
	#endif
};

LightmapSettings& GetLightmapSettings ();

