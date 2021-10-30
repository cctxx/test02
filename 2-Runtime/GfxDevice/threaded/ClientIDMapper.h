#ifndef CLIENTIDMAPPER_H
#define CLIENTIDMAPPER_H

#include "Runtime/Utilities/dynamic_array.h"
#include "Runtime/GfxDevice/GfxDevice.h"

#if ENABLE_GFXDEVICE_REMOTE_PROCESS
#define ClientIDWrapper(type) ClientIDMapper::ClientID
#define ClientIDWrapperHandle(type) ClientIDMapper::ClientID
#else
#define ClientIDWrapper(type) type*
#define ClientIDWrapperHandle(type) type
#endif

class ClientIDMapper {
public:
	typedef UInt32 ClientID;
	
	ClientIDMapper() :
		m_HighestAllocatedID(0)
	{
	}
	
	ClientID CreateID()
	{
		if (m_FreeIDs.empty())
			return ++m_HighestAllocatedID;
		else
		{
			ClientID retval = m_FreeIDs.back();
			m_FreeIDs.pop_back();
			return retval;
		}
	}
	
	void FreeID(ClientID cid)
	{
		m_FreeIDs.push_back(cid);
	}
	
private:
	ClientID m_HighestAllocatedID;
	dynamic_array<ClientID> m_FreeIDs;
};

#endif