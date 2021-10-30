#pragma once
#include "Editor/Src/Undo/UndoPropertyModification.h"

enum
{
	kMaximumSingleUndoSize = 50 * 1024 * 1024,
	kMaximumAllocatedUndoSize = 100 * 1024 * 1024
};

class ObjectUndo;
class UndoBase;

class UndoManager
{
	typedef std::list<UndoBase*>::iterator iterator;
	std::list<UndoBase*> m_UndoStack;
	std::list<UndoBase*> m_RedoStack;

	bool m_IsUndoing;
	bool m_IsRedoing;
	int m_UndoLevel;
	int m_CurrentGroup;


	int CountRealUndos (std::list<UndoBase*>& stack);
	std::string GetPriorityName (std::list<UndoBase*>& undos);
	void PopFrontIfExceedsLimit ();

	// Apply undo for top stack items with same event index only
	void Apply (std::list<UndoBase*>& stack, bool registerRedo);
	
	void ApplyAll (std::list<UndoBase*>& stack, bool registerRedo);

	bool ValidateUndo (std::list<UndoBase*>& stack);
	unsigned GetUndoStackAllocatedSize ();
	void DebugPrintUndoAndRedoStacks() const;
	void SyncAfterUndo ();

	friend UndoManager& GetUndoManager();
public:

	UndoManager ();
	static void StaticInitialize();
	static void StaticDestroy();

	// Updates the undo name in the menu
	void UpdateUndoName ();
	
	/// Registers an undo operation
	void RegisterUndo (UndoBase* undo);
	
	/// Clears all object undo operations with a specific identifier
	void ClearUndoIdentifier (Object* identifier);
	
	void ClearAllPlaymodeUndo ();

	/// Executes the undo menu item.
	/// All undo operations with the same undo group as the last one will be executed in one step.
	void Undo ();
	/// Executes the undo menu item.
	/// All redo operations with the same undo group as the last one will be executed in one step.
	void Redo ();
	
	/// Are there any undo operations on the undo stack
	bool HasUndo();
	
	/// Are there any redo operations on the redo stack
	bool HasRedo ();
	
	// Are we currently performing an undo operation.
	bool IsUndoing ();
	// Are we currently performing an undo operation.
	bool IsRedoing ();

	void Apply (std::list<UndoBase*>& stack, bool registerRedo, bool topEventIndexOnly);
	
	void ClearAll ();
	int GetUndoStackSize ()  { return m_UndoStack.size(); }
	
	// Grouping of undo operations in the undo system is automatic.
	// MouseDown / key down and some other events increment the group and thus break undo operations into seperate ones.
	// Undo operations with the same index are batched together automatically.
	void IncrementCurrentGroup ();

	int  GetCurrentGroup () const;
	
	// Reverts all undo operations down to a specific group retrieved with GetCurrentGroup
	void RevertAllDownToGroup (int group);
	// Reverts all undo operations of the current group
	void RevertAllInCurrentGroup ();
	
	// Allows you to combine undo operations at the end of the stack down groupIndex into a single 
	// group. This way clicking the undo button will undo all the operations in a single step.
	// Used by ColorPicker & ObjectSelector.
	void CollapseUndoOperations (int groupIndex);
	
	bool CompareLastUndoEvent (Object* identifier, Object** o, int size, const std::string &actionName, int eventIndex);

	// Removes any property modification from the input list which might already be part of the current event group
	void RemoveDuplicateModifications(UndoPropertyModifications& modifications, const UnityStr& actionName, int eventGroup);
};

UndoManager& GetUndoManager ();
