#ifndef GAMEMANAGER_H
#define GAMEMANAGER_H

#include "EditorExtension.h"

/// Any game manager (eg. AudioManager, dynamicsmanager) that needs serialization
/// has to derive from either LevelGameManager or GlobalGameManager.
/// Every level contains its own GameManager for that Level (eg. Scene, PhysicsManager)
/// LevelGameManagers are destroyed and reloaded from the new scene when loading a new scene.
/// GlobalGameManagers are singletons and loaded on 
/// startup of the gameplayer/editor (eg. InputManager, TagManager)

class EXPORT_COREMODULE GameManager : public Object
{
	public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (GameManager, Object)
	GameManager(MemLabelId label, ObjectCreationMode mode) : Super(label, mode) { }
//	virtual ~GameManager ();
	
	///@TODO: Get rid of this. I am not sure why this is not just done in the destructor / cleanup class
	virtual void NetworkOnApplicationQuit () { AssertString("not implemented"); }
	virtual void NetworkUpdate () { AssertString("not implemented"); }
};


class EXPORT_COREMODULE LevelGameManager : public GameManager
{
	public:
	
	virtual char const* GetName () const { return GetClassName().c_str (); }
		
	REGISTER_DERIVED_ABSTRACT_CLASS (LevelGameManager, GameManager)
	DECLARE_OBJECT_SERIALIZE (GameManager)

	LevelGameManager(MemLabelId label, ObjectCreationMode mode);

//	virtual ~LevelGameManager ();
};


class EXPORT_COREMODULE GlobalGameManager : public GameManager
{
	public:
	
	REGISTER_DERIVED_ABSTRACT_CLASS (GlobalGameManager, GameManager)
	DECLARE_OBJECT_SERIALIZE (GlobalGameManager)

	GlobalGameManager(MemLabelId label, ObjectCreationMode mode);

//	virtual ~GlobalGameManager ();
	
	virtual char const* GetName () const;
};

GameManager* GetGameManagerIfExists (int index);

inline GameManager* CreateGameManager (int classID)
{
	Object* o = Object::Produce (classID);
	o->Reset ();
	o->AwakeFromLoad(kDefaultAwakeFromLoad);
	o->SetNameCpp (Object::ClassIDToString (classID));
	return static_cast<GameManager*> (o);
}

#define CALL_MANAGER_IF_EXISTS(x,func) { GameManager* _manager = GetGameManagerIfExists(x); if (_manager) _manager->func; }

#endif
