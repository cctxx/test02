#ifndef NAVMESHPROFILER_H
#define NAVMESHPROFILER_H

#include "Runtime/Profiler/Profiler.h"
#include "DetourContext.h"

PROFILER_INFORMATION (gCrowdManagerPathFinding, "CrowdManager.PathFinding", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerPathFollowing, "CrowdManager.PathFollowing", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerPathFollowingLate, "CrowdManager.PathFollowing.Late", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerAvoidance, "CrowdManager.Avoidance", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerAvoidanceSampling, "CrowdManager.Avoidance.Sampling", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerProximity, "CrowdManager.Proximity", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerProximityInsert, "CrowdManager.Proximity.Insert", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerProximityCollect, "CrowdManager.Proximity.Collect", kProfilerAI)
PROFILER_INFORMATION (gCrowdManagerCollision, "CrowdManager.Collision", kProfilerAI)

class CrowdProfiler : public dtContext
{
public:
	CrowdProfiler ()
	: dtContext (true)
	{
	}
	virtual ~CrowdProfiler ()
	{
	}

protected:
	virtual void doStartTimer (const dtTimerLabel label)
	{
		switch (label)
		{
		case DT_TIMER_UPDATE_PATHFINDING:
			PROFILER_BEGIN (gCrowdManagerPathFinding, NULL);
			break;
		case DT_TIMER_UPDATE_PATHFOLLOWING:
			PROFILER_BEGIN (gCrowdManagerPathFollowing, NULL);
			break;
		case DT_TIMER_UPDATE_PATHFOLLOWING_LATE:
			PROFILER_BEGIN (gCrowdManagerPathFollowingLate, NULL);
			break;
		case DT_TIMER_UPDATE_AVOIDANCE:
			PROFILER_BEGIN (gCrowdManagerAvoidance, NULL);
			break;
		case DT_TIMER_UPDATE_AVOIDANCE_SAMPLING:
			PROFILER_BEGIN (gCrowdManagerAvoidanceSampling, NULL);
			break;
		case DT_TIMER_UPDATE_PROXIMITY:
			PROFILER_BEGIN (gCrowdManagerProximity, NULL);
			break;
		case DT_TIMER_UPDATE_PROXIMITY_INSERT:
			PROFILER_BEGIN (gCrowdManagerProximityInsert, NULL);
			break;
		case DT_TIMER_UPDATE_PROXIMITY_COLLECT:
			PROFILER_BEGIN (gCrowdManagerProximityCollect, NULL);
			break;
		case DT_TIMER_UPDATE_COLLISION:
			PROFILER_BEGIN (gCrowdManagerCollision, NULL);
			break;
		default:
			break;
		}
	}

	virtual void doStopTimer (const dtTimerLabel /*label*/)
	{
		PROFILER_END
	}
};

#endif
