#pragma once

#include "UnityPrefix.h"
#include "Runtime/Utilities/dynamic_array.h"


// Class to generate small unique IDs with allocate and remove functions. 
class UniqueIDGenerator 
{
public:
	UniqueIDGenerator();
	unsigned int AllocateID();
	void RemoveID( unsigned int _ID);

private:
	dynamic_array<unsigned int> m_IDs;
	unsigned int m_free;
};
