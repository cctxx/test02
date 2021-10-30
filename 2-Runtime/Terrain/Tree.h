#ifndef TREE_H
#define TREE_H

#include "Runtime/BaseClasses/GameObject.h"

class MonoBehaviour;



class Tree : public Unity::Component
{
public:
public:
	REGISTER_DERIVED_CLASS(Tree, Component)
	DECLARE_OBJECT_SERIALIZE(Tree)
	
	Tree(MemLabelId label, ObjectCreationMode mode);
	// ~Tree(); declared by a macro
		
	void SetTreeData (PPtr<MonoBehaviour> tree);
	PPtr<MonoBehaviour> GetTreeData ();
	
	static void InitializeClass ();
	static void CleanupClass () {}
	
	void OnWillRenderObject ();

private:
	UInt32 CalculateSupportedMessages ();
	
	#if UNITY_EDITOR
	PPtr<MonoBehaviour> m_TreeData;
	#endif
};

#endif
