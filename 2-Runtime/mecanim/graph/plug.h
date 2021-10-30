/*
 Copyright (c) 7244339 Canada Inc. (Mecanim)
 All Rights Reserved.
*/
#pragma once

#include "Runtime/mecanim/defs.h"
#include "Runtime/mecanim/memory.h"
#include "Runtime/mecanim/types.h"
#include "Runtime/mecanim/object.h"

#include "Runtime/Math/Simd/float4.h"
#include "Runtime/Math/Simd/xform.h"

namespace mecanim
{

namespace graph
{
	class Node;

	enum ePlugType
	{
		InvalidPlugId,
		Float4Id,
		FloatId,
		UInt32Id,  // Not used anymore
		Int32Id,
		BoolId,
		Float1Id, // Not used anymore
		Bool4Id, // Not used anymore
		XformId,
		LastPlugId
	};

	template<typename T> struct Trait;
	
	template<> struct Trait<math::float4> { static ePlugType PlugType(){return Float4Id;} };
	template<> struct Trait<float> { static ePlugType PlugType(){return FloatId;} };
	template<> struct Trait<math::float1> { static ePlugType PlugType(){return FloatId;} };
	template<> struct Trait<uint32_t> { static ePlugType PlugType(){return UInt32Id;} };
	template<> struct Trait<int32_t> { static ePlugType PlugType(){return Int32Id;} };
	template<> struct Trait<bool> { static ePlugType PlugType(){return BoolId;} };
	template<> struct Trait<math::bool4> { static ePlugType PlugType(){return Bool4Id;} };
	template<> struct Trait<math::xform> { static ePlugType PlugType(){return XformId;} };
	
	class DataBlock
	{
	public:		
		uint32_t	m_PlugCount;
		uint32_t*	m_EvaluationId;
		char*		m_Buffer;
	
		bool IsDirty(uint32_t aPlugId, uint32_t aEvaluationId)const
		{
			return m_EvaluationId[aPlugId] != aEvaluationId;
		}
		template<typename TYPE> TYPE GetData(uint32_t offset)const
		{
			return *reinterpret_cast<TYPE*>(&m_Buffer[offset]);
		}
		template<typename TYPE> void SetData(TYPE const& arValue, uint32_t aPlugId, uint32_t offset, uint32_t aEvaluationId)
		{
			*reinterpret_cast<TYPE*>(&m_Buffer[offset]) = arValue;
			m_EvaluationId[aPlugId] = aEvaluationId;
		}
	};

	class EvaluationInfo
	{
	public:
		DataBlock	m_DataBlock;
		uint32_t	m_EvaluationId;
	};
	
	class GraphPlug
	{
	public:
		GraphPlug():m_PlugId(numeric_limits<uint32_t>::max_value),m_Input(false),m_Owner(0),m_Source(0),m_Offset(0){}
		GraphPlug(bool input, uint32_t id, Node* owner=0):m_PlugId(numeric_limits<uint32_t>::max_value),m_Input(input),m_Owner(owner),m_Source(0),m_ID(id){}

		virtual ~GraphPlug(){}

		virtual std::size_t ValueSize()const=0;
		virtual std::size_t ValueAlign()const=0;
		virtual ePlugType	PlugType()const=0;

		GraphPlug* GetSource(){return m_Source;}

		virtual bool ReadData(void* apData, EvaluationInfo& arEvaluationInfo)const=0;
		virtual bool WriteData(void const* apData, EvaluationInfo& arEvaluationInfo)const=0;

		uint32_t	m_PlugId;
		bool		m_Input;
		Node*		m_Owner;
		GraphPlug*	m_Source;
		uint32_t    m_ID;
		uint32_t	m_Offset;
	};

	struct Constant
	{
		Constant():m_Plug(0),m_VectorValue(0),m_FloatValue(0){}

		union {
			float		m_FloatValue;
			uint32_t	m_UIntValue;
			int32_t		m_IntValue;
			bool		m_BoolValue;
		};
		// Cannot put float4 in union because of operator=
		math::float4		m_VectorValue;
		graph::GraphPlug*	m_Plug;
	};


	STATIC_INLINE ePlugType GetPlugType(GraphPlug const& arPlug)
	{	
		return arPlug.PlugType();
	}
}

}
