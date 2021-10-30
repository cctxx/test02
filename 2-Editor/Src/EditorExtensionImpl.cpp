#include "UnityPrefix.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "EditorExtensionImpl.h"
#include "Runtime/BaseClasses/EditorExtension.h"
#include "Runtime/Utilities/algorithm_utility.h"
#include "Runtime/Utilities/BitSetSerialization.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Editor/Src/Prefabs/PrefabBackwardsCompatibility.h"
#include "Editor/Src/Prefabs/Prefab.h"
#include "Editor/Src/Prefabs/GenerateCachedTypeTree.h"
#include "Runtime/Serialize/PersistentManager.h"

IMPLEMENT_OBJECT_SERIALIZE (EditorExtensionImpl)
IMPLEMENT_CLASS (EditorExtensionImpl)

EditorExtensionImpl::EditorExtensionImpl (MemLabelId label, ObjectCreationMode mode)
:	Super(label, mode)
{
	m_TemplateFather = NULL;
	m_Object = NULL;
	m_ShouldDisplayInEditorDeprecated = -1;
}

EditorExtensionImpl::~EditorExtensionImpl ()
{
}

template<class TransferFunction>
void EditorExtensionImpl::Transfer (TransferFunction& transfer)
{
	transfer.SetVersion (6);
	if (transfer.IsOldVersion (5) || transfer.IsOldVersion (4) || transfer.IsOldVersion (3) || transfer.IsOldVersion (2))
	{
		TRANSFER (m_Object);
		
		transfer.Transfer (m_TemplateFatherImpl, "m_TemplateFather");
		
		if (transfer.IsOldVersion (5))
		{
			// m_TemplateFather is now kept around when uncoupling the datatemplate. So no need for m_LastTemplateFather anymore
			// But we have to assign m_TemplateFather based on the last template father for backwards compatibility
			PPtr<EditorExtensionImpl> lastTemplateFather;
			transfer.Transfer (lastTemplateFather, "m_LastTemplateFather");
			if (m_TemplateFatherImpl.GetInstanceID() == 0)
				m_TemplateFatherImpl = lastTemplateFather;
		}
		
		// We have to translate the m_TemplateFatherImpl PPtr to a the Object PPtr.
		// Fortunately all prefabs and imported models always have the editorextension impl written in the file ID before the impl.
		SerializedObjectIdentifier parentIdentifier;
		if (m_TemplateFatherImpl.GetInstanceID() != 0 && GetPersistentManager().InstanceIDToSerializedObjectIdentifier(m_TemplateFatherImpl.GetInstanceID(), parentIdentifier))
		{
			if (parentIdentifier.localIdentifierInFile & 1)
			{
				parentIdentifier.localIdentifierInFile--;
				m_TemplateFather.SetInstanceID(GetPersistentManager().SerializedObjectIdentifierToInstanceID(parentIdentifier));
			}	
		}

		TRANSFER (m_DataTemplate);

		TRANSFER (m_OverrideVariable);

		// Transfer typetree
		UNITY_TEMP_VECTOR(UInt8) flattenedTypeTree;
		transfer.Transfer (flattenedTypeTree, "gFlattenedTypeTree");
		UInt8 const* iterator = &flattenedTypeTree[0];
		if (transfer.IsReading () && m_OverrideVariable.size () != 0)
			ReadVersionedTypeTreeFromVector (&m_LastMergedTypeTree, iterator, iterator + flattenedTypeTree.size() , transfer.ConvertEndianess());
	}
	
	if (transfer.IsOldVersion (3))
	{
		bool m_ShouldDisplayInEditor = false;
		TRANSFER (m_ShouldDisplayInEditor);
		m_ShouldDisplayInEditorDeprecated = m_ShouldDisplayInEditor;
	}
}

char const* EditorExtensionImpl::GetName () const
{
	return "Deprecated EditorExtensionImpl";
}