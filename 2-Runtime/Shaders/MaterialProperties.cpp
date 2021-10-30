#include "UnityPrefix.h"
#include "MaterialProperties.h"
#include "Material.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Math/Matrix4x4.h"

inline size_t CalculateSizeFromProperty (const MaterialPropertyBlock::Property& prop)
{
	return prop.rows * prop.cols * prop.arraySize;
}

MaterialPropertyBlock::MaterialPropertyBlock(Property* props, size_t propCount, float* buffer, size_t bufSize)
{
	m_Properties.assign_external(props, props + propCount);
	m_Buffer.assign_external(buffer, buffer + bufSize);
}

void MaterialPropertyBlock::AddProperty(const ShaderLab::FastPropertyName& name, const float* data, UInt8 rows, UInt8 cols, size_t arraySize)
{
	size_t offset = m_Buffer.size();
	Property prop = { name.index, rows, cols, kTexDimNone, arraySize, offset };
	m_Properties.push_back(prop);
	size_t size = rows * cols * arraySize;
	m_Buffer.resize_uninitialized(offset + size);
	memcpy(&m_Buffer[offset], data, size * sizeof(float));
}

void MaterialPropertyBlock::AddPropertyTexture(const ShaderLab::FastPropertyName& name, TextureDimension dim, TextureID tid)
{
	AddProperty (name, reinterpret_cast<float*> (&tid.m_ID), 1, 1, 1);
	m_Properties.back().texDim = dim;
}

void MaterialPropertyBlock::AddPropertyFloat(const ShaderLab::FastPropertyName& name, float val)
{
	AddProperty(name, &val, 1, 1, 1);
}

void MaterialPropertyBlock::AddPropertyVector(const ShaderLab::FastPropertyName& name, const Vector4f& vec)
{
	AddProperty(name, vec.GetPtr(), 1, 4, 1);
}

void MaterialPropertyBlock::AddPropertyColor(const ShaderLab::FastPropertyName& name, const ColorRGBAf& col)
{
	ColorRGBAf converted = GammaToActiveColorSpace (col);
	AddProperty(name, converted.GetPtr(), 1, 4, 1);
}

void MaterialPropertyBlock::AddPropertyMatrix(const ShaderLab::FastPropertyName& name, const Matrix4x4f& mat)
{
	AddProperty(name, mat.GetPtr(), 4, 4, 1);
}

void MaterialPropertyBlock::ReplacePropertyTexture(const ShaderLab::FastPropertyName& name, TextureDimension dim, TextureID tid)
{
	int index = GetPropertyIndex(name);
	if (index == -1)
		AddPropertyTexture(name, dim, tid);
	else
	{
		Property& prop = m_Properties[index];
		if (prop.rows == 1 && prop.cols == 1 && prop.arraySize == 1)
		{
			TextureID* buf = reinterpret_cast<TextureID*> (&m_Buffer[prop.offset]);
			*buf = tid;
			prop.texDim = dim;
		}
		else
		{
			ErrorString("The material property is different from already stored property.");
		}
	}	
}

void MaterialPropertyBlock::ReplacePropertyColor(const ShaderLab::FastPropertyName& name, const ColorRGBAf& col)
{
	ColorRGBAf activeColor = GammaToActiveColorSpace (col);
	ReplacePropertyVector (name, *reinterpret_cast<const Vector4f*> (&activeColor));
}

void MaterialPropertyBlock::ReplacePropertyVector(const ShaderLab::FastPropertyName& name, const Vector4f& vec)
{
	int index = GetPropertyIndex(name);
	if (index == -1)
		AddPropertyVector(name, vec);
	else
	{
		const Property& prop = m_Properties[index];
		if (prop.rows == 1 && prop.cols == 4 && prop.arraySize == 1)
		{
			Vector4f* buf = reinterpret_cast<Vector4f*> (&m_Buffer[prop.offset]);
			*buf = vec;
		}
		else
		{
			ErrorString("The material property is different from already stored property.");
		}
	}
}

void MaterialPropertyBlock::ReplacePropertyFloat(const ShaderLab::FastPropertyName& name, float data)
{
	ReplacePartialFloatProperty(name, data, 1, 0);
}

void MaterialPropertyBlock::ReplacePartialFloatColorProperty(const ShaderLab::FastPropertyName& name, float data, UInt8 cols, UInt8 colIndex)
{
	ReplacePartialFloatProperty(name, GammaToActiveColorSpace(data), cols, colIndex);
}

void MaterialPropertyBlock::ReplacePartialFloatProperty(const ShaderLab::FastPropertyName& name, float data, UInt8 cols, UInt8 colIndex)
{
	int index = GetPropertyIndex(name);
	if (index == -1)
	{
		float prop[4] = { 0.0F, 0.0F, 0.0F, 0.0F };
		prop[colIndex] = data;
		
		AddProperty(name, prop, 1, cols, 1);
	}
	else
	{
		const Property& prop = m_Properties[index];
		if (prop.rows == 1 && prop.cols == cols && prop.arraySize == 1)
		{
			float* buffer = reinterpret_cast<float*> (&m_Buffer[prop.offset]);
			buffer[colIndex] = data;
		}
		else
		{
			ErrorString("The material property is different from already stored property.");
		}
	}
}

int MaterialPropertyBlock::GetPropertyIndex (const ShaderLab::FastPropertyName& name) const
{
	for (int i=0;i<m_Properties.size();i++)
	{
		if (m_Properties[i].nameIndex == name.index)
			return i;
	}
	return -1;
}

const void* MaterialPropertyBlock::Find(const ShaderLab::FastPropertyName& name, UInt8 rows, UInt8 cols, size_t arraySize) const
{
	for (size_t i = 0, n = m_Properties.size(); i != n; ++i)
	{
		const Property& prop = m_Properties[i];
		if (name.index == prop.nameIndex && prop.cols == cols && prop.rows == rows)
			return &m_Buffer[prop.offset];
	}
	return NULL;
}


const float* MaterialPropertyBlock::FindFloat(const ShaderLab::FastPropertyName& name) const
{
	return static_cast<const float*> (Find(name, 1, 1, 1));
}

const Vector4f* MaterialPropertyBlock::FindVector(const ShaderLab::FastPropertyName& name) const
{
	return static_cast<const Vector4f*> (Find(name, 1, 4, 1));
}

const Matrix4x4f* MaterialPropertyBlock::FindMatrix(const ShaderLab::FastPropertyName& name) const
{
	return static_cast<const Matrix4x4f*> (Find(name, 4, 4, 1));
}

bool MaterialPropertyBlock::GetColor(const ShaderLab::FastPropertyName& name, ColorRGBAf& outColor) const
{
	const ColorRGBAf* color = static_cast<const ColorRGBAf*> (Find(name, 1, 4, 1));
	if (color != NULL)
	{
		outColor = ActiveToGammaColorSpace(*color);
		return true;
	}
	else
		return false;
}

const TextureID MaterialPropertyBlock::FindTexture(const ShaderLab::FastPropertyName& name) const
{
	for (size_t i = 0, n = m_Properties.size(); i != n; ++i)
	{
		const Property& prop = m_Properties[i];
		if (name.index == prop.nameIndex && prop.texDim != kTexDimNone)
			return TextureID(*(const int*)&m_Buffer[prop.offset]);
	}
	return TextureID();
}
