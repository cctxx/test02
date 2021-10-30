#include "BlendTree.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Misc/UserList.h"
#include "Editor/Src/AssetPipeline/AssetImporter.h"


IMPLEMENT_OBJECT_SERIALIZE (BlendTree)
IMPLEMENT_CLASS_HAS_INIT (BlendTree)

#define DIRTY_AND_INVALIDATE_BLEND_TREE() SetDirty();  Motion::NotifyObjectUsers(kDidModifyMotion);

static const float BlendTreeMinTimeScale = 0.01f;

BlendTree::BlendTree(MemLabelId label, ObjectCreationMode mode)
: Super(label,mode), 
 m_BlendParameter("Blend"),
 m_BlendParameterY("Blend"),
 m_BlendType(Simple),
 m_UseAutomaticThresholds(true),
 m_MinThreshold(0.f),
 m_MaxThreshold(1.f)
{
}

BlendTree::~BlendTree ()
{
	NotifyObjectUsers(kDidModifyMotion);
}

void BlendTree::InitializeClass()
{
	RegisterAllowNameConversion (BlendTree::GetClassStringStatic(), "m_BlendEvent", "m_BlendParameter");
	RegisterAllowNameConversion (BlendTree::GetClassStringStatic(), "m_BlendEventY", "m_BlendParameterY");
}

template<class TransferFunction>
void BlendTree::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	TRANSFER(m_Childs);	
	TRANSFER(m_BlendParameter);	
	TRANSFER(m_BlendParameterY);
	TRANSFER(m_MinThreshold);
	TRANSFER(m_MaxThreshold);		
	TRANSFER(m_UseAutomaticThresholds);
	transfer.Align();
	TRANSFER(m_BlendType);
}

void BlendTree::AwakeFromLoad(AwakeFromLoadMode mode)
{	
	Motion::NotifyObjectUsers(kDidModifyMotion);
	Super::AwakeFromLoad(mode);
}

void BlendTree::CheckConsistency()
{
	Super::CheckConsistency();

	for(int i = 0 ; i < GetChildCount(); i++)
	{
		if(GetChildTimeScale(i) == 0)
		{
			WarningStringObject ("BlendTree child should not have a speed of 0", this);			
			SetChildTimeScale(i, 0.01f);
		}
	}
}


const UnityStr& BlendTree::GetBlendParameter() const
{ 
	return m_BlendParameter; 
}

const UnityStr& BlendTree::GetBlendParameterY()  const
{ 
	return m_BlendParameterY; 
}

void BlendTree::SetBlendParameter(const std::string& blendParameter)
{
	if(m_BlendParameter != blendParameter)
	{
		m_BlendParameter = blendParameter;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}

}

void BlendTree::SetBlendParameterY(const std::string& blendParameterY)
{
	if(m_BlendParameterY != blendParameterY)
	{
		m_BlendParameterY = blendParameterY;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
	
}

int BlendTree::AddMotion(Motion* motion)
{
	Child newChild;
	newChild.m_Motion = motion;
	newChild.m_IsAnim = true;
	newChild.m_Threshold = 0.0f;
	newChild.m_Position = Vector2f::zero;

	m_Childs.push_back(newChild);
	DIRTY_AND_INVALIDATE_BLEND_TREE();
	return m_Childs.size() - 1;
}

void BlendTree::SetMotion(int index, Motion* motion)
{
	if(!ValidateChildIndex(index))
		return;

	if(m_Childs[index].m_Motion != PPtr<Object>(motion))
	{
		m_Childs[index].m_Motion = motion;	
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

Motion* BlendTree::GetMotion(int index) const
{
	if(!ValidateChildIndex(index))
		return 0;

	return m_Childs[index].m_Motion;	
}

BlendTree* BlendTree::AddNewBlendTree()
{
	BlendTree* blendTree = CreateObjectFromCode<BlendTree>();					
	blendTree->SetHideFlags( this->TestHideFlag(kDontSave) ? kHideInHierarchy | kHideInspector | kDontSave :  kHideInHierarchy | kHideInspector);
	if(IsPersistent()) 
		AddAssetToSameFile(*blendTree, *this, true);			

	AddMotion(blendTree);

	return blendTree;
}


int BlendTree::GetChildCount() const 
{
	return m_Childs.size();
}

void BlendTree::SetBlendType(BlendTreeType type)
{
	m_BlendType = type;
}

BlendTreeType BlendTree::GetBlendType() const
{
	return (BlendTreeType)m_BlendType;
}

float BlendTree::GetChildTreshold(int index)
{
	if(!ValidateChildIndex(index))
		return 0;

	if(m_UseAutomaticThresholds)
		ComputeAutomaticThresholds();

	return m_Childs[index].m_Threshold;
}

Vector2f BlendTree::GetChildPosition(int index)
{
	if(!ValidateChildIndex(index))
		return Vector2f::zero;
	return m_Childs[index].m_Position;
}

static bool CompareChildThresholds (const BlendTree::Child& a, const BlendTree::Child& b) 
{
	return a.m_Threshold < b.m_Threshold;
}

void BlendTree::SortChildren()
{
	if (m_BlendType == 0)
	{
		sort(m_Childs.begin(), m_Childs.end(), CompareChildThresholds);
		SetDirty();
	}
}

void BlendTree::SetChildTreshold(int index, float value)
{
	if(!ValidateChildIndex(index))
		return;

	if(m_Childs[index].m_Threshold != value)
	{
		m_Childs[index].m_Threshold = value;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

void BlendTree::SetChildPosition(int index, Vector2f value)
{
	if(!ValidateChildIndex(index))
		return;
	
	if(m_Childs[index].m_Position != value)
	{
		m_Childs[index].m_Position = value;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

float BlendTree::GetChildTimeScale(int index)
{
	if(!ValidateChildIndex(index))
		return 1;

	return m_Childs[index].m_TimeScale;
}

void BlendTree::SetChildTimeScale(int index, float value)
{
	if(!ValidateChildIndex(index))
		return;

	if(m_Childs[index].m_TimeScale != value)
	{
		m_Childs[index].m_TimeScale = value;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

bool BlendTree::GetChildMirror(int index)
{
	if(!ValidateChildIndex(index)) return false;

	return m_Childs[index].m_Mirror;
}

float BlendTree::GetChildCycleOffset(int index)
{
	if(!ValidateChildIndex(index)) return 0;
	
	return m_Childs[index].m_CycleOffset;
}

void BlendTree::RemoveAtIndex(int index)
{
	if(!ValidateChildIndex(index))
		return;

	m_Childs.erase(m_Childs.begin() + index);
	DIRTY_AND_INVALIDATE_BLEND_TREE();
}

int BlendTree::GetRecursiveBlendParameterCount() 
{	
	BlendParameterList parameterList;
	GetParameterList(parameterList);

	return parameterList.size();	
}


std::string BlendTree::GetRecursiveBlendParameter(int index)
{	
	BlendParameterList parameterList;
	GetParameterList(parameterList);

	if(index >= 0 && index < parameterList.size())
	{
		return parameterList[index];
	}
	return "";
		
}


bool BlendTree::GetRecursiveBlendParameterMin(std::string blendParameter, float& currentMin)
{
	bool ret = false;
	float min = currentMin;
	if(GetChildCount() > 0)
	{
		if(m_BlendType == 0)
		{
			if(GetBlendParameter() == blendParameter)
				min = GetChildTreshold(0);
		}
		else
		{
			if(GetBlendParameter() == blendParameter)
				min = GetBlendParameterMin(0);
			if(GetBlendParameterY() == blendParameter)
				min = GetBlendParameterMin(1);
		}
	}
	
	if(min < currentMin)
	{
		currentMin = min;
		ret = true;
	}

	for(int i = 0 ; i < GetChildCount() ; i++)
	{				
		BlendTree* child = dynamic_pptr_cast<BlendTree*>(GetMotion(i));
		if(child)
			ret |= child->GetRecursiveBlendParameterMin(blendParameter,currentMin);
	}
	return ret;		
}

bool BlendTree::GetRecursiveBlendParameterMax(std::string blendParameter, float& currentMax)
{
	bool ret = false;
	float max = currentMax;
	if(GetChildCount() > 0)
	{
		if(m_BlendType == 0)
		{
			if(GetBlendParameter() == blendParameter)
				max = GetChildTreshold(GetChildCount() - 1);
		}
		else
		{
			if(GetBlendParameter() == blendParameter)
				max = GetBlendParameterMax(0);
			if(GetBlendParameterY() == blendParameter)
				max = GetBlendParameterMax(1);
		}
	
	}
	if(max > currentMax)
		{
		currentMax = max;
			ret = true;
		}

	for(int i = 0 ; i < GetChildCount(); i++)
	{		
		BlendTree* child = dynamic_pptr_cast<BlendTree*>(GetMotion(i));		
		if(child)
			ret |= child->GetRecursiveBlendParameterMax(blendParameter,currentMax);
	}
	return ret;		
}

float BlendTree::GetBlendParameterMin (int coord)
{
	float min = 0;
	for(int i = 0 ; i < GetChildCount(); i++)
	{
		if (m_Childs[i].m_Position[coord] < min)
			min = m_Childs[i].m_Position[coord];
	}
	return min;
}

float BlendTree::GetBlendParameterMax (int coord)
{
	float max = 0;
	for(int i = 0 ; i < GetChildCount(); i++)
	{
		if (m_Childs[i].m_Position[coord] > max)
			max = m_Childs[i].m_Position[coord];
	}
	return max;
}

bool BlendTree::IsBlendParameterIndexValid(int index)
{	
	return index >= 0 && index < GetRecursiveBlendParameterCount();
}

float BlendTree::GetRecursiveBlendParameterMin(int index)
{	
	if(!IsBlendParameterIndexValid(index))
		return 0;

	std::string blendParameter = GetRecursiveBlendParameter(index);
	float currentMin = 1e6;
	if(GetRecursiveBlendParameterMin(blendParameter,currentMin))
		return currentMin;
	else
		return 0;
	
}

float BlendTree::GetRecursiveBlendParameterMax(int index)
{	
	if(!IsBlendParameterIndexValid(index))
		return 1;
	

	std::string blendParameter = GetRecursiveBlendParameter(index);
	float currentMax = -1e6;
	if(GetRecursiveBlendParameterMax(blendParameter,currentMax))
		return currentMax;
	else
		return 1;
}


void BlendTree::SetInputBlendValue(std::string blendValueName, float value)
{	
	UpdateInputBlendValues();
	for(int i = 0; i < m_InputBlendValues.size() ; i++)
	{
		if(m_InputBlendValues[i].first == blendValueName)
		{
			m_InputBlendValues[i].second = value;
		}
	}	
}
 
float BlendTree::GetInputBlendValue(std::string blendValueName)
{
	for(int i = 0; i < m_InputBlendValues.size() ; i++)
	{
		if(m_InputBlendValues[i].first == blendValueName)
			return m_InputBlendValues[i].second; 
	}
	return 0;	
}

bool BlendTree::GetUseAutomaticThresholds()
{
	return m_UseAutomaticThresholds;
}

void BlendTree::SetUseAutomaticThresholds(bool val)
{
	if(m_UseAutomaticThresholds != val)
	{
		ComputeAutomaticThresholds();
		m_UseAutomaticThresholds = val;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

void BlendTree::SetMinThreshold(float min)
{	
	if(m_MinThreshold != min)
	{
		ComputeAutomaticThresholds();
		m_MinThreshold = min;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

void BlendTree::SetMaxThreshold(float max)
{
	if(m_MaxThreshold != max)
	{
		ComputeAutomaticThresholds();
		m_MaxThreshold = max;
		DIRTY_AND_INVALIDATE_BLEND_TREE();
	}
}

float BlendTree::GetMinThreshold()
{
	return m_MinThreshold;
}

float BlendTree::GetMaxThreshold()
{
	return m_MaxThreshold;
}

void BlendTree::ComputeAutomaticThresholds()
{	
	for(int i = 0 ; i < GetChildCount() ; i++)
	{
		if(GetChildCount()>1)
			m_Childs[i].m_Threshold = m_MinThreshold + (( float(i)/float(GetChildCount()-1.f)) * (m_MaxThreshold - m_MinThreshold));
		else
			m_Childs[i].m_Threshold = 0.0f;			
	}
}

MotionVector BlendTree::GetChildsRecursive()
{
	MotionVector childs;

	childs.push_back(this);
	for(int i = 0 ; i < GetChildCount(); i++)
	{
		Motion* motion = GetMotion(i);		
		BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);		

		if(tree)
		{			
			MotionVector treeChilds = tree->GetChildsRecursive();
			childs.insert(childs.end(), treeChilds.begin(), treeChilds.end());		
		}		
		else if(motion)
		{
			childs.push_back(motion);
		}
	}		

	return childs;

}

AnimationClipVector BlendTree::GetAnimationClips()const
{
	AnimationClipVector clips;
	
	for(int i = 0 ; i < GetChildCount(); i++)
	{
		Motion* motion = GetMotion(i);		
		BlendTree* tree  = dynamic_pptr_cast<BlendTree*>(motion);
		AnimationClip* clip = dynamic_pptr_cast<AnimationClip*>(motion);
					
		if(tree)
		{			
			AnimationClipVector treeClips = tree->GetAnimationClips();
			clips.insert(clips.end(), treeClips.begin(), treeClips.end());		
		}
		else if(clip)
		{
			clips.push_back(clip);
		}		
	}		
	
	return clips;
}

float BlendTree::GetAverageDuration()
{
	AnimationClipVector clips = GetAnimationClips();
	
	float count = 0;
	float sum = 0;

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it)
		{
			sum += (*it)->GetAverageDuration();
			count+= 1;
		}		
	}

	return count > 0 ? sum/count : 0;
}

float BlendTree::GetAverageAngularSpeed()
{
	AnimationClipVector clips = GetAnimationClips();
	
	float count = 0;
	float sum = 0;

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it && (*it)->IsAnimatorMotion())
		{
			sum += (*it)->GetAverageAngularSpeed();
			count+= 1;
		}		
	}

	return count > 0 ? sum/count : 0;
}

Vector3f BlendTree::GetAverageSpeed()
{
	AnimationClipVector clips = GetAnimationClips();
	
	float count = 0;
	Vector3f sum = Vector3f::zero;

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it && (*it)->IsAnimatorMotion())
		{
			sum += (*it)->GetAverageSpeed();
			count += 1;
		}		
	}

	return count > 0 ? sum/count : Vector3f::zero;
}

float BlendTree::GetApparentSpeed()
{
	AnimationClipVector clips = GetAnimationClips();
	
	float count = 0;
	float sum = 0;

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it && (*it)->IsAnimatorMotion())
		{
			sum += (*it)->GetApparentSpeed();
			count+= 1;
		}		
	}

	return count > 0 ? sum/count : 0;
}

bool BlendTree::ValidateIfRetargetable(bool showWarning)
{
	return true; // always retargetable
}

bool BlendTree::IsLooping()
{
	// all clips must be looping
	AnimationClipVector clips = GetAnimationClips();

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it)
		{
			if(!(*it)->IsLooping())
				return false;			
		}		
	}

	return true;
}

bool BlendTree::IsAnimatorMotion()const
{
	// all clips must be animator
	AnimationClipVector clips = GetAnimationClips();

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it)
		{
			if(!(*it)->IsAnimatorMotion())
				return false;			
		}		
	}

	return true;

}

bool BlendTree::IsHumanMotion()
{
	// all clips must be human
	AnimationClipVector clips = GetAnimationClips();

	for(AnimationClipVector::iterator it = clips.begin(); it < clips.end() ; it++)
	{				
		if(*it)
		{
			if(!(*it)->IsHumanMotion())
				return false;			
		}		
	}
	return true;
}

void BlendTree::RenameParameter(const std::string& newName, const std::string& oldName)
{
	if(GetBlendParameter() == oldName)
	{
		SetBlendParameter(newName == "" ? "Blend" :  newName);
	}
	if(GetBlendType() != 0 && GetBlendParameterY() == oldName) //
	{
		SetBlendParameterY(newName == "" ? "Blend" :  newName);
	}

	for( int i = 0 ; i < GetChildCount() ; i++)
	{
		BlendTree* tree = dynamic_pptr_cast<BlendTree*>(GetMotion(i));
		if (tree)
		{
			tree->RenameParameter(newName,oldName);
		}		
	}

}


void BlendTree::GetParameterList(BlendParameterList& parameterList)
{	
	if(std::find(parameterList.begin(), parameterList.end(), m_BlendParameter) == parameterList.end())
	{
		parameterList.push_back(m_BlendParameter);
	}
	if(m_BlendType > 0 && std::find(parameterList.begin(), parameterList.end(), m_BlendParameterY) == parameterList.end())
	{
		parameterList.push_back(m_BlendParameterY);
	}
	for( int i = 0 ; i < GetChildCount() ; i++)
	{
		BlendTree* tree = dynamic_pptr_cast<BlendTree*>(GetMotion(i));

		if (tree)
		{
			tree->GetParameterList(parameterList);
		}		
	}
}

void BlendTree::UpdateInputBlendValues()
{
	if(m_InputBlendValues.size() != GetRecursiveBlendParameterCount())
	{
		m_InputBlendValues.resize(GetRecursiveBlendParameterCount());
	}

	for(int i = 0 ; i < m_InputBlendValues.size(); i++)
	{
		m_InputBlendValues[i].first = GetRecursiveBlendParameter(i);
	}
}


void BlendTree::BuildChildRuntimeAssetRecursive( std::vector<mecanim::animation::BlendTreeNodeConstant*> &treeNodes, int& childIndex, UserList& dependencies, TOSVector& tos, mecanim::memory::Allocator& alloc)
{
	dependencies.AddUser(GetUserList());

	childIndex++;

	std::vector<mecanim::uint32_t> childIndicesArray;
	std::vector<float> childThresholds;	
	std::vector<Vector2f> childPositions;

	for(int i = 0 ; i < GetChildCount(); i++)
	{				
		if(GetMotion(i))
		{
			childIndicesArray.push_back(0);
			
			// Threhold
			childThresholds.push_back(GetChildTreshold(i));	
			
			// Position
			childPositions.push_back(GetChildPosition(i));
		}
	}

	mecanim::uint32_t blenderID = ProccessString(tos, GetBlendParameter());
	mecanim::uint32_t blenderYID = ProccessString(tos, GetBlendParameterY());
	mecanim::uint32_t* childIndices  =  childIndicesArray.size() > 0 ?  &childIndicesArray[0] : 0;
	
	mecanim::animation::BlendTreeNodeConstant* treeConstant;
	if (m_BlendType == 0)
	{
	float* blendTreshold  =  childThresholds.size() > 0 ?  &childThresholds[0] : 0;
		treeConstant = mecanim::animation::CreateBlendTreeNodeConstant(blenderID, childIndicesArray.size(), childIndices, blendTreshold, alloc);
	}
	else
	{
		Vector2f* blendPositions = childPositions.size() > 0 ? &childPositions[0] : 0;
		treeConstant = mecanim::animation::CreateBlendTreeNodeConstant(blenderID, blenderYID, m_BlendType, childIndicesArray.size(), childIndices, blendPositions, alloc);
	}

	treeNodes.push_back(treeConstant);

	int currentNodeIndex = 0;
	
	for(int i = 0 ; i < GetChildCount(); i++)
	{				
		Motion* child = GetMotion(i);
		BlendTree* tree = dynamic_pptr_cast<BlendTree*>(child);
		AnimationClip* clip = dynamic_pptr_cast<AnimationClip*>(child);

		if(tree || clip)
		{	
			treeConstant->m_ChildIndices[currentNodeIndex] = childIndex; // set correct index
			if(clip)
			{
				mecanim::uint32_t clipID = ProccessString(tos, clip->GetName());				
				treeNodes.push_back(mecanim::animation::CreateBlendTreeNodeConstant(clipID, 1.0f /  GetChildTimeScale(i), GetChildMirror(i), GetChildCycleOffset(i),alloc));					
				childIndex++;			
			}

			else 
			{
				tree->BuildChildRuntimeAssetRecursive(treeNodes, childIndex, dependencies, tos, alloc);
			}												
			currentNodeIndex++;
		}	
	}
}

mecanim::animation::BlendTreeConstant* BlendTree::BuildRuntimeAsset(UserList& dependencies, TOSVector& tos, mecanim::memory::Allocator& alloc)
{
	int childIndex = 0;
	std::vector<mecanim::animation::BlendTreeNodeConstant*> treeNodes;

	BuildChildRuntimeAssetRecursive(treeNodes, childIndex, dependencies, tos, alloc);	

	if(treeNodes.size())
		return mecanim::animation::CreateBlendTreeConstant(&treeNodes[0], treeNodes.size(), alloc);
	else
		return 0;
}

bool BlendTree::ValidateChildIndex(int index) const 
{
	if(index >= 0 && index < GetChildCount())
	{
		return true;	
	}

	ErrorString("Invalid Child Index");
	return false;	

}

