#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Graphics/Transform.h"
#include "Editor/Src/Undo/Undo.h"
#include "Editor/Src/SceneInspector.h"

struct TransformHelper : public MenuInterface {
	virtual bool Validate (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
	
		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
		{
			Transform* tc = dynamic_pptr_cast<Transform*> (*i);
			if (tc != NULL)
			{
				GameObject *object = tc->GetGameObjectPtr();
				if (IsUserModifiable(*object))
				{
					switch (idx) {
					case 0:
						if (tc->GetLocalPosition() != Vector3f::zero)
							return true;
					case 1:
						if (tc->GetLocalRotation() != Quaternionf::identity() || tc->GetLocalEulerAngles() != Vector3f::zero)
							return true;
					case 2:
						if (tc->GetLocalScale() != Vector3f::one)
							return true;
					}
				}
			}
		}
		return false;
	}

	virtual void Execute (const MenuItem &menuItem) {
		int idx = atoi (menuItem.m_Command.c_str());
		for (std::vector<Object*>::const_iterator i = menuItem.context.begin(); i != menuItem.context.end(); i++)
		{
			Transform* trs = dynamic_pptr_cast<Transform*> (*i);
			if (trs != NULL)
			{
				GameObject *object = trs->GetGameObjectPtr();
				if (IsUserModifiable(*object))
				{
					switch (idx) {
					case 0:
						RecordUndoDiff(object, Append(Append("Reset ", object->GetName()), " Position"));
						trs->SetLocalPosition (Vector3f::zero);
						break;
					case 1:
						RecordUndoDiff(object, Append(Append("Reset ", object->GetName()), " Rotation"));
						trs->SetLocalEulerAngles (Vector3f::zero);
						trs->SetLocalRotation (Quaternionf::identity());
						break;
					case 2:
						RecordUndoDiff(object, Append(Append("Reset ", object->GetName()), " Scale"));
						trs->SetLocalScale (Vector3f::one);
						break;
					}
				}
				GetSceneTracker ().ForceReloadInspector ();
			}
		}
	}
};

static TransformHelper *gTransformHelper;
void TransformHelperRegisterMenu ();
void TransformHelperRegisterMenu () {
	gTransformHelper = new TransformHelper;

	MenuController::AddContextItem ("Transform/Reset Position", "0", gTransformHelper);
	MenuController::AddContextItem ("Transform/Reset Rotation", "1", gTransformHelper);
	MenuController::AddContextItem ("Transform/Reset Scale", 	"2", gTransformHelper);
}

STARTUP (TransformHelperRegisterMenu)	// Call this on startup.
