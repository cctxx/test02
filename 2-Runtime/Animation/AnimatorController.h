
#ifndef EDITABLEAVATARCONTROLLER_H
#define EDITABLEAVATARCONTROLLER_H

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/BaseClasses/MessageIdentifier.h"
#include "Runtime/BaseClasses/RefCounted.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/Animation/RuntimeAnimatorController.h"
#include "Runtime/Misc/UserList.h"


#if UNITY_EDITOR
#include "Editor/Src/Animation/AnimatorControllerParameter.h"
#include "Editor/Src/Animation/AnimatorControllerLayer.h"

typedef std::vector<AnimatorControllerLayer>			AnimatorControllerLayerVector;	
typedef std::vector<AnimatorControllerParameter>		AnimatorControllerParameterVector;

#endif 

template<class T>
class PPtr;
class AnimationClip;
class StateMachine;
class AvatarMask;

class AnimatorController : public RuntimeAnimatorController
{

public :
	REGISTER_DERIVED_CLASS (AnimatorController, RuntimeAnimatorController)
	DECLARE_OBJECT_SERIALIZE (AnimatorController)

	AnimatorController (MemLabelId label, ObjectCreationMode mode);	

	static void InitializeClass ();

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void CheckConsistency ();		

#if UNITY_EDITOR		
	
	//////////////////////////////////////////
	// AnimatorControllerLayers
	AnimatorControllerLayer*			GetLayer(int index);
	const AnimatorControllerLayer*		GetLayer(int index)const ;
	int									GetLayerCount()const;
	void								AddLayer(const std::string& name);
	void								RemoveLayer(int index);

	//////////////////////////////////////////
	// AnimatorControllerParameter
	AnimatorControllerParameter*		GetParameter(int index) ;
	const AnimatorControllerParameter*	GetParameter(int index) const;	
	int									GetParameterCount() const ;
	void								AddParameter(const std::string& name, AnimatorControllerParameterType type);
	void								RemoveParameter(int index);
	int									FindParameter(const std::string& name) const;


	std::vector<PPtr<Object> >			CollectObjectsUsingParameter(const string& parameterName);
	/////////////////////////////////////////////
	
	string MakeUniqueParameterName(const string& newName) const;
	string MakeUniqueLayerName(const string& newName) const;

	bool ValidateLayerIndex(int index) const;
	bool ValidateParameterIndex(int index) const;

	
private:

	bool ValidAnimationSet();	
	void BuildAsset();	
		
	
	AnimatorControllerLayerVector			m_AnimatorLayers;	
	AnimatorControllerParameterVector		m_AnimatorParameters;	
	UserList                                m_Dependencies;


	template<class T>
	void ParametersAndLayersBackwardsCompatibility (T& transfer);

	AnimatorControllerLayer*		CreateAnimatorControllerLayer();
	AnimatorControllerParameter*	CreateAnimatorControllerParameter();

#endif // UNITY_EDITOR

public:

	virtual UnityEngine::Animation::AnimationSetBindings* GetAnimationSetBindings();
	virtual AnimationClipVector GetAnimationClips() const ;
	virtual mecanim::animation::ControllerConstant*	GetAsset();


	virtual std::string	StringFromID(unsigned int ID) const;		
	void OnInvalidateAnimatorController();
	bool IsAssetBundled() { return m_IsAssetBundled; }

private :

	void ClearAsset();
	void OnAnimationClipDeleted();
	virtual AnimationClipVector GetAnimationClipsToRegister() const;
		

	AnimationClipVector									m_AnimationClips;		
	bool												m_IsAssetBundled;

	mecanim::memory::ChainedAllocator					m_Allocator;
	mecanim::animation::ControllerConstant*				m_Controller;
	UInt32												m_ControllerSize;
	UnityEngine::Animation::AnimationSetBindings*		m_AnimationSetBindings;	
	TOSVector											m_TOS;
};


#endif //EDITABLEAVATARCONTROLLER_H
