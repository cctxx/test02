#include "UnityPrefix.h"
#include "EditorExtension.h"

#if !GAMERELEASE

#include "Editor/Src/EditorExtensionImpl.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Prefabs/PrefabBackwardsCompatibility.h"

EditorExtension::EditorExtension (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
}

EditorExtension::~EditorExtension ()
{
	Assert(m_DeprecatedExtensionPtr.GetInstanceID() == 0);
}

bool EditorExtension::IsPrefabParent () const
{
	Prefab* prefab = m_Prefab;
	return prefab != NULL && prefab->IsPrefabParent();
}

template<class TransferFunction>
void EditorExtension::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	if (!transfer.IsSerializingForGameRelease ())
	{
		if (SerializePrefabIgnoreProperties(transfer))
		{
			transfer.Transfer (m_PrefabParentObject, "m_PrefabParentObject", kHideInEditorMask | kIgnoreWithInspectorUndoMask);
			transfer.Transfer (m_Prefab, "m_PrefabInternal", kHideInEditorMask | kIgnoreWithInspectorUndoMask);
		}
		
		if (transfer.IsReadingBackwardsCompatible())
			transfer.Transfer (m_DeprecatedExtensionPtr, "m_ExtensionPtr", kHideInEditorMask | kIgnoreWithInspectorUndoMask);
	}
}

PPtr<EditorExtensionImpl> GetDeprecatedExtensionPtrIfExists (const Object& o)
{
	EditorExtension* extension = dynamic_pptr_cast<EditorExtension*> (&o);
	if (extension)
		return extension->m_DeprecatedExtensionPtr;
	else
		return NULL;
}

void EditorExtension::PatchPrefabBackwardsCompatibility ()
{
	if (m_DeprecatedExtensionPtr.IsValid ())
	{
		m_Prefab = m_DeprecatedExtensionPtr->m_DataTemplate;
		if (m_DeprecatedExtensionPtr->m_TemplateFather.IsValid())
			m_PrefabParentObject = m_DeprecatedExtensionPtr->m_TemplateFather;
		
		if (m_Prefab.IsValid() && m_PrefabParentObject.IsValid() && m_DeprecatedExtensionPtr->m_Object == PPtr<EditorExtension> (this))
			ReadOldPrefabFormat (*m_Prefab, *this, *m_PrefabParentObject, *m_DeprecatedExtensionPtr);
		
		///@TODO: DESTROY!!!
		/// DestroyObject(m_DeprecatedExtensionPtr)
	}
	m_DeprecatedExtensionPtr = NULL;
	
}


void EditorExtension::AwakeFromLoad (AwakeFromLoadMode mode)
{
	Super::AwakeFromLoad(mode);
	PatchPrefabBackwardsCompatibility ();
}

IMPLEMENT_OBJECT_SERIALIZE (EditorExtension)
IMPLEMENT_CLASS (EditorExtension)
INSTANTIATE_TEMPLATE_TRANSFER (EditorExtension)

#else

IMPLEMENT_CLASS (EditorExtension)

EditorExtension::~EditorExtension ()
{

}

#endif

