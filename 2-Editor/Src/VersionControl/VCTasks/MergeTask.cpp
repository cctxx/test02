#include "UnityPrefix.h"
#include "MergeTask.h"
#include "DownloadRevisionUtility.h"
#include "Editor/Src/AssetPipeline/AssetInterface.h"
#include "Editor/Src/VersionControl/VCAsset.h"
#if UNITY_WIN
#include "PlatformDependent/Win/PathUnicodeConversion.h"
#endif

MergeTask::MergeTask(VCAssetList const& assetlist, MergeMethod method) : m_InputList(assetlist), m_MergeMethod(method)
{
}

MergeTask::MergeTask()
{
}

void MergeTask::Execute()
{
	// We need to download the 
	// base version and the conflicting version in order to do a 3 way merge.
	GetVCProvider().ClearPluginMessages();
	VCPluginSession& p = GetVCProvider().EnsurePluginIsRunning(m_Messages);
	p.SetProgressListener(this);
	
	m_DestTmpDir = PathToAbsolutePath(GetUniqueTempPathInProject());
	
	
	VCChangeSetIDs revs;
	revs.push_back("mineAndConflictingAndBase");
	
	m_Success = DownloadRevisions(p, m_Messages, m_Assetlist, m_InputList, revs, m_DestTmpDir);
}

// In main thread
void MergeTask::Done ()
{
	if (!m_Success)
	{
		//		DeleteFileOrDirectory(m_DestTmpDir);
		VCTask::Done();
		return;
	}
	
	if (m_Assetlist.size() != m_InputList.size() * 3)
	{
		//		DeleteFileOrDirectory(m_DestTmpDir);
		ErrorString(Format("Received mismatching number of conflicting files from VCS plugin (%i - %i)", m_Assetlist.size(), m_InputList.size() * 3));
		VCTask::Done();
		return;
	}
	
	// In this case we are doing a manual merge using a merge tool 
	
	// Compare the fetched content agains the working directory
	int idx = 0;
	
	// Reult container for successfull merged assets
	VCAssetList resolvedList;
	resolvedList.reserve(m_InputList.size());
	
	set<string> reimportAssets;
	
	VCAssetList::const_iterator j = m_Assetlist.begin();
	for (VCAssetList::const_iterator i = m_InputList.begin(); i != m_InputList.end(); i++, j += 3, idx++)
	{
		DisplayProgressbar("Compare", "Initializing merge...", (float)(idx+1)/m_InputList.size());
		
		const VCAsset& outputAsset = *i;
		const VCAsset& mineAsset = *j;
		const VCAsset& conflictingAsset = *(j+1);
		const VCAsset& baseAsset = *(j+2);

		bool handled = false;
				
		if (baseAsset.HasState(kMissing) && conflictingAsset.HasState(kMissing))
		{
			ErrorString(Format("Could't get conflict information from version control plugin for '%s'", GetLastPathNameComponent(outputAsset.GetPath()).c_str()));
		}
		// No base means that this is an unmergeable file e.g. binary blob
		else if (baseAsset.HasState(kMissing))
		{
			int res = DisplayDialogComplex("Select revision to keep",
										   Format("This is an umergeable file. Please select the revision of '%s' you want to keep", GetLastPathNameComponent(outputAsset.GetPath()).c_str()),
										   "Accept Mine", "Accept Others", "Cancel");
			switch (res)
			{
				case 0:
					if (mineAsset.GetPath() != outputAsset.GetPath())
					{
						MoveReplaceFile(mineAsset.GetPath(), outputAsset.GetPath());
					}
					handled = true;
					break;
				case 1:
					MoveReplaceFile(conflictingAsset.GetPath(), outputAsset.GetPath());
					handled = true;
					break;
				default:
					break;
			}
		}
		else if (MergeSingleFile(outputAsset, mineAsset, conflictingAsset, baseAsset))
		{
			handled = true;
		}

		if (handled)
		{
			resolvedList.push_back(outputAsset);

			// When both .meta and asset has changed we only want to import once.
			reimportAssets.insert(outputAsset.GetAssetPath());
		}
		
		Thread::Sleep(0.001f); // To avoid launching more than one instance of FileMerge
	}
	
	for (set<string>::const_iterator i = reimportAssets.begin(); i != reimportAssets.end(); ++i)
		AssetInterface::Get().ImportAtPath(*i, kAssetWasModifiedOnDisk);
	
	ClearProgressbar();
	
	// Return the files that were successfully merged
	m_Assetlist.swap(resolvedList);
	
	// Delete temp dir
	//		DeleteFileOrDirectory(m_DestTmpDir);
	
	VCTask::Done(false);
}

bool MergeTask::MergeSingleFile(const VCAsset& outputAsset,
								const VCAsset& mineAsset,
								const VCAsset& conflictingAsset,
								const VCAsset& baseAsset)
{
	string otitle = outputAsset.GetPath();
	string mtitle = mineAsset.GetPath();
	string rtitle = conflictingAsset.GetPath();
	string btitle = baseAsset.GetPath();
	
	string opath = outputAsset.GetPath();
	string rpath = conflictingAsset.GetPath();
	string bpath = baseAsset.GetPath();
	string mpath = mineAsset.GetPath();
	
	// The merge result file path is equal to the conflict file path with
	// the "conflicting" end part replaced with "merged"
	string mergedFile = m_DestTmpDir + "/mergeresult";
	
	if (!IsDirectoryCreated(m_DestTmpDir))
		CreateDirectory(m_DestTmpDir);
	
	bool manualMergeRequired = true;
	
	std::string diff3Path;
#if UNITY_OSX || UNITY_LINUX
	diff3Path = "/usr/bin/diff3";
#elif UNITY_WIN
	diff3Path = AppendPathName( GetApplicationContentsPath(), "Tools/diff3.exe" );
#else
#error "Unknown platform"
#endif
	if(IsFileCreated(diff3Path))
	{
		string output;
		vector<string> args;
		
		// The diff3 command with  --merge and --text will have an exit code != 0 if 
		// a clean merge is not possible
		args.push_back("--merge");
		args.push_back("--text");
		
#if UNITY_OSX || UNITY_LINUX
		args.push_back(mpath);
		args.push_back(bpath);
		args.push_back(rpath);
		
		// Diff3 outputs the merge file to stdout: 
		bool automergeOK = LaunchTaskArray(diff3Path, &output, args, true);
#elif UNITY_WIN
		// diff3 does not like spaces on windows
		args.push_back(ToShortPathName(PathToAbsolutePath(mpath)));
		args.push_back(ToShortPathName(PathToAbsolutePath(bpath)));
		args.push_back(ToShortPathName(PathToAbsolutePath(rpath)));
		
		// Diff3 outputs the merge file to stdout: 
		bool automergeOK = LaunchTaskArray(diff3Path, &output, args, true, AppendPathName( GetApplicationContentsPath(), "Tools"));
#else
#error "Unknown platform"
#endif
		
		// WarningString("Trying automerge " + mergedFile);
		if (automergeOK && WriteStringToFile(output, mergedFile, kNotAtomic, kFileFlagDontIndex | kFileFlagTemporary))
			manualMergeRequired = false;
	}
	
	if(manualMergeRequired)
	{
		// If we were asked to only merge non conflicting files then stop right here with this file.
		if (m_MergeMethod == kMergeNonConflicting)
			return false;
		
		if (IsFileCreated(mergedFile)) 
		{
			if (! DeleteFile(mergedFile)) 
			{
				ErrorString("Could not delete temporary file: " + mergedFile);
				return false;
			}
		}
		
		for(;;)
		{
			string errorText = InvokeMergeTool(Format("%s (%s)", rtitle.c_str(), "theirs"), rpath,
											   Format("%s (mine)", mtitle.c_str()), mpath,
											   Format("%s (%s)", btitle.c_str(), "base"), bpath,
											   mergedFile);
			
			LogString(string("Merged to ") + mergedFile);
			
			if( !errorText.empty() ) 
			{
				ErrorString(errorText);
				return false;
			}
			
			Thread::Sleep( 1.0f ); // Hack to allow the diff tool to show up before opening our dialog
			
			// Now, pop-up an alert to the user to finish the merge before continuing, and wait for user to accept
			if( !DisplayDialog("Accept merge?", 
							   Format("Finish merging (and saving) %s before pressing Accept.", GetLastPathNameComponent(opath).c_str()),
							   "Accept",
							   "Cancel") )
			{
				WarningString("Manual merge was aborted");
				return false;
			}
			
			// If target does not exist, the user probably forgot to save the merge before hitting accept.
			// In this case tell the user that he needs to save the merge.
			if( ! IsFileCreated(mergedFile) ) 
			{
				if( !DisplayDialog("Could not find merged file",
								   Format("Could not find merged file for %s. Please hit save in the merge tool before accepting the merge in Unity.", 
										  GetLastPathNameComponent(opath).c_str()),
								   "Reopen Merge Tool",
								   "Cancel") )
				{
					WarningString("Manual merge was aborted");
					return false;
				}
			}
			else 
			{ // Everything is fine, so we break out of the loop
				break;
			}
		}
	}

	MoveReplaceFile(mergedFile, opath);

	return true;
}
