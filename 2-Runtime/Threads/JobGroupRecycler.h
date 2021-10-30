#ifndef JOB_GROUP_RECYCLER_H
#define JOB_GROUP_RECYCLER_H

#include "JobScheduler.h"

#if ENABLE_MULTITHREADED_CODE

template<int MaxGroups>
class JobGroupRecycler
{
public:
	// Limits the amount of groups used by finishing older ones first.
	// Restrictions: Must always be called from the same thread.
	// All jobs must be submitted before beginning a new group.

	JobGroupRecycler()
	{
		memset( m_SlotOwners, 0, sizeof(m_SlotOwners) );
		memset( m_JobGroups, 0, sizeof(m_JobGroups) );
		m_NextSlot = 0;
	}

	int BeginGroup( void* owner, int maxJobs )
	{
		int slot = m_NextSlot;
		if( m_SlotOwners[slot] )
			GetJobScheduler().WaitForGroup( m_JobGroups[slot] );
		m_JobGroups[slot] = GetJobScheduler().BeginGroup( maxJobs );
		m_SlotOwners[slot] = owner;
		m_NextSlot = (m_NextSlot + 1) % MaxGroups;
		return slot;
	}

	bool SubmitJob( void* owner, int slot, JobScheduler::JobFunction func, void *data, JobScheduler::ReturnCode *returnCode )
	{
		Assert( slot >= 0 && slot < MaxGroups );
		Assert( m_SlotOwners[slot] == owner );
		return GetJobScheduler().SubmitJob( m_JobGroups[slot], func, data, returnCode );
	}

	void WaitForGroup( void* owner, int slot )
	{
		// Maybe someone else reused the job group
		if( m_SlotOwners[slot] != owner )
			return;
		GetJobScheduler().WaitForGroup( m_JobGroups[slot] );
		m_SlotOwners[slot] = NULL;
	}

private:
	JobScheduler::JobGroupID m_JobGroups[MaxGroups];
	void* m_SlotOwners[MaxGroups];
	int m_NextSlot;
};


#endif // ENABLE_MULTITHREADED_CODE

#endif
