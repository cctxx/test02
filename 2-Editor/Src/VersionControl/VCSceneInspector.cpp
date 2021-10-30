#include "UnityPrefix.h"
#include "Editor/Src/AssetPipeline/AssetDatabase.h"
#include "Editor/Src/VersionControl/VCCache.h"
#include "Editor/Src/VersionControl/VCSceneInspector.h"

void VCSceneInspector::ObjectHasChanged(PPtr<Object> object)
{
	if (object.IsValid() && object->IsPersistent() && object->IsPersistentDirty())
		GetVCCache().SetDirty(ObjectToGUID(object));
}
