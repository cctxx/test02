
#include "UnityPrefix.h"

#include "AnimatorController.h"

#include "Runtime/Animation/RuntimeAnimatorController.h"

#include "Runtime/mecanim/animation/avatar.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/Blobification/BlobWrite.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/mecanim/generic/stringtable.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/AnimationSetBinding.h"

#if UNITY_EDITOR
#include "Editor/Src/Animation/StateMachine.h"
#include "Editor/Src/Animation/AvatarMask.h"
#include "Runtime/Scripting/Backend/ScriptingInvocation.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"
#endif

#include "Runtime/Scripting/Backend/ScriptingInvocation.h"


#define DIRTY_AND_INVALIDATE() OnInvalidateAnimatorController(); SetDirty();



IMPLEMENT_OBJECT_SERIALIZE (AnimatorController)
IMPLEMENT_CLASS_HAS_INIT(AnimatorController)



AnimatorController::AnimatorController(MemLabelId label, ObjectCreationMode mode)
	:	Super(label, mode),
	m_Allocator(1024*4),
	m_Controller(0),
	m_ControllerSize(0),
	m_AnimationSetBindings(0),	
	m_IsAssetBundled(true)	
#if UNITY_EDITOR
	,
	m_Dependencies(this)	
#endif
{

}

AnimatorController::~AnimatorController()
{	
#if UNITY_EDITOR
	m_Dependencies.Clear();	
#endif

	NotifyObjectUsers( kDidModifyAnimatorController );
}

void AnimatorController::InitializeClass () 
{
	REGISTER_MESSAGE_VOID(AnimatorController, kDidModifyMotion, OnInvalidateAnimatorController);

#if UNITY_EDITOR
	RegisterAllowNameConversion (AnimatorController::GetClassStringStatic(), "m_Layers", "m_AnimatorLayers");
	RegisterAllowNameConversion (AnimatorController::GetClassStringStatic(), "m_AnimatorEvents", "m_AnimatorParameters");	
	RegisterAllowTypeNameConversion( "AnimatorEvent", "AnimatorControllerParameter") ;
#endif
}

void AnimatorController::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);

#if UNITY_EDITOR
	OnInvalidateAnimatorController();
	
	// Force load the AnimatorController
	// This ensures that when we build a player the Controller is fully initialized.
	// @TODO: This is kind of a hack. It would be better if we make sure when building a player we will ensure that the ControllerConstant has been created.
	GetAsset();

#endif
	
	if (m_AnimationSetBindings == NULL && m_Controller != NULL) 
	{
		RegisterAnimationClips();		
		m_AnimationSetBindings = UnityEngine::Animation::CreateAnimationSetBindings(m_Controller, GetAnimationClips(), m_Allocator); 
	}
}


void AnimatorController::CheckConsistency () 
{
	Super::CheckConsistency();
#if UNITY_EDITOR

	AnimatorControllerParameterVector toRemove;
	for(int i=0;i<m_AnimatorParameters.size();++i)
	{
		// This is the old Vector type which is not supported anymore
		if(m_AnimatorParameters[i].GetType() == 0)
			toRemove.push_back(m_AnimatorParameters[i]);		
	}

	for(int i=0;i<toRemove.size();++i)
	{
		for(int j=0;j<m_AnimatorParameters.size();++j)
		{
			if( strcmp(m_AnimatorParameters[j].GetName(), toRemove[i].GetName()) == 0 )
			{
				RemoveParameter(i);
				break;
			}
		}		
	}

	for(int i=0;i<m_AnimatorLayers.size();++i)
	{
		m_AnimatorLayers[i].SetController(this);
		
		if( m_AnimatorLayers[i].GetSyncedLayerIndex() >= static_cast<int>(m_AnimatorLayers.size()))
		{
			m_AnimatorLayers[i].SetSyncedLayerIndexInternal(-1);
			m_AnimatorLayers[i].SetStateMachineMotionSetIndex(0);			
		}

		if( m_AnimatorLayers[i].GetSyncedLayerIndex() != -1)
		{
			StateMachine* stateMachine = m_AnimatorLayers[i].GetStateMachine();
			int motionSetCount = stateMachine->GetMotionSetCount();

			if(m_AnimatorLayers[i].GetStateMachineMotionSetIndex() >= motionSetCount)
			{
				// if there is only 2 motion set and only one layer is synchronize we can reconnect
				if(motionSetCount == 2)
				{
					bool valid = true;
					for(int j=0;j<m_AnimatorLayers.size() && valid;++j)
					{
						if( i!=j && m_AnimatorLayers[i].GetSyncedLayerIndex() == m_AnimatorLayers[j].GetSyncedLayerIndex())
							valid = false;
					}

					if(valid)
						m_AnimatorLayers[i].SetStateMachineMotionSetIndex(1);
					else
					{
						m_AnimatorLayers[i].SetSyncedLayerIndexInternal(-1);
						m_AnimatorLayers[i].SetStateMachineMotionSetIndex(0);							
					}					
				}
				else
				{
					m_AnimatorLayers[i].SetSyncedLayerIndexInternal(-1);
					m_AnimatorLayers[i].SetStateMachineMotionSetIndex(0);	
				}
			}
		}			
	}
	for(int i=0;i<m_AnimatorParameters.size();++i)
	{
		m_AnimatorParameters[i].SetController(this);
	}
#endif
}

template <typename TransferFunction>
bool IsLoadingFromAssetBundle (TransferFunction& transfer)
{
	return transfer.IsReading () && transfer.IsSerializingForGameRelease();
}


template<class TransferFunction>
void AnimatorController::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);		
	transfer.SetVersion(2);	

	if (transfer.IsSerializingForGameRelease())
	{
		TRANSFER(m_ControllerSize);

		if(m_Controller == 0) 
			m_Allocator.Reserve(m_ControllerSize);

		transfer.SetUserData(&m_Allocator);
		TRANSFER_NULLABLE(m_Controller, mecanim::animation::ControllerConstant);
		TRANSFER(m_TOS);		
	
		transfer.Transfer (m_AnimationClips, "m_AnimationClips");
	}

	TRANSFER_EDITOR_ONLY_HIDDEN(m_AnimatorParameters);	
	transfer.Align();
	TRANSFER_EDITOR_ONLY_HIDDEN(m_AnimatorLayers);	

	// case 491674 Crash when avatar is selected in the Hiererchy when in play mode
	// cannot display a controller in UI if it come from an asset bundle
#if UNITY_EDITOR
	if(transfer.IsReading ())
		m_IsAssetBundled = IsLoadingFromAssetBundle(transfer) && m_AnimatorLayers.size() == 0;

	if (transfer.IsRemapPPtrTransfer() && !transfer.IsSerializingForGameRelease())
	{
		transfer.Transfer (m_AnimationClips, "m_AnimationClips");
	}
#endif

}


#if UNITY_EDITOR

void AnimatorController::BuildAsset() 
{	
	ClearAsset();

	m_Dependencies.Clear();
	m_TOS.clear();

	// Insert all reserve keyword
	mecanim::ReserveKeyword* staticTable = mecanim::ReserveKeywordTable();
	int i;
	for(i=0;i<mecanim::eLastString;i++)
	{
		m_TOS.insert( std::make_pair(staticTable[i].m_ID, std::string(staticTable[i].m_Keyword) ) );
	}

	/// Parameters
	dynamic_array<mecanim::uint32_t> types (kMemTempAlloc);	
	dynamic_array<int> eventIds (kMemTempAlloc);
	for(i = 0 ; i < GetParameterCount() ; i++)
	{
		AnimatorControllerParameter* parameter = GetParameter(i);
		eventIds.push_back( mecanim::processCRC32( mecanim::String(parameter->GetName())));
		m_TOS.insert( std::make_pair(eventIds[i],  parameter->GetName()) );
		types.push_back(parameter->GetType());
	}				

	mecanim::ValueArrayConstant* values	= mecanim::CreateValueArrayConstant(types.begin(), types.size(), m_Allocator);

	for(i = 0 ; i < GetParameterCount() ; i++)
		values->m_ValueArray[i].m_ID = eventIds[i];	


	mecanim::ValueArray* defaultValues =  mecanim::CreateValueArray(values, m_Allocator);

	for(i = 0 ; i < GetParameterCount() ; i++)
	{
		AnimatorControllerParameter* parameter = GetParameter(i);
		switch(parameter->GetType())
		{
		case AnimatorControllerParameterTypeFloat:
			{
				float val = parameter->GetDefaultFloat();
				defaultValues->WriteData(val, values->m_ValueArray[i].m_Index);
				break;
			}				
		case AnimatorControllerParameterTypeInt:
			{
				mecanim::int32_t val =  parameter->GetDefaultInt();
				defaultValues->WriteData(val, values->m_ValueArray[i].m_Index);
				break;
			}				
		case AnimatorControllerParameterTypeTrigger:
		case AnimatorControllerParameterTypeBool:
			{
				bool val = parameter->GetDefaultBool();
				defaultValues->WriteData(val, values->m_ValueArray[i].m_Index);
				break;
			}				
		}
	}


	/// Layers

	std::vector<mecanim::animation::LayerConstant*> layerVector;
	std::vector<mecanim::statemachine::StateMachineConstant*> stateMachineVector;
	std::vector<int> stateMachineIndexVector;

	int stateMachineIndex  = 0; 
	for(int i = 0 ; i < GetLayerCount() ; i++)
	{
		if(m_AnimatorLayers[i].GetSyncedLayerIndex() == -1)
			stateMachineIndexVector.push_back(stateMachineIndex++);
		else
			stateMachineIndexVector.push_back(-1);
	}

	for(int i = 0 ; i < GetLayerCount() ; i++)
	{	
		int stateMachineIndex  = 0; 

		StateMachine* editorStateMachine = m_AnimatorLayers[i].GetStateMachine();

		if(m_AnimatorLayers[i].GetSyncedLayerIndex() == -1)
		{			
			if(editorStateMachine)
			{					
				mecanim::statemachine::StateMachineConstant* stateMachine = editorStateMachine->BuildRuntimeAsset(m_Dependencies, m_TOS, i, m_Allocator);	
				AssertIf(stateMachine == 0);
				if(stateMachine)
				{					
					editorStateMachine->GetAnimationClips(m_AnimationClips, m_AnimatorLayers[i].GetStateMachineMotionSetIndex());									
				}

				stateMachineVector.push_back(stateMachine);
				stateMachineIndex = stateMachineIndexVector[i];
			}			
		}
		else 
		{
			if (!GetBuildSettings().hasAdvancedVersion)
			{
				ErrorString("Sync Layer is only supported in Unity Pro. Layer will be discarded in game");
				stateMachineIndex = mecanim::DISABLED_SYNCED_LAYER_IN_NON_PRO;
			}

			else
			{
				stateMachineIndex  = stateMachineIndexVector[m_AnimatorLayers[i].GetSyncedLayerIndex()];
				if(editorStateMachine)
				{	
					editorStateMachine->GetAnimationClips(m_AnimationClips, m_AnimatorLayers[i].GetStateMachineMotionSetIndex());			
				}
			}
		}

		AnimatorControllerLayer &animatorLayer = m_AnimatorLayers[i];
		
		mecanim::animation::LayerConstant*	layer	= mecanim::animation::CreateLayerConstant(stateMachineIndex, animatorLayer.GetStateMachineMotionSetIndex(), m_Allocator);
		layer->m_Binding = mecanim::processCRC32( mecanim::String( animatorLayer.GetName()));
		m_TOS.insert( std::make_pair(layer->m_Binding,  animatorLayer.GetName()) );
		layer->m_IKPass = animatorLayer.GetIKPass();
		layer->m_LayerBlendingMode = animatorLayer.GetBlendingMode();
		layer->m_DefaultWeight = animatorLayer.GetDefaultWeight();
		layer->m_SyncedLayerAffectsTiming = animatorLayer.GetBlendingMode() == AnimatorLayerBlendingModeOverride ? animatorLayer.GetSyncedLayerAffectsTiming() : false;

		AvatarMask* mask = animatorLayer.GetMask();
		layer->m_BodyMask = mask != NULL ? mask->GetHumanPoseMask(m_Dependencies) : mecanim::human::FullBodyMask();
		layer->m_SkeletonMask = mask != NULL ? mask->GetSkeletonMask(m_Dependencies, m_Allocator) : 0;

 		layerVector.push_back(layer);	
									
	}			

	// Early exit if no layer or statemachine
	if(layerVector.size() == 0 || stateMachineVector.size() == 0)
		return;


	mecanim::animation::ControllerConstant*	controllerConstant	= 
		mecanim::animation::CreateControllerConstant(	layerVector.size(), layerVector.size() ? &layerVector.front() : 0,
		stateMachineVector.size(), stateMachineVector.size() ? &stateMachineVector.front() :0, 
		values, defaultValues, m_Allocator);
	AssertIf(controllerConstant == 0);
	if(controllerConstant)
	{
		m_Controller = controllerConstant;

		BlobWrite::container_type data;
		BlobWrite blobWrite (data, kNoTransferInstructionFlags, kBuildNoTargetPlatform);
		blobWrite.Transfer(*m_Controller, "Base");

		m_ControllerSize = data.size();

		RegisterAnimationClips();
		m_AnimationSetBindings = UnityEngine::Animation::CreateAnimationSetBindings(m_Controller, GetAnimationClips(), m_Allocator);
	}
	
}


AnimatorControllerLayer* AnimatorController::GetLayer(int index)
{
	if(ValidateLayerIndex(index))
		return &m_AnimatorLayers[index];

	return 0;
}

const AnimatorControllerLayer* AnimatorController::GetLayer(int index) const
{
	if(ValidateLayerIndex(index))
		return &m_AnimatorLayers[index];

	return 0;
}

int	AnimatorController::GetLayerCount() const
{
	return m_AnimatorLayers.size();
}

void AnimatorController::AddLayer(const std::string& name)
{
	m_IsAssetBundled = false;	
	AnimatorControllerLayer layer;
	layer.SetController(this);

	StateMachine *stateMachine = CreateObjectFromCode<StateMachine>();
	
	stateMachine->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
	if(IsPersistent()) 
		AddAssetToSameFile(*stateMachine, *this, true);			
		
	layer.SetStateMachine(stateMachine);	
	layer.SetName(name.c_str());

	m_AnimatorLayers.push_back(layer);
	DIRTY_AND_INVALIDATE();
}

void AnimatorController::RemoveLayer(int index)
{
	if(ValidateLayerIndex(index))
	{
		if(GetLayer(index)->GetSyncedLayerIndex() != -1)
			GetLayer(index)->SetSyncedLayerIndex(-1); // this will remove motion set and ensure consistency
	
		// Update sync layer index
		for(int i=0;i<m_AnimatorLayers.size();++i)
		{
			AnimatorControllerLayer *layer = GetLayer(i);
			if( i!=index && layer->GetSyncedLayerIndex() > index)
				layer->SetSyncedLayerIndexInternal(layer->GetSyncedLayerIndex()-1);

			// If a layer is sync on this layer break the synchronization
			if( i!=index && layer->GetSyncedLayerIndex() == index)
				layer->SetSyncedLayerIndex(-1);
		}	
		m_AnimatorLayers.erase(m_AnimatorLayers.begin() + index);	

		DIRTY_AND_INVALIDATE();
	}
}


int	AnimatorController::GetParameterCount() const 
{
	return m_AnimatorParameters.size();
}


void AnimatorController::AddParameter(const std::string& name, AnimatorControllerParameterType type)
{	
	m_AnimatorParameters.push_back(AnimatorControllerParameter());
	AnimatorControllerParameter *animatorParameter = GetParameter(GetParameterCount()-1);
	animatorParameter->SetController(this);
	animatorParameter->SetName(name.c_str());
	animatorParameter->SetType(type);	

	int count = 0; 
	for(int i = 0 ; i < GetParameterCount(); i++)
		if(GetParameter(i)->GetType() == type) count++;

	if(count == 1)
	{
		for(int i = 0 ; i < GetLayerCount(); i++)
		{
			if(GetLayer(i)->GetStateMachine())
				GetLayer(i)->GetStateMachine()->AddFirstParameterOfType(animatorParameter->GetName(),type);
		}
	}
	DIRTY_AND_INVALIDATE();
}

void AnimatorController::RemoveParameter(int index)
{
	if(ValidateParameterIndex(index))
	{
		AnimatorControllerParameterType eventType = GetParameter(index)->GetType();

		int otherSameTypeParameterIndex = -1;
		//find other event of type
		for(int i = 0 ; i < GetParameterCount() && otherSameTypeParameterIndex == -1; i++)
		{
			if( i != index && GetParameter(i)->GetType() == eventType)
				otherSameTypeParameterIndex = i;
		}

		for(int i = 0 ; i < GetLayerCount(); i++)
		{
			if(GetLayer(i)->GetStateMachine())
				GetLayer(i)->GetStateMachine()->RenameParameter(otherSameTypeParameterIndex != -1 ? GetParameter(otherSameTypeParameterIndex)->GetName() :  "", GetParameter(index)->GetName());
		}

		m_AnimatorParameters.erase(m_AnimatorParameters.begin() + index);
		DIRTY_AND_INVALIDATE();
	}
}

int AnimatorController::FindParameter(const std::string&name) const
{
	int ret = -1;

	for(int i = 0; i < m_AnimatorParameters.size() && ret == -1; i++)
	{
		if(strcmp (m_AnimatorParameters[i].GetName(), name.c_str()) == 0)		
		{
			ret = i;
		}
	}

	return ret;
}

AnimatorControllerParameter* AnimatorController::GetParameter(int index) 
{
	if(ValidateParameterIndex(index))
		return &m_AnimatorParameters[index];

	return 0;
}
const AnimatorControllerParameter* AnimatorController::GetParameter(int index) const
{
	if(ValidateParameterIndex(index))
		return &m_AnimatorParameters[index];

	return 0;
}


std::vector<PPtr<Object> > AnimatorController::CollectObjectsUsingParameter(const string& parameterName)
{
	std::vector<PPtr<Object> > ret;
	for(int i = 0 ;  i < GetLayerCount() ; i++)
	{
		StateMachine* stateMachine = GetLayer(i)->GetStateMachine();

		if(stateMachine)
		{
			std::vector<PPtr<Object> > currentRet = stateMachine->CollectObjectsUsingParameter(parameterName);
			ret.insert(ret.end(), currentRet.begin(), currentRet.end());
		}
	}

	return ret;	
}

string AnimatorController::MakeUniqueParameterName(const string& newName) const
{
	string attemptName = newName;
	int attempt = 0;
	while (true)
	{
		int i = 0;
		for (i = 0; i < GetParameterCount(); i++)
		{
			if (attemptName == GetParameter(i)->GetName())
			{
				attemptName = newName;
				attemptName += Format(" %d", attempt);
				attempt++;
				break;
			}
		}
		if (i == GetParameterCount())
			break;
	}

	return attemptName;
}

string AnimatorController::MakeUniqueLayerName(const string& newName) const
{
	string attemptName = newName;
	int attempt = 0;
	while (true)
	{
		int i = 0;
		for (i = 0; i < GetLayerCount(); i++)
		{
			if (attemptName == GetLayer(i)->GetName())
			{
				attemptName = newName;
				attemptName += Format(" %d", attempt);
				attempt++;
				break;
			}
		}
		if (i == GetLayerCount())
			break;
	}

	return attemptName;

}


bool AnimatorController::ValidateLayerIndex(int index) const
{
	if(index >= 0 && index < GetLayerCount())
	{
		return true;
	}
	
	ErrorString("Invalid Layer index");
	return false;	
}

bool AnimatorController::ValidateParameterIndex(int index) const
{
	if(index >=  0 && index < GetParameterCount())
	{		
		return true;
	}
	
	ErrorString("Invalid Parameter index");
	return false;
}


#endif


AnimationClipVector AnimatorController::GetAnimationClips() const
{
	const_cast<AnimatorController*>(this)->GetAsset(); // @TODO: Force load the AnimatorController. This is kind of a hack.

	return m_AnimationClips;
}

AnimationClipVector AnimatorController::GetAnimationClipsToRegister() const
{
	return GetAnimationClips();
}



void AnimatorController::OnInvalidateAnimatorController()
{	
#if UNITY_EDITOR
	if(	!m_IsAssetBundled)
	{	
		ClearAsset();	

		ScriptingInvocation invocation("UnityEditorInternal","AnimatorController", "OnInvalidateAnimatorController");
		invocation.AddObject(Scripting::ScriptingWrapperFor(this));
		invocation.Invoke();
	}

#endif

	NotifyObjectUsers( kDidModifyAnimatorController );	
}

mecanim::animation::ControllerConstant*	AnimatorController::GetAsset()
{
#if UNITY_EDITOR
	if (m_Controller == 0)
		BuildAsset();
#endif

	return m_Controller;
}

UnityEngine::Animation::AnimationSetBindings* AnimatorController::GetAnimationSetBindings()
{
#if UNITY_EDITOR
	if (m_AnimationSetBindings == 0)
		BuildAsset();
#endif

	return m_AnimationSetBindings;
}


void AnimatorController::ClearAsset()
{		
	m_AnimationSetBindings = NULL;
	m_Controller = NULL;
	m_TOS.clear(); 
	m_Allocator.Reset();

	m_AnimationClips.clear();
}

std::string	AnimatorController::StringFromID(unsigned int id) const
{
	TOSVector::const_iterator it = m_TOS.find(id);
	if(it != m_TOS.end())
		return it->second;
	return "";
}


#undef DIRTY_AND_INVALIDATE
