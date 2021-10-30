
#include "UnityPrefix.h"
#include "UndoManager.h"
#include "ObjectUndo.h"
#include "PropertyDiffUndoRecorder.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Editor/Platform/Interface/UndoPlatformDependent.h"
#include "Editor/Src/WebViewWrapper.h"
#include "Runtime/Misc/GameObjectUtility.h"
#include "Runtime/Utilities/InitializeAndCleanup.h"

#include <list>

using namespace std;

static const bool kDebugUndo = false;
static UndoManager* gUndoManager = NULL;

static RegisterRuntimeInitializeAndCleanup s_UndoManagerCallbacks(UndoManager::StaticInitialize, UndoManager::StaticDestroy);

void UndoManager::StaticInitialize()
{
	gUndoManager = UNITY_NEW_AS_ROOT(UndoManager, kMemUndo, "Undo", "UndoManager");
}

void UndoManager::StaticDestroy()
{
	UNITY_DELETE(gUndoManager, kMemUndo);
	gUndoManager = NULL;
}

static void ClearPlaymodeStack (std::list<UndoBase*>& stack)
{
    std::list<UndoBase*>::iterator next = stack.begin();
	for (std::list<UndoBase*>::iterator i=stack.begin();i != stack.end();)
	{
        next++;
        
		UndoBase* cur = *i;
		if (cur->GetUndoType () == UndoBase::kPlayModeSceneUndo)
		{
			UNITY_DELETE(cur, kMemUndo);
			stack.erase(i);
		}

        i = next;
	}
}

static void ClearStack (std::list<UndoBase*>& stack)
{
	for (std::list<UndoBase*>::iterator i=stack.begin();i != stack.end();i++)
	{
		UndoBase* cur = *i;
		UNITY_DELETE(cur, kMemUndo);
	}
	stack.clear();
}

//----------------------------------------------------------------------------------------

void ClearUndoIdentifier (Object* identifier)
{
	GetUndoManager().ClearUndoIdentifier(identifier);
}


//----------------------------------------------------------------------------------------

UndoManager::UndoManager ()
:	m_UndoLevel(500)
,	m_IsUndoing(false)
,	m_IsRedoing(false)
,	m_CurrentGroup(0)
{
}

bool UndoManager::IsUndoing ()
{
	return m_IsUndoing;
}

bool UndoManager::IsRedoing ()
{
	return m_IsRedoing;
}

int UndoManager::CountRealUndos (std::list<UndoBase*>& stack)
{
	if (stack.empty())
		return 0;
	
	// Count undo steps (combined steps)
	// And ignore IgnoreInUndoLevel undos
	int counter = 0;
	int group = stack.front()->GetGroup();
	for (iterator i=stack.begin();i!=stack.end();i++)
	{
		UndoBase& undo = **i;
		if (group != undo.GetGroup() && !undo.IgnoreInUndoLevel())
		{
			counter++;
			group = undo.GetGroup();
		}
	}
	return counter;
}

unsigned UndoManager::GetUndoStackAllocatedSize ()
{
	unsigned size = 0;
	for (iterator i=m_UndoStack.begin();i != m_UndoStack.end();i++)
	{
		UndoBase* undo = *i;
		size += undo->GetAllocatedSize();
	}
	return size;
}

void UndoManager::PopFrontIfExceedsLimit ()
{
	// Pop from front if we exceed the limit
	// (Just removing the last one won't work because we might be adding a bunch of IgnoreInUndoLevel undos)
	while (CountRealUndos(m_UndoStack) > m_UndoLevel || GetUndoStackAllocatedSize() > kMaximumAllocatedUndoSize)
	{
		// Always remove entire groups together
		int group = m_UndoStack.front()->GetGroup();
		while (!m_UndoStack.empty() && group == m_UndoStack.front()->GetGroup())
		{
			UNITY_DELETE(m_UndoStack.front(), kMemUndo);
			m_UndoStack.pop_front();
		}
	}
}

void UndoManager::RegisterUndo (UndoBase* undo)
{
	undo->SetGroup(GetUndoManager().GetCurrentGroup());
	if (!m_IsUndoing)
	{
		if (kDebugUndo)
			printf_console("Registering undo: '%s' [%d]. Stacks:\n",undo->GetName().c_str(), undo->GetGroup());

		// Don't overflow the undo buffer
		PopFrontIfExceedsLimit();

		m_UndoStack.push_back(undo);

		// Clear redo stack when we add another undo operation.
		// We dont clear the redo stack when we add a selection undo.
		if (!undo->IgnoreInUndoLevel() && !m_IsRedoing)
			ClearStack(m_RedoStack);
	}
	else
	{
		if (kDebugUndo)
			printf_console("Registering redo: '%s' [%d]. Stacks:\n", undo->GetName().c_str(), undo->GetGroup());

		m_RedoStack.push_back(undo);
	}

	UpdateUndoName ();

	if (kDebugUndo)
		DebugPrintUndoAndRedoStacks();
}


string UndoManager::GetPriorityName (std::list<UndoBase*>& undos)
{
	string undoName;
	if (undos.empty())
        return undoName;
    
    undoName = undos.back()->GetName();
    int priority = undos.back()->GetNamePriority();
    int group = undos.back()->GetGroup();

    for (list<UndoBase*>::reverse_iterator i=undos.rbegin(); i != undos.rend() && group == (**i).GetGroup();i++)
    {
        UndoBase& undo = (**i);
        if (undo.GetNamePriority() > priority && !undo.GetName().empty())
        {
            undoName = undo.GetName();
            priority = undo.GetNamePriority();
        }
    }
	return undoName;
}

void UndoManager::UpdateUndoName ()
{
	string undoName = GetPriorityName(m_UndoStack);
	string redoName = GetPriorityName(m_RedoStack);

	#if ENABLE_ASSET_STORE
	if (WebViewWrapper::GetFocusedWebView() != NULL)
	{
		undoName = "";
		redoName = "";
	}
	#endif

	SetUndoMenuNamePlatformDependent(undoName, redoName);
}

void UndoManager::Apply (std::list<UndoBase*>& stack, bool registerRedo)
{
	if (stack.empty())
		return;
	// Apply all undos with the same event number
	int eventNumber = stack.back()->GetGroup();
	while (!stack.empty() && stack.back()->GetGroup() == eventNumber)
	{
		UndoBase* operation = stack.back();

		stack.pop_back();

		if (kDebugUndo)
			printf_console("restoring undo %s - event index: %i\n", operation->GetName().c_str(), operation->GetGroup());

		operation->Restore(registerRedo);

		UNITY_DELETE(operation, kMemUndo);
	}
}

/// TODO: Implement proper
static bool IsHotControlUsed ()
{
	return GetEternalGUIState()->m_HotControl != 0;
}

bool UndoManager::ValidateUndo (std::list<UndoBase*>& stack)
{
	return !stack.empty();
}

bool UndoManager::HasUndo ()
{
	#if ENABLE_ASSET_STORE
	if (WebViewWrapper::GetFocusedWebView() != NULL)
		return WebViewWrapper::GetFocusedWebView()->HasUndo();
	#endif
	return ValidateUndo(m_UndoStack);
}

bool UndoManager::HasRedo ()
{
	#if ENABLE_ASSET_STORE
	if (WebViewWrapper::GetFocusedWebView() != NULL)
		return WebViewWrapper::GetFocusedWebView()->HasRedo();
	#endif
	return ValidateUndo(m_RedoStack);
}

void UndoManager::CollapseUndoOperations (int group)
{
	GetPropertyDiffUndoRecorder().Flush();

	for (iterator i=m_UndoStack.begin(); i != m_UndoStack.end(); i++)
	{
		UndoBase* undo = *i;
		if (undo->GetGroup () > group)
			undo->SetGroup(group);
	}
}

void UndoManager::RevertAllDownToGroup (int group)
{
	if (group < 0)
	{
		ErrorString("Invalid event index");
		return;
	}
	
	GetPropertyDiffUndoRecorder().Flush();
	
	bool didRevert = false;
	while (!m_UndoStack.empty() && group <= m_UndoStack.back()->GetGroup())
	{
		AssertIf(m_IsUndoing);
		m_IsUndoing = true;
		Apply(m_UndoStack, false);
		m_IsUndoing = false;
		
		didRevert = true;
	}
	
	if (didRevert)
		SyncAfterUndo ();
}

void UndoManager::RevertAllInCurrentGroup ()
{
	RevertAllDownToGroup (GetCurrentGroup());
}


void UndoManager::SyncAfterUndo ()
{
	UpdateUndoName ();
	
	//Update Current Edit text
	ExecuteCommandOnAllWindows("UndoRedoPerformed");
	CallStaticMonoMethod("Undo", "Internal_CallUndoRedoPerformed");
}


void UndoManager::Undo ()
{
	GetPropertyDiffUndoRecorder().Flush();
	
	#if ENABLE_ASSET_STORE
	if (WebViewWrapper::GetFocusedWebView() != NULL)
	{
		 WebViewWrapper::GetFocusedWebView()->Undo();
		 return;
	}
	#endif

	// Do not allow to undo while someone is using hotControl
	if (IsHotControlUsed())
		return;

	IncrementCurrentGroup();

	AssertIf(m_IsUndoing);
	m_IsUndoing = true;
	Apply(m_UndoStack, true);
	m_IsUndoing = false;
	
	SyncAfterUndo ();
}

void UndoManager::Redo ()
{
	GetPropertyDiffUndoRecorder().Flush();
	
	#if ENABLE_ASSET_STORE
	if (WebViewWrapper::GetFocusedWebView() != NULL)
	{
		 WebViewWrapper::GetFocusedWebView()->Redo();
		return;
	}
	#endif

	// Do not allow to redo while someone is using hotControl
	if (IsHotControlUsed())
		return;

	IncrementCurrentGroup();

	AssertIf(m_IsRedoing);
	m_IsRedoing = true;
	Apply(m_RedoStack, true);
	m_IsRedoing = false;
	SyncAfterUndo ();
}

void UndoManager::ClearUndoIdentifier (Object* identifier)
{
	if (identifier == NULL)
		return;

	// Check for identifier and delete
	iterator next;
	for (iterator i=m_UndoStack.begin();i != m_UndoStack.end();i=next)
	{
		next = i;
		next++;

		UndoBase* cur = reinterpret_cast<UndoBase*> (*i);
		if (identifier->GetInstanceID() == cur->GetIdentifier())
		{
			UNITY_DELETE(cur, kMemUndo);
			m_UndoStack.erase(i);
		}
	}

	// Check for identifier and delete
	for (iterator i=m_RedoStack.begin();i != m_RedoStack.end();i=next)
	{
		next = i;
		next++;

		UndoBase* cur = reinterpret_cast<UndoBase*> (*i);
		if (identifier->GetInstanceID() == cur->GetIdentifier())
		{
			UNITY_DELETE(cur, kMemUndo);
			m_RedoStack.erase(i);
		}
	}

	UpdateUndoName ();
}

void UndoManager::ClearAll ()
{
	//@TODO: Maybe we should reset m_CurrentGroup??
	
	ClearStack (m_UndoStack);
	ClearStack (m_RedoStack);

	UpdateUndoName ();
}

bool UndoManager::CompareLastUndoEvent (Object* identifier, Object** o, int size, const std::string &actionName, int group)
{
	Assert(!m_IsUndoing);
	Assert(!m_IsRedoing);
	
	if (m_UndoStack.empty())
		return false;
	
	ObjectUndo* undo = dynamic_cast<ObjectUndo*> (m_UndoStack.back());
	if (undo == NULL)
		return false;
	
	if (undo->GetGroup() != group)
		return false;

	return undo->Compare (identifier, o, size);
}

int UndoManager::GetCurrentGroup() const
{
	return m_CurrentGroup;
}

void UndoManager::DebugPrintUndoAndRedoStacks() const
{
	std::list<UndoBase*>::const_reverse_iterator i;
	for (i=m_UndoStack.rbegin();i!=m_UndoStack.rend();i++)
		printf_console(" *** undo: '%s' evt %i pri %i\n",(**i).GetName().c_str(), (**i).GetGroup(), (**i).GetNamePriority() );
	if (m_RedoStack.size())
		printf_console(" -----------------------------------------\n");
	for (i=m_RedoStack.rbegin();i!=m_RedoStack.rend();i++)
		printf_console(" *** redo: '%s' evt %i pri %i\n",(**i).GetName().c_str(), (**i).GetGroup(), (**i).GetNamePriority() );
	printf_console("\n");
}

void UndoManager::IncrementCurrentGroup ()
{
	// Flush must happen before incrementing the group, so that after the increment there is nothing on the undo stack for the new group.
	// This is important for drag & drop, otherwise the operation might get reverted by the dragging exit function.
	GetPropertyDiffUndoRecorder().Flush();
	
	m_CurrentGroup++;
    if (kDebugUndo)
        printf_console("*** undo: increment group %d\n", m_CurrentGroup);
}

void UndoManager::RemoveDuplicateModifications(UndoPropertyModifications& modifications, const UnityStr& actionName, int eventGroup)
{
	Assert(!m_IsUndoing);
	Assert(!m_IsRedoing);
	
	// Assert that this is only used on the current event group
	// (just for now, not sure if there is a use case where you would compare against a previous event group)
	Assert(m_CurrentGroup == eventGroup);

	if (m_UndoStack.empty())
		return;

	for (list<UndoBase*>::reverse_iterator i = m_UndoStack.rbegin(); i != m_UndoStack.rend(); i++)
	{
		// Stop when we encounter a new event group
		if ((*i)->GetGroup() != eventGroup)
			return;

		PropertyDiffUndo* undo = dynamic_cast<PropertyDiffUndo*> (*i);
		// Skip Undo item which are not property diffs
		if (undo == NULL)
			continue;

		undo->RemoveDuplicates (modifications);
	}
}

void UndoManager::ClearAllPlaymodeUndo ()
{
	::ClearPlaymodeStack (m_UndoStack);
	::ClearPlaymodeStack (m_RedoStack);
    
    UpdateUndoName ();
}


UndoManager& GetUndoManager ()
{
	return *gUndoManager;
}
