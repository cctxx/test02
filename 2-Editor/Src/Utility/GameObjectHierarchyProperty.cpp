#include "UnityPrefix.h"
#include "GameObjectHierarchyProperty.h"
#include "Editor/Src/SceneInspector.h"
#include "Editor/Src/PrefKeys.h"
#include "Editor/Src/EditorExtensionImpl.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Utility/TransformUtility.h"
#include "Editor/Platform/Interface/DragAndDrop.h"

static PREFCOLOR (kGameObjectPrefab, 0x1F3C6CFF);
static PREFCOLOR (kGameObjectLostPrefab, 0x880000FF);
static PREFCOLOR (kGameObjectDisabled, 0xA0A0A0FF);
static PREFCOLOR (kGameObjectNormal, 0x000000FF);


using namespace std;

GameObjectHierarchyProperty::GameObjectHierarchyProperty ()
{
	m_SortedIteratorStack.reserve(10);
	Reset ();
}

void GameObjectHierarchyProperty::Reset ()
{
	m_SortedIteratorStack.resize(0);
	m_Transform = NULL;
	m_Row = -1;
}

bool GameObjectHierarchyProperty::HasChildren ()
{
	if (m_Transform == NULL)
		return false;

	int childCount = m_Transform->GetChildrenCount ();
	for (int i=0;i<childCount;i++)
	{
		if (!m_Transform->GetChild(i).TestHideFlag(Object::kHideInHierarchy))
			return true;
	}
	return false;
}
//PROFILER_INFORMATION(gProf, "sort", kProfilerScripts);

bool CompareTransformName (Transform* lhs, Transform* rhs)
{
	const char* lhsName = lhs ? lhs->GetName() : "";
	const char* rhsName = rhs ? rhs->GetName() : "";
	return SemiNumericCompare(lhsName, rhsName) < 0;
}

int GameObjectHierarchyProperty::CountTransforms (Transform& transform, int* expanded, int expandedSize)
{
	if (transform.TestHideFlag(Object::kHideInHierarchy))
		return 0;
	
	if (transform.GetChildrenCount() == 0)
		return 1;
	
	int instanceID = transform.GetGameObjectInstanceID();
	if (expanded != NULL && !IsInExpandedArray(instanceID, expanded, expandedSize))
		return 1;
	
	int counter = 1;
	int size = transform.GetChildrenCount();
	for (int i=0;i<size;i++)
	{
		Transform& curTransform = transform.GetChild(i);
		counter += CountTransforms(curTransform, expanded, expandedSize);
	}
		
	return counter;
}


bool GameObjectHierarchyProperty::Skip (int rows, int* expanded, int expandedSize)
{
	// Fast path
	if (!m_HasAnyFilters && m_Row == -1)
	{
		const Transform::VisibleRootMap&  rootMap = GetSceneTracker().GetVisibleRootTransforms();
		RootIterator rootIterator = rootMap.begin ();

		if (rows > 0 && !rootMap.empty())
		{
			// Always enter the first element
			SyncRootIterator (rootIterator);
			rows--;
			m_Row++;
			
			// Skip entire root game object hierarchies without having to sort them
			for (RootIterator i=rootMap.begin();i!=rootMap.end();i++)
			{
				Transform* transform = i->second;
				
				if (transform)
				{
					int childCount = CountTransforms(*transform, expanded, expandedSize);
					if (childCount <= rows)
					{
						rows -= childCount;
						m_Row += childCount;
						rootIterator = i;
						SyncRootIterator (rootIterator);
						rootIterator++;
						if (rootIterator == rootMap.end())
							return false;
						else
							SyncRootIterator (rootIterator);
					}
					else
					{
						break;
					}
				}
			}
			
		}
	}
	
	return BaseHierarchyProperty::Skip(rows, expanded, expandedSize);
}


int GameObjectHierarchyProperty::CountRemaining (int* expanded, int expandedSize)
{	
	if (!m_HasAnyFilters && m_Row == -1)
	{
		// Fast path simply iterate children
		const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();
		
		int counter = 0;
		for (RootIterator i=rootMap.begin();i!=rootMap.end();i++)
		{
			Transform* transform = i->second;
			if (transform)
				counter += CountTransforms(*transform, expanded, expandedSize);
		}
		return counter;
	}
	else
	{
		return BaseHierarchyProperty::CountRemaining (expanded, expandedSize);
	}
}

bool GameObjectHierarchyProperty::DoNext (bool enterChildren)
{
	Transform* parent;
	
	const Transform::VisibleRootMap& rootMap = GetSceneTracker().GetVisibleRootTransforms();

	RootIterator rootIterator = rootMap.find (m_RootKey);

	// Enter child transform
	if (m_Transform != NULL)
	{
		if (enterChildren)
		{
			m_SortedIteratorStack.push_back(SortedIteratorStack());
			
			vector<Transform*>& sorted = m_SortedIteratorStack.back().sorted;
			int size = m_Transform->GetChildrenCount();
			sorted.reserve(size);
			for (int i=0;i<size;i++)
			{
				Transform& curChild = m_Transform->GetChild(i);
				if (!curChild.TestHideFlag(Object::kHideInHierarchy))
					sorted.push_back(&curChild);
			}
			
#if ENABLE_EDITOR_HIERARCHY_ORDERING
			sort(sorted.begin(), sorted.end(), Transform::CompareDepths);
#else
			sort(sorted.begin(), sorted.end(), CompareTransformName);
#endif
			
			m_Transform = sorted[0];
					
			return true;
		}
	}
	// Enter into first root transform
	else
	{
		rootIterator = rootMap.begin();
		if (rootIterator != rootMap.end())
		{
			SyncRootIterator (rootIterator);
			return true;
		}
		else
		{
			return false;
		}
	}
	
	// Handle root level - next
	if (m_SortedIteratorStack.empty())
	{
		rootIterator++;
		if (rootIterator != rootMap.end())
		{
			SyncRootIterator (rootIterator);
			return true;
		}
		else
		{
			m_Transform = NULL;
			return false;
		}
	}
	// Handle transform child - next
	else
	{
		int& index = m_SortedIteratorStack.back().index;
		index++;
		
		
		// Select next if there is a next
		parent = m_Transform->GetParent();
		
		vector<Transform*>& sorted = m_SortedIteratorStack.back().sorted;
		if (index < sorted.size())
		{
			m_Transform = sorted[index];
			return true;
		}
		else
		{
			// Step out until there is a transform that has children we haven't traversed.
			while (true)
			{
				m_SortedIteratorStack.pop_back();

				m_Transform = m_Transform->GetParent();
				AssertIf(m_Transform == NULL);
				
				// Pop out into the next root level transform
				if (m_SortedIteratorStack.empty())
				{
					rootIterator++;
					if (rootIterator != rootMap.end())
					{
						SyncRootIterator (rootIterator);
						return true;
					}
					else
					{
						m_Transform = NULL;
						return false;
					}
				}
				// Pop out into the next 
				else
				{
					SortedIteratorStack& it = m_SortedIteratorStack.back();
					it.index++;

					if(it.index < it.sorted.size())
					{
						m_Transform = it.sorted[it.index];
						return true;
					}
				}
			}
		}
		return false;
	}
}

const char* GameObjectHierarchyProperty::GetName ()
{
	if (m_Transform)
		return m_Transform->GetName();
	else
		return "";
}

int GameObjectHierarchyProperty::GetInstanceID () const
{
	if (m_Transform)
		return m_Transform->GetGameObjectInstanceID();
	else
		return 0;
}

int GameObjectHierarchyProperty::GetColorCode ()
{
	if (m_Transform)
	{
		GameObject& go = m_Transform->GetGameObject();
		bool inactive = !go.IsActive ();

		// Datatemplates get highlighted blue / red
		Prefab* prefab = go.GetPrefab();
		if (prefab != NULL)
		{
			// Color blue if parent prefab can be found
			if (prefab->GetParentPrefab ())
				return inactive ? 5 : 1 ;
			// Otherwise color red
			else
				return inactive ? 6 : 2;				
		}
		// Non datatemplates get drawn black / grey
		else
		{
			return inactive ? 4 : 0;
		}
	}
	else
		return 3;
}

bool GameObjectHierarchyProperty::IsExpanded (int* expanded, int expandedSize)
{
	if (!HasChildren ())
		return false;

	if (expanded == NULL || m_Transform == NULL)
		return true;
	else
	{
		return IsInExpandedArray(GetInstanceID(), expanded, expandedSize);
	}
}

bool GameObjectHierarchyProperty::Previous (int* expanded, int expandedSize)
{
	int nextInstanceID = GetInstanceID();
	int previousInstanceID = 0;
	
	Reset ();
	while (Next (expanded, expandedSize))
	{
		if (nextInstanceID == GetInstanceID())
			return Find(previousInstanceID, expanded, expandedSize);
		
		previousInstanceID = GetInstanceID();
	}
	return false;
}

bool GameObjectHierarchyProperty::Parent ()
{
	m_Row = -1;
	
	if (m_SortedIteratorStack.empty())
	{
		m_Transform = NULL;
		return false;
	}
	else
	{
		m_Transform = m_Transform->GetParent();
		m_SortedIteratorStack.pop_back();
		return true;
	}
}

vector<int> GameObjectHierarchyProperty::GetAncestors()
{
	vector<int> result;
	result.reserve(m_SortedIteratorStack.size());
	
	for( const Transform* ancestor = m_Transform->GetParent() ; ancestor != NULL ; ancestor = ancestor->GetParent() )
		result.push_back(ancestor->GetGameObjectInstanceID());
	
	return result;
}

void MarkSingleSceneObjectVisible(int instanceID, bool otherVisibility)
{
	GameObjectHierarchyProperty hierarchy;
	
	while (hierarchy.Next (NULL, 0))
	{
		GameObject* go = dynamic_pptr_cast<GameObject*>(Object::IDToPointer(hierarchy.GetInstanceID()));
		if (go)
			go->SetMarkedVisible(otherVisibility);
	}
	
	GameObject* go = dynamic_pptr_cast<GameObject*>(Object::IDToPointer(instanceID));
	if (go)
		go->SetMarkedVisible(true);
}

#if ENABLE_EDITOR_HIERARCHY_ORDERING
bool GameObjectHierarchyProperty::DragTransformHierarchy(bool dropUpon)
{
	GameObject* go = dynamic_pptr_cast<GameObject*>(Object::IDToPointer(GetInstanceID()));

	if (!go) 
		return false;

	std::vector<PPtr<Object> > objects = GetDragAndDrop().GetPPtrs();
	dynamic_array<Transform*> transforms (kMemTempAlloc);
	transforms.reserve(objects.size());

	for (int i = 0; i < objects.size(); ++i)
	{
		GameObject* objectGO = dynamic_pptr_cast<GameObject*>(objects[i]);

		if (!objectGO) 
			return false;

		transforms.push_back(objectGO->QueryComponent(Transform));
	}

	OrderTransformHierarchy(transforms, go->QueryComponent(Transform), dropUpon);
	return true;
}
#endif
