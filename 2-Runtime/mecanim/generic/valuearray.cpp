#include "UnityPrefix.h"
#include "Runtime/mecanim/generic/valuearray.h"

#include "Runtime/Math/Simd/float4.h"
#include "Runtime/Math/Simd/bool4.h"
#include "Runtime/Math/Simd/xform.h"

namespace mecanim
{
	template<typename TYPE>
	static void ValueCopy(TYPE const * RESTRICT source, TYPE * RESTRICT destination, uint32_t sourceIndex, uint32_t destinationIndex)
	{
		destination[destinationIndex] = source[sourceIndex];
	}

	template<typename TYPE>
	static void ValueArrayCopy(TYPE const * RESTRICT source, TYPE * RESTRICT destination, uint32_t sourceCount, uint32_t destinationCount)
	{
		sourceCount = min (destinationCount, sourceCount);
		
		uint32_t i;
		for(i = 0; i < sourceCount; ++i)
		{
			destination[i] = source[i];	
		}
	}
	
	template<typename TYPE>
	static void ValueArrayCopyMask(TYPE const * RESTRICT source, TYPE * RESTRICT destination, const bool* RESTRICT mask, uint32_t sourceCount)
	{
		uint32_t i;
		for(i = 0; i < sourceCount; ++i)
		{
			if (mask[i])
				destination[i] = source[i];	
		}
	}

	void SetupValueArrayConstant(ValueArrayConstant* cst, ValueType aType, uint32_t aCount, memory::Allocator& alloc)
	{
		cst->m_Count = aCount;
		cst->m_ValueArray = alloc.ConstructArray<ValueConstant>(cst->m_Count);

		uint32_t i;
		for(i=0;i<aCount;++i)
		{
			cst->m_ValueArray[i].m_Type = aType;
			cst->m_ValueArray[i].m_Index = i;
		}
	}

	ValueArrayConstant* CreateValueArrayConstant(uint32_t* apTypeArray, uint32_t aCount, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ValueArrayConstant);

		ValueArrayConstant* cst = alloc.Construct<ValueArrayConstant>();

		cst->m_Count = aCount;
		cst->m_ValueArray = alloc.ConstructArray<ValueConstant>(cst->m_Count);

		uint32_t positionCount = 0;
		uint32_t quaternionCount = 0;
		uint32_t scaleCount = 0;
		uint32_t floatCount = 0;
		uint32_t intCount = 0;
		uint32_t boolCount = 0;

		uint32_t i;
		for(i=0;i<aCount;++i)
		{
			cst->m_ValueArray[i].m_Type = apTypeArray[i];

			switch(apTypeArray[i])
			{
				case kPositionType: cst->m_ValueArray[i].m_Index = positionCount++; break;
				case kQuaternionType: cst->m_ValueArray[i].m_Index = quaternionCount++; break;
				case kScaleType: cst->m_ValueArray[i].m_Index = scaleCount++; break;
				case kFloatType: cst->m_ValueArray[i].m_Index = floatCount++; break;
				case kInt32Type: cst->m_ValueArray[i].m_Index = intCount++; break;
				case kTriggerType:
				case kBoolType: cst->m_ValueArray[i].m_Index = boolCount++; break;
				default: assert(false); break;
			}
		}
		return cst;
	}

	ValueArrayConstant* CreateValueArrayConstant(ValueType aType, uint32_t aCount, memory::Allocator& alloc)
	{
		
		SETPROFILERLABEL(ValueArrayConstant);
		ValueArrayConstant* cst = alloc.Construct<ValueArrayConstant>();

		SetupValueArrayConstant(cst, aType, aCount, alloc);
		return cst;
	}
	
	ValueArrayConstant* CreateValueArrayConstantCopy (const ValueArrayConstant* sourceConstant, uint32_t count, memory::Allocator& alloc)
	{
		
		SETPROFILERLABEL(ValueArrayConstant);

		Assert(count <= sourceConstant->m_Count);
		
		ValueArrayConstant* cst = alloc.Construct<ValueArrayConstant>();
		cst->m_Count = count;
		cst->m_ValueArray = alloc.ConstructArray<ValueConstant>(sourceConstant->m_ValueArray.Get(), count);
		
		return cst;
	}


	void DestroyValueArrayConstant(ValueArrayConstant * apCst, memory::Allocator& alloc)
	{
		if(apCst)
		{
			alloc.Deallocate(apCst->m_ValueArray);
			alloc.Deallocate(apCst);
		}
	}

	void SetValueMask(ValueArrayMask *valueArrayMask, bool value)
	{
		for(int i = 0; i < valueArrayMask->m_PositionCount; i++)
			valueArrayMask->m_PositionValues[i] = value;
		
		for(int i = 0; i < valueArrayMask->m_QuaternionCount; i++)
			valueArrayMask->m_QuaternionValues[i] = value;

		for(int i = 0; i < valueArrayMask->m_ScaleCount; i++)
			valueArrayMask->m_ScaleValues[i] = value;
		
		for(int i = 0; i < valueArrayMask->m_FloatCount; i++)
			valueArrayMask->m_FloatValues[i] = value;
		
		for(int i = 0; i < valueArrayMask->m_IntCount; i++)
			valueArrayMask->m_IntValues[i] = value;
	}

	void CopyValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask)
	{
		for(int i = 0; i < valueArrayMask->m_PositionCount; i++)
            valueArrayMask->m_PositionValues[i] = srcValueArrayMask->m_PositionValues[i];

		for(int i = 0; i < valueArrayMask->m_QuaternionCount; i++)
            valueArrayMask->m_QuaternionValues[i] = srcValueArrayMask->m_QuaternionValues[i];
		
		for(int i = 0; i < valueArrayMask->m_ScaleCount; i++)
            valueArrayMask->m_ScaleValues[i] = srcValueArrayMask->m_ScaleValues[i];
		
		for(int i = 0; i < valueArrayMask->m_FloatCount; i++)
            valueArrayMask->m_FloatValues[i] = srcValueArrayMask->m_FloatValues[i];
		
		for(int i = 0; i < valueArrayMask->m_IntCount; i++)
            valueArrayMask->m_IntValues[i] = srcValueArrayMask->m_IntValues[i];
	}

	void OrValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask)
	{
		for(int i = 0; i < valueArrayMask->m_PositionCount; i++)
			valueArrayMask->m_PositionValues[i] = valueArrayMask->m_PositionValues[i] || srcValueArrayMask->m_PositionValues[i];
		
		for(int i = 0; i < valueArrayMask->m_QuaternionCount; i++)
			valueArrayMask->m_QuaternionValues[i] = valueArrayMask->m_QuaternionValues[i] || srcValueArrayMask->m_QuaternionValues[i];
		
		for(int i = 0; i < valueArrayMask->m_ScaleCount; i++)
            valueArrayMask->m_ScaleValues[i] = valueArrayMask->m_ScaleValues[i] || srcValueArrayMask->m_ScaleValues[i];
		
		for(int i = 0; i < valueArrayMask->m_FloatCount; i++)
            valueArrayMask->m_FloatValues[i] = valueArrayMask->m_FloatValues[i] || srcValueArrayMask->m_FloatValues[i];
		
		for(int i = 0; i < valueArrayMask->m_IntCount; i++)
            valueArrayMask->m_IntValues[i] = valueArrayMask->m_IntValues[i] || srcValueArrayMask->m_IntValues[i];
	}

	void AndValueMask(ValueArrayMask *valueArrayMask,ValueArrayMask const *srcValueArrayMask)
	{
		for(int i = 0; i < valueArrayMask->m_PositionCount; i++)
            valueArrayMask->m_PositionValues[i] = valueArrayMask->m_PositionValues[i] && srcValueArrayMask->m_PositionValues[i];

		for(int i = 0; i < valueArrayMask->m_QuaternionCount; i++)
            valueArrayMask->m_QuaternionValues[i] = valueArrayMask->m_QuaternionValues[i] && srcValueArrayMask->m_QuaternionValues[i];
		
		for(int i = 0; i < valueArrayMask->m_ScaleCount; i++)
            valueArrayMask->m_ScaleValues[i] = valueArrayMask->m_ScaleValues[i] && srcValueArrayMask->m_ScaleValues[i];
		
		for(int i = 0; i < valueArrayMask->m_FloatCount; i++)
            valueArrayMask->m_FloatValues[i] = valueArrayMask->m_FloatValues[i] && srcValueArrayMask->m_FloatValues[i];
		
		for(int i = 0; i < valueArrayMask->m_IntCount; i++)
            valueArrayMask->m_IntValues[i] = valueArrayMask->m_IntValues[i] && srcValueArrayMask->m_IntValues[i];
	}

	void InvertValueMask(ValueArrayMask *valueArrayMask)
	{
		for(int i = 0; i < valueArrayMask->m_PositionCount; i++)
            valueArrayMask->m_PositionValues[i] = !valueArrayMask->m_PositionValues[i];

		for(int i = 0; i < valueArrayMask->m_QuaternionCount; i++)
            valueArrayMask->m_QuaternionValues[i] = !valueArrayMask->m_QuaternionValues[i];
		
		for(int i = 0; i < valueArrayMask->m_ScaleCount; i++)
            valueArrayMask->m_ScaleValues[i] = !valueArrayMask->m_ScaleValues[i];

		for(int i = 0; i < valueArrayMask->m_FloatCount; i++)
            valueArrayMask->m_FloatValues[i] = !valueArrayMask->m_FloatValues[i];

		for(int i = 0; i < valueArrayMask->m_IntCount; i++)
            valueArrayMask->m_IntValues[i] = !valueArrayMask->m_IntValues[i];
	}

    ValueArrayMask* CreateValueArrayMask(ValueArrayConstant const* constant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ValueArrayMask);
        ValueArrayMask* valueArrayMask = alloc.Construct<ValueArrayMask>();

		uint32_t i;
		for(i=0;i<constant->m_Count;++i)
		{
			switch(constant->m_ValueArray[i].m_Type)
			{
				case kPositionType: valueArrayMask->m_PositionCount++; break;
				case kQuaternionType: valueArrayMask->m_QuaternionCount++; break;
				case kScaleType: valueArrayMask->m_ScaleCount++; break;
				case kFloatType: valueArrayMask->m_FloatCount++; break;
				case kInt32Type: valueArrayMask->m_IntCount++; break;
				default: assert(false); break;
			}
		}
        
		valueArrayMask->m_IntValues = alloc.ConstructArray<bool>(valueArrayMask->m_IntCount);
		valueArrayMask->m_FloatValues = alloc.ConstructArray<bool>(valueArrayMask->m_FloatCount);
		valueArrayMask->m_PositionValues = alloc.ConstructArray<bool>(valueArrayMask->m_PositionCount);
		valueArrayMask->m_QuaternionValues = alloc.ConstructArray<bool>(valueArrayMask->m_QuaternionCount);
		valueArrayMask->m_ScaleValues = alloc.ConstructArray<bool>(valueArrayMask->m_ScaleCount);
		
		SetValueMask (valueArrayMask, false);
		
		return valueArrayMask;
	}
	
	void DestroyValueArrayMask(ValueArrayMask *valueArrayMask, memory::Allocator& alloc)
	{
		if(valueArrayMask)
		{
			alloc.Deallocate(valueArrayMask->m_IntValues);
			alloc.Deallocate(valueArrayMask->m_FloatValues);
			alloc.Deallocate(valueArrayMask->m_PositionValues);
			alloc.Deallocate(valueArrayMask->m_QuaternionValues);
			alloc.Deallocate(valueArrayMask->m_ScaleValues);
			alloc.Deallocate(valueArrayMask);
		}
	}
	
	ValueArray* CreateValueArray(ValueArrayConstant const*	apValueArrayConstant, memory::Allocator& alloc)
	{
		SETPROFILERLABEL(ValueArray);
		ValueArray* valueArray = alloc.Construct<ValueArray>();

		uint32_t i;
		for(i=0;i<apValueArrayConstant->m_Count;++i)
		{
			switch(apValueArrayConstant->m_ValueArray[i].m_Type)
			{
				case kPositionType: valueArray->m_PositionCount++; break;
				case kQuaternionType: valueArray->m_QuaternionCount++; break;
				case kScaleType: valueArray->m_ScaleCount++; break;
				case kFloatType: valueArray->m_FloatCount++; break;
				case kInt32Type: valueArray->m_IntCount++; break;
				case kTriggerType:
				case kBoolType: valueArray->m_BoolCount++; break;
				default: assert(false); break;
			}
		}

		valueArray->m_BoolValues = alloc.ConstructArray<bool>(valueArray->m_BoolCount);
		valueArray->m_IntValues = alloc.ConstructArray<int32_t>(valueArray->m_IntCount);
		valueArray->m_FloatValues = alloc.ConstructArray<float>(valueArray->m_FloatCount);
		valueArray->m_PositionValues = alloc.ConstructArray<math::float4>(valueArray->m_PositionCount);
		valueArray->m_QuaternionValues = alloc.ConstructArray<math::float4>(valueArray->m_QuaternionCount);
		valueArray->m_ScaleValues = alloc.ConstructArray<math::float4>(valueArray->m_ScaleCount);

		for(i=0;i<valueArray->m_BoolCount;++i)
			valueArray->m_BoolValues[i] = false;

		for(i=0;i<valueArray->m_IntCount;++i)
			valueArray->m_IntValues[i] = 0;

		for(i=0;i<valueArray->m_FloatCount;++i)
			valueArray->m_FloatValues[i] = 0.f;

		for(i=0;i<valueArray->m_PositionCount;++i)
			valueArray->m_PositionValues[i] = math::float4::zero();

		for(i=0;i<valueArray->m_QuaternionCount;++i)
			valueArray->m_QuaternionValues[i] = math::quatIdentity();

		for(i=0;i<valueArray->m_ScaleCount;++i)
			valueArray->m_ScaleValues[i] = math::float4::one();

		return valueArray;
	}

	void DestroyValueArray(ValueArray * apInput, memory::Allocator& alloc)
	{
		if(apInput)
		{
			alloc.Deallocate(apInput->m_BoolValues);
			alloc.Deallocate(apInput->m_IntValues);
			alloc.Deallocate(apInput->m_FloatValues);
			alloc.Deallocate(apInput->m_PositionValues);
			alloc.Deallocate(apInput->m_QuaternionValues);
			alloc.Deallocate(apInput->m_ScaleValues);

			alloc.Deallocate(apInput);
		}
	}

	void ValueArrayCopy(ValueArray const* apSourceValueArray, ValueArray * apDestinationValueArray)
	{
		ValueArrayCopy(apSourceValueArray->m_BoolValues.Get(), apDestinationValueArray->m_BoolValues.Get(), apSourceValueArray->m_BoolCount, apDestinationValueArray->m_BoolCount); 
		ValueArrayCopy(apSourceValueArray->m_IntValues.Get(), apDestinationValueArray->m_IntValues.Get(), apSourceValueArray->m_IntCount, apDestinationValueArray->m_IntCount);  
		ValueArrayCopy(apSourceValueArray->m_FloatValues.Get(), apDestinationValueArray->m_FloatValues.Get(), apSourceValueArray->m_FloatCount, apDestinationValueArray->m_FloatCount);  
		ValueArrayCopy(apSourceValueArray->m_PositionValues.Get(), apDestinationValueArray->m_PositionValues.Get(), apSourceValueArray->m_PositionCount, apDestinationValueArray->m_PositionCount);
		ValueArrayCopy(apSourceValueArray->m_QuaternionValues.Get(), apDestinationValueArray->m_QuaternionValues.Get(), apSourceValueArray->m_QuaternionCount, apDestinationValueArray->m_QuaternionCount);
		ValueArrayCopy(apSourceValueArray->m_ScaleValues.Get(), apDestinationValueArray->m_ScaleValues.Get(), apSourceValueArray->m_ScaleCount, apDestinationValueArray->m_ScaleCount);
	}

	void ValueArrayCopy(ValueArrayConstant const* sourceConstant, ValueArray const* source, ValueArrayConstant const* destinationConstant, ValueArray* destination, int32_t const* destinationInSourceIndexArray)
	{
		int dstCount = destinationConstant->m_Count;

		for(int dstIter = 0; dstIter < dstCount; dstIter++)
		{
			int32_t srcIndex = destinationInSourceIndexArray[dstIter];
			int32_t dstIndex = dstIter;

			if(srcIndex != -1 && sourceConstant->m_ValueArray[srcIndex].m_Type == destinationConstant->m_ValueArray[dstIndex].m_Type)
			{
				switch(sourceConstant->m_ValueArray[srcIndex].m_Type)
				{
					case kPositionType:
					{
						math::float4 value = source->ReadPosition(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WritePosition(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kQuaternionType:
					{
						math::float4 value = source->ReadQuaternion(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteQuaternion(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kScaleType:
					{
						math::float4 value = source->ReadScale(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteScale(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kFloatType:
					{
						float value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);						
						break;
					}
					case kInt32Type:
					{
						int32_t value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kTriggerType:
					case kBoolType:
					{
						bool value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
				}		
			}
		}
	}

	void ValueArrayReverseCopy(ValueArrayConstant const* sourceConstant, ValueArray const* source, ValueArrayConstant const* destinationConstant, ValueArray* destination, int32_t const* sourceInDestinationIndexArray)
	{
		int srcCount = sourceConstant->m_Count;

		for(int srcIter = 0; srcIter < srcCount; srcIter++)
		{
			int32_t dstIndex = sourceInDestinationIndexArray[srcIter];
			int32_t srcIndex = srcIter;

			if(dstIndex != -1 && sourceConstant->m_ValueArray[srcIndex].m_Type == destinationConstant->m_ValueArray[dstIndex].m_Type)
			{
				switch(sourceConstant->m_ValueArray[srcIndex].m_Type)
				{
					case kPositionType:
					{
						math::float4 value = source->ReadPosition(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WritePosition(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kQuaternionType:
					{
						math::float4 value = source->ReadQuaternion(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteQuaternion(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kScaleType:
					{
						math::float4 value = source->ReadScale(sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteScale(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kFloatType:
					{
						float value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);						
						break;
					}
					case kInt32Type:
					{
						int32_t value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
					case kTriggerType:
					case kBoolType:
					{
						bool value;
						source->ReadData(value, sourceConstant->m_ValueArray[srcIndex].m_Index);
						destination->WriteData(value, destinationConstant->m_ValueArray[dstIndex].m_Index);
						break;
					}
				}		
			}
		}
	}

	void ValueArrayCopy(ValueArray const *aSource, ValueArray* aValues, ValueArrayMask const *mask)
	{
		ValueArrayCopyMask (aSource->m_PositionValues.Get(), aValues->m_PositionValues.Get(), mask->m_PositionValues.Get(), aValues->m_PositionCount);
		ValueArrayCopyMask (aSource->m_QuaternionValues.Get(), aValues->m_QuaternionValues.Get(), mask->m_QuaternionValues.Get(), aValues->m_QuaternionCount);
		ValueArrayCopyMask (aSource->m_ScaleValues.Get(), aValues->m_ScaleValues.Get(), mask->m_ScaleValues.Get(), aValues->m_ScaleCount);
		ValueArrayCopyMask (aSource->m_FloatValues.Get(), aValues->m_FloatValues.Get(), mask->m_FloatValues.Get(), aValues->m_FloatCount);
		ValueArrayCopyMask (aSource->m_IntValues.Get(), aValues->m_IntValues.Get(), mask->m_IntValues.Get(), aValues->m_IntCount);
		ValueArrayCopyMask (aSource->m_BoolValues.Get(), aValues->m_BoolValues.Get(), mask->m_BoolValues.Get(), aValues->m_BoolCount);
	}

	static uint32_t GetLargestBlendIndex(const float *apWeightArray, uint32_t aCount)
	{
		float largest = apWeightArray[0];
		uint32_t index = 0;
		for (int i=1;i<aCount;i++)
		{
			if (apWeightArray[i] > largest)
			{
				index = i;
				largest = apWeightArray[i];
			}
		}
			
		return index;
	}

	
	void ValueArrayBlend(ValueArray const *apValuesDefault,ValueArray* aValues, ValueArray ** aValuesArray, const float *apWeightArray, uint32_t aCount, ValueArrayMask const *mask)
	{
		// Blend positions
        for (int valueIndex=0;valueIndex<aValues->m_PositionCount;valueIndex++)
		{
			if(!mask->m_PositionValues[valueIndex])
                continue;
            
			float sumW = 0;
            math::float4 value4 = math::float4::zero();
            
            for(int blendIter = 0; blendIter < aCount; blendIter++)
            {
				sumW += apWeightArray[blendIter];
                math::float1 w = math::float1(apWeightArray[blendIter]);
                
                math::float4 valuei = aValuesArray[blendIter]->ReadPosition(valueIndex);
                value4 += valuei*w;
            }
            
			if(sumW < 1.0f)
			{
				math::float1 wd = math::float1(1.0-sumW);
				math::float4 valued = apValuesDefault->ReadPosition(valueIndex);
                value4 += valued*math::float1(wd);
			}

            aValues->WritePosition(value4,valueIndex);
        }
		
		// Blend Quaternions
        for (int valueIndex=0;valueIndex<aValues->m_QuaternionCount;valueIndex++)
		{
			if(!mask->m_QuaternionValues[valueIndex])
                continue;
            
            float sumW = 0;
            math::float4 value4 = math::float4::zero();

            for(int blendIter = 0; blendIter < aCount; blendIter++)
            {
                sumW += apWeightArray[blendIter];
				math::float1 w = math::float1(apWeightArray[blendIter]);
                
                math::float4 valuei = aValuesArray[blendIter]->ReadQuaternion(valueIndex);
                value4 += math::cond(math::dot(value4,valuei) < math::float1::zero(), valuei * -w, valuei * w);
            }
            
			if(sumW < 1.0f)
			{
				math::float1 wd = math::float1(1.0-sumW);
                math::float4 valued = apValuesDefault->ReadQuaternion(valueIndex);
                value4 += math::cond(math::dot(value4,valued) < math::float1::zero(), valued * -wd, valued * wd);
			}

			value4 = math::normalize(value4);
            aValues->WriteQuaternion(value4,valueIndex);
        }
        
		// Blend scale
        for (int valueIndex=0;valueIndex<aValues->m_ScaleCount;valueIndex++)
		{
			if(!mask->m_ScaleValues[valueIndex])
                continue;
            
			float sumW = 0;
            math::float4 value4 = math::float4::one();
            
            for(int blendIter = 0; blendIter < aCount; blendIter++)
            {
                sumW += apWeightArray[blendIter];
				math::float1 w = math::float1(apWeightArray[blendIter]);
                
                math::float4 valuei = aValuesArray[blendIter]->ReadScale(valueIndex);
				math::float4 sng = math::sgn(valuei);
				value4 = sng * math::abs( value4 * scaleWeight(valuei, w));
            }
            
			if(sumW < 1.0f)
			{
				math::float1 wd = math::float1(1.0-sumW);

                math::float4 valued = apValuesDefault->ReadScale(valueIndex);
				math::float4 sng = math::sgn(valued);
				value4 = sng * math::abs( value4 * scaleWeight(valued, wd));
			}

            aValues->WriteScale(value4,valueIndex);
		}
        
        // Blend floats
        for (int valueIndex=0;valueIndex<aValues->m_FloatCount;valueIndex++)
		{
			if(!mask->m_FloatValues[valueIndex])
                continue;
            
			float sumW = 0;
            float value = 0;
            
            for(int blendIter = 0; blendIter < aCount; blendIter++)
            {
                float w = apWeightArray[blendIter];
                sumW += w;
                
                float valuei;
                aValuesArray[blendIter]->ReadData(valuei,valueIndex);
                value += valuei*w;
            }
            
			if(sumW < 1.0f)
			{
				float wd = 1.0-sumW;

                float valued;
                apValuesDefault->ReadData(valued,valueIndex);
                value += valued*wd;
			}

            aValues->WriteData(value,valueIndex);
		}

		// Blend integers (pick by largest weight)
		uint32_t largestBlendIndex = GetLargestBlendIndex (apWeightArray, aCount);
		for (int valueIndex=0;valueIndex<aValues->m_IntCount;valueIndex++)
		{
			if(!mask->m_IntValues[valueIndex])
                continue;
            
			int32_t valueInt;
			aValuesArray[largestBlendIndex]->ReadData(valueInt,valueIndex);
			aValues->WriteData(valueInt,valueIndex);
		}
	}
	
	void ValueArrayAdd(ValueArray const *apValuesDefault, ValueArray const* apValues, ValueArrayMask const *readMask, float aWeight, bool aAdditive, ValueArray* apValuesOut, ValueArrayMask *defaultMask)
	{
		math::float1 w(aWeight);
		float base1;
		float value1;
		math::float4 base4;
		math::float4 value4;
		int32_t valueInt;

		// Positions
        for (int valueIndex=0;valueIndex<apValues->m_PositionCount;valueIndex++)
		{
			if(!readMask->m_PositionValues[valueIndex])
                continue;

            value4 = apValues->ReadPosition(valueIndex);
            if(aAdditive)
            {
				 if(defaultMask->m_PositionValues[valueIndex])
                        base4 = apValuesDefault->ReadPosition(valueIndex);
                 else
						base4 = apValuesOut->ReadPosition(valueIndex);
                value4 = base4 + value4 * w;
            }
            else
            {
                if(aWeight < 1)
                {
                    if(defaultMask->m_PositionValues[valueIndex])
                        base4 = apValuesDefault->ReadPosition(valueIndex);
                    else
                        base4 = apValuesOut->ReadPosition(valueIndex);

                    value4 = math::lerp(base4,value4,w);
                }
            }
            apValuesOut->WritePosition(value4,valueIndex);
            defaultMask->m_PositionValues[valueIndex] = false;
        }
        
        // Quaternions
        for (int valueIndex=0;valueIndex<apValues->m_QuaternionCount;valueIndex++)
		{
			if(!readMask->m_QuaternionValues[valueIndex])
                continue;

            value4 = apValues->ReadQuaternion(valueIndex);
            if(aAdditive)
            {
				if(defaultMask->m_QuaternionValues[valueIndex])
					base4 = apValuesDefault->ReadQuaternion(valueIndex);
				else 
					base4 = apValuesOut->ReadQuaternion(valueIndex);

                value4 = math::quatMul(base4,math::quatWeight(value4,w));
            }
            else
            {
                if(aWeight < 1)
                {
                    if(defaultMask->m_QuaternionValues[valueIndex])
                        base4 = apValuesDefault->ReadQuaternion(valueIndex);
                    else
                        base4 = apValuesOut->ReadQuaternion(valueIndex);

                    value4 = math::quatLerp(base4,value4,w);
                }
            }
            apValuesOut->WriteQuaternion(value4,valueIndex);
            defaultMask->m_QuaternionValues[valueIndex] = false;
        }
        
		// Scale
        for (int valueIndex=0;valueIndex<apValues->m_ScaleCount;valueIndex++)
		{
			if(!readMask->m_ScaleValues[valueIndex])
                continue;
            
            value4 = apValues->ReadScale(valueIndex);
            if(aAdditive)
            {
				if(defaultMask->m_ScaleValues[valueIndex])
					base4 = apValuesDefault->ReadScale(valueIndex);
				else 
					base4 = apValuesOut->ReadScale(valueIndex);

                value4 = base4 * math::scaleWeight(value4,w);
            }
            else
            {
                if(aWeight < 1)
                {
                    if(defaultMask->m_ScaleValues[valueIndex])
                        base4 = apValuesDefault->ReadScale(valueIndex);
                    else
                        base4 = apValuesOut->ReadScale(valueIndex);

                    value4 = math::scaleBlend(base4,value4,w);
                }
            }
            apValuesOut->WriteScale(value4,valueIndex);
            defaultMask->m_ScaleValues[valueIndex] = false;
        }
        
		// Floats
        for (int valueIndex=0;valueIndex<apValues->m_FloatCount;valueIndex++)
		{
			if(!readMask->m_FloatValues[valueIndex])
                continue;

            apValues->ReadData(value1,valueIndex);
            if(aAdditive)
            {
				if(defaultMask->m_FloatValues[valueIndex])
					apValuesDefault->ReadData(base1,valueIndex);
				else
					apValuesOut->ReadData(base1,valueIndex);

                value1 = base1 + value1 * aWeight;
            }
            else
            {
                if(aWeight < 1)
                {
                    if(defaultMask->m_FloatValues[valueIndex])
                        apValuesDefault->ReadData(base1,valueIndex);
                    else
                        apValuesOut->ReadData(base1,valueIndex);

                    value1 = (1-aWeight) * base1 + value1 * aWeight; 
                }
            }
            apValuesOut->WriteData(value1,valueIndex);
            defaultMask->m_FloatValues[valueIndex] = false;
        }

		// Ints
        if (aWeight > 0.5F)
        {
			for (int valueIndex=0;valueIndex<apValues->m_IntCount;valueIndex++)
			{
				if(!readMask->m_IntValues[valueIndex])
	                continue;
				
				apValues->ReadData(valueInt,valueIndex);
				apValuesOut->WriteData(valueInt,valueIndex);
	            defaultMask->m_IntValues[valueIndex] = false;
			}
        }
        else
        {
            for (int valueIndex=0;valueIndex<apValues->m_IntCount;valueIndex++)
			{
				if(!readMask->m_IntValues[valueIndex])
	                continue;
				
                if(defaultMask->m_IntValues[valueIndex])
                    apValuesDefault->ReadData(valueInt,valueIndex);
                else
                    apValuesOut->ReadData(valueInt,valueIndex);

				apValuesOut->WriteData(valueInt,valueIndex);
	            defaultMask->m_IntValues[valueIndex] = false;
			}
        }
	}

	void ValueArraySub(ValueArray const &starts, ValueArray &values, ValueArrayMask const *mask)
	{
		float value,start;
		math::float4 value4,start4;

		// Positions
        for (int valueIndex=0;valueIndex<values.m_PositionCount;valueIndex++)
		{
			if(!mask->m_PositionValues[valueIndex])
                continue;

            value4 = values.ReadPosition(valueIndex);
            start4 = starts.ReadPosition(valueIndex);

            value4 -= start4;

            values.WritePosition(value4,valueIndex);
        }

		// Quaternions
        for (int valueIndex=0;valueIndex<values.m_QuaternionCount;valueIndex++)
		{
			if(!mask->m_QuaternionValues[valueIndex])
                continue;
            
            value4 = values.ReadQuaternion(valueIndex);
            start4 = starts.ReadQuaternion(valueIndex);

            value4 = math::normalize(math::quatMul(math::quatConj(start4),value4));

            values.WriteQuaternion(value4,valueIndex);
        }

		// Scale
        for (int valueIndex=0;valueIndex<values.m_ScaleCount;valueIndex++)
		{
			if(!mask->m_ScaleValues[valueIndex])
                continue;
            
            value4 = values.ReadScale(valueIndex);
            start4 = starts.ReadScale(valueIndex);

            value4 /= start4;

            values.WriteScale(value4,valueIndex);
        }

		// Floats
        for (int valueIndex=0;valueIndex<values.m_FloatCount;valueIndex++)
		{
			if(!mask->m_FloatValues[valueIndex])
                continue;
            
            values.ReadData(value,valueIndex);
            starts.ReadData(start,valueIndex);

            value -= start;
            
            values.WriteData(value,valueIndex);
        }
		
		// Integer substraction does not make sense
	}

	void ValueArrayLoop(	ValueArray const &starts, 
							ValueArray const &stops, 
							ValueArray &values, 
							float loopWeight,
							const ValueArrayMask& mask)
	{
		math::float1 loopWeight1(loopWeight);
		
		float value,start,stop;
		math::float4 value4,start4,stop4;

		// Positions
        for (int valueIndex=0;valueIndex<values.m_PositionCount;valueIndex++)
		{
			if(!mask.m_PositionValues[valueIndex])
                continue;
            
			value4 = values.ReadPosition(valueIndex);
			start4 = starts.ReadPosition(valueIndex);
			stop4 = stops.ReadPosition(valueIndex);
			
			value4 += (start4 - stop4) * loopWeight1;
			
			values.WritePosition(value4,valueIndex);
		}	

		// Quaternions
        for (int valueIndex=0;valueIndex<values.m_QuaternionCount;valueIndex++)
		{
			if(!mask.m_QuaternionValues[valueIndex])
                continue;
            
			value4 = values.ReadQuaternion(valueIndex);
			start4 = starts.ReadQuaternion(valueIndex);
			stop4 = stops.ReadQuaternion(valueIndex);
			
			value4 = math::normalize(math::quatMul(value4,math::quatWeight(math::quatMul(math::quatConj(stop4),start4),loopWeight1))); 
			
			values.WriteQuaternion(value4,valueIndex);
		}	

		// Scales
        for (int valueIndex=0;valueIndex<values.m_ScaleCount;valueIndex++)
		{
			if(!mask.m_ScaleValues[valueIndex])
                continue;
            
			value4 = values.ReadScale(valueIndex);
			start4 = starts.ReadScale(valueIndex);
			stop4 = stops.ReadScale(valueIndex);
			
			value4 *= math::scaleWeight(start4/stop4,loopWeight1);
			
			values.WriteScale(value4,valueIndex);
		}	

		// Floats
        for (int valueIndex=0;valueIndex<values.m_FloatCount;valueIndex++)
		{
			if(!mask.m_FloatValues[valueIndex])
                continue;
            
			values.ReadData(value,valueIndex);
			starts.ReadData(start,valueIndex);
			stops.ReadData(stop,valueIndex);
			
			value += (start - stop) * loopWeight;
			
			values.WriteData(value,valueIndex);
		}	
	}

	int32_t FindValueIndex(const ValueArrayConstant *aValueArrayConstant, uint32_t id)
	{
		int32_t ret = -1;
		if(aValueArrayConstant)
		{
			uint32_t i;
			for( i = 0 ; i < aValueArrayConstant->m_Count ; i++)
			{
				if(aValueArrayConstant->m_ValueArray[i].m_ID == id)
				{
					return i;
				}
			}
		}
		return ret;
	}
}
