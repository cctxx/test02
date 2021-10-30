#pragma once

#include "Runtime/Utilities/dynamic_array.h"
#include "EditorCurveBinding.h"
#include "Runtime/BaseClasses/ClassIDs.h"
#include "BoundCurve.h"

typedef UInt32 BindingHash;

namespace Unity { class GameObject;  }
class Transform;
class Object;
class MonoScript;
template<class T> class PPtr;

class IAnimationBinding;

namespace UnityEngine
{
namespace Animation
{
	
	struct CachedComponentBindings;
	struct GenericBinding;
	
	bool IsMuscleBinding (const GenericBinding& binding);

	inline bool AnimationFloatToBool (float result)
	{
		//@TODO: Maybe we should change the behaviour to be that > 0.01F means enabled instead of the close to zero logic....
		return result > 0.001F || result < -0.001F;
	}
	
	inline float AnimationBoolToFloat (bool value)
	{
		return value ? 1.0F : 0.0F;
	}

	class GenericAnimationBindingCache
	{
	public:
		
		GenericAnimationBindingCache ();
		~GenericAnimationBindingCache ();
		
		ClassIDType		BindGeneric (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound);
		ClassIDType		BindPPtrGeneric (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound);
		
		void			CreateGenericBinding (const UnityStr& path, int classID, PPtr<MonoScript> script, const UnityStr& attribute, bool pptrCurve, GenericBinding& outputBinding) const;
		
		static void		DidReloadDomain ();
		
		void			RegisterIAnimationBinding (ClassIDType classID, int inCustomType, IAnimationBinding* bindingInterface);

		// Editor API
		void			GetAnimatableProperties (Transform& transform, int classID, const PPtr<MonoScript>& script, std::vector<EditorCurveBinding>& outProperties);
		void			GetAllAnimatableProperties (Unity::GameObject& gameObject, Unity::GameObject& root, std::vector<EditorCurveBinding>& outProperties);

		std::string		SerializedPropertyPathToCurveAttribute (Object& target, const char* propertyPath) const;
		std::string		CurveAttributeToSerializedPath (Unity::GameObject& root, const EditorCurveBinding& binding) const;
		
	private:

		typedef dynamic_array<CachedComponentBindings*> CachedComponentBindingArray;

		ClassIDType BindGenericComponent (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound);
		ClassIDType BindScript (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound);
		ClassIDType BindCustom (const GenericBinding& inputBinding, Transform& transform, BoundCurve& bound) const;

		static void Clear (CachedComponentBindingArray& array);
		
		struct CustomBinding
		{
			int					classID;
			int					customBindingType;
		};
		
		dynamic_array<CustomBinding>		m_CustomBindings;
		dynamic_array<IAnimationBinding*>	m_CustomBindingInterfaces;
		
		CachedComponentBindingArray			m_Classes;
		CachedComponentBindingArray			m_Scripts;
		BindingHash							m_IsActiveHash;
	};
	
	GenericAnimationBindingCache& GetGenericAnimationBindingCache ();
	
	void InitializeGenericAnimationBindingCache ();
	void CleanupGenericAnimationBindingCache ();

	// Runtime API
	// NOt sure if i like this???
//	void	CreateGenericBinding (const UnityStr& path, int classID, PPtr<MonoScript> script, const UnityStr& attribute, bool pptrCurve, GenericBinding& outputBinding);
	void	CreateTransformBinding (const UnityStr& path, int bindType, GenericBinding& outputBinding);
	
	float   GetBoundCurveFloatValue        (const BoundCurve& bind);
	void    SetBoundCurveFloatValue        (const BoundCurve& bind, float value);
	
	void	SetBoundCurveIntValue (const BoundCurve& bind, int value);
	int		GetBoundCurveIntValue (const BoundCurve& bind);
	
	void    BoundCurveValueAwakeGeneric    (Object& targetObject);
	bool    ShouldAwakeGeneric             (const BoundCurve& bind);
	
#if UNITY_EDITOR
	// Editor API. 
	// Returns the ClassID of the bound value. (ClassID(Undefined) if it could not be bound)
	ClassIDType GetFloatValue (Unity::GameObject& root, const EditorCurveBinding& binding, float* value);
	ClassIDType GetPPtrValue  (Unity::GameObject& root, const EditorCurveBinding& binding, int* instanceID);

	bool BindEditorCurve  (Unity::GameObject& root, const EditorCurveBinding& binding, BoundCurve& boundCurve);
	
	Object* FindAnimatedObject (Unity::GameObject& root, const EditorCurveBinding& inputBinding);

	ClassIDType GetEditorCurveValueClassID (Unity::GameObject& root, const EditorCurveBinding& binding);
#endif
}
}
