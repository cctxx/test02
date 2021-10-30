#ifndef _PROFILERHISTORY_H_
#define _PROFILERHISTORY_H_

#include "Configuration/UnityConfigure.h"

#if ENABLE_PROFILER && UNITY_EDITOR

#include "Runtime/Threads/Thread.h"
#include "TimeHelper.h"
#include "ProfilerImpl.h"
#include "ProfilerFrameData.h"
#include "ProfilerStats.h"
#include "Runtime/Misc/SystemInfo.h"

struct ProfilerSample;

class ProfilerHistory
{
public:

	typedef dynamic_array<ProfilerFrameData*> ProfileFrameVector;

		// Memory overview
		//void* memoryOverview;

	~ProfilerHistory();
	
	static void Initialize();
	static void Cleanup();

	// Singleton accessor for profiler
	static ProfilerHistory& Get() { return *ms_Instance; }

	void AddFrameDataAndTransferOwnership(ProfilerFrameData* frame, int guid);

	// Deletes one frame from the history if we have too many in the history
	void KillOneFrame();
	
	// Release memory allocated for all frame history
	void CleanupFrameHistory();
		
	const ProfilerFrameData* GetFrameData(int frame) const;
	ProfilerFrameData* GetFrameData(int frame);
	const ProfilerFrameData* GetLastFrameData() const;
	ProfilerFrameData* GetLastFrameData();
	
	// Frame index access functions
	int GetFirstFrameIndex ();
	int GetLastFrameIndex ();
	int GetNextFrameIndex (int frame);
	int GetPreviousFrameIndex (int frame);

	// The maximum amount of history samples. (-1 because there is one sample reserved for the currently selected history position, which is not shown in graphs)
	int GetMaxFrameHistoryLength () { return m_MaxFrameHistoryLength - 1; }
	
	void GetStatisticsValuesBatch(int identifier, int firstValue, float scale, float* buffer, int size, float* outMaxValue);
	std::string GetFormattedStatisticsValue(ProfilerFrameData& frame, int identifier);
	std::string GetFormattedStatisticsValue(int frame, int identifier);

	ProfilerString GetOverviewTextForProfilerArea (int frame, ProfilerArea profilerArea);
	
	int GetStatisticsValue(ProfilerFrameData& frameData, int identifier);
	int GetStatisticsIdentifier(const std::string& statName);
	void GetAllStatisticsProperties(std::vector<std::string>& all);
	void GetGraphStatisticsPropertiesForArea(ProfilerArea area, std::vector<std::string>& all);

	void SetHistoryPosition (int pos) { m_HistoryPosition = pos; }
	int GetHistoryPosition () const { return m_HistoryPosition; }
	int GetFrameCounter () { return m_FrameCounter; }

	bool IsGPUProfilerBuggyOnDriver ();
	bool IsGPUProfilerSupportedByOS ();
	bool IsGPUProfilerSupported ();
	
	void SetSelectedPropertyPath (const std::string& samplePath);
	const std::string& GetSelectedPropertyPath () { return m_SelectedPropertyPath; }

private:
	ProfilerHistory();

	void CalculateSelectedTimeAndChart (ProfilerFrameData& frameData);
	void CleanupActiveFrame();

	bool AddFrame (ProfilerFrameData* frame, int source);

private:
	dynamic_array<StatisticsProperty> m_Properties;
	
	// Profile history parameters
	int m_MaxFrameHistoryLength;
	int m_FrameCounter;	
	int m_HistoryPosition;
	
	int m_FramesWithGPUData;
	
	UInt32	m_BytesUsedLastFrame;

	// STL vector to store lists of profile sample objects of all functions being called within one frame
	ProfileFrameVector m_Frames;
	
#if SUPPORT_THREADS
	Thread::ThreadID m_MainThreadID;
#endif

	std::string m_SelectedPropertyPath;
	
	// Profiler instance to use with singleton pattern
	static ProfilerHistory* ms_Instance;
};

#endif

#endif
