#include "UnityPrefix.h"
#include "UndoBase.h"
#include "Undo.h"
#include "UndoManager.h"
#include "UndoTransformParent.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/BaseClasses/IsPlaying.h"

class UndoTransformParent : public UndoBase
{
	PPtr<Transform> m_Target;
	PPtr<Transform> m_Parent;
	
	Vector3f	m_LocalPosition;
	Quaternionf m_LocalRotation;
	Vector3f	m_LocalScale;
	
public:
	
	UndoTransformParent (Transform& target, string actionName)
	{
        SetIsSceneUndo(!target.IsPersistent());
		m_Target = &target;
		m_Parent = target.GetParent();
		m_LocalPosition = target.GetLocalPosition();
		m_LocalRotation = target.GetLocalRotation();
		m_LocalScale = target.GetLocalScale();
		
		m_Name = actionName;
	}
	
	~UndoTransformParent ()
	{
	}
	
	virtual bool Restore (bool registerRedo)
	{
		Transform* target = m_Target;
		if (target == NULL)
			return false;

		Transform* parent = m_Parent;
		if (parent == NULL && m_Parent.GetInstanceID() != 0)
			return false;
		
		if (registerRedo)
			SetTransformParentUndo (*target, parent, m_Name);
		else
			target->SetParent(parent);
		
		target->SetLocalPosition(m_LocalPosition);
		target->SetLocalRotation(m_LocalRotation);
		target->SetLocalScale(m_LocalScale);
		
		return true;
	}
};

///@TODO: Maybe this should instead perform the set parent too?

bool SetTransformParentUndo (Transform& transform, Transform* newParent, Transform::SetParentOption option, const std::string& actionName)
{
	if (!IsPrefabTransformParentChangeAllowed (transform, newParent))
		DestroyObjectUndoable (GetPrefabFromAnyObjectInPrefab(&transform));

	UndoTransformParent* undo = new UndoTransformParent (transform, actionName);
	GetUndoManager().RegisterUndo(undo);
	
	return transform.SetParent(newParent, option);
}

bool SetTransformParentUndo (Transform& transform, Transform* newParent, const std::string& actionName)
{
	return SetTransformParentUndo (transform, newParent, Transform::kWorldPositionStays, actionName);
}
