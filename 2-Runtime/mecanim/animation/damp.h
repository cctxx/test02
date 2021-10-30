#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/float4.h"

namespace mecanim
{

namespace dynamics
{
	class ScalDamp
	{		
		public:

		float m_DampTime;
		float m_Value;

		ScalDamp() { Reset(); }

		void Reset() { m_DampTime = 0; m_Value = 0; }
		void Evaluate(float value, float deltaTime);
	};


	class VectorDamp
	{
	public:

		float			m_DampTime;
		math::float4	m_Value;

		VectorDamp() { Reset(); }

		void Reset() { m_DampTime = 0; m_Value = math::float4::zero(); }
		void Evaluate(math::float4 const& value, float deltaTime);
	};

} // namespace dynamics

}
