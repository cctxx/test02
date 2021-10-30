#include "HumanTemplate.h"

//HumanBone const* HumanDescription::Find(std::string const& name)const
//{
//	HumanBoneList::const_iterator it = std::find_if(m_Human.begin(), m_Human.end(), FindBoneName(name));
//	if(it!=m_Human.end())
//		return &(*it);
//	return NULL;
//}

HumanTemplate::HumanTemplate(MemLabelId label, ObjectCreationMode mode)
: Super(label, mode)
{

}

HumanTemplate::~HumanTemplate()
{	
}

void HumanTemplate::AwakeFromLoad(AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
}

void HumanTemplate::Reset()
{
	Super::Reset();

	m_BoneTemplate.clear();
}

IMPLEMENT_OBJECT_SERIALIZE (HumanTemplate)
IMPLEMENT_CLASS (HumanTemplate)

template<class TransferFunction>
void HumanTemplate::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);

	transfer.Transfer (m_BoneTemplate, "m_BoneTemplate"/*, kHideInEditorMask*/);
}

void HumanTemplate::Insert(std::string const& name, std::string const& templateName)
{
	// In Motionbuilder and maya object name can have a prefix
	// remove it for template matching
	size_t pos = templateName.find_first_of(':');
	std::string templateNameWithoutPrefix = pos != std::string::npos ? templateName.substr(pos+1) : templateName;

	m_BoneTemplate[name] = templateNameWithoutPrefix;
	SetDirty();
}

std::string HumanTemplate::Find(std::string const& name)const
{
	MapTemplate::const_iterator it = m_BoneTemplate.find(name);
	if(it!=m_BoneTemplate.end())
	{
		return it->second;
	}
	return std::string("");
}

void HumanTemplate::ClearTemplate()
{
	m_BoneTemplate.clear();
	SetDirty();
}

