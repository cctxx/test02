#ifndef BLENDTREE_H
#define BLENDTREE_H

#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Animation/MecanimUtility.h"
#include "Runtime/mecanim/animation/blendtree.h"

typedef std::vector<std::pair<UnityStr,float> > InputBlendValues;
typedef std::vector<UnityStr> BlendParameterList;

typedef std::vector<PPtr<Motion> > MotionVector;

class UserList;

enum BlendTreeType
{
	Simple = 0 ,
	SimpleDirectionnal2D,
	FreeformDirectionnal2D,
	FreeformCartesian2D
};

class BlendTree : public Motion
{
public:

	REGISTER_DERIVED_CLASS (BlendTree, Motion)
	DECLARE_OBJECT_SERIALIZE (BlendTree)

	BlendTree (MemLabelId label, ObjectCreationMode mode);

	static void InitializeClass ();
	static void CleanupClass () {}

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);	
	virtual void CheckConsistency();	

	mecanim::animation::BlendTreeConstant* BuildRuntimeAsset(UserList& userList, TOSVector& tos, mecanim::memory::Allocator& alloc);
	void BuildChildRuntimeAssetRecursive( std::vector<mecanim::animation::BlendTreeNodeConstant*> &treeNodes, int& childIndex, UserList& dependencies, TOSVector& tos, mecanim::memory::Allocator& alloc);
	
	struct Child
	{
		Child() : m_TimeScale(1), m_CycleOffset(0), m_Mirror(false) {}		
		PPtr<Motion>	m_Motion;
		float			m_Threshold;
		Vector2f		m_Position;
		float			m_TimeScale;
		float			m_CycleOffset;
		bool			m_IsAnim;		
		bool			m_Mirror;

		DEFINE_GET_TYPESTRING (Child)

		template<class TransferFunction>
		void Transfer (TransferFunction& transfer)
		{			
			transfer.Transfer(m_Motion, "m_Motion", kStrongPPtrMask);
			TRANSFER(m_Threshold);
			TRANSFER(m_Position);
			TRANSFER(m_TimeScale);
			TRANSFER(m_CycleOffset);
			TRANSFER(m_IsAnim);
			TRANSFER(m_Mirror);
			transfer.Align ();
		}
	};

	typedef std::vector< Child > ChildVector;
			
	void SetBlendType(BlendTreeType type);
	BlendTreeType GetBlendType() const;
			
	const UnityStr& GetBlendParameter() const ;
	const UnityStr& GetBlendParameterY() const ;
	void SetBlendParameter(const std::string& blendParameter);	
	void SetBlendParameterY(const std::string& blendParameterY);
	
	int AddMotion(Motion* motion);
	void SetMotion(int index, Motion* motion);
	BlendTree* AddNewBlendTree();
	Motion* GetMotion(int index) const;
	
	int GetChildCount() const;	
	
	float GetChildTreshold(int index);
	Vector2f GetChildPosition(int index);
	void SetChildTreshold(int index, float value);
	void SetChildPosition(int index, Vector2f value);
	void SortChildren();

	float GetChildTimeScale(int index);
	void SetChildTimeScale(int index, float value);

	bool GetChildMirror(int index);
	float GetChildCycleOffset(int index);

	void RemoveAtIndex(int index);

	int GetRecursiveBlendParameterCount() ;
	std::string GetRecursiveBlendParameter(int index);
	float GetRecursiveBlendParameterMin(int index);
	float GetRecursiveBlendParameterMax(int index);
	float GetBlendParameterMin(int coord);
	float GetBlendParameterMax(int coord);


	void SetInputBlendValue(std::string blendValueName, float value);
	float GetInputBlendValue(std::string blendValueName);

	bool GetUseAutomaticThresholds();
	void SetUseAutomaticThresholds(bool val);
	void SetMinThreshold(float min);	     
	void SetMaxThreshold(float max);
	float GetMinThreshold();
	float GetMaxThreshold();
	
		
	MotionVector GetChildsRecursive();
	virtual float GetAverageDuration();
	virtual float GetAverageAngularSpeed();
	virtual Vector3f GetAverageSpeed();
	virtual float GetApparentSpeed();
	virtual bool ValidateIfRetargetable(bool showWarning = true);
	virtual bool IsLooping();

	virtual bool IsAnimatorMotion()const;
	virtual bool IsHumanMotion();

	void RenameParameter(const std::string& newName, const std::string& oldName);

	void GetParameterList(BlendParameterList& parameterList);
	
	virtual AnimationClipVector GetAnimationClips()const;	
	
private:		
		
	InputBlendValues		m_InputBlendValues;	
	ChildVector				m_Childs;
	
	UnityStr				m_BlendParameter;
	UnityStr				m_BlendParameterY;
	float					m_MinThreshold;
	float					m_MaxThreshold;	
	bool					m_UseAutomaticThresholds;

	int						m_BlendType; ///< enum { 1D = 0, 2D Simple Directional = 1, 2D Freeform Directional = 2, 2D Freeform Cartesian = 3 } Blend Node type.
					
	void UpdateInputBlendValues();
	void ComputeAutomaticThresholds();

	bool IsBlendParameterIndexValid(int index);
	bool GetRecursiveBlendParameterMin(std::string blendParameter, float& currentMin);
	bool GetRecursiveBlendParameterMax(std::string blendParameter, float& currentMax);
	
	
	bool ValidateChildIndex(int index) const ;

};


typedef std::vector<PPtr<BlendTree> > BlendTreeVector;

#endif
