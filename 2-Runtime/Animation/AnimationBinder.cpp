#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "AnimationBinder.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include "Runtime/Animation/Animator.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Animation/AnimatorController.h"
#include "Runtime/mecanim/animation/avatar.h"
#if ENABLE_MONO
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#endif
#include "External/shaderlab/Library/FastPropertyName.h"
#include "External/shaderlab/Library/properties.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

using namespace ShaderLab;
using namespace std;

void AnimationBinder::InitCurveIDLookup (AnimationBinder::CurveIDLookup& curveIDLookup)
{
	curveIDLookup.set_empty_key(CurveID("", -1, NULL, "", 0));
	curveIDLookup.set_deleted_key(CurveID("", -1, NULL, "", 1));
	curveIDLookup.resize(1024);
}

inline bool IsAnimatableProperty (const TypeTree* variable, bool isScript, Object* targetObject)
{
	if (variable && isScript)
	{
		#if ENABLE_MONO
		MonoBehaviour* behaviour = static_cast<MonoBehaviour*> (targetObject);
		MonoObject* instance = behaviour->GetInstance();
		if (instance)
		{
			UInt32 offset = reinterpret_cast<UInt8*> (variable->m_DirectPtr) - reinterpret_cast<UInt8*> (instance);
			UInt32 size = mono_class_instance_size(behaviour->GetClass());
			return offset < size || variable->m_ByteOffset != -1;
		}
		#endif
		return false;
	}
	else if (variable)
	{
		return variable->m_ByteOffset != -1;
	}
	else
		return false;
}

AnimationBinder::~AnimationBinder ()
{
	for (TypeTreeCache::iterator i=m_TypeTreeCache.begin();i!=m_TypeTreeCache.end();i++)
		delete i->second;
}

inline int GetAnimatableBindType (const TypeTree& variable)
{
	if (variable.m_Type == "float")
	{
		return kBindFloat;
	}
	else if (variable.m_Type == "bool" || (variable.m_Type == "UInt8" && (variable.m_MetaFlag & kEditorDisplaysCheckBoxMask)))
	{
		return kBindFloatToBool;
	}
	else if (variable.m_Type == "PPtr<Material>")
	{
		return kBindMaterialPPtrToRenderer;
	}
#if ENABLE_SPRITES
	else if (variable.m_Type == "PPtr<Sprite>")
	{
		return kBindSpritePPtrToSpriteRenderer;
	}
#endif
	else
		return kUnbound;
}


#if UNITY_EDITOR
bool AnimationBinder::IsAnimatablePropertyOrHasAnimatableChild (const TypeTree& variable, bool isScript, Object* targetObject)
{
	if (variable.m_Children.empty())
	{
		if ( IsAnimatableProperty(&variable, isScript, targetObject))
		{
			return GetAnimatableBindType(variable) != kUnbound;
		}
	}
	
	for (TypeTree::const_iterator i=variable.begin();i!=variable.end();++i)
	{
		if (IsAnimatablePropertyOrHasAnimatableChild(*i, isScript, targetObject))
			return true;
	}
	
	return false;
}
#endif


static const char* ParseBlendShapeWeightName (const char* attribute)
{
	const char* prefix = "blendShape.";
	if (BeginsWith(attribute, prefix))
		return attribute + strlen(prefix);
	else
		return NULL;
}

static bool BlendShapeCalculateTargetPtr(Object* targetObject, const std::string& attribute, void** targetPtr, int* type)
{
	const char* name = ParseBlendShapeWeightName (attribute.c_str());
	if (name == NULL)
		return false;
	
	SkinnedMeshRenderer* renderer = static_cast<SkinnedMeshRenderer*>(targetObject);
	Assert(renderer);

	const Mesh* mesh = renderer->GetMesh();
	if (mesh == NULL)
		return false;
	
	const BlendShapeData& blendShapes = mesh->GetBlendShapeData();
	int index = GetChannelIndex (blendShapes, name);
	if (index == -1)
		return false;
	
	// Encode targetType
	*type = kBindFloatToBlendShapeWeight | (index << BoundCurveDeprecated::kBindTypeBitCount);
	*targetPtr = renderer;
	
	Assert((*type >> BoundCurveDeprecated::kBindTypeBitCount) == index);
	
	return true;
}

bool AnimationBinder::CalculateTargetPtr(int classID, Object* targetObject, const char* attribute, void** targetPtr, int* type)
{
	Assert(kBindTypeCount <= (1 << BoundCurveDeprecated::kBindTypeBitCount));
	AssertIf(targetObject == NULL);

	if (classID == ClassID(Transform))
	{
		Transform* transformTarget = static_cast<Transform*> (targetObject);
		
		if (strcmp(attribute, "m_LocalPosition") == 0)
		{
			*type = kBindTransformPosition;
			*targetPtr = &transformTarget->m_LocalPosition;
			return true;
		}
		else if (strcmp(attribute, "m_LocalScale") == 0)
		{
			*type = kBindTransformScale;
			*targetPtr = &transformTarget->m_LocalScale;
			return true;
		}
		else if (strcmp (attribute, "m_LocalRotation") == 0)
		{
			*type = kBindTransformRotation;
			*targetPtr = &transformTarget->m_LocalRotation;
			return true;
		}
	}
	else if (classID == ClassID(Material))
	{
//		Renderer* renderer = static_cast<Transform*> (targetObject);

	// [0].mainTex.offset.x
	// [0].mainTex.offset.y
	// [0].mainTex.scale.y
	// [0].mainTex.rotation
	// [0].mainColor.r
	// [0].mainColor.x
	// [0].floatPropertyName
		
		int materialIndex = 0;
		int shaderPropertyIndex = 0;
		int newTargetType = kUnbound;
		int targetIndex = 0;
		
		// Grab material index "[3]."
		const char* a = attribute;
		if (*a == '[')
		{
			while (*a != 0 && *a != '.')
				a++;
			
			if (*a == '.')
				materialIndex = StringToInt(attribute + 1);
			else
				return false;
			attribute = a + 1;
		}


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
		
		// No . must be float property
		if (dotIndex == -1)
		{
			newTargetType = kBindFloatToMaterial;
			shaderPropertyIndex = Property(attribute).index;
		}
		// Calculate different property types
		else
		{
			shaderPropertyIndex = Property(string(attribute, attribute + dotIndex)).index;
			attribute += dotIndex + 1;
			
			switch (*attribute)
			{
				// g color or y vector
				case 'g':
				case 'y':
					newTargetType = kBindFloatToColorMaterial;
					targetIndex = 1;
					break;

				// b or z vector
				case 'b':
				case 'z':
					newTargetType = kBindFloatToColorMaterial;
					targetIndex = 2;
					break;
				
				// alpha or w vector
				case 'a':
				case 'w':
					newTargetType = kBindFloatToColorMaterial;
					targetIndex = 3;
					break;
				
				// uv scale
				case 's':
					newTargetType = kBindFloatToMaterialScaleAndOffset;
					targetIndex = *lastCharacter == 'x' ? 0 : 1;
					break;
				// uv offset
				case 'o':
					newTargetType = kBindFloatToMaterialScaleAndOffset;
					targetIndex = *lastCharacter == 'x' ? 2 : 3;
					break;
				
				// r color
				case 'r':
					if (lastCharacter == attribute)
					{
						newTargetType = kBindFloatToColorMaterial;
						targetIndex = 0;
					}
					break;
				// x vector
				case 'x':
					newTargetType = kBindFloatToColorMaterial;
					targetIndex = 0;
					break;
			}
		}
		
		if (newTargetType != kUnbound)
		{
			Assert(BoundCurveDeprecated::kBindTypeBitCount + BoundCurveDeprecated::kBindMaterialShaderPropertyNameBitCount < 32);
			Assert(newTargetType < (1 << BoundCurveDeprecated::kBindTypeBitCount));
			Assert(shaderPropertyIndex < (1 << BoundCurveDeprecated::kBindMaterialShaderPropertyNameBitCount));
			Assert(targetIndex < (1 << (32 - BoundCurveDeprecated::kBindTypeBitCount - BoundCurveDeprecated::kBindMaterialShaderPropertyNameBitCount)));

			// Encode targetType
			newTargetType |= targetIndex << (BoundCurveDeprecated::kBindMaterialShaderPropertyNameBitCount + BoundCurveDeprecated::kBindTypeBitCount);
			newTargetType |= shaderPropertyIndex << BoundCurveDeprecated::kBindTypeBitCount;
			*targetPtr = reinterpret_cast<void*> (materialIndex);
			*type = newTargetType;
			return true;
		}
		else
		{
			*targetPtr = NULL;
			*type = kUnbound;
			return false;
		}
	}
	else if (classID == ClassID(GameObject))
	{
		if (strcmp(attribute, "m_IsActive") == 0)
		{
			*type = kBindFloatToGameObjectActivate;
			*targetPtr = targetObject;
			return true;
		}
	}
	else if (classID == ClassID(SkinnedMeshRenderer))
	{
		// We do not return on false, because this paths handles only animation for "blendShapeWeights[i]" 
		// and user might want to animate something else
		if (BlendShapeCalculateTargetPtr(targetObject, attribute, targetPtr, type))
			return true;
	}
	
	bool isScript =
		#if ENABLE_MONO
		classID == ClassID(MonoBehaviour);
		#else
		false;
		#endif

	TypeTree* typeTree = NULL;
	if (m_TypeTreeCache.count(classID))
		typeTree = m_TypeTreeCache.find(classID)->second;
	else
	{
		// Build proxy
		typeTree = new TypeTree();	
		GenerateTypeTree (*targetObject, typeTree);
		if (!isScript)
			m_TypeTreeCache[classID] = typeTree;
	}
	
	*type = kUnbound;
	*targetPtr = NULL;
	
	// Find attribute
	// * Check if we support binding that value
	// * scripts use direct ptrs but it only works reliable at the root level, because other variables may be moved/deleted arbitrarily
	const TypeTree* variable = FindAttributeInTypeTreeNoArrays (*typeTree, attribute);

	if (IsAnimatableProperty(variable, isScript, targetObject))
	{
		*type = GetAnimatableBindType(*variable);

		if (*type != kUnbound)
		{
			if (variable->m_ByteOffset != -1)
				*targetPtr = reinterpret_cast<UInt8*>(targetObject) + variable->m_ByteOffset;
			else
				*targetPtr = variable->m_DirectPtr;
		}
	}

	if (isScript)
		delete typeTree;
	
	return *type != kUnbound;
}


//@TODO: Stop supporting this. Only support batched BindCurve
bool AnimationBinder::BindCurve (const CurveID& curveID, BoundCurveDeprecated& bound, Transform& transform)
{
	// Lookup without path
	Object* targetObject = NULL;
	Transform* child = &transform;
	if (curveID.path[0] != '\0')
	{
		child = FindRelativeTransformWithPath(*child, curveID.path);
		if (child == NULL)
			return false;
	}
	
	// Lookup gameobject
	if (curveID.classID == ClassID(GameObject))
	{
		targetObject = &child->GetGameObject();
	}
	// Lookup material
	else if (curveID.classID == ClassID(Material))
	{
		targetObject = GetComponentWithScript(child->GetGameObject(), ClassID(Renderer), curveID.script);
		if (targetObject == NULL)
			return false;
	}
	// Lookup component
	else if (curveID.classID != ClassID(Material))
	{
		targetObject = GetComponentWithScript(child->GetGameObject(), curveID.classID, curveID.script);
		if (targetObject == NULL)
			return false;
	}	
	
	int type;
	void* targetPtr;
	if (!CalculateTargetPtr(curveID.classID, targetObject, curveID.attribute, &targetPtr, &type))
		return false;
	
	bound.targetPtr = reinterpret_cast<UInt8*>(targetPtr);
	bound.targetType = type;
	bound.targetObject = targetObject;
	bound.targetInstanceID = targetObject->GetInstanceID();

	return true;
}

static void ClearTransformTemporaryFlag (Transform& transform)
{
	transform.SetTemporaryFlags(0);
	Transform::iterator end = transform.end();
	for (Transform::iterator i=transform.begin();i!=end;i++)
		ClearTransformTemporaryFlag(**i);
}

static void CalculateTransformRoots (Transform& transform, AnimationBinder::AffectedRootTransforms& affectedRootTransforms)
{
	if (transform.GetTemporaryFlags())
	{
		affectedRootTransforms.push_back(&transform);
	}
	else
	{
		Transform::iterator end = transform.end();
		for (Transform::iterator i=transform.begin();i!=end;i++)
			CalculateTransformRoots(**i, affectedRootTransforms);
	}
}

void AnimationBinder::RemoveUnboundCurves (CurveIDLookup& lookup, BoundCurves& outBoundCurves)
{
	CurveIDLookup::iterator i;
	// Some curves couldn't be bound. We want to avoid the runtime check so we remove from the lookup and boundcurves array completely.
	// - we already removed them from the lookup table
	// - Now we need to remap the bound curves and 
	if (lookup.size() != outBoundCurves.size())
	{
		if (lookup.empty())
		{
			outBoundCurves.clear();
			return;
		}
		
		BoundCurves tempBoundCurves;
		tempBoundCurves.resize_uninitialized(lookup.size());
		
		// Build a remap table that will compact the array - erasing the curves that are undefined
		vector<int> remap;
		remap.resize(outBoundCurves.size());
		int validCount = 0;
		for (int j=0;j<outBoundCurves.size();j++)
		{
			remap[j] = validCount;
			if (outBoundCurves[j].targetType != kUnbound)
			{
				tempBoundCurves[validCount] = outBoundCurves[j];
				validCount++;
			}
		}
		
		for (i=lookup.begin();i != lookup.end();i++)
			i->second = remap[i->second];
		
		tempBoundCurves.swap(outBoundCurves);	
	}
}

void AnimationBinder::BindCurves (const CurveIDLookup& lookup, GameObject& rootGameObject, BoundCurves& outBoundCurves)
{
	AffectedRootTransforms affectedRoot;
	int transformMessageMask = 0;
	BindCurves(lookup, rootGameObject.GetComponent(Transform), outBoundCurves, affectedRoot, transformMessageMask);
}



void AnimationBinder::BindCurves (const CurveIDLookup& lookup, Transform& transform, BoundCurves& outBoundCurves, AffectedRootTransforms& affectedRootTransforms, int& transformChangedMask)
{
	outBoundCurves.resize_uninitialized(lookup.size());
	affectedRootTransforms.clear();
	transformChangedMask = 0;
	ClearTransformTemporaryFlag(transform);
	
	// Go through all lookups. Find their binder and assign it.
	CurveIDLookup::const_iterator next, i;
	for (i=lookup.begin();i != lookup.end();i=next)
	{
		next = i;
		next++;
		
		const CurveID& curveID = i->first;
		int bindIndex = i->second;
		
		outBoundCurves[bindIndex].targetPtr = NULL;
		outBoundCurves[bindIndex].targetObject = NULL;
		outBoundCurves[bindIndex].targetInstanceID = 0;
		outBoundCurves[bindIndex].targetType = kUnbound;
		
		// Lookup without path
		Object* targetObject = NULL;
		GameObject* go = NULL;
		if (curveID.path[0] != '\0')
		{
			// Lookup child
			Transform* child = FindRelativeTransformWithPath(transform, curveID.path);
			if (child == NULL)
			{
				#if DEBUG_ANIMATIONS
				LogString(Format("Animation bind couldn't find transform child %s", curveID.path));
				#endif
				#if COMPACT_UNBOUND_CURVES
				lookup.erase(i);
				#endif
				continue;					
			}
			
			go = &child->GetGameObject();

		}
		else
		{
			go = &transform.GetGameObject();
		}

		// Lookup component

		if (curveID.classID == ClassID(GameObject))
		{
			targetObject = go;
		}
		else if (curveID.classID != ClassID(Material))
		{
			targetObject = GetComponentWithScript(*go, curveID.classID, curveID.script);
			if (targetObject == NULL)
			{
				#if DEBUG_ANIMATIONS
				LogString(Format("Animation couldn't find %s", Object::ClassIDToString(curveID.classID))); 
				#endif
				#if COMPACT_UNBOUND_CURVES
				lookup.erase(i);
				#endif
				continue;
			}
		}
		// Lookup material
		else
		{
			targetObject = GetComponentWithScript(*go, ClassID(Renderer), curveID.script);
			if (targetObject == NULL)
			{
				#if DEBUG_ANIMATIONS
				LogString(Format("Animation couldn't find %s", Object::ClassIDToString(curveID.classID))); 
				#endif
				#if COMPACT_UNBOUND_CURVES
				lookup.erase(i);
				#endif
				continue;
			}
		}
		
		
		int type;
		void* targetPtr;
		if (!CalculateTargetPtr(curveID.classID, targetObject, curveID.attribute, &targetPtr, &type))
		{
			#if DEBUG_ANIMATIONS
			LogString(Format("Couldn't bind animation attribute %s.%s", Object::ClassIDToString(curveID.classID).c_str(), curveID.attribute));
			#endif
			#if COMPACT_UNBOUND_CURVES
			lookup.erase(i);
			#endif
			continue;
		}

		// - Precalculate affected root transform (Where do we send the transform changed message to)
		// - Precalculated transform changed mask (What part of the transform has changed)
		if (curveID.classID == ClassID(Transform))
		{
			targetObject->SetTemporaryFlags(1);
		
			if ((transformChangedMask & Transform::kRotationChanged) == 0 && BeginsWith(curveID.attribute, "m_LocalRotation"))
				transformChangedMask |= Transform::kRotationChanged;
			if ((transformChangedMask & Transform::kPositionChanged) == 0 && BeginsWith(curveID.attribute, "m_LocalPosition"))
				transformChangedMask |= Transform::kPositionChanged;
			if ((transformChangedMask & Transform::kScaleChanged) == 0 && BeginsWith(curveID.attribute, "m_LocalScale"))
				transformChangedMask |= Transform::kScaleChanged;
		}
			
		outBoundCurves[bindIndex].targetPtr = targetPtr;
		outBoundCurves[bindIndex].targetType = type;
		outBoundCurves[bindIndex].targetObject = targetObject;
		outBoundCurves[bindIndex].targetInstanceID = targetObject->GetInstanceID();
		
		#if DEBUG_ANIMATIONS
		outBoundCurves[bindIndex].attribute = curveID.attribute;
		outBoundCurves[bindIndex].path = curveID.path;
		outBoundCurves[bindIndex].klass = Object::ClassIDToString(curveID.classID);
		#endif
	}
	
	CalculateTransformRoots (transform, affectedRootTransforms);
}

AnimationBinder* AnimationBinder::s_Instance = NULL;

void AnimationBinder::StaticInitialize()
{
	s_Instance = UNITY_NEW(AnimationBinder, kMemAnimation);
}

void AnimationBinder::StaticDestroy()
{
	UNITY_DELETE(s_Instance, kMemAnimation);
}

static RegisterRuntimeInitializeAndCleanup s_AnimationBinderCallbacks(AnimationBinder::StaticInitialize, AnimationBinder::StaticDestroy);

AnimationBinder& GetAnimationBinder()
{
	return *AnimationBinder::s_Instance;
}


inline Material* GetInstantiatedMaterial (const BoundCurveDeprecated& bind)
{
	unsigned int materialIndex = reinterpret_cast<intptr_t> (bind.targetPtr);
	Renderer* renderer = static_cast<Renderer*> (bind.targetObject);
	
	if (materialIndex < renderer->GetMaterialCount())
		return renderer->GetAndAssignInstantiatedMaterial(materialIndex, true);
	else
		return NULL;
}

bool AnimationBinder::SetFloatValue (const BoundCurveDeprecated& bind, float value)
{
	UInt32 targetType = bind.targetType;
	Assert(bind.targetType != kUnbound && bind.targetType != kBindTransformRotation && bind.targetType != kBindTransformPosition && bind.targetType != kBindTransformScale);
	
	targetType &= BoundCurveDeprecated::kBindTypeMask;
	
	if( targetType == kBindFloat )
	{
		*reinterpret_cast<float*>(bind.targetPtr) = value;
		return true;
	}
	else if( targetType == kBindFloatToGameObjectActivate )
	{
		bool activeState = AnimationFloatToBool(value);
		GameObject* go = static_cast<GameObject*> (bind.targetObject);
		go->SetSelfActive (activeState);
		
		return true;
	}
	else if( targetType == kBindFloatToBool )
	{
		*reinterpret_cast<UInt8*>(bind.targetPtr) = AnimationFloatToBool(value);
		return true;
	}
	else if (targetType == kBindFloatToBlendShapeWeight)
	{
		SkinnedMeshRenderer* renderer = reinterpret_cast<SkinnedMeshRenderer*>(bind.targetObject);

		const int shapeIndex = bind.targetType >> BoundCurveDeprecated::kBindTypeBitCount;
		renderer->SetBlendShapeWeight(shapeIndex, value);
		return true;
	}
	else
	{
		Material* material = GetInstantiatedMaterial (bind);
		if (material != NULL)
		{
			targetType = bind.targetType;
			
			// Extract value index, shader property name and real targetType
			int valueIndex = (targetType >> 28) & 0xF;
			int propertyName = (targetType >> 4) & 0xFFFFF;
			targetType = targetType & 0xF;
			
			ShaderLab::FastPropertyName name;
			name.index = propertyName;
			if (targetType == kBindFloatToMaterial)
				material->SetFloat(name, value);
			else if (targetType == kBindFloatToMaterialScaleAndOffset)
				material->SetTextureScaleAndOffsetIndexed(name, valueIndex, value);
			else if (targetType == kBindFloatToColorMaterial)
				material->SetColorIndexed(name, valueIndex, value);
			else
			{
				AssertString("Unsupported bind mode!");
				return false;
			}
			
			return true;
		}
		return false;
	}
}

void AnimationBinder::SetValueAwakeGeneric (const BoundCurveDeprecated& bind)
{
	if (ShouldAwakeGeneric(bind))
	{
		bind.targetObject->AwakeFromLoad(kDefaultAwakeFromLoad);
		bind.targetObject->SetDirty();
	}
}

int AnimationBinder::InsertCurveIDIntoLookup (CurveIDLookup& curveIDLookup, const CurveID& curveID)
{
	return curveIDLookup.insert(std::make_pair(curveID, curveIDLookup.size())).first->second;
}

void CurveID::CalculateHash ()
{
	hash_cstring h;
	hash = max(h (path) ^ ClassID(Transform) ^ h (attribute), (unsigned)2);
}



