#include "UnityPrefix.h"
#include "TextureSettings.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/GfxDevice/GfxDevice.h"

static int gUserMinAniso = 1;
static int gUserMaxAniso = 16;


void TextureSettings::SetAnisoLimits (int minAniso, int maxAniso)
{
	gUserMinAniso = minAniso;
	gUserMaxAniso = maxAniso;
	ErrorIf (gUserMinAniso < 1);
	ErrorIf (gUserMaxAniso > 16);
}

void TextureSettings::GetAnisoLimits (int& minAniso, int& maxAniso)
{
	minAniso = gUserMinAniso;
	maxAniso = gUserMaxAniso;
}


void TextureSettings::Reset ()
{
	m_FilterMode = kTexFilterBilinear;
	m_Aniso = 1;
	m_MipBias = 0.0f;
	m_WrapMode = 0;
}

void TextureSettings::CheckConsistency()
{
	m_FilterMode = clamp<int> (m_FilterMode, 0, kTexFilterCount-1);
	m_WrapMode = clamp<int> (m_WrapMode, 0, kTexWrapCount-1);
}


void TextureSettings::Apply (TextureID texture, TextureDimension texDim, bool hasMipMap, TextureColorSpace colorSpace) const
{
	GfxDevice& device = GetGfxDevice();

	int aniso;
	// Never use anisotropic on textures where we certainly don't want it,
	// and on Point filtered textures.
	if (m_Aniso == 0 || m_FilterMode == kTexFilterNearest)
		aniso = 1;
	else
		aniso = clamp (m_Aniso, gUserMinAniso, gUserMaxAniso);
	
	device.SetTextureParams (texture, texDim, (TextureFilterMode)m_FilterMode,
		(TextureWrapMode)m_WrapMode, aniso, hasMipMap, colorSpace);
}

#if UNITY_EDITOR
void TextureSettings::Invalidate ()
{
	m_FilterMode = -1;
	m_Aniso = -1;
	m_MipBias = -1.0f;
	m_WrapMode = -1;
}
#endif
