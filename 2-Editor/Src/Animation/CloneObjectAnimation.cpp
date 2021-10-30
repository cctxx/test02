
#include "Editor/Src/Animation/CloneObjectAnimation.h"
#include "Runtime/GameCode/CloneObject.h"

#include "Runtime/Animation/Animator.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/ArrayUtility.h"


bool MustRemoveFromObject(PPtr<Object> object)
{
	const int animClassIDs[] = {ClassID(Transform), ClassID(Animator), ClassID(Animation), ClassID(Renderer), ClassID(MeshFilter) };

	if(object->IsDerivedFrom(ClassID(Component)))
	{		
		for (int i=0;i<ARRAY_SIZE(animClassIDs);i++)
		{
			if(object->IsDerivedFrom(animClassIDs[i]))
				return false;
		}
		return true;
	}

	return false;		
	
}

Object& InstantiateObjectRemoveAllNonAnimationComponents (Object& inObject, const Vector3f& worldPos, const Quaternionf& worldRot)
{
	TempRemapTable ptrs;
	Object& obj = InstantiateObject (inObject, worldPos, worldRot, ptrs);

	TempRemapTable::vector_container& remappedPtrTable = ptrs.get_vector();
	
	TempRemapTable purgedPtrs;
	purgedPtrs.reserve(ptrs.size());
			
	for(TempRemapTable::vector_container::const_iterator it =  remappedPtrTable.begin(); it != remappedPtrTable.end() ; it++)
	{
		PPtr<Object> newObject = PPtr<Object> (it->second);
		if(MustRemoveFromObject(newObject))
		{
			newObject->HackSetResetWasCalled();
			newObject->HackSetAwakeWasCalled();				
			DestroyObjectHighLevel(newObject,true);		
		}	
		else
		{
			purgedPtrs.get_vector().push_back(*it);		
		}
	}
	

	AwakeAndActivateClonedObjects(purgedPtrs);

	return obj;

}
