#ifndef NAMETOOBJECTMAP_H
#define NAMETOOBJECTMAP_H

#include <string>
#include <map>
#include "Runtime/BaseClasses/BaseObject.h"
#if UNITY_EDITOR
#include "Editor/Src/BuildPipeline/BuildSerialization.h"
#endif

using std::map;
using std::string;
using std::pair;


static inline bool IsBuiltinResourceObject(Object* o)
{
	// built-in resources have this flag set
	if (o->TestHideFlag(Object::kHideAndDontSave))
		return true;

	// Resources from builtin_extra only have "not editable" flag set, so the above check doesn't catch them.
	// In the editor (where this matters mostly when building resource files), catch that by detecting
	// if the object came from any built-in resources file.
	#if UNITY_EDITOR
	if (IsAnyDefaultResourcesObject(o->GetInstanceID()))
		return true;
	#endif

	return false;
}

template<class Type, class ObjectToName, class NameToObject>
class NameToObjectMap
{
private:
	typedef typename ObjectToName::iterator ObjectToNameIterator;
	typedef typename NameToObject::iterator NameToObjectIterator;
	
	ObjectToName m_ObjectToName;
	NameToObject m_NameToObject;
	Object* m_ObjectToDirty;

public:

	DECLARE_SERIALIZE (NameToObjectMap)
	
	void SetObjectToDirty (Object* dirty) { m_ObjectToDirty = dirty; }

	void Add (const string& name, PPtr<Type> o)
	{
		Remove (o);
		m_ObjectToName.insert (make_pair (o, name));
		m_NameToObject.insert (make_pair (name, o));
		AssertIf (m_NameToObject.size () != m_ObjectToName.size ());
		m_ObjectToDirty->SetDirty ();
	}

	bool Remove (PPtr<Type> object)
	{
		AssertIf (m_NameToObject.size () != m_ObjectToName.size ());
		int oldSize = m_NameToObject.size ();
		{
			pair<NameToObjectIterator, NameToObjectIterator> range = make_pair(m_NameToObject.begin(), m_NameToObject.end());

			NameToObjectIterator i, next;
			for (i=range.first;i!=range.second;i=next)
			{
				next = i; next++;
				if (i->second == object)
				{
					m_NameToObject.erase (i);
				}
			}
		}

		{
			pair<ObjectToNameIterator, ObjectToNameIterator> range;
			range = m_ObjectToName.equal_range (object);
			m_ObjectToName.erase(range.first, range.second);
		}
		
		m_ObjectToDirty->SetDirty ();
		AssertIf (m_NameToObject.size () != m_ObjectToName.size ());
		return oldSize != m_NameToObject.size ();
	}

	Type* Find (const string& name)
	{
		// Get all with name 'name'
		pair<NameToObjectIterator, NameToObjectIterator> range;
		range = m_NameToObject.equal_range (name);
		NameToObjectIterator i, next;
		Type* found = NULL;
		// Then find the first that is loaded, those that can't be loaded
		// are removed.
		for (i=range.first;i!=range.second;i=next)
		{
			next = i; next++;
			Type* o = i->second;
			
			if (o)
			{
				// When there are two shaders one builtin resource and one normal shader
				// Then we want the one in the project folder not the builtin one. So people can override shaders
				// At some point we should try to get the ordering of shader includes better defined!
				if (found && IsBuiltinResourceObject(o))
					continue;
				
				found = o;
			}			
		}
		
		return found;
	}

	std::vector<PPtr<Type> > GetAllObjects ()
	{
		std::vector<PPtr<Type> > objects;
		for (NameToObjectIterator i=m_NameToObject.begin ();i!=m_NameToObject.end ();i++)
		{
			objects.push_back(i->second);
		}
		return objects;
	}

	const NameToObject& GetAll ()
	{
		return m_NameToObject;
	}

private:

	void Rebuild ()
	{
		// Rebuild name -> object
		m_NameToObject.clear ();
		ObjectToNameIterator i;
		for (i=m_ObjectToName.begin ();i != m_ObjectToName.end ();i++)
			m_NameToObject.insert (make_pair (i->second, i->first));
	}
};

template<class Type, class ObjectToName, class NameToObject>
template<class TransferFunction>
void NameToObjectMap<Type, ObjectToName, NameToObject>::Transfer (TransferFunction& transfer)
{
	TRANSFER (m_ObjectToName);
	if (transfer.IsReading ())
		Rebuild ();
}

#endif
