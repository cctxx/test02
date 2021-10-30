#pragma once

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"


// Texture filter, anisotropy, wrap mode settings.
struct TextureSettings
{
	DECLARE_SERIALIZE_NO_PPTR (GLTextureSettings) // keep the name GLTextureSettings here, it's for serialized stuff!

	int 	m_FilterMode;	///< enum { Nearest, Bilinear, Trilinear } Texture filter mode
	int 	m_Aniso;		///< Anisotropy factor (1 = None, 0 = Always disabled)
	float	m_MipBias;		///< Bias used for LOD-selection (0 = none)
	int		m_WrapMode;		///< enum {Repeat, Clamp} Texture wrapping mode.
	
	TextureSettings () { Reset (); }
	
	// Set default values
	void Reset();

	void CheckConsistency();

	#if UNITY_EDITOR
	// Set all numbers to -1, marking them as invalid
	void Invalidate();
	#endif

	void Apply (TextureID texture, TextureDimension texDim, bool hasMipMap, TextureColorSpace colorSpace) const;
	
	static void SetAnisoLimits (int minAniso, int maxAniso);
	static void GetAnisoLimits (int& minAniso, int& maxAniso);
};

template<class TransferFunc>
void TextureSettings::Transfer (TransferFunc& transfer)
{
	TRANSFER (m_FilterMode);
	TRANSFER (m_Aniso);
	TRANSFER (m_MipBias);
	TRANSFER (m_WrapMode);
}
