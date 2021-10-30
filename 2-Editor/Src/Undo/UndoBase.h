#pragma once

#include <string>

class UndoBase
{
public:

	enum UndoType { kEditModeSceneUndo, kPlayModeSceneUndo, kAssetUndo };


	UndoBase ();
	virtual ~UndoBase ();

	std::string GetName() { return m_Name; }
	void SetName(std::string name) { m_Name = name; }

	void  SetPlatformDependentData (void*  platform) { m_PlatformDependentData = platform; }
	void* GetPlatformDependentData () { return m_PlatformDependentData; }

	void SetIdentifier(int identifier) { m_Identifier = identifier; }
	int GetIdentifier() { return m_Identifier; }

	void SetGroup (int group) { m_UndoGroup = group; }
	int GetGroup () { return m_UndoGroup; }

	void SetIsSceneUndo (bool sceneUndo);
	UndoType GetUndoType () const { return m_UndoType; }

	bool IsSceneUndo () const { return m_UndoType == kEditModeSceneUndo || m_UndoType == kPlayModeSceneUndo; }

	/// When collapsing multiple undos into one action, the name of the action with the highest priority is chosen
	/// (We never want to show selection undo to show up when Creating / Deleting objects)
	void SetNamePriority (int prior) { m_NamePriority = prior; }
	int GetNamePriority () { return m_NamePriority; }

	// Should this undo be ignored whenn counting for undo levels.
	/// (Selection change is so small to store we just ignore them when counting undo levels)
	virtual bool IgnoreInUndoLevel () { return false; }

	/// Restores the undo operation
	/// Returns if the undo operation was performed successfully
	virtual bool Restore (bool registerRedo) = 0;

	virtual unsigned GetAllocatedSize () const { return 0; }


protected:

	std::string	m_Name;
	int			m_Identifier;
	void*		m_PlatformDependentData;
	int			m_UndoGroup;
	int			m_NamePriority;
	UndoType	m_UndoType;

};
