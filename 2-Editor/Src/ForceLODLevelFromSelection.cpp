#include "UnityPrefix.h"
#include "SceneInspector.h"
#include "Runtime/Camera/LODGroupManager.h"
#include "Runtime/Camera/LODGroup.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Camera/UnityScene.h"
#include "EditorHelper.h"
#include "Runtime/BaseClasses/IsPlaying.h"

class ForceLODLevelFromSelection : public ISceneInspector
{
	public: 
	
	virtual void SelectionHasChanged (const std::set<int>& selection)
	{
		LODGroupManager& m = GetLODGroupManager();
		m.ClearAllForceLODMask ();

		if (selection.empty())
			return;
		if (IsWorldPlaying())
			return;
		
		// Force LOD levels of any renderers
		for (std::set<int>::const_iterator i=selection.begin();i != selection.end();++i)
		{
			GameObject* go = dynamic_instanceID_cast<GameObject*> (*i);
			if (go == NULL)
				continue;
			
			Renderer* renderer = go->QueryComponent(Renderer);
			if (renderer == NULL || !renderer->IsInScene ())
				continue;
			
			const SceneNode& node = GetScene().GetRendererNode(renderer->GetSceneHandle());
			if (node.lodIndexMask == 0 || BitsInMask(node.lodIndexMask) != 1)
				continue;
			
			UInt32 mask = node.lodIndexMask | m.GetForceLODMask(node.lodGroup);
			m.SetForceLODMask (node.lodGroup, mask);
		}

		// If multiple renderers belonging to multiple lod levels in the same LODGroup.
		// Don't perform force LOD level. Since it is unclear which should be shown...
		////@TODO: unsure about this one...
//		for (int i=0;i<m.GetLODGroupCount();i++)
//		{
//			UInt32 mask = m.GetForceEditorMask (i);
//			if (mask != 0 && BitsInMask (mask) != 1)
//				GetLODGroupManager().SetForceEditorMask (i, 0);
//		}
		
		// If any LOD groups are selected, never force the LOD level.
		// Otherwise the preview of the LODGroup will not work.
		for (std::set<int>::const_iterator i=selection.begin();i != selection.end();++i)
		{
			GameObject* go = dynamic_instanceID_cast<GameObject*> (*i);
			if (go == NULL)
				continue;
			
			LODGroup* group = go->QueryComponent(LODGroup);
			if (group == NULL || group->GetLODGroup() == -1)
				continue;
			
			m.SetForceLODMask (group->GetLODGroup(), 0);
		}
	}
};

static void RegisterForceLODLevelFromSelection ()
{
	ForceLODLevelFromSelection* selection = new ForceLODLevelFromSelection();
	GetSceneTracker().AddSceneInspector(selection);
}

STARTUP (RegisterForceLODLevelFromSelection)
