#pragma once
#include "UndoBase.h"
#include "Runtime/BaseClasses/ClassIDs.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <string>

class Object;
class TypeTree;
class MonoScript;
namespace Unity { class GameObject; class Component; }

struct SingleObjectUndo
{
	LocalIdentifierInFileType	localIdentifierInFileHint;
	int							instanceID;
	ClassIDType					classID;
	PPtr<MonoScript>            script;
	
	bool                        isDestroyed;
	dynamic_array<UInt8>		state;
	TypeTree*				typeTree;

	SingleObjectUndo () : state (kMemUndoBuffer), typeTree (NULL) {  }
};
 
class ObjectUndo : public UndoBase
{
public:	
	ObjectUndo (const std::string& actionName, int inNamePriority);
	virtual ~ObjectUndo ();

	bool Store (SInt32 identifier, SInt32* objects, int size, SInt32 redoDestroyObject);
	bool Store (Object* identifier, Object** objects, int size, Object* redoDestroyObject);
	void StoreObjectDestroyedForAddedComponents (Unity::GameObject& gameObject);

	bool Restore (bool registerRedo);
	bool RegisterRedo ();
	
	bool Compare (Object* identifier, Object** objects, int size);
	
	virtual unsigned GetAllocatedSize () const;

private:
	bool HasObject (int instanceID);
	
	SInt32							m_DestroyObjectRedo;
	SInt32							m_Identifier;
	std::vector<SingleObjectUndo>	m_Undos;
};

void RegisterUndo (Object* o, const std::string& actionName, int namePriority);
void RegisterUndo (Object* identifier, Object** o, int size, const std::string& actionName, int namePriority);
void DestroyObjectUndoable (Object* o);
void RegisterFullObjectHierarchyUndo (Object* o);

Unity::Component* AddComponentUndoable (Unity::GameObject& go, int classID, MonoScript* script, std::string* error);

ObjectUndo* PrepareAddComponentUndo (Unity::GameObject& go);
void PostAddComponentUndo (ObjectUndo* undo, Unity::GameObject& go);

