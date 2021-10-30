#ifndef MATERIAL_PROPERTIES_H
#define MATERIAL_PROPERTIES_H

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Modules/ExportModules.h"
#include "Runtime/GfxDevice/GfxDeviceTypes.h"

class Vector4f;
class Matrix4x4f;
class ColorRGBAf;
namespace Unity { class Material; }
namespace ShaderLab { struct FastPropertyName; }

// Tightly packed buffer of material properties
class EXPORT_COREMODULE MaterialPropertyBlock
{
public:
	struct Property
	{
		int		nameIndex;
		UInt8	rows;
		UInt8	cols;
		UInt8	texDim; // if texDim==None, this is a value property
		// These should not be size_t, as the GfxDevice may run across processes of different
		// bitness, and the data serialized in the command buffer must match.
		UInt32	arraySize;
		UInt32	offset;
	};

	MaterialPropertyBlock() {}

	// Does not copy data!
	MaterialPropertyBlock(Property* props, size_t propCount, float* buffer, size_t bufSize);

	// Clear all properties
	void Clear();

	// Add Properties without checking if another property with the same name exists
	void AddProperty(const ShaderLab::FastPropertyName& name, const float* data, UInt8 rows, UInt8 cols, size_t arraySize);
	void AddPropertyFloat(const ShaderLab::FastPropertyName& name, float val);
	void AddPropertyVector(const ShaderLab::FastPropertyName& name, const Vector4f& vec);
	void AddPropertyColor(const ShaderLab::FastPropertyName& name, const ColorRGBAf& col);
	void AddPropertyMatrix(const ShaderLab::FastPropertyName& name, const Matrix4x4f& mat);
	void AddPropertyTexture(const ShaderLab::FastPropertyName& name, TextureDimension dim, TextureID tid);

	// Replace properties
	void ReplacePropertyFloat(const ShaderLab::FastPropertyName& name, float data);
	void ReplacePropertyVector(const ShaderLab::FastPropertyName& name, const Vector4f& col);
	void ReplacePropertyColor(const ShaderLab::FastPropertyName& name, const ColorRGBAf& col);
	void ReplacePropertyTexture(const ShaderLab::FastPropertyName& name, TextureDimension dim, TextureID tid);
	
	// Replaces a single float property on either a float1 or one component of a float4.
	/// If other components on a float4 are not yet defined, they will be initialized to zero
	void ReplacePartialFloatProperty(const ShaderLab::FastPropertyName& name, float data, UInt8 cols, UInt8 colIndex);
	void ReplacePartialFloatColorProperty(const ShaderLab::FastPropertyName& name, float data, UInt8 cols, UInt8 colIndex);


	const float* FindFloat(const ShaderLab::FastPropertyName& name) const;
	const Vector4f* FindVector(const ShaderLab::FastPropertyName& name) const;
	bool GetColor(const ShaderLab::FastPropertyName& name, ColorRGBAf& outColor) const;
	const Matrix4x4f* FindMatrix(const ShaderLab::FastPropertyName& name) const;
	const TextureID FindTexture(const ShaderLab::FastPropertyName& name) const;
	
	const Property*	GetPropertiesBegin() const	{ return m_Properties.begin(); }
	const Property*	GetPropertiesEnd() const	{ return m_Properties.end(); }
	const float*	GetBufferBegin() const		{ return m_Buffer.begin(); }
	const float*	GetBufferEnd() const		{ return m_Buffer.end(); }

	
	const void* Find(const ShaderLab::FastPropertyName& name, UInt8 rows, UInt8 cols, size_t arraySize) const;
	int GetPropertyIndex (const ShaderLab::FastPropertyName& name) const;

private:
	dynamic_array<Property> m_Properties;
	dynamic_array<float> m_Buffer;
};

inline void MaterialPropertyBlock::Clear()
{
	m_Properties.resize_uninitialized(0);
	m_Buffer.resize_uninitialized(0);
}


#endif
