#include "UnityPrefix.h"
#include "Runtime/IMGUI/NamedKeyControlList.h"
namespace IMGUI
{
	
void NamedKeyControlList::AddNamedControl (const std::string &name, int id, int windowID)
{
	m_NamedControls[name] = NamedControl (id, windowID);
}

std::string NamedKeyControlList::GetNameOfControl (int id) 
{
	for (std::map<std::string, NamedControl>::const_iterator i = m_NamedControls.begin(); i != m_NamedControls.end(); i++)
		if (i->second.ID == id)
			return i->first;
	return std::string ("");
}

NamedControl* NamedKeyControlList::GetControlNamed (const std::string &name)
{
	std::map<std::string, NamedControl>::iterator i = m_NamedControls.find (name);
	if (i != m_NamedControls.end ())
		return &i->second;
	return NULL;
}

} // namespace