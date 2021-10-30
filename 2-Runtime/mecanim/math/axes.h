#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/math.h"
#include "Runtime/Math/Simd/quaternion.h"

namespace math
{
	struct Limit
	{
		DEFINE_GET_TYPESTRING(Limit)

		float4 m_Min; // m_Min > 0 -> free, m_Min == 0 -> lock, m_Min < 0 -> limit 
		float4 m_Max; // m_Max < 0 -> free, m_Max == 0 -> lock, m_Max > 0 -> limit

		inline Limit() : m_Min(1,1,1,1), m_Max(-1,-1,-1,-1) {}
		inline Limit(float4 const& aMin, float4 const& aMax, float aRange) { m_Min = aMin; m_Max = aMax; }

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_Min);
			TRANSFER(m_Max);
		}
	};

	enum AxesType { kFull, kZYRoll, kRollZY, kEulerXYZ };

	struct Axes
	{	
		DEFINE_GET_TYPESTRING(Axes)

		float4 m_PreQ;
		float4 m_PostQ;
		float4 m_Sgn;
		Limit m_Limit;
		float m_Length;
		mecanim::uint32_t m_Type; // AxesType

		inline Axes() : m_PreQ(0,0,0,1), m_PostQ(0,0,0,1), m_Sgn(1,1,1,1), m_Length(1), m_Type(kEulerXYZ) {}
		inline Axes(float4 const& aPreQ, float4 const& aPostQ, float4 const& aSgn, float const &aLength, AxesType const& aType) { m_PreQ = aPreQ; m_PostQ = aPostQ; m_Sgn = aSgn; m_Length = aLength; m_Type = aType; }

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_PreQ);
			TRANSFER(m_PostQ);
			TRANSFER(m_Sgn);
			TRANSFER(m_Limit);
			TRANSFER(m_Length);
			TRANSFER(m_Type);
		}	
	};

	STATIC_INLINE float4 LimitSmootClamp(Limit const& l, float4 const& v, float1 const& smoothRange)
	{
		const float4 min = cond(l.m_Min<float1::zero(), -smoothClamp(-v,float4::one(),smoothRange), cond(l.m_Min>float1::zero(),v,float4::zero()));
		const float4 max = cond(l.m_Max>float1::zero(), smoothClamp(v,float4::one(),smoothRange), cond(l.m_Max<float1::zero(),v,float4::zero()));
		return math::cond(v<float1::zero(),min,max);
	}

	STATIC_INLINE float LimitProject(float min, float max, float v)
	{
		float i = min < 0 ? -v / min : min > 0 ? v : 0;
		float a = max > 0 ? +v / max : max < 0 ? v : 0;
		return v < 0 ? i : a;
	}	

	STATIC_INLINE float LimitUnproject(float min, float max, float v)
	{
		float i = min < 0 ? -v * min : min > 0 ? v : 0;
		float a = max > 0 ? +v * max : max < 0 ? v : 0;
		return v < 0 ?  i : a;
	}

	STATIC_INLINE float4 LimitProject(Limit const& l, float4 const& v)
	{
		const float4 min = cond(l.m_Min<float1::zero(),-v/l.m_Min,cond(l.m_Min>float1::zero(),v,float4::zero()));
		const float4 max = cond(l.m_Max>float1::zero(),+v/l.m_Max,cond(l.m_Max<float1::zero(),v,float4::zero()));
		return math::cond(v<float1::zero(),min,max);
	}	

	STATIC_INLINE float4 LimitUnproject(Limit const& l, float4 const& v)
	{
		const float4 min = cond(l.m_Min<float1::zero(),-v*l.m_Min,cond(l.m_Min>float1::zero(),v,float4::zero()));
		const float4 max = cond(l.m_Max>float1::zero(),+v*l.m_Max,cond(l.m_Max<float1::zero(),v,float4::zero()));
		return math::cond(v<float1::zero(),min,max);
	}

	STATIC_INLINE float4 AxesProject(Axes const& a, float4 const& q)
	{
		return normalize(quatMul(quatConj(a.m_PreQ),quatMul(q,a.m_PostQ)));
	}

	STATIC_INLINE float4 AxesUnproject(Axes const& a, float4 const& q)
	{
		return normalize(quatMul(a.m_PreQ,quatMul(q,quatConj(a.m_PostQ))));
	}

	STATIC_INLINE float4 ToAxes(Axes const& a, float4 const& q)
	{
		const float4 qp = AxesProject(a,q);
		float4 xyz;
		switch(a.m_Type)
		{
			case kEulerXYZ: xyz = LimitProject(a.m_Limit, quatQuatToEuler(qp)); break;
			case kZYRoll: xyz = LimitProject(a.m_Limit,doubleAtan(quat2ZYRoll(qp)*sgn(a.m_Sgn))); break;
			case kRollZY: xyz = LimitProject(a.m_Limit,doubleAtan(quat2RollZY(qp)*sgn(a.m_Sgn))); break;
			default: xyz = LimitProject(a.m_Limit,doubleAtan(quat2Qtan(qp)*sgn(a.m_Sgn))); break;
		};		
		return xyz;
	}

	STATIC_INLINE float4 FromAxes(Axes const& a, float4 const& uvw)
	{
		float4 q;
		switch(a.m_Type)
		{
			case kEulerXYZ: q = quatEulerToQuat(uvw); break;
			case kZYRoll: q = ZYRoll2Quat(halfTan(LimitUnproject(a.m_Limit,uvw))*sgn(a.m_Sgn)); break;
			case kRollZY: q = RollZY2Quat(halfTan(LimitUnproject(a.m_Limit,uvw))*sgn(a.m_Sgn)); break;
			default: q = qtan2Quat(halfTan(LimitUnproject(a.m_Limit,uvw))*sgn(a.m_Sgn)); break;
		};

		return AxesUnproject(a,q);
	}
}
