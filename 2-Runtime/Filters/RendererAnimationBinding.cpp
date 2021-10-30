#include "UnityPrefix.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Animation/AnimationClipBindings.h"
#include "Renderer.h"
#include "External/shaderlab/Library/FastPropertyName.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/MaterialProperties.h"
#include "Runtime/Interfaces/IAnimationBinding.h"

#define MATERIAL_ANIMATION 1

class RendererAnimationBinding : public IAnimationBinding
{
#if UNITY_EDITOR
	virtual void GetAllAnimatableProperties (Object& targetObject, std::vector<EditorCurveBinding>& outProperties) const
	{
		Renderer& renderer = static_cast<Renderer&> (targetObject);
		
		for (int i=0;i<renderer.GetMaterialCount();i++)
			AddPPtrBinding (outProperties, targetObject.GetClassID(), Format("m_Materials.Array.data[%d]", i));
	}
#endif
	
	virtual float GetFloatValue (const UnityEngine::Animation::BoundCurve& bind) const
	{
		AssertString("unsupported"); return 0.0F;
	}
	
	virtual void SetFloatValue (const UnityEngine::Animation::BoundCurve& bound, float value) const
	{
		AssertString("unsupported");
	}
	
	virtual void SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const
	{
		Renderer& renderer = *static_cast<Renderer*> (bound.targetObject);
		int index = reinterpret_cast<int> (bound.targetPtr);
		
		if (index < renderer.GetMaterialCount ())
			renderer.SetMaterial(PPtr<Material> (value), index);
	}
	
	virtual SInt32 GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const
	{
		Renderer& renderer = *static_cast<Renderer*> (bound.targetObject);
		int index = reinterpret_cast<int> (bound.targetPtr);
		
		if (index < renderer.GetMaterialCount ())
			return renderer.GetMaterial(index).GetInstanceID();
		else
			return 0;
	}
	
	virtual bool GenerateBinding (const UnityStr& attribute, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const
	{
		int index = ParseIndexAttributeIndex (attribute, "m_Materials.Array.data[");
		if (index != -1 && pptrCurve)
		{
			outputBinding.attribute = index;
			return true;
		}
		
		return false;
	}
	
	virtual ClassIDType BindValue (Object& target, const UnityEngine::Animation::GenericBinding& outputBinding, UnityEngine::Animation::BoundCurve& bound) const
	{
		bound.targetPtr = reinterpret_cast<void*> (outputBinding.attribute);
		return ClassID(Material);
	}
};

#if MATERIAL_ANIMATION

const char* kMaterialPrefix = "material.";
class RendererMaterialAnimationBinding : public IAnimationBinding
{
public:

#if UNITY_EDITOR
	virtual void GetAllAnimatableProperties (Object& targetObject, std::vector<EditorCurveBinding>& outProperties) const
	{
		Renderer& renderer = static_cast<Renderer&> (targetObject);
		if (renderer.GetMaterialCount() == 0)
			return;

		int startIndex = outProperties.size();
		for (int i=0;i<renderer.GetMaterialCount();i++)
		{
			Material* material = renderer.GetMaterial(i);
			if (material == NULL)
				continue;

			ExtractAllMaterialAnimatableAttributes (*material, targetObject.GetClassID(), outProperties, startIndex);
		}
	}	
		
	static void ExtractAllMaterialAnimatableAttributes (Material& targetObject, int classID, std::vector<EditorCurveBinding>& outProperties, int startIndex)
	{
		Material& material = static_cast<Material&> (targetObject);
		
		const ShaderLab::PropertySheet& properties = material.GetProperties();
		
		const string materialPrefix = kMaterialPrefix;

		// Get all float properties
		const ShaderLab::PropertySheet::Floats& floats = properties.GetFloatsMap();
		for (ShaderLab::PropertySheet::Floats::const_iterator i=floats.begin();i != floats.end();i++)
		{
			AddBindingCheckUnique (outProperties, startIndex, classID, materialPrefix + i->first.GetName());
		}
		
		// Get all vector properties
		const ShaderLab::PropertySheet::Vectors& vectors = properties.GetVectorMap();
		for (ShaderLab::PropertySheet::Vectors::const_iterator i=vectors.begin();i != vectors.end();i++)
		{
			string prefix = materialPrefix + i->first.GetName();
			if (properties.GetColorTag(i->first))
			{
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".r");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".g");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".b");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".a");
			}
			else
			{
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".x");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".y");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".z");
				AddBindingCheckUnique (outProperties, startIndex, classID, prefix + ".w");
			}

		}
	}
#endif
	
	struct MaterialBinding
	{
		enum { kFloat4, kColor4, kFloat1 };
		
		UInt32 propertyName : 28;
		UInt32 colIndex : 2;
		UInt32 type : 2;
	};

	static UInt32 BindingToUInt32 (MaterialBinding binding)
	{
		UInt32 data;
		data = binding.propertyName;
		data |= binding.colIndex << 28;
		data |= binding.type << 30;
		return data;
	}

	static MaterialBinding UInt32ToBinding (UInt32 data)
	{
		MaterialBinding binding;
		binding.propertyName = data & 0x3FFFFFFF;
		binding.colIndex = (data >> 28) & 0x3;
		binding.type = (data >> 30) & 0x3;
		return binding;
	}
	
	virtual float GetFloatValue (const UnityEngine::Animation::BoundCurve& bind) const
	{
		Renderer& renderer = *static_cast<Renderer*> (bind.targetObject);
		MaterialBinding binding = *reinterpret_cast<const MaterialBinding*> (&bind.targetPtr);

		ShaderLab::FastPropertyName name;
		name.index = binding.propertyName;

		const MaterialPropertyBlock* block = renderer.GetPropertyBlock();

		// Extract from material property block
		if (block)
		{
			if (binding.type == MaterialBinding::kFloat1)
			{
				const float* value = block->FindFloat (name);
				if (value != NULL)
					return *value;
			}
			else if (binding.type == MaterialBinding::kFloat4)
			{
				const Vector4f* value = block->FindVector (name);
				if (value != NULL)
					return value->GetPtr()[binding.colIndex];
			}
			else if (binding.type == MaterialBinding::kColor4)
			{
				ColorRGBAf color;
				if (block->GetColor (name, color))
					return color.GetPtr()[binding.colIndex];
			}
		}
		
		// Extract from material
		for (int i=0;i<renderer.GetMaterialCount ();i++)
		{
			Material* material = renderer.GetMaterial(0);
			if (material == NULL)
				continue;
			if (!material->HasProperty (name))
				continue;

			if (binding.type == MaterialBinding::kFloat1)
				return material->GetFloat(name);
			else if (binding.type == MaterialBinding::kColor4)
				return material->GetColor(name).GetPtr()[binding.colIndex];
			else if (binding.type == MaterialBinding::kFloat4)
	            return material->GetColor(name).GetPtr()[binding.colIndex];
		}
			
		return 0.0F;
	}
	
	virtual void SetFloatValue (const UnityEngine::Animation::BoundCurve& bound, float value) const
	{
		Renderer& renderer = *static_cast<Renderer*> (bound.targetObject);
		MaterialBinding binding = *reinterpret_cast<const MaterialBinding*> (&bound.targetPtr);
		
		MaterialPropertyBlock& block = renderer.GetPropertyBlockRememberToUpdateHash();
		
		ShaderLab::FastPropertyName name;
		name.index = binding.propertyName;
		
		if (binding.type == MaterialBinding::kFloat1)
		{
			block.ReplacePropertyFloat (name, value);
		}
		else if (binding.type == MaterialBinding::kFloat4)
		{
			block.ReplacePartialFloatProperty (name, value, 4, binding.colIndex);
		}
		else if (binding.type == MaterialBinding::kColor4)
		{
			block.ReplacePartialFloatColorProperty (name, value, 4, binding.colIndex);
		}
		
		renderer.ComputeCustomPropertiesHash();
		
		// Force a repaint
		#if UNITY_EDITOR
		renderer.SetDirty();
		#endif
	}
	
	virtual void SetPPtrValue (const UnityEngine::Animation::BoundCurve& bound, SInt32 value) const { }
	
	virtual SInt32 GetPPtrValue (const UnityEngine::Animation::BoundCurve& bound) const { return 0; }
	
	virtual bool GenerateBinding (const UnityStr& attributeStr, bool pptrCurve, UnityEngine::Animation::GenericBinding& outputBinding) const
	{
		if (pptrCurve)
			return false;
		
		if (!BeginsWith(attributeStr, kMaterialPrefix))
			return false;
		
		MaterialBinding binding;
		
		const char* attribute = attributeStr.c_str() + strlen(kMaterialPrefix);
		const char* a = attribute;
		
		// Parse this:
		// mainColor.r
		// mainColor.x
		// mainColor.y
		// floatPropertyName
		
		// Find shader propertyname 
		int dotIndex = -1;
		const char* lastCharacter;
		while (*a != 0)
		{
			if (*a == '.' && dotIndex == -1)
				dotIndex = a - attribute;
			a++;
		}
		lastCharacter = a - 1;
		
		// No '.' found, thus it must be a float property
		if (dotIndex == -1)
		{
			binding.propertyName = ShaderLab::GenerateFastPropertyName28BitHash(attribute);
			binding.type = MaterialBinding::kFloat1;
			binding.colIndex = 0;
		}
		// Calculate different property types
		else
		{
			binding.propertyName = ShaderLab::GenerateFastPropertyName28BitHash(string(attribute, attribute + dotIndex).c_str());
			
			// There must be exactly one property ('r', 'g' etc after the .)
			if (dotIndex + 2 != strlen(attribute)) 
				return false;

			binding.type = MaterialBinding::kFloat4;
			switch (*lastCharacter)
			{
				case 'r':
				case 'g':
				case 'b':
				case 'a':
					binding.type = MaterialBinding::kColor4;
			}
			
			switch (*lastCharacter)
			{
				// r color or x vector
				case 'r':
				case 'x':
					binding.colIndex = 0;
					break;

				// g color or y vector
				case 'g':
				case 'y':
					binding.colIndex = 1;
					break;
					
				// b or z vector
				case 'b':
				case 'z':
					binding.colIndex = 2;
					break;
					
				// alpha or w vector
				case 'a':
				case 'w':
					binding.colIndex = 3;
					break;
				default:
					return false;
			}
		}
		
		outputBinding.attribute = BindingToUInt32 (binding);
		return true;
	}
	
	virtual ClassIDType BindValue (Object& target, const UnityEngine::Animation::GenericBinding& outputBinding, UnityEngine::Animation::BoundCurve& bound) const
	{
		MaterialBinding materialBinding = UInt32ToBinding(outputBinding.attribute);

		ShaderLab::FastPropertyName prop;
		prop.InitBy28BitHash (materialBinding.propertyName);
		materialBinding.propertyName = prop.index;

		bound.targetPtr = *reinterpret_cast<void**> (&materialBinding);
		return ClassID(float);
	}
};
#endif


static RendererAnimationBinding* gRendererBinding = NULL;
#if MATERIAL_ANIMATION
static RendererMaterialAnimationBinding* gMaterialBinding = NULL;
#endif

void InitializeRendererAnimationBindingInterface ()
{
	gRendererBinding = UNITY_NEW (RendererAnimationBinding, kMemAnimation);
	UnityEngine::Animation::GetGenericAnimationBindingCache ().RegisterIAnimationBinding (ClassID(Renderer), UnityEngine::Animation::kRendererMaterialPPtrBinding, gRendererBinding);

	#if MATERIAL_ANIMATION
	gMaterialBinding = UNITY_NEW (RendererMaterialAnimationBinding, kMemAnimation);
	UnityEngine::Animation::GetGenericAnimationBindingCache ().RegisterIAnimationBinding (ClassID(Renderer), UnityEngine::Animation::kRendererMaterialPropertyBinding, gMaterialBinding);
	#endif
}

void CleanupRendererAnimationBindingInterface ()
{
	UNITY_DELETE (gRendererBinding, kMemAnimation);
	#if MATERIAL_ANIMATION
	UNITY_DELETE (gMaterialBinding, kMemAnimation);
	#endif
}


#if ENABLE_UNIT_TESTS
#include "External/UnitTest++/src/UnitTest++.h"

SUITE (MaterialBindingTests)
{
	TEST (MaterialBindingUInt32Conversion)
	{
		RendererMaterialAnimationBinding::MaterialBinding binding;
		binding.propertyName = 12345678;
		binding.colIndex = 3;
		binding.type = 3;
		
		RendererMaterialAnimationBinding::MaterialBinding converted = RendererMaterialAnimationBinding::UInt32ToBinding (RendererMaterialAnimationBinding::BindingToUInt32 (binding));
		CHECK_EQUAL (converted.propertyName, binding.propertyName);
		CHECK_EQUAL (converted.colIndex, binding.colIndex);
		CHECK_EQUAL (converted.type, binding.type);
	}

	TEST (MaterialBindingCorrectlyEncodesAllBits)
	{
		CHECK_EQUAL ( RendererMaterialAnimationBinding::BindingToUInt32(RendererMaterialAnimationBinding::UInt32ToBinding (0xFFFFFFFF)), 0xFFFFFFFF);
		CHECK_EQUAL ( RendererMaterialAnimationBinding::BindingToUInt32(RendererMaterialAnimationBinding::UInt32ToBinding (0)), 0);
	}
}
#endif