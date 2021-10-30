#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"

#include "Runtime/mecanim/generic/crc32.h"
#include "Runtime/mecanim/generic/typetraits.h"

#include "Runtime/Serialize/Blobification/offsetptr.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Animation/MecanimArraySerialization.h"

namespace mecanim
{	
	struct ValueConstant
	{
		DEFINE_GET_TYPESTRING(ValueConstant)
		
		ValueConstant():m_ID(0),m_Type(kLastType),m_TypeID(0),m_Index(0){}

		uint32_t    m_ID;
		uint32_t    m_TypeID; //@TODO: This is deprecated. We should probably make this webplayer only?
		uint32_t	m_Type;
		uint32_t	m_Index;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER(m_ID);
			TRANSFER(m_TypeID);
			TRANSFER(m_Type);
			TRANSFER(m_Index);
		}
	};

	struct ValueArrayConstant
	{
		DEFINE_GET_TYPESTRING(ValueArrayConstant)

		ValueArrayConstant():m_Count(0){}

		uint32_t					m_Count;
		OffsetPtr<ValueConstant>	m_ValueArray;

		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_Count);
			MANUAL_ARRAY_TRANSFER2(ValueConstant, m_ValueArray, m_Count);
		}
	};

	struct ValueArrayMask
	{
		DEFINE_GET_TYPESTRING(ValueArrayConstant)
		
		ValueArrayMask():m_BoolCount(0),m_IntCount(0),m_FloatCount(0),m_PositionCount(0),m_QuaternionCount(0),m_ScaleCount(0) {}
		
		uint32_t			m_BoolCount;
		OffsetPtr<bool>		m_BoolValues;
		uint32_t			m_IntCount;
		OffsetPtr<bool>		m_IntValues;
		uint32_t			m_FloatCount;
		OffsetPtr<bool>		m_FloatValues;
		uint32_t			m_PositionCount;
		OffsetPtr<bool>		m_PositionValues;
		uint32_t			m_QuaternionCount;
		OffsetPtr<bool>		m_QuaternionValues;
		uint32_t			m_ScaleCount;
		OffsetPtr<bool>		m_ScaleValues;
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_BoolCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_BoolValues, m_BoolCount);

			TRANSFER_BLOB_ONLY(m_IntCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_IntValues, m_IntCount);

			TRANSFER_BLOB_ONLY(m_FloatCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_FloatValues, m_FloatCount);

			TRANSFER_BLOB_ONLY(m_PositionCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_PositionValues, m_PositionCount);

			TRANSFER_BLOB_ONLY(m_QuaternionCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_QuaternionValues, m_QuaternionCount);

			TRANSFER_BLOB_ONLY(m_ScaleCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_ScaleValues, m_ScaleCount);
		}
	};
	
	struct ValueArray
	{	
		DEFINE_GET_TYPESTRING(ValueArray)

		ValueArray():m_BoolCount(0),m_IntCount(0),m_FloatCount(0),m_PositionCount(0),m_QuaternionCount(0),m_ScaleCount(0) {}

		uint32_t				m_BoolCount;
		OffsetPtr<bool>			m_BoolValues;
		uint32_t				m_IntCount;
		OffsetPtr<int32_t>		m_IntValues;
		uint32_t				m_FloatCount;
		OffsetPtr<float>		m_FloatValues;
		uint32_t				m_PositionCount;
		OffsetPtr<math::float4>	m_PositionValues;
		uint32_t				m_QuaternionCount;
		OffsetPtr<math::float4>	m_QuaternionValues;
		uint32_t				m_ScaleCount;
		OffsetPtr<math::float4>	m_ScaleValues;

		MECANIM_FORCE_INLINE void ReadData(bool& data, uint32_t index)const
		{
			Assert(index < m_BoolCount);
			data = m_BoolValues[index];
		}
		
		MECANIM_FORCE_INLINE void WriteData(bool const& data, uint32_t index)
		{
			Assert(index < m_BoolCount);
			m_BoolValues[index] = data;
		}

		MECANIM_FORCE_INLINE void ReadData(int32_t& data, uint32_t index)const
		{
			Assert(index < m_IntCount);
			data = m_IntValues[index];
		}
		
		MECANIM_FORCE_INLINE void WriteData(int32_t const& data, uint32_t index)
		{
			Assert(index < m_IntCount);
			m_IntValues[index] = data;
		}

		MECANIM_FORCE_INLINE void ReadData(float& data, uint32_t index)const
		{
			Assert(index < m_FloatCount);
			data = m_FloatValues[index];
		}
		
		MECANIM_FORCE_INLINE void WriteData(float const& data, uint32_t index)
		{
			Assert(index < m_FloatCount);
			m_FloatValues[index] = data;
		}

		MECANIM_FORCE_INLINE math::float4 ReadPosition(uint32_t index)const
		{
			Assert(index < m_PositionCount);
			return m_PositionValues[index];
		}

		MECANIM_FORCE_INLINE void WritePosition(math::float4 const& data, uint32_t index)
		{
			Assert(index < m_PositionCount);
			m_PositionValues[index] = data;
		}
		
		MECANIM_FORCE_INLINE math::float4 ReadQuaternion(uint32_t index)const
		{
			Assert(index < m_QuaternionCount);
			return m_QuaternionValues[index];
		}

		MECANIM_FORCE_INLINE void WriteQuaternion(math::float4 const& data, uint32_t index)
		{
			Assert(index < m_QuaternionCount);
			m_QuaternionValues[index] = data;
		}

		MECANIM_FORCE_INLINE math::float4 ReadScale(uint32_t index)const
		{
			Assert(index < m_ScaleCount);
			return m_ScaleValues[index];
		}

		MECANIM_FORCE_INLINE void WriteScale(math::float4 const& data, uint32_t index)
		{
			Assert(index < m_ScaleCount);
			m_ScaleValues[index] = data;
		}

		const float* GetFloatValues () const
		{
			return m_FloatValues.Get();
		}

		float* GetFloatValues ()
		{
			return m_FloatValues.Get();
		}
		
		template<class TransferFunction>
		inline void Transfer (TransferFunction& transfer)
		{
			TRANSFER_BLOB_ONLY(m_BoolCount);
			MANUAL_ARRAY_TRANSFER2(bool, m_BoolValues, m_BoolCount);
			transfer.Align();

			TRANSFER_BLOB_ONLY(m_IntCount);
			MANUAL_ARRAY_TRANSFER2(int32_t, m_IntValues, m_IntCount);				

			TRANSFER_BLOB_ONLY(m_FloatCount);
			MANUAL_ARRAY_TRANSFER2(float, m_FloatValues, m_FloatCount);			
			
			TRANSFER_BLOB_ONLY(m_PositionCount);
			MANUAL_ARRAY_TRANSFER2(math::float4, m_PositionValues, m_PositionCount);			
			
			TRANSFER_BLOB_ONLY(m_QuaternionCount);
			MANUAL_ARRAY_TRANSFER2(math::float4, m_QuaternionValues, m_QuaternionCount);			

			TRANSFER_BLOB_ONLY(m_ScaleCount);
			MANUAL_ARRAY_TRANSFER2(math::float4, m_ScaleValues, m_ScaleCount);			
		}
	};

	void SetupValueArrayConstant(ValueArrayConstant* constant, ValueType aType, uint32_t aCount, memory::Allocator& alloc);	

	ValueArrayConstant* CreateValueArrayConstant(uint32_t*	typeArray, uint32_t count, memory::Allocator& alloc);
	ValueArrayConstant* CreateValueArrayConstantCopy(const ValueArrayConstant* constant, uint32_t count, memory::Allocator& alloc);
	ValueArrayConstant* CreateValueArrayConstant(ValueType type, uint32_t count, memory::Allocator& alloc);
	void DestroyValueArrayConstant(ValueArrayConstant * constant, memory::Allocator& alloc);

	ValueArrayMask* CreateValueArrayMask(ValueArrayConstant const*	constant, memory::Allocator& alloc);
	void DestroyValueArrayMask(ValueArrayMask *valueArrayMask, memory::Allocator& alloc);
	void SetValueMask(ValueArrayMask *valueArrayMask, bool value);
	void CopyValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask);
	void OrValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask);
	void AndValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask);
	void InvertValueMask(ValueArrayMask *valueArrayMask);

	ValueArray* CreateValueArray(ValueArrayConstant const*	constant, memory::Allocator& alloc);
	void DestroyValueArray(ValueArray * valueArray, memory::Allocator& alloc);

	void ValueArrayCopy(ValueArray const* source, ValueArray* destination);
	void ValueArrayCopy(ValueArrayConstant const* sourceConstant, ValueArray const* source, ValueArrayConstant const* destinationConstant, ValueArray* destination, int32_t const* destinationInSourceIndexArray);
	void ValueArrayReverseCopy(ValueArrayConstant const* sourceConstant, ValueArray const* source, ValueArrayConstant const* destinationConstant, ValueArray* destination, int32_t const* sourceInDestinationIndexArray);
	void ValueArrayCopy(ValueArray const *aSource, ValueArray* aValues, ValueArrayMask const *mask);

	void ValueArrayBlend(ValueArray const *apValuesDefault,ValueArray* aValues, ValueArray ** aValuesArray, const float *apWeightArray, uint32_t aCount, const ValueArrayMask *mask);

	void ValueArrayAdd(ValueArray const *apValuesDefault, ValueArray const* apValues, ValueArrayMask const *readMask, float aWeight, bool aAdditive, ValueArray* apValuesOut, ValueArrayMask *defaultMask);
	void ValueArraySub(ValueArray const &starts, ValueArray &values, ValueArrayMask const *mask);
	void ValueArrayLoop(ValueArray const &starts, ValueArray const &stops, ValueArray &values, float loopWeight, const ValueArrayMask& mask);

	int32_t FindValueIndex(const ValueArrayConstant *constant, uint32_t id);

	STATIC_INLINE int32_t FindValueIndex(const ValueArrayConstant *constant, char const* binding)
	{
		return FindValueIndex(constant, processCRC32(binding));
	}
}
