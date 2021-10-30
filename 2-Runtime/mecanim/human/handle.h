#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/xform.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

namespace mecanim
{

namespace human
{
	struct Handle
	{
		DEFINE_GET_TYPESTRING(Handle)

		Handle():
			m_X(math::xformIdentity()),
			m_ParentHumanIndex(numeric_limits<uint32_t>::max_value),
			m_ID(numeric_limits<uint32_t>::max_value)
		{			
		}

		math::xform	m_X;					// Local tranform		
		uint32_t	m_ParentHumanIndex;	// Related parent's human bone index
		uint32_t	m_ID;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_X);
			TRANSFER(m_ParentHumanIndex);
			TRANSFER(m_ID);
		}
	};
}

}
