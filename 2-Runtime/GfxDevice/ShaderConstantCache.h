#pragma once

#include "GfxDeviceTypes.h"
#include "Runtime/Math/Vector4.h"
//#include "Runtime/Utilities/fixed_bitset.h"
#include "D3D9Utils.h"


template<int SIZE>
class ShaderConstantCache2 {
public:
	ShaderConstantCache2() { memset(m_Values, 0, sizeof(m_Values)); Invalidate(); }

	void SetValues( int index, const float* values, int count )
	{
		DebugAssert( index >= 0 && count > 0 && (index + count) <= SIZE );
		float* dest = m_Values[index].GetPtr();
		UInt8 andedFlags = m_Flags[index];
		for (int i = 1; i < count; i++)
			andedFlags &= m_Flags[index + i];
		// Not worth filtering values bigger than one register
		if (andedFlags == kValid && count == 1)
		{
			// We have a valid value which is not dirty
			UInt32* destInt32 = reinterpret_cast<UInt32*>(dest);
			int sizeInt32 = count * sizeof(Vector4f) / sizeof(UInt32);
			if (CompareArrays(destInt32, reinterpret_cast<const UInt32*>(values), sizeInt32))
				return;
		}
		memcpy(dest, values, count * sizeof(Vector4f));
		// If all values are marked as dirty we are done
		if (andedFlags & kDirty)
			return;
		// Update flags
		for (int i = 0; i < count; i++)
			m_Flags[index + i] = kValid | kDirty;
		// Add dirty range or append to last range
		if (!m_DirtyRanges.empty() && m_DirtyRanges.back().second == index)
			m_DirtyRanges.back().second += count;
		else
			m_DirtyRanges.push_back(Range(index, index + count));
	}

	void Invalidate()
	{
		memset(m_Flags, 0, sizeof(m_Flags));
	}

	void CommitVertexConstants()
	{
		IDirect3DDevice9* dev = GetD3DDevice();
		int numRanges = m_DirtyRanges.size();
		for (int i = 0; i < numRanges; i++)
		{
			const Range& range = m_DirtyRanges[i];
			int size = range.second - range.first;
			D3D9_CALL(dev->SetVertexShaderConstantF( range.first, m_Values[range.first].GetPtr(), size ));
			// Update flags
			for (int i = 0; i < size; i++)
				m_Flags[range.first + i] = kValid;
		}
		m_DirtyRanges.clear();
	}

	void CommitPixelConstants()
	{
		IDirect3DDevice9* dev = GetD3DDevice();
		int numRanges = m_DirtyRanges.size();
		for (int i = 0; i < numRanges; i++)
		{
			const Range& range = m_DirtyRanges[i];
			int size = range.second - range.first;
			D3D9_CALL(dev->SetPixelShaderConstantF( range.first, m_Values[range.first].GetPtr(), size ));
			// Update flags
			for (int i = 0; i < size; i++)
				m_Flags[range.first + i] = kValid;
		}
		m_DirtyRanges.clear();
	}

private:
	enum
	{
		kValid = 1 << 0,
		kDirty = 1 << 1
	};
	UInt8 m_Flags[SIZE];
	Vector4f m_Values[SIZE];
	typedef std::pair<int,int> Range;
	std::vector<Range>	m_DirtyRanges;
};


template<int SIZE>
class ShaderConstantCache {
public:
	ShaderConstantCache() { Invalidate(); }

	bool CheckCache( int index, const float value[4] )
	{
		DebugAssertIf( index < 0 );
		if( index >= SIZE )
			return false;

		if( m_Values[index] == value )
			return true;

		m_Values[index].Set( value );
		return false;
	}

	bool CheckCache( int index, const float* values, int count )
	{
		DebugAssertIf( index < 0 );

		// Checking whole range for validity seems to be overkill from profiling (just slows it down a bit)
		// So just invalidate those cache entries.

		if( index + count > SIZE )
			count = SIZE-index;

		for( int i = 0; i < count; ++i )
		{
			m_Values[index+i].x = -std::numeric_limits<float>::infinity();
		}
		return false;
	}

	void Invalidate()
	{
		for (int i = 0; i < SIZE; ++i)
			m_Values[i].x = -std::numeric_limits<float>::infinity();
	}

private:
	Vector4f	m_Values[SIZE];
};


typedef ShaderConstantCache2<256> VertexShaderConstantCache;
typedef ShaderConstantCache2<256> PixelShaderConstantCache;

