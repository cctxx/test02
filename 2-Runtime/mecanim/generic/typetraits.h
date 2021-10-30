#pragma once

#include "Runtime/mecanim/types.h"
#include "Runtime/Math/Simd/float4.h"
#include "Runtime/Math/Simd/xform.h"
#include "Runtime/Math/Simd/bool4.h"

namespace mecanim
{
	enum ValueType
	{
		kFloatType = 1,
		kInt32Type = 3,
		kBoolType = 4,
		kPositionType = 6,
		kQuaternionType = 7,
		kScaleType = 8,
		kTriggerType = 9,
		kLastType	
	};

	template<typename TYPE> struct traits;

	template<> struct traits<int32_t>
	{
		typedef int32_t value_type;

		static value_type zero() { return 0; }
		static ValueType type() { return kInt32Type; }
	};

	template<> struct traits<float>
	{
		typedef float value_type;

		static value_type zero() { return 0.f; }
		static ValueType type() { return kFloatType; }
	};

	template<> struct traits<bool>
	{
		typedef bool value_type;

		static value_type zero() { return false; }
		static ValueType type() { return kBoolType; }
	};
}
