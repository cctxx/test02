#ifndef NAMEDOBJECT_H
#define NAMEDOBJECT_H

#include "EditorExtension.h"
#include "Runtime/Containers/ConstantString.h"

class EXPORT_COREMODULE NamedObject : public EditorExtension
{
	public:
	
	virtual char const* GetName () const { return m_Name.c_str (); }
	virtual void SetName (char const* name);

	REGISTER_DERIVED_ABSTRACT_CLASS (NamedObject, EditorExtension)
	DECLARE_OBJECT_SERIALIZE (NamedObject)

	NamedObject (MemLabelId label, ObjectCreationMode mode);
	protected:
	
	ConstantString m_Name;
};

#endif
