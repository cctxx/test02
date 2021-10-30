#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK

#include "PackStateSpecialized.h"
#include "Runtime/Dynamics/RigidBody.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/AnimationState.h"
#include "BitStreamPacker.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScriptCache.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Configuration/UnityConfigure.h"
#include "Runtime/Scripting/Scripting.h"
#include "Runtime/Interfaces/IPhysics.h"
#include "Runtime/Interfaces/IAnimationStateNetworkProvider.h"

const float kPositionEpsilon = 0.00001F;
const float kRotationEpsilon = 0.0001F;

bool PackTransform (Transform& transform, BitstreamPacker& packer)
{	
	Vector3f pos = transform.GetLocalPosition();
	Quaternionf rot = transform.GetLocalRotation();
	Vector3f scale = transform.GetLocalScale();
	
	packer.Serialize(pos, kPositionEpsilon);
	packer.Serialize(rot, kRotationEpsilon);
	packer.Serialize(scale, kPositionEpsilon);
	return packer.HasChanged();
}

void UnpackTransform (Transform& transform, BitstreamPacker& packer)
{
	Vector3f pos;
	Quaternionf rot;
	Vector3f scale;
	
	packer.Serialize(pos, kPositionEpsilon);
	packer.Serialize(rot, kRotationEpsilon);
	packer.Serialize(scale, kPositionEpsilon);
		
	transform.SetLocalTRS(pos, rot, scale);
}

bool SerializeAnimation (Animation& animation, BitstreamPacker& packer)
{
	IAnimationStateNetworkProvider* animationSystem = GetIAnimationStateNetworkProvider();
	if (animationSystem == NULL)
		return false;
	
	int count = animationSystem->GetNetworkAnimationStateCount (animation);
	
	AnimationStateForNetwork* serialized;
	ALLOC_TEMP(serialized, AnimationStateForNetwork, count);
		
		if (packer.IsWriting())
		animationSystem->GetNetworkAnimationState (animation, serialized, count);
	
	for (int i=0;i<count;i++)
		{
		packer.Serialize(serialized[i].enabled);
		packer.Serialize(serialized[i].weight, 0.01F);
		packer.Serialize(serialized[i].time, 0.01F);
		}
		
		if (packer.IsReading())
		animationSystem->SetNetworkAnimationState (animation, serialized, count);
	
	return packer.HasChanged();
}


/* Specialized function for packing rigid bodies. We want certain parameters
 * from the Rigidbody object: position, rotation, anglar velocity and velocity.
 */
bool SerializeRigidbody (Rigidbody& rigidbody, BitstreamPacker& packer)
{	
	IPhysics::RigidBodyState state;
	IPhysics& module = *GetIPhysics();

	if (packer.IsWriting())
		module.GetRigidBodyState(rigidbody, &state);
	
	packer.Serialize(state.position, kPositionEpsilon);
	packer.Serialize(state.rotation, kRotationEpsilon);
	packer.Serialize(state.velocity, kPositionEpsilon);
	packer.Serialize(state.avelocity, kPositionEpsilon);
	
	if (packer.IsReading())
		module.SetRigidBodyState(rigidbody, state);

	return packer.HasChanged();
}

bool SerializeMono (MonoBehaviour& mono, BitstreamPacker& deltaState, NetworkMessageInfo& timeStamp)
{
	if (mono.IsActive() && mono.GetInstance())
	{
		ScriptingMethodPtr method = mono.GetMethod(MonoScriptCache::kSerializeNetView);
		if (method)
		{
			MonoObject* monoStream = mono_object_new(mono_domain_get(), GetMonoManager().GetCommonClasses().bitStream);
			ExtractMonoObjectData<BitstreamPacker*> (monoStream) = &deltaState;
			void* values[] = { monoStream, &timeStamp };
			MonoException* exception;
			MonoObject* instance = mono.GetInstance();
			mono_runtime_invoke_profiled (method->monoMethod, mono.GetInstance(), values, &exception);

			if (exception != NULL)
				Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
		
			ExtractMonoObjectData<BitstreamPacker*> (monoStream) = NULL;
		}
	}
	
	return deltaState.HasChanged();
}


#endif
