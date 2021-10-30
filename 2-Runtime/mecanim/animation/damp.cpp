#include "UnityPrefix.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/mecanim/animation/damp.h"

namespace mecanim
{

namespace dynamics
{
	void ScalDamp::Evaluate(float value, float deltaTime)
	{
		m_Value = math::cond(m_DampTime > 0, m_Value + (value - m_Value) * math::abs(deltaTime) / (m_DampTime + math::abs(deltaTime)), value);
	}

	void VectorDamp::Evaluate(math::float4 const& value, float deltaTime)
	{
		math::float1 dt(deltaTime);
		math::float1 dampTime(m_DampTime);

		m_Value = math::cond( math::bool4(m_DampTime > 0), m_Value + (value - m_Value) * math::abs(dt) / (dampTime + math::abs(dt)), value);
	}
}

}
