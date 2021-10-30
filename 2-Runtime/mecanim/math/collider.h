#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/xform.h"

#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"

namespace math
{
	enum ColliderType { kNone = 0, kCube, kSphere, kCylinder, kCapsule };
	enum JointType { kIgnored = 0, kLocked, kLimited };

	struct Collider
	{	
		DEFINE_GET_TYPESTRING(Collider)

		xform m_X;
		mecanim::uint32_t m_Type; // ColliderType

		inline Collider() : m_Type(kCube) {}

		// Joint information
		mecanim::uint32_t m_XMotionType;
		mecanim::uint32_t m_YMotionType;
		mecanim::uint32_t m_ZMotionType;
		float m_MinLimitX;
		float m_MaxLimitX;
		float m_MaxLimitY;
		float m_MaxLimitZ;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_X);
			TRANSFER(m_Type);
			TRANSFER(m_XMotionType);
			TRANSFER(m_YMotionType);
			TRANSFER(m_ZMotionType);
			TRANSFER(m_MinLimitX);
			TRANSFER(m_MaxLimitX);
			TRANSFER(m_MaxLimitY);
			TRANSFER(m_MaxLimitZ);
		}
	};
	
	STATIC_INLINE float4 SphereCollide(math::xform const &sphereX,math::float4 const &pos)
	{
		math::float4 ret = math::xformInvMulVec(sphereX,pos);
		
		ret *= math::rcp(math::length(ret)); 

		return math::xformMulVec(sphereX,ret);
	}
}
