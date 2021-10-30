#include "UnityPrefix.h"
#include "ShaderUtility.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Misc/AssetBundle.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "External/shaderlab/Library/SLParserData.h"
#include "Editor/Src/Selection.h"
#include "Editor/Src/Undo/Undo.h"
#include "Runtime/Scripting/Scripting.h"

static Texture* GetTextureFromRenderer(const ShaderLab::FastPropertyName& propName)
{
	GameObject* go = Selection::GetActiveGO();
	if (!go)
		return NULL;
	Renderer* renderer = go->QueryComponent(Renderer);
	if (!renderer)
		return NULL;
	if (!renderer->HasPropertyBlock())
		return NULL;

	///@TODO: Const??
	const MaterialPropertyBlock& props = renderer->GetPropertyBlockRememberToUpdateHash();

	TextureID v = props.FindTexture (propName);
	if (v.m_ID == 0)
		return NULL;
	Texture* tex = Texture::FindTextureByID (v);
	return tex;
}

inline bool IsTextureChangeBit (int changedMask) { return (changedMask & 1) != 0; }
inline bool IsScaleAndOffsetBit (int changedMask) { changedMask >>= 1; return changedMask != 0; }

static Material* GetMaterialFromMonoArray(MonoArray* mats, int index)
{
	ScriptingObjectPtr o = Scripting::GetScriptingArrayElementNoRef<ScriptingObjectPtr>(mats,index);
	return ScriptingObjectToObject<Material>(o);
}

void ApplyMaterialPropertyToMaterialPropertyBlock (const MonoMaterialProperty& prop, int changedMask, MaterialPropertyBlock& propertyBlock)
{
	string propNameStr = scripting_cpp_string_for(prop.m_Name);
	ShaderLab::FastPropertyName propName = ShaderLab::Property(propNameStr);
	
	switch (prop.m_Type)
	{
	case ShaderLab::ParserProperty::kColor:
		propertyBlock.ReplacePropertyColor (propName, ExtractMonoObjectData<ColorRGBAf>(prop.m_Value));
		break;
	case ShaderLab::ParserProperty::kVector:
		propertyBlock.ReplacePropertyVector (propName, ExtractMonoObjectData<Vector4f>(prop.m_Value));
		break;
	case ShaderLab::ParserProperty::kFloat:
	case ShaderLab::ParserProperty::kRange:
		propertyBlock.ReplacePropertyFloat (propName, ExtractMonoObjectData<float>(prop.m_Value));
		break;
	case ShaderLab::ParserProperty::kTexture:
	{
		Texture* v = ScriptingObjectToObject<Texture>(prop.m_Value);
		if (IsTextureChangeBit (changedMask))
			propertyBlock.ReplacePropertyTexture(propName, v->GetDimension(), v->GetTextureID());

		if (IsScaleAndOffsetBit (changedMask))
		{
			propertyBlock.ReplacePropertyVector (ShaderLab::Property(propNameStr + "_ST"), prop.m_TextureScaleAndOffset);
		}		
		break;
	}
	default:
		AssertString("unknown shader property type");
		break;
	}		
}


void ApplyMaterialPropertyBlockToMaterialProperty (const MaterialPropertyBlock& propertyBlock, MonoMaterialProperty& prop)
{
	string propNameStr = scripting_cpp_string_for(prop.m_Name);
	ShaderLab::FastPropertyName propName = ShaderLab::Property (propNameStr);

	switch (prop.m_Type)
	{
		case ShaderLab::ParserProperty::kColor:
		{
			propertyBlock.GetColor(propName, ExtractMonoObjectData<ColorRGBAf>(prop.m_Value));
		}
		break;
	
		case ShaderLab::ParserProperty::kVector:
		{
			const Vector4f* v = propertyBlock.FindVector(propName);
			if (v)
				ExtractMonoObjectData<Vector4f>(prop.m_Value) = *v;
		}
		break;
	
		case ShaderLab::ParserProperty::kFloat:
		case ShaderLab::ParserProperty::kRange:
		{
			const float* floatValue =  propertyBlock.FindFloat(propName);
			if (floatValue)
				ExtractMonoObjectData<float>(prop.m_Value) = *floatValue;
		}
		break;

		case ShaderLab::ParserProperty::kTexture:
		{
			Texture* tex = Texture::FindTextureByID(propertyBlock.FindTexture(propName));
			if (tex)
				prop.m_Value = Scripting::ScriptingWrapperFor(tex);
		
			const Vector4f* v = propertyBlock.FindVector(ShaderLab::Property(propNameStr + "_ST"));
			if (v)
				prop.m_TextureScaleAndOffset = *v;
		}
		break;
	default:
		AssertString("unknown shader property type");
		break;
	}
}

Vector4f ExtractScaleOffset (Material& material, ShaderLab::FastPropertyName propName)
{
	Vector2f scale = material.GetTextureScale (propName);
	Vector2f offset = material.GetTextureOffset (propName);

	return Vector4f (scale.x, scale.y, offset.x, offset.y);
}


void ExtractMonoMaterialProperty (Shader* shader, int propertyIndex, MonoArray* mats, MonoMaterialProperty& res)
{
	Assert (shader);
	const int propCount = shader->GetPropertyCount();
	if (propertyIndex < 0 || propertyIndex >= propCount)
		return;

	const ShaderLab::ParserProperty& prop = *shader->GetPropertyInfo(propertyIndex);
	res.m_Targets = mats;
	res.m_Name = scripting_string_new(prop.m_Name);
	res.m_DisplayName = scripting_string_new(prop.m_Description);

	res.m_Value = SCRIPTING_NULL;
	res.m_MixedValueMask = 0;
	ShaderLab::FastPropertyName propName = ShaderLab::Property(prop.m_Name);
	const int matsSize = GetScriptingArraySize(mats);
	switch (prop.m_Type)
	{
	case ShaderLab::ParserProperty::kColor:
	case ShaderLab::ParserProperty::kVector:
		{
			res.m_Value = mono_object_new(mono_domain_get(),
				prop.m_Type==ShaderLab::ParserProperty::kColor ? MONO_COMMON.color : MONO_COMMON.vector4);
			ColorRGBAf& v = ExtractMonoObjectData<ColorRGBAf>(res.m_Value);
			if (mats != SCRIPTING_NULL)
			{
				v = GetMaterialFromMonoArray(mats,0)->GetColor (propName);
				for (size_t i = 1; i < matsSize; ++i)
				{
					if (GetMaterialFromMonoArray(mats,i)->GetColor (propName).NotEquals(v))
					{
						res.m_MixedValueMask = 1;
						break;
					}
				}
			}
			else
			{
				v.r = prop.m_DefValue[0];
				v.g = prop.m_DefValue[1];
				v.b = prop.m_DefValue[2];
				v.a = prop.m_DefValue[3];
			}
		}
		break;
	case ShaderLab::ParserProperty::kFloat:
	case ShaderLab::ParserProperty::kRange:
		{
			res.m_Value = mono_object_new(mono_domain_get(), MONO_COMMON.floatSingle);
			float& v = ExtractMonoObjectData<float>(res.m_Value);
			if (mats != SCRIPTING_NULL)
			{
				v = GetMaterialFromMonoArray(mats,0)->GetFloat (propName);
				for (size_t i = 1; i < matsSize; ++i)
				{
					if (GetMaterialFromMonoArray(mats,i)->GetFloat (propName) != v)
					{
						res.m_MixedValueMask = 1;
						break;
					}
				}
			}
			else
			{
				v = prop.m_DefValue[0];
			}
		}
		break;
	case ShaderLab::ParserProperty::kTexture:
		{
			Texture* v = NULL;
			Vector4f scaleAndOffset = Vector4f (1,1,0,0);
			if (mats != SCRIPTING_NULL)
			{
				if (prop.m_Flags & ShaderLab::ParserProperty::kPropFlagPerRendererData)
				{
					v = GetTextureFromRenderer (propName);
					// If no per-renderer texture is set, display some indicator texture
					if (!v)
						v = GetEditorAssetBundle()->Get<Texture>("Previews/Textures/textureExternal.png");
				}

				if (v == NULL)
				{
					Material* curMat = GetMaterialFromMonoArray(mats,0);
					scaleAndOffset = ExtractScaleOffset(*curMat, propName);
					v = curMat->GetTexture(propName);
					for (size_t i = 1; i < matsSize; ++i)
					{
						curMat = GetMaterialFromMonoArray(mats,i);

						// For textures mixed mask 0 bit represents the texture
						if (curMat->GetTexture (propName) != v)
							res.m_MixedValueMask |= 1;

						// 1-4 bit represents the scale and offset
						Vector4f curScaleAndOffset = ExtractScaleOffset(*curMat, propName);
						for (int c=0;c<4;c++)
						{
							bool isComponentMixed = curScaleAndOffset[c] != scaleAndOffset[c];
							if (isComponentMixed)
								res.m_MixedValueMask |= 1 << (c + 1);
						}
					}
				}
			}
			else
			{
				//@TODO: default texture
			}

			res.m_Value = Scripting::ScriptingWrapperFor(v);
			res.m_TextureScaleAndOffset = scaleAndOffset;
		}
		break;
	default:
		AssertString("unknown shader property type");
		break;
	}

	res.m_RangeLimits.x = prop.m_DefValue[1];
	res.m_RangeLimits.y = prop.m_DefValue[2];
	res.m_Type = prop.m_Type;
	res.m_Flags = prop.m_Flags;
	res.m_TextureDimension = prop.m_DefTexture.m_TexDim;
}


void MonoMaterialsArrayToUnityArray(MonoArray* mats, dynamic_array<Unity::Material*>& materials)
{
	if (mats == SCRIPTING_NULL)
		Scripting::RaiseNullException("material array is null");

	int matSize = GetScriptingArraySize(mats);
	materials.resize_uninitialized (matSize);
	for (int i = 0; i < matSize; ++i)
	{
		ScriptingObjectPtr o = Scripting::GetScriptingArrayElementNoRef<ScriptingObjectPtr>(mats,i);
		materials[i] = ScriptingObjectToObject<Material>(o);
	}
}


void ApplyMonoMaterialProperty(MonoMaterialProperty& prop, int changedMask, const std::string& undoName)
{
	if (prop.m_Targets == SCRIPTING_NULL)
		return;

	dynamic_array<Material*> materials;
	MonoMaterialsArrayToUnityArray (prop.m_Targets, materials);
	if (materials.empty())
		return;

	// register undo
	RegisterUndo(materials[0], (Object**)&materials[0], materials.size(), undoName, 0);

	ShaderLab::FastPropertyName propName = ShaderLab::Property(scripting_cpp_string_for(prop.m_Name));
	switch (prop.m_Type)
	{
	case ShaderLab::ParserProperty::kColor:
	case ShaderLab::ParserProperty::kVector:
		{
			ColorRGBAf& v = ExtractMonoObjectData<ColorRGBAf>(prop.m_Value);
			for (size_t i = 0, n = materials.size(); i != n; ++i)
				materials[i]->SetColor (propName, v);
		}
		break;
	case ShaderLab::ParserProperty::kFloat:
	case ShaderLab::ParserProperty::kRange:
		{
			float& v = ExtractMonoObjectData<float>(prop.m_Value);
			for (size_t i = 0, n = materials.size(); i != n; ++i)
				materials[i]->SetFloat (propName, v);
		}
		break;
	case ShaderLab::ParserProperty::kTexture:
		{
			Texture* v = ScriptingObjectToObject<Texture>(prop.m_Value);
			if (IsTextureChangeBit (changedMask))
			{
				for (size_t i = 0, n = materials.size(); i != n; ++i)
					materials[i]->SetTexture (propName, v);
			}

			if (IsScaleAndOffsetBit (changedMask))
			{
				Vector4f scaleAndOffset = prop.m_TextureScaleAndOffset;
				for (size_t i = 0, n = materials.size(); i != n; ++i)
				{
					materials[i]->SetTextureOffset (propName, Vector2f (scaleAndOffset.z, scaleAndOffset.w));
					materials[i]->SetTextureScale (propName, Vector2f (scaleAndOffset.x, scaleAndOffset.y));
				}
			}
		}
		break;
	default:
		AssertString("unknown shader property type");
		break;
	}
}

int FindShaderPropertyIndex(const Shader* shader, const std::string& name)
{
	if (!shader)
		return -1;
	const int size = shader->GetPropertyCount();
	for (int i = 0; i < size; ++i)
	{
		const ShaderLab::ParserProperty& prop = *shader->GetPropertyInfo(i);
		if (prop.m_Name == name)
			return i;
	}
	return -1;
}
