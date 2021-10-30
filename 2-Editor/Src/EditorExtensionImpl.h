#ifndef EDITOREXTENSIONIMPL_H
#define EDITOREXTENSIONIMPL_H

#include "Runtime/BaseClasses/NamedObject.h"
#include <deque>
#include <list>
#include <vector>
#include <set>
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/dynamic_bitset.h"
#include "Runtime/Utilities/dynamic_array.h"

class AwakeFromLoadQueue;
class EditorExtension;
class Prefab;
using std::vector;

class EditorExtensionImpl : public Object
{
	public:

	REGISTER_DERIVED_CLASS (EditorExtensionImpl, Object)
	DECLARE_OBJECT_SERIALIZE (EditorExtensionImpl)
	
	EditorExtensionImpl (MemLabelId label, ObjectCreationMode mode);
	
	virtual char const* GetName () const;
	
	typedef std::set<PPtr<EditorExtensionImpl> > ChildrenContainer;
	typedef ChildrenContainer::iterator				 ChildrenIterator;
	
	PPtr<EditorExtension>           m_Object;
	PPtr<EditorExtensionImpl>       m_TemplateFatherImpl;
	PPtr<EditorExtension>           m_TemplateFather;

	dynamic_bitset                  m_OverrideVariable;
	dynamic_array<UInt8>            m_LastMergedTemplateState;
	TypeTree                        m_LastMergedTypeTree;

	PPtr<Prefab>                    m_DataTemplate;
		
	// Deprecated and moved to BaseObject directly in Unity 1.6.0b1
	int                                       m_ShouldDisplayInEditorDeprecated;
};

#endif
