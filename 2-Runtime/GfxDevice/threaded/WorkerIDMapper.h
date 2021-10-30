#ifndef WORKERIDMAPPER_H
#define WORKERIDMAPPER_H

#include "ClientIDMapper.h"
#include "Runtime/Utilities/dynamic_array.h"

template <class T>
class WorkerIDMapper {
public:

	WorkerIDMapper ()
	{
		(*this)[0] = NULL;
	}
	
	T*& operator [] (ClientIDMapper::ClientID cid)
	{
		if (m_IDMapping.size() <= cid)
			m_IDMapping.resize_uninitialized(cid+1);
		return m_IDMapping[cid];
	}
	
private:
	dynamic_array<T*> m_IDMapping;
};

#if ENABLE_GFXDEVICE_REMOTE_PROCESS
#define WorkerIDWrapper(type,val) m_##type##Mapper[val]
#else
#define WorkerIDWrapper(type,val) val
#endif

#endif