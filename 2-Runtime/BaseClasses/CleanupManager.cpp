#include "UnityPrefix.h"
#if UNITY_EDITOR
#include "CleanupManager.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Graphics/Transform.h"
#include <algorithm>

using namespace std;

void CleanupManager::MarkForDeletion( PPtr<Unity::Component> comp, std::string const& reason )
{
	// Ignore component that is already marked for deletion
	list<struct MarkedComponent>::iterator a = std::find(m_markedComponents.begin(), m_markedComponents.end(), static_cast<Unity::Component*>(comp));
	if (a != m_markedComponents.end())
		return;

	struct MarkedComponent marker;
	m_markedComponents.push_back (marker);
	m_markedComponents.back ().component = comp;
	m_markedComponents.back ().reason = reason;
}

void CleanupManager::Flush()
{
	while (m_markedComponents.size() > 0)
	{
		struct MarkedComponent& marked_component = m_markedComponents.front ();
		PPtr<Unity::Component> comp = marked_component.component;
		
		Unity::Component* compPtr = comp;
		if (compPtr)
		{
			LogString(Format("%s component deleted: %s", comp->GetClassName ().c_str (), marked_component.reason.c_str()));
			if (marked_component.component->GetGameObjectPtr () != NULL)
			{
				DestroyObjectHighLevel (comp);
			}
			else
			{
				// if the component is a transform remove the references
				if (comp->GetClassID () == ClassID(Transform))
				{
					DestroyTransformComponentAndChildHierarchy(static_cast<Transform&>(*comp));
				}
				
				DestroySingleObject (comp);
			}
		}

		m_markedComponents.pop_front ();
	}
}

static CleanupManager* singleton = NULL;
CleanupManager& GetCleanupManager ()
{
	if (singleton == NULL)
	{
		singleton = new CleanupManager();
	}

	return *singleton;
}

#endif