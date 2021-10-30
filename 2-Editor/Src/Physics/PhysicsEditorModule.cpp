#include "UnityPrefix.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "PhysicsManager.h"
#include "Runtime/Dynamics/PhysicsManager.h"
#include "External/PhysX/builds/SDKs/Physics/include/NxPhysics.h"
#include "External/PhysX/builds/SDKs/Cooking/include/NxCooking.h"
#include "Runtime/Dynamics/NxMeshCreation.h"
#include "Runtime/Dynamics/nxmemorystream.h"

//only implementation.
class PhysicsModule : public IPhysicsEditor
{
public:	

	virtual void BakeMesh (Mesh* mesh, bool convex, bool bigEndian, dynamic_array<UInt8>& outstream)
	{
		NxCookingParams oldParams = NxGetCookingParams();
		NxCookingParams params = oldParams;
		if (bigEndian)
			params.targetPlatform = PLATFORM_PPCOSX;
		else
			params.targetPlatform = PLATFORM_PC;
		
		NxSetCookingParams(params);
		
		MemoryStream memoryStream(NULL, 0);
		Matrix4x4f matrix;
		if (GetIPhysics()->CreateNxStreamFromUnityMesh(mesh, convex, matrix, kNoScaleTransform, memoryStream))
		{
			outstream.assign(memoryStream.mBuffer, memoryStream.mBuffer + memoryStream.mLen);
		}
		else
		{
			outstream.clear();
		}
		
		NxSetCookingParams(oldParams);
	}

	virtual void RefreshWhenPaused()
	{
		GetPhysicsManager().RefreshWhenPaused();
	}
};

void InitializePhysicsEditorModule ()
{
	SetIPhysicsEditor(UNITY_NEW_AS_ROOT(PhysicsEditorModule, kMemPhysics, "PhysicsEditorInterface", ""));
}

void CleanupPhysicsEditorModule ()
{
	PhysicsEditorModule* module = reinterpret_cast<PhysicsEditorModule*> (GetIPhysicsEditor ());
	UNITY_DELETE(module, kMemPhysics);
	SetIPhysicsEditor (NULL);
}