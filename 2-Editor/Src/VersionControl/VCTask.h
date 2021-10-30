#pragma once
#include "VCAsset.h"
#include "VCChangeSet.h"
#include "VCProvider.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/Threads/Semaphore.h"
#include "Runtime/Threads/AtomicRefCounter.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/Application.h"
#include "Editor/Src/EditorUserSettings.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "Editor/Src/Utility/DiffTool.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"

#include <string>
#include <vector>

/// Must be kept in sync with C# CompletionAction (VCTaskBindings.txt)
enum CompletionAction
{
	kNoAction = 0,
	kUpdatePendingWindow = 1,
	kOnContentsChangePendingWindow = 2,
	kOnIncomingPendingWindow = 3,
	kOnChangeSetsPendingWindow = 4,
	kOnGotLatestPendingWindow = 5,
	kOnSubmittedChangeWindow = 6,
	kOnAddedChangeWindow = 7,
};

ScriptingObjectPtr CreateManagedTask(VCTask* task);


class VCTask : public VCProgressListener
{
public:
	typedef void(*DoneFunc) (VCTask* task);

	VCTask();
	virtual ~VCTask();

	virtual void Execute() {}
	virtual void Done();

	void Retain ();
	void Release ();
	
	void Wait();

	void SetCompletionAction (CompletionAction completionAction);
	
	bool GetSuccess() const {return m_Success;}
	void SetSuccess(bool success);

	int GetSecondsSpent() const { return m_SecondsSpent; }
	int GetProgressPct() const { return m_ProgressPct; }
	const std::string& GetProgressMessage() const { return m_ProgressMsg; }
	
	VCAssetList const& GetAssetList() const {return m_Assetlist;}
	ScriptingArray* GetMonoAssetList() const;
	ScriptingArray* GetMonoChangeSets() const;
	ScriptingArray* GetMonoMessages() const;
	
	virtual const std::string& GetDescription() const;

	std::string GetText() const {return m_Text;}
	int GetResultCode() const {return m_ResultCode;}

	int		GetUserIdentifier () const { return m_UserIdentifier; }
	void	SetUserIdentifier (int  userID) { m_UserIdentifier = userID; }
	
	void	EnableRepaintOnDone ();

	void	SetDoneCallback(DoneFunc doneCallback) {Assert(m_DoneCallback == NULL); m_DoneCallback = doneCallback;}
	
	virtual void OnProgress(int pctDone, int totalTimeSpent, const std::string& msg);

protected:
	
	void Done(bool assetsShouldUpdateDatabase);
	void CreateFoldersFromMetaFiles();
	
	// Return true if offline state is the last state change in the message list
	static bool HasOfflineState(const VCMessages& msgs);

	Semaphore			m_Done;
	bool				m_RepaintOnDone;
	CompletionAction    m_CompletionAction;
	volatile int		m_SecondsSpent;
	volatile int		m_ProgressPct;
	volatile int		m_NextProgressMsg;
	std::string			m_ProgressMsg;
	VCAssetList			m_Assetlist;
	VCChangeSets		m_ChangeSets;
	VCMessages			m_Messages;
	std::string			m_Text;
	int					m_ResultCode;
	bool				m_Success;
	int					m_UserIdentifier;
	DoneFunc			m_DoneCallback;
	AtomicRefCounter	m_RefCounter;
	
	friend class VCProvider; // In order for VCProvider to signal m_Done
	friend class VCCache;    // In order for VCCache to change asset modified state according to current dirtyState
	
	
private:
};
