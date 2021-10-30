#include "UnityPrefix.h"
#include "GenericAnimationBindingCache.h"
#include "AnimationClipBindings.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/mecanim/animation/clipmuscle.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Interfaces/IAnimationBinding.h"
#include "Animator.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"
#include "AnimatorController.h"
#include "Runtime/Core/Callbacks/GlobalCallbacks.h"

const static char* kIsActive = "m_IsActive";

typedef UInt32 BindingHash;

namespace UnityEngine
{
namespace Animation
{

bool IsMuscleBinding(const GenericBinding& binding)
{
	return binding.classID == ClassID(Animator) && binding.customType == kBindMuscle;
}

struct CachedBinding
{
	BindingHash   propertyHash;
	int           offset;
	int           bindType;
};
	
struct CachedComponentBindings
{
	ScriptingClassPtr            scriptingClass;
	int                          classID;
	size_t                       bindingSize;
	CachedBinding*               bindings;
};

	
static GenericAnimationBindingCache* gGenericBindingCache = NULL;
	
static CachedComponentBindings* GenerateComponentBinding (int classID, ScriptingObjectPtr scriptingInstance, ScriptingClassPtr scriptingClass, Object* targetObject);
static const CachedBinding* FindBinding (const CachedComponentBindings& componentBinding, BindingHash attribute);
static void DestroyBinding (CachedComponentBindings* binding);
	
bool operator < (const CachedBinding& lhs, const CachedBinding& rhs)
{
	return lhs.propertyHash < rhs.propertyHash;
}
	
	
static ClassIDType BindCurve (const CachedComponentBindings& cachedBinding, const GenericBinding& inputBinding, Object* targetObject, void* targetPtr, BoundCurve& bound)
{
	const CachedBinding* found = FindBinding (cachedBinding, inputBinding.attribute);
	if (found == NULL)
	{
		bound.targetType = kUnbound;
		return ClassID(Undefined);
	}
	
	bound.targetObject = targetObject;
	bound.targetPtr = reinterpret_cast<UInt8*> (targetPtr) + found->offset;
	bound.targetType = found->bindType;
	
	if (found->bindType == kBindFloatToBool)
		return ClassID(bool);
	else
		return ClassID(float);
}
		
static int GetTypeTreeBindType (const TypeTree& variable)
{
	if (variable.m_MetaFlag & kDontAnimate)
		return kUnbound;
	
	if (variable.m_Type == "float")
		return kBindFloat;
	else if (variable.m_Type == "bool" || (variable.m_Type == "UInt8" && (variable.m_MetaFlag & kEditorDisplaysCheckBoxMask)))
		return kBindFloatToBool;
	else
		return kUnbound;
}

static int GetAnimatablePropertyOffset (const TypeTree* variable, ScriptingObjectPtr scriptingInstance)
{
	if (variable && scriptingInstance)
	{
#if ENABLE_MONO
		if (scriptingInstance)
		{
			UInt32 offset = reinterpret_cast<UInt8*> (variable->m_DirectPtr) - reinterpret_cast<UInt8*> (scriptingInstance);
			UInt32 size = mono_class_instance_size(mono_object_get_class(scriptingInstance));
			if (offset < size)
				return offset;
		}
#endif
		return -1;
	}
	else if (variable)
		return variable->m_ByteOffset;
	else
		return -1;
}
		
static void GetGenericAnimatablePropertiesRecurse (const TypeTree& typeTree, std::string& path, ScriptingObjectPtr scriptingInstance, const EditorCurveBinding& baseBinding, std::vector<EditorCurveBinding>& outProperties)
{
	size_t previousSize = path.size();
	if (!path.empty())
		path += '.';
	path += typeTree.m_Name;
		
	int offset = GetAnimatablePropertyOffset (&typeTree, scriptingInstance);
	if (offset != -1)
	{
		int bindType = GetTypeTreeBindType (typeTree);
		if (bindType != kUnbound)
		{
			outProperties.push_back(baseBinding);
			outProperties.back().attribute = path;
		}
	}

	for (TypeTree::const_iterator i=typeTree.begin();i != typeTree.end();++i)
	{
		GetGenericAnimatablePropertiesRecurse (*i, path, scriptingInstance, baseBinding, outProperties);
	}
	
	path.resize(previousSize);
}
	
static void GetGenericAnimatableProperties (int classID, Object& targetObject, std::vector<EditorCurveBinding>& outProperties)
{
	//@TODO: Use temp TypeTree mem?
	TypeTree typeTree;
	GenerateTypeTree (targetObject, &typeTree);
	
	EditorCurveBinding baseBinding;
	baseBinding.classID = classID;

	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&targetObject);
	ScriptingObjectPtr scriptingInstance = SCRIPTING_NULL;;
	if (behaviour)
	{
		scriptingInstance = behaviour->GetInstance();
		baseBinding.script = behaviour->GetScript();
	}
	
	// Create cached bindings (We don't want to include the root name, so iterate over the children directly)
	std::string path;
	for (TypeTree::const_iterator i=typeTree.begin();i != typeTree.end();++i)
		GetGenericAnimatablePropertiesRecurse (*i, path, scriptingInstance, baseBinding, outProperties);
}
	
static void GenerateBindingRecurse (const TypeTree& typeTree, ScriptingObjectPtr scriptingInstance, mecanim::crc32 attributeHash, dynamic_array<CachedBinding>& bindings)
{
	// Update hash recursively
	if (attributeHash.checksum() != 0)
		attributeHash.process_bytes(".", 1);
	attributeHash.process_bytes(typeTree.m_Name.c_str(), strlen(typeTree.m_Name.c_str()));
	
	int offset = GetAnimatablePropertyOffset (&typeTree, scriptingInstance);
	if (offset != -1)
	{
		int bindType = GetTypeTreeBindType (typeTree);
		if (bindType != kUnbound)
		{
			CachedBinding& binding = bindings.push_back();
			binding.propertyHash = attributeHash.checksum();
			binding.offset = offset;
			binding.bindType = bindType;
		}
	}
	
	for (TypeTree::const_iterator i=typeTree.begin();i != typeTree.end();++i)
		GenerateBindingRecurse (*i, scriptingInstance, attributeHash, bindings);
}


template<class T>
bool has_duplicate_sorted (const T* begin, size_t count)
{
	if (count == 0)
		return false;
	
	const T* previous = begin;
	const T* i = begin;
	const T* end = begin + count;
	i++;
	for (; i != end;++i)
	{
		if (!(*previous < *i))
			return true;
		
		previous = i;
	}
	
	return false;
}

// Find a value in a sorted array
// Returns NULL if there is no value in the array
template<class T>
const T* find_binary_search (const T* begin, size_t count, const T& value)
{
	const T* found = std::lower_bound (begin, begin + count, value);
	if (found == begin + count || value < *found)
		return NULL;
	else
		return found;
}		


static const CachedBinding* FindBinding (const CachedComponentBindings& componentBinding, BindingHash attribute)
{
	CachedBinding proxy;
	proxy.propertyHash = attribute;
	
	return find_binary_search(componentBinding.bindings, componentBinding.bindingSize, proxy);
}

static void DestroyBinding (CachedComponentBindings* binding)
{
	UNITY_FREE(kMemAnimation, binding);
}


static CachedComponentBindings* GenerateComponentBinding (int classID, ScriptingObjectPtr scriptingInstance, ScriptingClassPtr scriptingClass, Object* targetObject)
{
	//@TODO: Use temp TypeTree mem?
	TypeTree typeTree;
	GenerateTypeTree (*targetObject, &typeTree);
	
	dynamic_array<CachedBinding> bindings (kMemTempAlloc);
	
	// Create cached bindings (We don't want to include the root name, so iterate over the children directly)
	for (TypeTree::const_iterator i=typeTree.begin();i != typeTree.end();++i)
		GenerateBindingRecurse (*i, scriptingInstance, mecanim::crc32(), bindings);
	
	std::sort (bindings.begin(), bindings.end());
	
#if DEBUGMODE
	if (has_duplicate_sorted (bindings.begin(), bindings.size()))
	{
		///@TODO: make a nice fullclassname...
		
		
		WarningString(Format("Animation bindings for %s are not unique. Some properties might get bound incorrectly.", targetObject->GetClassName().c_str()));
	}	
#endif
	
	size_t size = sizeof (CachedComponentBindings) + sizeof (CachedBinding) * bindings.size();
	mecanim::memory::InPlaceAllocator allocator (UNITY_MALLOC(kMemAnimation, size), size);
	
	CachedComponentBindings* binding = allocator.Construct<CachedComponentBindings> ();
	
	binding->classID = classID;
	binding->scriptingClass = scriptingClass;
	binding->bindingSize = bindings.size();
	binding->bindings = allocator.ConstructArray<CachedBinding> (bindings.begin(), bindings.size());
	
	return binding;
}

void GenericAnimationBindingCache::DidReloadDomain ()
{
	if (gGenericBindingCache != NULL)
		Clear(gGenericBindingCache->m_Scripts);
}
	
void InitializeGenericAnimationBindingCache ()
{
	mecanim::crc32::crc32_table_type::init_table();

	gGenericBindingCache = UNITY_NEW_AS_ROOT(GenericAnimationBindingCache, kMemAnimation, "AnimationBindingCache", "");

	GlobalCallbacks::Get().didReloadMonoDomain.Register(GenericAnimationBindingCache::DidReloadDomain);
}

void CleanupGenericAnimationBindingCache ()
{
	UNITY_DELETE(gGenericBindingCache, kMemAnimation);
	
	GlobalCallbacks::Get().didReloadMonoDomain.Unregister(GenericAnimationBindingCache::DidReloadDomain);
}
	
static RegisterRuntimeInitializeAndCleanup s_RegisterBindingCache (InitializeGenericAnimationBindingCache, CleanupGenericAnimationBindingCache);
	
GenericAnimationBindingCache& GetGenericAnimationBindingCache ()
{
	return *gGenericBindingCache;
}

GenericAnimationBindingCache::GenericAnimationBindingCache ()
{
	m_IsActiveHash = mecanim::processCRC32 (kIsActive);
	m_Classes.resize_initialized (kLargestRuntimeClassID, NULL);
	m_CustomBindingInterfaces.resize_initialized (kAllBindingCount, NULL);
}

GenericAnimationBindingCache::~GenericAnimationBindingCache ()
{
	Clear(m_Classes);
	Clear(m_Scripts);
}	


void CreateTransformBinding (const UnityStr& path, int bindType, GenericBinding& outputBinding)
{
	outputBinding.path = mecanim::processCRC32(path.c_str());
	outputBinding.attribute = bindType;
	outputBinding.classID = ClassID(Transform);
	outputBinding.customType = kUnbound;
	outputBinding.isPPtrCurve = false;
	outputBinding.script = NULL;
}

void GenericAnimationBindingCache::CreateGenericBinding (const UnityStr& path, int classID, PPtr<MonoScript> script, const UnityStr& attribute, bool pptrCurve, GenericBinding& outputBinding) const
{
	outputBinding.path = mecanim::processCRC32(path.c_str());
	outputBinding.attribute = mecanim::processCRC32(attribute.c_str());
	outputBinding.classID = classID;
	outputBinding.customType = kUnbound;
	outputBinding.isPPtrCurve = pptrCurve;
	outputBinding.script = script;
	
	// Pre-bind muscle indices
	//@TODO: is this really worth doing? Maybe we should just make it more consistent?
	if (!pptrCurve)
	{
		if (classID == ClassID(Animator))
		{
			mecanim::int32_t muscleIndex = mecanim::animation::FindMuscleIndex(outputBinding.attribute);
			if (muscleIndex != -1)
			{
				outputBinding.attribute = muscleIndex;
				outputBinding.customType = kBindMuscle;
				return;
			}
		}
	}
	
	// Search custom bindings
	for (int i=0;i<m_CustomBindings.size();i++)
	{
		int customBindingType = m_CustomBindings[i].customBindingType;
		const IAnimationBinding* bindingInterface = m_CustomBindingInterfaces[customBindingType];
		if (Object::IsDerivedFromClassID (classID, m_CustomBindings[i].classID) && bindingInterface->GenerateBinding (attribute, pptrCurve, outputBinding))
		{
			outputBinding.customType = customBindingType;
			return;
		}
	}		
}

static inline MonoBehaviour* GetComponentWithScript (Transform& transform, PPtr<Object> target)
{
	MonoScript* script = dynamic_instanceID_cast<MonoScript*> (target.GetInstanceID());
	return static_cast<MonoBehaviour*> (GetComponentWithScript (transform.GetGameObject(), ClassID(MonoBehaviour), script));
}

#if ENABLE_MONO
	
ClassIDType GenericAnimationBindingCache::BindScript (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound)
{
	MonoBehaviour* behaviour = GetComponentWithScript (transform, inputBinding.script);

	ScriptingObjectPtr instance = behaviour ? behaviour->GetInstance() : SCRIPTING_NULL;
	// No valid instance -> no bound curve
	if (instance == NULL)
	{
		bound.targetType = kUnbound;
		return ClassID(Undefined);
	}
	
	ScriptingClassPtr klass = behaviour->GetClass();

	// Find cached binding stored by ScriptingClassPtr
	CachedComponentBindings* bindings = NULL;
	for (int i=0;i<m_Scripts.size();i++)
	{
		if (m_Scripts[i]->scriptingClass == klass)
		{
			bindings = m_Scripts[i];
			break;
		}
	}
	
	// Create a new binding for this ScriptingClassPtr
	if (bindings == NULL)
	{
		bindings = GenerateComponentBinding(inputBinding.classID, instance, klass, behaviour);
		m_Scripts.push_back(bindings);
	}
	
	// Bind the specific curve
	return BindCurve (*bindings, inputBinding, behaviour, instance, bound);
}
#endif
	
ClassIDType GenericAnimationBindingCache::BindGenericComponent (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound)
{
	Unity::Component* target = transform.GetGameObject().QueryComponentT<Unity::Component>(inputBinding.classID);
	if (target == NULL)
		return ClassID(Undefined);
	
	if (m_Classes[inputBinding.classID] == NULL)
		m_Classes[inputBinding.classID] = GenerateComponentBinding(inputBinding.classID, SCRIPTING_NULL, SCRIPTING_NULL, target);
	
	return BindCurve (*m_Classes[inputBinding.classID], inputBinding, target, target, bound);
}

Object* FindAnimatedObject (Unity::GameObject& root, const EditorCurveBinding& inputBinding)
{
	Transform* transform = FindRelativeTransformWithPath (root.GetComponent(Transform), inputBinding.path.c_str());
	if (transform == NULL)
		return NULL;
	
	if (inputBinding.classID == ClassID(GameObject))
	{
		return &transform->GetGameObject();
	}
	else if (inputBinding.classID == ClassID(MonoBehaviour))
	{
		return GetComponentWithScript (*transform, inputBinding.script);
	}
	else
	{
		return transform->GetGameObject().QueryComponentT<Unity::Component>(inputBinding.classID);
	}
}	
	
ClassIDType GenericAnimationBindingCache::BindPPtrGeneric (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound)
{
	if (inputBinding.customType == kUnbound)
		return ClassID(Undefined);

	return BindCustom (inputBinding, transform, bound);
}

ClassIDType GenericAnimationBindingCache::BindCustom (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound) const
{
	Assert(inputBinding.customType != kUnbound);
	
	Unity::Component* target;
	if (inputBinding.classID == ClassID(MonoBehaviour))
		target = GetComponentWithScript (transform, inputBinding.script);
	else
		target = transform.GetGameObject().QueryComponentT<Unity::Component>(inputBinding.classID);
	
	IAnimationBinding* customBinding = m_CustomBindingInterfaces[inputBinding.customType];
	if (customBinding != NULL && target != NULL)
	{
		BoundCurve tempBound;
		tempBound.targetType = inputBinding.customType;
		tempBound.customBinding = customBinding;
		tempBound.targetObject = target;
		
		ClassIDType type = customBinding->BindValue (*target, inputBinding, tempBound);
		if (type != ClassID (Undefined))
			bound = tempBound;
		
		return type;
	}
	
	return ClassID(Undefined);
}

	
ClassIDType GenericAnimationBindingCache::BindGeneric (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound)
{
	// Bind game object active state
	if (inputBinding.classID == ClassID(GameObject))
	{
		if (inputBinding.attribute == m_IsActiveHash && inputBinding.path != 0)
		{
			bound.targetPtr = NULL;
			bound.targetType = kBindGameObjectActive;
			bound.targetObject = transform.GetGameObjectPtr();
			return ClassID(bool);
		}

		return ClassID(Undefined);
	}
	// Custom animator bindings
	else if (inputBinding.classID == ClassID(Animator))
	{
		// We bind these in mecanim internally.
		return ClassID(float);
	}
	// Custom binding
	else if (inputBinding.customType != kUnbound)
		return BindCustom (inputBinding, transform, bound);
	// Script bindings
	else if (inputBinding.classID == ClassID(MonoBehaviour))
	{
#if ENABLE_MONO
		return BindScript(inputBinding, transform, bound);
#else
        return ClassID(Undefined);
#endif
	}
	// Generic bindings
	else
	{
		return BindGenericComponent(inputBinding, transform, bound);
	}
}

void GenericAnimationBindingCache::RegisterIAnimationBinding (ClassIDType classID, int customBindingType, IAnimationBinding* customBinding)
{
	CustomBinding bind;
	bind.classID = classID;
	bind.customBindingType = customBindingType;
	m_CustomBindings.push_back(bind);

	Assert(m_CustomBindingInterfaces[customBindingType] == NULL);
	m_CustomBindingInterfaces[customBindingType] = customBinding;
}

std::string GenericAnimationBindingCache::SerializedPropertyPathToCurveAttribute (Object& target, const char* propertyPath) const
{
	ClassIDType classID = target.GetClassID();
	// Search custom bindings
	for (int i=0;i<m_CustomBindings.size();i++)
	{
		if (!Object::IsDerivedFromClassID (classID, m_CustomBindings[i].classID))
			continue;
		
		int customBindingType = m_CustomBindings[i].customBindingType;
		const IAnimationBinding* customBinding = m_CustomBindingInterfaces[customBindingType];
		
		string attributeName = customBinding->SerializedPropertyPathToCurveAttribute (target, propertyPath);
		if (!attributeName.empty())
			return attributeName;
	}		
	
	return string();
}

std::string GenericAnimationBindingCache::CurveAttributeToSerializedPath (Unity::GameObject& root, const EditorCurveBinding& binding) const
{
	Transform* transform = FindRelativeTransformWithPath (root.GetComponent(Transform), binding.path.c_str());
	if (transform == NULL)
		return std::string();

	GenericBinding genericBinding;
	CreateGenericBinding (binding.path, binding.classID, binding.script, binding.attribute, binding.isPPtrCurve, genericBinding);
	
	if (genericBinding.customType == kUnbound)
		return std::string();
	
	BoundCurve bound;
	if (BindCustom (genericBinding, *transform, bound) == ClassID(Undefined))
		return std::string();

	if (bound.customBinding != NULL)
		return bound.customBinding->CurveAttributeToSerializedPath (bound);
	else
		return string();
}

void GenericAnimationBindingCache::Clear (CachedComponentBindingArray& array)
{
	for (int i=0;i<array.size();i++)
		UNITY_FREE(kMemAnimation, array[i]);
	array.clear();
}
	
int GetBoundCurveIntValue (const BoundCurve& bind)
{
	return bind.customBinding->GetPPtrValue (bind);
}

void SetBoundCurveIntValue (const BoundCurve& bind, int value)
{
	bind.customBinding->SetPPtrValue (bind, value);
}
	
void SetBoundCurveFloatValue (const BoundCurve& bind, float value)
{
	UInt32 targetType = bind.targetType;
	Assert(bind.targetType != kUnbound && bind.targetType != kBindTransformRotation && bind.targetType != kBindTransformPosition && bind.targetType != kBindTransformScale);
	
	if ( targetType == kBindFloat )
	{
		*reinterpret_cast<float*>(bind.targetPtr) = value;
	}
	else if ( targetType == kBindFloatToBool )
	{
		*reinterpret_cast<UInt8*>(bind.targetPtr) = AnimationFloatToBool(value);
	}
	else if ( targetType == kBindGameObjectActive)
	{
		GameObject* go = static_cast<GameObject*> (bind.targetObject);
		go->SetSelfActive (AnimationFloatToBool(value));
	}
	else
		bind.customBinding->SetFloatValue (bind, value);
}

float GetBoundCurveFloatValue (const BoundCurve& bind)
{
	UInt32 targetType = bind.targetType;
	Assert(bind.targetType >= kMinSinglePropertyBinding);
	
	if ( targetType == kBindFloat )
	{
		return *reinterpret_cast<float*>(bind.targetPtr);
	}
	else if ( targetType == kBindFloatToBool )
	{
		return AnimationBoolToFloat(*reinterpret_cast<UInt8*>(bind.targetPtr));
	}
	else if ( targetType == kBindGameObjectActive )
	{
		GameObject* go = static_cast<GameObject*> (bind.targetObject);
		return AnimationBoolToFloat (go->IsSelfActive ());
	}
	else
		return bind.customBinding->GetFloatValue (bind);
}
	
	
bool ShouldAwakeGeneric (const BoundCurve& bind)
{
	return bind.targetType == kBindFloat || bind.targetType == kBindFloatToBool;
}

void BoundCurveValueAwakeGeneric (Object& targetObject)
{
	targetObject.AwakeFromLoad(kDefaultAwakeFromLoad);
	targetObject.SetDirty();
}

#if UNITY_EDITOR
	
static void ExtractGameObjectIsActiveBindings (std::vector<EditorCurveBinding>& outProperties)
{
	AddBinding (outProperties, ClassID(GameObject), kIsActive);
}
	
static void ExtractAllAnimatorBindings (Unity::Component& targetObject, vector<EditorCurveBinding>& attributes)
{
	Animator& animator = static_cast<Animator&> (targetObject);
	if (animator.IsHuman())
	{
		for(int curveIter = 0; curveIter < mecanim::animation::s_ClipMuscleCurveCount; curveIter++)
			AddBinding (attributes, ClassID(Animator), mecanim::animation::GetMuscleCurveName(curveIter).c_str());
	}
	
	if(animator.GetAnimatorController())
	{
		int eventCount = animator.GetAnimatorController()->GetParameterCount();
		
		for(int eventIter = 0; eventIter < eventCount; eventIter++)
		{
			AnimatorControllerParameter* parameter = animator.GetAnimatorController()->GetParameter(eventIter);
			if (parameter->GetType() == 1)
				AddBinding (attributes, ClassID(Animator), parameter->GetName());
		}
	}
}

static void ProcessRelativePath (Unity::GameObject& gameObject, Unity::GameObject& root, std::vector<EditorCurveBinding>& outProperties)
{
	string path = CalculateTransformPath (gameObject.GetComponent(Transform), root.QueryComponent(Transform));
	for (int i=0;i<outProperties.size();i++)
		outProperties[i].path = path;
}
	
void GenericAnimationBindingCache::GetAllAnimatableProperties (Unity::GameObject& go, Unity::GameObject& root, std::vector<EditorCurveBinding>& outProperties)
{
	Assert(outProperties.empty());
	
	if (&go != &root)
		ExtractGameObjectIsActiveBindings(outProperties);
	
	for (int i=0;i<go.GetComponentCount ();i++)
	{
		Unity::Component& com = go.GetComponentAtIndex (i);
		int classID = com.GetClassID();
		
		// Search custom bindings
		for (int c=0;c<m_CustomBindings.size();c++)
		{
			int customBindingType = m_CustomBindings[c].customBindingType;
			if (Object::IsDerivedFromClassID (classID, m_CustomBindings[c].classID))
				m_CustomBindingInterfaces[customBindingType]->GetAllAnimatableProperties (com, outProperties);
		}		
		
		if (classID == ClassID(Animator))
			ExtractAllAnimatorBindings (com, outProperties);
		
		GetGenericAnimatableProperties (classID, go.GetComponentAtIndex(i), outProperties);
	}
	
	ProcessRelativePath (go, root, outProperties);
}

static bool GetFloatValueAnimatorBinding (Transform& transform, const EditorCurveBinding& binding, float* value)
{
	Animator* animator = transform.QueryComponent(Animator);
	
	if (animator == NULL)
		return false;
	
	BindingHash hash = mecanim::processCRC32 (binding.attribute.c_str());

	if(animator->GetMuscleValue(hash,value))
		return true;
	
	GetSetValueResult result = animator->GetFloat (hash, *value);
	return result == kGetSetSuccess;
}
	
static bool GetFloatValueTransformBinding (Transform& transform, const EditorCurveBinding& binding, float* value)
{
	int axis = -1;
	char lastCharacter = binding.attribute[binding.attribute.size()-1];
	if (lastCharacter == 'w')
		axis = 3;
	else if (lastCharacter >= 'x' && lastCharacter <= 'z')
		axis = lastCharacter - 'x';
	else
		return false;
		
	if (BeginsWith(binding.attribute, "m_LocalPosition") && axis < 3)
	{
		*value = transform.GetLocalPosition()[axis];
		return true;
	}
	else if (BeginsWith(binding.attribute, "m_LocalScale") && axis < 3)
	{
		*value = transform.GetLocalScale()[axis];
		return true;
	}
	else if (BeginsWith(binding.attribute, "m_LocalRotation") && axis < 4)
	{
		*value = transform.GetLocalRotation()[axis];
		return true;
	}
	else if (BeginsWith(binding.attribute, "localEulerAngles") && axis < 3)
	{
		*value = transform.GetLocalEulerAngles()[axis];
		return true;
	}
	else
		return false;
}

ClassIDType GetFloatValue (Unity::GameObject& root, const EditorCurveBinding& binding, float* value)
{
	*value = 0.0F;
	
	if (binding.isPPtrCurve)
		return ClassID(Undefined);
	
	Transform* transform = FindRelativeTransformWithPath (root.GetComponent(Transform), binding.path.c_str());
	if (transform == NULL)
		return ClassID(Undefined);
	
	// Transform has a special codepath for setting T/R/S as Vector3
	if (binding.classID == ClassID(Transform))
	{
		if (GetFloatValueTransformBinding (*transform, binding, value))
			return ClassID(float);
	}
	// Animator bindings are handled through a special more integrated code path in mecanim
	else if (binding.classID == ClassID(Animator))
	{
		if (GetFloatValueAnimatorBinding (*transform, binding, value))
			return ClassID(float);
	}
	else
	{
		GenericBinding genericBinding;
		GetGenericAnimationBindingCache().CreateGenericBinding (binding.path, binding.classID, binding.script, binding.attribute, binding.isPPtrCurve, genericBinding);
		
		BoundCurve bound;
		ClassIDType bindType = GetGenericAnimationBindingCache().BindGeneric (genericBinding, *transform, bound);
		if (bound.targetType == kUnbound)
			return ClassID(Undefined);
		
		*value = GetBoundCurveFloatValue (bound);
		
		return bindType;
	}
	
	return ClassID(Undefined);
}

ClassIDType GetPPtrValue (Unity::GameObject& root, const EditorCurveBinding& binding, int* instanceID)
{
	*instanceID = 0;
	
	if (!binding.isPPtrCurve)
		return ClassID(Undefined);

	Transform* transform = FindRelativeTransformWithPath (root.GetComponent(Transform), binding.path.c_str());
	if (transform == NULL)
		return ClassID(Undefined);
	
	GenericBinding genericBinding;
	GetGenericAnimationBindingCache().CreateGenericBinding (binding.path, binding.classID, binding.script, binding.attribute, binding.isPPtrCurve, genericBinding);
	
	BoundCurve bound;
	ClassIDType boundClassID = GetGenericAnimationBindingCache().BindPPtrGeneric (genericBinding, *transform, bound);
	if (bound.targetType == kUnbound)
		return ClassID(Undefined);
	
	*instanceID = GetBoundCurveIntValue (bound);
	return boundClassID;
}
	
ClassIDType GetEditorCurveValueClassID (Unity::GameObject& root, const EditorCurveBinding& binding)
{
	// Try if it's a PPtr curve
	int tempInstanceID;
	ClassIDType boundClassID = GetPPtrValue (root, binding, &tempInstanceID);
	if (boundClassID != ClassID(Undefined))
		return boundClassID;

	// Otherwise find the type of float curve it is
	float tempFloat;
	return GetFloatValue (root, binding, &tempFloat);
}
	
#endif
	
}
}
