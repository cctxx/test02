#pragma once

#include "Runtime/Graphics/Transform.h"
#include "BaseHierarchyProperty.h"
#include <stack>

class GameObjectHierarchyProperty : public BaseHierarchyProperty
{
	Transform* m_Transform;
	
	struct SortedIteratorStack
	{
		int index;
		std::vector<Transform*> sorted;
		SortedIteratorStack () { index = 0; }
	};
	
	std::vector<SortedIteratorStack>  m_SortedIteratorStack;
	typedef Transform::VisibleRootMap::const_iterator RootIterator;
	Transform::VisibleRootKey m_RootKey;
	
	bool DoNext (bool enterChildren);
	int CountTransforms (Transform& transform, int* expanded, int expandedSize);
	inline void SyncRootIterator (RootIterator iterator)
	{
		m_RootKey = iterator->first;
		m_Transform = iterator->second;
	}
	
	public:
	
	GameObjectHierarchyProperty ();
	virtual void Reset ();

	virtual bool HasChildren ();

	virtual bool IsExpanded (int* expanded, int expandedSize);

	virtual bool Previous (int* expanded, int expandedSize);
	virtual bool Parent ();
	virtual std::vector<int> GetAncestors();
	virtual int GetColorCode ();
	
	virtual UnityGUID GetGUID () { AssertString("GUID not supported for game object hierarchy"); return UnityGUID(); }
	
	virtual int GetInstanceID () const;
	virtual int GetDepth () { return m_SortedIteratorStack.size(); }
	virtual bool IsValid () { return m_Transform != NULL; }
	virtual const AssetLabels* GetLabels () { return NULL; }
		
	virtual Texture* GetCachedIcon () { return NULL; }

	
	virtual const char* GetName ();

	virtual bool CanAccessObjectsWhenFiltering () { return true; }
	virtual HierarchyType GetHierarchyType () { return kHierarchyGameObjects; }
	
	virtual int GetClassID () const { return ClassID(GameObject); }
	virtual string GetScriptFullClassName () { AssertString("Script classes not supported"); return ""; }

	// Specialized Optimized CountRemaining function
	virtual int CountRemaining (int* expanded, int expandedSize);
	virtual bool Skip (int rows, int* expanded, int expandedSize);

#if ENABLE_EDITOR_HIERARCHY_ORDERING
	virtual bool DragTransformHierarchy (bool dropUpon);
#endif
};


void MarkSingleSceneObjectVisible(int instanceID, bool otherVisibility);
