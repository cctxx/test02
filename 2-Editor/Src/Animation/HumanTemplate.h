#pragma once

#include "Runtime/BaseClasses/NamedObject.h"
#include "Runtime/Graphics/Transform.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

#include "AvatarUtility.h"

#include <map>

class HumanTemplate : public NamedObject
{
public:
	REGISTER_DERIVED_CLASS (HumanTemplate, NamedObject)
	DECLARE_OBJECT_SERIALIZE (HumanTemplate)
	
	static void InitializeClass (){};
	static void CleanupClass () {}
	
	HumanTemplate (MemLabelId label, ObjectCreationMode mode);	

	virtual void AwakeFromLoad(AwakeFromLoadMode mode);
	virtual void Reset();

	void		Insert(std::string const& name, std::string const& templateName);
	std::string Find(std::string const& name)const;
	void		ClearTemplate();

protected:
	typedef std::map<UnityStr, UnityStr>  MapTemplate;
	MapTemplate  m_BoneTemplate;	
};
