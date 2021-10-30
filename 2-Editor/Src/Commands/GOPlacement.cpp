#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Undo/UndoManager.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Editor/Src/Undo/Undo.h"
#include <set>

typedef std::set<Transform*> TransformSet;

struct GOPlacement : public MenuInterface
{
	void CenterOnChildren ()
	{
		TransformSet selection = GetTransformSelection ();
	
		std::string undoName = "Center on Children";

		for (TransformSet::iterator i=selection.begin ();i != selection.end ();i++)
		{
			Vector3f averagePos = Vector3f::zero;
			Transform& father = **i;

			vector<Vector3f> oldPositions;
			for (Transform::iterator j=father.begin ();j != father.end ();j++)
			{
				Transform& child = **j;

				oldPositions.push_back (child.GetPosition ());
				averagePos += oldPositions.back ();
			}

			if (father.GetChildrenCount () > 0)
			{
				averagePos /= (float)father.GetChildrenCount ();
				
				RecordUndoDiff (&father, undoName);
				father.SetPosition (averagePos);
				
				for (Transform::iterator j=father.begin ();j != father.end ();j++)
				{
					Transform& child = **j;
					RecordUndoDiff (&child, undoName);
					int index = std::distance (father.begin (), j);
					child.SetPosition (oldPositions[index]);
				}
			}
		}
	}
	
	bool CanCenterOnChildren ()
	{
		TransformSet selection = GetTransformSelection ();
	
		for (TransformSet::iterator i=selection.begin ();i != selection.end ();i++)
		{
			Transform& father = **i;
			if (father.GetChildrenCount () > 0)
				return true;
		}
		return false;
	}

	virtual bool Validate (const MenuItem &menuItem)
	{
		return CanCenterOnChildren ();
	}

	virtual void Execute (const MenuItem &menuItem)
	{
		CenterOnChildren ();
	}
};

static GOPlacement* gGOPlacement = NULL;
static void GOPlacementRegisterMenu ()
{
	gGOPlacement = new GOPlacement;

	MenuController::AddMenuItem ("GameObject/Center On Children", "0", gGOPlacement, 20);
}

STARTUP (GOPlacementRegisterMenu)
