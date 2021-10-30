#ifndef EDITOREXTENSION_H
#define EDITOREXTENSION_H

#include "BaseObject.h"
class TypeTree;
class Prefab;
class EditorExtensionImpl;

#if UNITY_EDITOR

class EXPORT_COREMODULE EditorExtension : public Object
{
	public:
	
	PPtr<EditorExtension> m_PrefabParentObject;
	PPtr<Prefab> m_Prefab;

	PPtr<EditorExtensionImpl>	m_DeprecatedExtensionPtr;

	REGISTER_DERIVED_ABSTRACT_CLASS (EditorExtension, Object)
	DECLARE_OBJECT_SERIALIZE (EditorExtension)
	
	EditorExtension (MemLabelId label, ObjectCreationMode mode);
	// ~EditorExtension (); declared-by-macro
	
	friend PPtr<EditorExtensionImpl> GetDeprecatedExtensionPtrIfExists (const Object& o);

	virtual bool IsPrefabParent () const;
	
	PPtr<Prefab> GetPrefab () { return m_Prefab; }
	PPtr<EditorExtension> GetPrefabParentObject () { return m_PrefabParentObject; }
	
	virtual void AwakeFromLoad (AwakeFromLoadMode mode);
	
	void PatchPrefabBackwardsCompatibility ();

	
	//std::string ExtractDeprecatedNameString ();
};

#else

class EXPORT_COREMODULE EditorExtension : public Object
{
	public:
	
	EditorExtension (MemLabelId label, ObjectCreationMode mode) : Super(label, mode) {}	
	// virtual ~EditorExtension (); declared-by-macro

	REGISTER_DERIVED_CLASS (EditorExtension, Object)

	virtual bool IsPrefabParent () const { return false; }
};


#endif

#endif
