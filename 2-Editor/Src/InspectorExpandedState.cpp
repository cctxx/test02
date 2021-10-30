#include "UnityPrefix.h"
#include "InspectorExpandedState.h"
#include "Editor/Src/AnnotationManager.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

static InspectorExpandedState* gSingleton = NULL;

namespace 
{
	std::string GetScriptClass (Object *obj)
	{
		if (MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (obj))
			return behaviour->GetScriptClassName();
		return "";
	}
}

InspectorExpandedState::InspectorExpandedState (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	AssertIf(gSingleton != NULL);
	gSingleton = this;
}

InspectorExpandedState::~InspectorExpandedState ()
{
	AssertIf(gSingleton != this);
	gSingleton = NULL;
}



InspectorExpandedState::ExpandedData* InspectorExpandedState::GetExpandedData (Object* obj)
{
	if (obj == NULL)
		return NULL;

	return GetExpandedData (obj->GetClassID(), GetScriptClass(obj));
}
InspectorExpandedState::ExpandedData* InspectorExpandedState::GetExpandedData (int classID, const std::string& scriptClass)
{
	for (ExpandedDatas::iterator i=m_ExpandedData.begin();i != m_ExpandedData.end();i++)
	{
		if (i->m_ClassID != classID || i->m_ScriptClass != scriptClass)
			continue;	
			
		return &(*i);
	}
	return NULL;
}


InspectorExpandedState::ExpandedData* InspectorExpandedState::CreateExpandedData (Object* obj)
{
	if (obj == NULL)
		return NULL;

	return CreateExpandedData (obj->GetClassID(), GetScriptClass(obj));
}
InspectorExpandedState::ExpandedData* InspectorExpandedState::CreateExpandedData (int classID, const std::string& scriptClass)
{
	InspectorExpandedState::ExpandedData data;
	data.m_ClassID = classID;
	data.m_InspectorExpanded = classID;
	data.m_ScriptClass = scriptClass;

	m_ExpandedData.push_back(data);
	return &m_ExpandedData.back();
}


bool InspectorExpandedState::IsInspectorExpanded (Object* obj)
{
	if (obj == NULL)
		return false;

	return IsInspectorExpanded (obj->GetClassID(), GetScriptClass(obj));
}
bool InspectorExpandedState::IsInspectorExpanded (int classID, const std::string& scriptClass)
{
	InspectorExpandedState::ExpandedData* data = GetInspectorExpandedState().GetExpandedData (classID, scriptClass);
	if (data != NULL)
		return data->m_InspectorExpanded;
	else
		return true;
}



void InspectorExpandedState::SetInspectorExpanded (Object* obj, bool inspectorExpanded)
{
	if (obj == NULL)
		return;

	SetInspectorExpanded (obj->GetClassID(), GetScriptClass(obj), inspectorExpanded);
}
void InspectorExpandedState::SetInspectorExpanded (int classID, const std::string& scriptClass, bool inspectorExpanded)
{

	ExpandedData* data = GetExpandedData(classID, scriptClass);
	if (data == NULL)
	{
		// No need to create data, we will only set it to the default value
		if (inspectorExpanded)
			return;

		data = CreateExpandedData(classID, scriptClass);
		if (data == NULL)
			return;
	}
	
	if (data->m_InspectorExpanded != inspectorExpanded)
	{
		data->m_InspectorExpanded = inspectorExpanded;
		SetDirty();	
		GetAnnotationManager().InspectorExpandedStateWasChanged (data->m_ClassID, data->m_ScriptClass, inspectorExpanded);
	}
}

void InspectorExpandedState::SetExpandedData (Object* obj, const std::vector<UnityStr>& properties)
{
	ExpandedData* data = GetExpandedData(obj);
	if (data == NULL)
	{
		// No need to create data, we will only set it to the default value
		if (properties.empty())
			return;

		data = CreateExpandedData(obj);
		if (data == NULL)
			return;
	}

	if (data->m_ExpandedProperties != properties)
	{
		data->m_ExpandedProperties = properties;
		SetDirty();		
	}
}

template<class T>
void InspectorExpandedState::ExpandedData::Transfer (T& transfer)
{
	TRANSFER(m_InspectorExpanded);
	transfer.Align();
	TRANSFER(m_ClassID);
	TRANSFER(m_ScriptClass);
	TRANSFER(m_ExpandedProperties);
}

template<class T>
void InspectorExpandedState::Transfer (T& transfer)
{
	Super::Transfer(transfer);
	TRANSFER(m_ExpandedData);
}

InspectorExpandedState& GetInspectorExpandedState ()
{
	AssertIf(gSingleton == NULL);
	return *gSingleton;
}

IMPLEMENT_CLASS(InspectorExpandedState)
IMPLEMENT_OBJECT_SERIALIZE(InspectorExpandedState)
