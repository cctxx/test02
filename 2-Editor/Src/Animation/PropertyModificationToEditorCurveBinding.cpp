#include "UnityPrefix.h"
#include "PropertyModificationToEditorCurveBinding.h"
#include "Editor/Src/Prefabs/PrefabUtility.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Animation/GenericAnimationBindingCache.h"
#include "Runtime/Interfaces/IAnimationBinding.h"

MonoScript* GetScriptFromObject (Object& object)
{
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (&object);
	if (behaviour != NULL)
		return behaviour->GetScript();
	else
		return NULL;
}

ClassIDType PropertyModificationToEditorCurveBinding (const PropertyModification& modification, GameObject& rootGameObject, EditorCurveBinding& outBinding)
{
	Object* target = modification.target;
	Transform* targetTransform = GetTransformFromComponentOrGameObject (target);
	Transform& root = rootGameObject.GetComponent (Transform);
	
	if (targetTransform == NULL || !IsChildOrSameTransform(*targetTransform, root.GetComponent(Transform)))
		return ClassID(Undefined);
	
	std::string relativePath = CalculateTransformPath (*targetTransform, &root);

	outBinding = EditorCurveBinding (relativePath, target->GetClassID(), GetScriptFromObject (*target), modification.propertyPath, true);
	
	ClassIDType type;

	// Try PPtr curve
	type = UnityEngine::Animation::GetEditorCurveValueClassID (rootGameObject, outBinding);
	if (type != ClassID(Undefined))
		return type;
	
	// Try float curve
	outBinding.isPPtrCurve = false;
	type = UnityEngine::Animation::GetEditorCurveValueClassID (rootGameObject, outBinding);
	if (type != ClassID(Undefined))
		return type;

	// Try custom curve
	outBinding.attribute = UnityEngine::Animation::GetGenericAnimationBindingCache().SerializedPropertyPathToCurveAttribute(*target, modification.propertyPath.c_str());
	type = UnityEngine::Animation::GetEditorCurveValueClassID (rootGameObject, outBinding);
	if (!outBinding.attribute.empty() && type != ClassID(Undefined))
		return type;
	 
	return ClassID(Undefined);
}

void EditorCurveBindingToPropertyModification (Unity::GameObject& rootGameObject, const EditorCurveBinding& binding, PropertyModification& modification)
{
	Object* targetObject = UnityEngine::Animation::FindAnimatedObject (rootGameObject, binding);
	if (targetObject == NULL)
		return;

	modification.target = targetObject;
	modification.propertyPath = binding.attribute;

	std::string translatedBinding = UnityEngine::Animation::GetGenericAnimationBindingCache().CurveAttributeToSerializedPath (rootGameObject, binding);
	if (!translatedBinding.empty())
		modification.propertyPath = translatedBinding;
}