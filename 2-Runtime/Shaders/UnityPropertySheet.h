#pragma once

#include "External/shaderlab/Library/FastPropertyName.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Color.h"
#include "Runtime/Graphics/Texture.h"

namespace ShaderLab {
	class PropertySheet;
	struct ParserShader;
}

// Serialized material property data (colors, textures, ...).
// This is used only for saving & loading of materials; at runtime ShaderLab PropertySheets
// are used (they can have more data that's not serialized, like matrices etc.).
class UnityPropertySheet
{
public:	
	DECLARE_SERIALIZE (UnityPropertySheet)
	struct UnityTexEnv {
		UnityTexEnv();
		
		DECLARE_SERIALIZE (UnityTexEnv)
		Vector2f m_Scale;
		Vector2f m_Offset;
		PPtr<Texture> m_Texture;
	};
	typedef std::map<ShaderLab::FastPropertyName, UnityTexEnv> TexEnvMap;
	typedef std::map<ShaderLab::FastPropertyName, float> FloatMap;
	typedef std::map<ShaderLab::FastPropertyName, ColorRGBAf> ColorMap;
	TexEnvMap m_TexEnvs;
	FloatMap m_Floats;
	ColorMap m_Colors;
	
	// Set the properties of target.
	// This attempts to fill the properties of target with any info this may have.
	// It never adds a property to target.
	void AssignDefinedPropertiesTo (ShaderLab::PropertySheet &target);
	
	// Add any properties defined by source, without overwriting anything already here.
	bool AddNewShaderlabProps (const ShaderLab::PropertySheet &source);
	void AddNewSerializedProps (const UnityPropertySheet &source);
	
#if UNITY_EDITOR
	// Remove any properties not used by source.
	// This is called when building a player by Material::Transfer
	void CullUnusedProperties (const ShaderLab::ParserShader* source);
#endif
};

template<class TransferFunc>
void UnityPropertySheet::Transfer (TransferFunc& transfer)
{
	transfer.SetVersion (2);

	TRANSFER (m_TexEnvs);
	TRANSFER (m_Floats);
	TRANSFER (m_Colors);
}

template<class TransferFunc>
void UnityPropertySheet::UnityTexEnv::Transfer (TransferFunc& transfer)
{
	TRANSFER (m_Texture);
	TRANSFER (m_Scale);
	TRANSFER (m_Offset);
}
