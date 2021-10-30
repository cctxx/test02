#include "UnityPrefix.h"
#include "LightmapSettings.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Scripting.h"

const char* const LightmapSettings::kLightmapsModeNames[] =
{
	"Single Lightmaps",
	"Dual Lightmaps",
	"Directional Lightmaps",
	"RNM"
};

template<class T>
void LightmapData::Transfer (T& transfer)
{
	TRANSFER(m_Lightmap);
	TRANSFER(m_IndirectLightmap);
}

template<class T>
void LightmapSettings::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	TRANSFER(m_LightProbes);
	TRANSFER(m_Lightmaps);
	TRANSFER(m_LightmapsMode);
	TRANSFER(m_BakedColorSpace);
	TRANSFER(m_UseDualLightmapsInForward);
	transfer.Align ();
	TRANSFER_EDITOR_ONLY(m_LightmapEditorSettings);
}

LightmapSettings::LightmapSettings(MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
,	m_LightmapTextures(NULL)
,	m_LightmapTextureCount(0)
,	m_LightmapsMode(kDualLightmapsMode)
,	m_UseDualLightmapsInForward(false)
,	m_BakedColorSpace(0)
{
}

LightmapSettings::~LightmapSettings ()
{
	delete[] m_LightmapTextures;
}

void LightmapSettings::AwakeFromLoad(AwakeFromLoadMode awakeMode)
{
	Super::AwakeFromLoad(awakeMode);
	Rebuild ();
}

void LightmapSettings::Rebuild ()
{
	delete[] m_LightmapTextures;

	const size_t lightmapCount = m_Lightmaps.size();

	m_LightmapTextures = new TextureTriple[lightmapCount];
	m_LightmapTextureCount = lightmapCount;

	for (size_t i = 0; i < lightmapCount; ++i)
	{
		Texture2D* tex = m_Lightmaps[i].m_Lightmap;
		Texture2D* texInd = m_Lightmaps[i].m_IndirectLightmap;
		Texture2D* texThird = m_Lightmaps[i].m_ThirdLightmap;
		TextureTriple p;
		p.first = tex ? tex->GetTextureID() : TextureID();
		p.second = texInd ? texInd->GetTextureID() : TextureID();
		p.third = texThird ? texThird->GetTextureID() : TextureID();
		m_LightmapTextures[i] = p;
	}
}

void LightmapSettings::ClearLightmaps()
{
	m_Lightmaps.clear();
	Rebuild();
	SetDirty();
}

void LightmapSettings::SetLightmaps (const std::vector<LightmapData>& data)
{
	m_Lightmaps = data;
	Rebuild();
	SetDirty();
}

void LightmapSettings::SetLightProbes (LightProbes* lightProbes)
{
	if (lightProbes && !GetBuildSettings().hasAdvancedVersion)
	{
		ErrorString ("Light probes require Unity Pro.");
		return;
	}

	m_LightProbes = lightProbes;
	SetDirty();
}

LightProbes* LightmapSettings::GetLightProbes ()
{
	return m_LightProbes;
}

bool LightmapSettings::GetUseDualLightmapsInForward () const
{
	if (IS_CONTENT_NEWER_OR_SAME (kUnityVersion3_5_a1))
		return (m_LightmapsMode == kDualLightmapsMode) && m_UseDualLightmapsInForward;
	else
		return m_UseDualLightmapsInForward;
}

void LightmapSettings::AppendLightmaps (const std::vector<LightmapData>& data)
{
	int originalSize = m_Lightmaps.size();
	int dataSize = data.size();

	if (originalSize + data.size() > LightmapSettings::kMaxLightmaps)
	{
		int newDataSize = max(0, LightmapSettings::kMaxLightmaps - originalSize);
		ErrorString(Format(
			"Can't append %i lightmaps, since that would exceed the %i lightmaps limit. "
			"Appending only %i lightmaps. Objects that use lightmaps past that limit won't get proper lightmaps.",
			dataSize, LightmapSettings::kMaxLightmaps, newDataSize));
		dataSize = newDataSize;
	}
	if ( dataSize <= 0 ) return;
	m_Lightmaps.resize(originalSize + dataSize);
	std::copy(data.begin(), data.begin() + dataSize, m_Lightmaps.begin() + originalSize);

	Rebuild();
	SetDirty();
}

#if ENABLE_SCRIPTING
void LightmapDataToMono (const LightmapData &src, LightmapDataMono &dest) {
	dest.m_Lightmap = Scripting::ScriptingWrapperFor (src.m_Lightmap);
	dest.m_IndirectLightmap = Scripting::ScriptingWrapperFor (src.m_IndirectLightmap);
}
void LightmapDataToCpp (LightmapDataMono &src, LightmapData &dest) {
	dest.m_Lightmap = ScriptingObjectToObject<Texture2D> (src.m_Lightmap);
	dest.m_IndirectLightmap = ScriptingObjectToObject<Texture2D> (src.m_IndirectLightmap);
}
#endif

IMPLEMENT_CLASS (LightmapSettings)
IMPLEMENT_OBJECT_SERIALIZE (LightmapSettings)
GET_MANAGER (LightmapSettings)
