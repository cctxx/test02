#include "UnityPrefix.h"
#include "Editor/Src/MenuController.h"
#include "Editor/Src/EditorHelper.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Gizmos/GizmoUtil.h"
#include "Runtime/Shaders/Material.h"
#include "Runtime/Shaders/Shader.h"
#include "Runtime/Filters/Renderer.h"

struct MaterialHelper : public MenuInterface {
	Material *GetActiveMaterial (Object *context) {
		Material *activeMaterial = dynamic_pptr_cast<Material*> (context);
		if (!activeMaterial ) {
			Renderer *rend =  dynamic_pptr_cast<Renderer*> (context);;
			if (rend && rend->GetMaterialCount () > 0)
				activeMaterial = rend->GetMaterial (0);
		} 
		return activeMaterial;
	}
	virtual bool Validate (const MenuItem &menuItem) {
		Assert (!menuItem.context.empty());
		Material *activeMaterial = GetActiveMaterial(menuItem.context[0]);
		if (!activeMaterial)
			return false;

		int idx = atoi (menuItem.m_Command.c_str());
		switch (idx) {
		case 0: {
		case 1:
			Shader *s = activeMaterial->GetShader();
			return s != NULL && IsUserModifiableScript (*s);
		}
		case 2:
			return IsUserModifiable (*activeMaterial);
			break;
		}
		return false;
	}

	virtual void Execute (const MenuItem &menuItem) {
		Assert (!menuItem.context.empty());
		int idx = atoi (menuItem.m_Command.c_str());
		Material *activeMaterial = GetActiveMaterial(menuItem.context[0]);
		AssertIf (!activeMaterial);
		switch (idx) {
		case 0:
			SetActiveObject (activeMaterial->GetShader());
			break;
		case 1:
			OpenAsset (activeMaterial->GetShader()->GetInstanceID());
			break;
		case 2:
			SetActiveObject (activeMaterial);
			break;
		}
	}
};

static MaterialHelper *gMaterialHelper;
void MaterialHelperRegisterMenu ();
void MaterialHelperRegisterMenu () {
	gMaterialHelper = new MaterialHelper;

	MenuController::AddContextItem ("Material/Select Shader ", "0", gMaterialHelper);
	MenuController::AddContextItem ("Material/Edit Shader...", "1", gMaterialHelper);
	MenuController::AddContextItem ("Renderer/Select Material ", 	  "2", gMaterialHelper);
}

STARTUP (MaterialHelperRegisterMenu)	// Call this on startup.
