#pragma once

#include "Runtime/Mono/MonoTypes.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"

class Shader;
namespace Unity { class Material; }
class MaterialPropertyBlock;

// match MaterialProperty layout!
struct MonoMaterialProperty
{
	MonoArray*	m_Targets;
	MonoObject* m_DidModifyMaterialProperty;
	MonoString* m_Name;
	MonoString* m_DisplayName;
	MonoObject* m_Value;
	Vector4f    m_TextureScaleAndOffset;
	Vector2f	m_RangeLimits;
	int			m_Type;
	UInt32		m_Flags;
	int			m_TextureDimension;
	UInt32		m_MixedValueMask;
};

void ExtractMonoMaterialProperty (Shader* shader, int propertyIndex, MonoArray* mats, MonoMaterialProperty& res);
void MonoMaterialsArrayToUnityArray(MonoArray* mats, dynamic_array<Unity::Material*>& materials);
void ApplyMonoMaterialProperty(MonoMaterialProperty& monoPropPtr, int changedMask, const std::string& undoName);
int FindShaderPropertyIndex(const Shader* shader, const std::string& name);
void ApplyMaterialPropertyBlockToMaterialProperty (const MaterialPropertyBlock& propertyBlock, MonoMaterialProperty& res);
void ApplyMaterialPropertyToMaterialPropertyBlock (const MonoMaterialProperty& prop, int changedMask, MaterialPropertyBlock& propertyBlock);