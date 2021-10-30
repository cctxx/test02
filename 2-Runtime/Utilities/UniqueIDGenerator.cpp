#include "UnityPrefix.h"
#include "UniqueIDGenerator.h"

UniqueIDGenerator::UniqueIDGenerator()
{
	m_IDs.clear();
	m_IDs.push_back(2);	// Generated ID sequence should not contain 0 
	m_free = 1;
}

unsigned int UniqueIDGenerator::AllocateID () 
{
	DebugAssert( m_free <= m_IDs.size() );

	if (m_free == m_IDs.size())
	{
		m_IDs.push_back(m_free+1);
	}

	unsigned int result = m_free;
	m_free = m_IDs[m_free];

	DebugAssert(result != 0);

	return result;
}

void UniqueIDGenerator::RemoveID (unsigned int _ID)
{
	DebugAssert( (_ID > 0) && (_ID < m_IDs.size()) );

	m_IDs[_ID] = m_free;
	m_free = _ID;
}
