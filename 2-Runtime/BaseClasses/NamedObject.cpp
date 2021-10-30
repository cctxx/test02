#include "UnityPrefix.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "NamedObject.h"
#include "Runtime/Containers/ConstantStringSerialization.h"

NamedObject::NamedObject (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}	

NamedObject::~NamedObject ()
{
}

template<class TransferFunction>
void NamedObject::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	TransferConstantString(m_Name, "m_Name", kHideInEditorMask, GetMemoryLabel(), transfer);
}

void NamedObject::SetName (char const* name)
{
	if (strcmp (m_Name.c_str (), name) != 0)
	{
		m_Name.assign (name, GetMemoryLabel());
		SetDirty ();
	}
}

IMPLEMENT_CLASS (NamedObject)
IMPLEMENT_OBJECT_SERIALIZE (NamedObject)
INSTANTIATE_TEMPLATE_TRANSFER_EXPORTED(NamedObject)