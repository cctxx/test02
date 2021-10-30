#include "dynamic_bitset.h"
#include "Runtime/Serialize/TypeTree.h"
#include "Runtime/Serialize/SwapEndianArray.h"

template<>
class SerializeTraits<dynamic_bitset>: public SerializeTraitsBase<dynamic_bitset>
{
	public:

	typedef dynamic_bitset	value_type;

	inline static const char* GetTypeString (value_type*)	{ return "bitset"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return false; }
	inline static bool AllowTransferOptimization ()	{ return false; }
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		SInt32 bitCount = data.size ();
		transfer.Transfer (bitCount, "bitCount");
		
		unsigned byteSize = data.num_blocks () * sizeof (value_type::block_type);
		transfer.TransferTypeless  (&byteSize, "bitblocks");
		AssertIf (sizeof (value_type::block_type) != 4);
		AssertIf (byteSize % 4 != 0);

		if (transfer.IsReading ())
		{
			data.resize (bitCount);
			transfer.TransferTypelessData (byteSize, data.m_bits);
			if (transfer.ConvertEndianess ())
				SwapEndianArray (data.m_bits, sizeof (value_type::block_type), byteSize / 4);
			data.m_zero_unused_bits ();
		}
		else if (transfer.IsWriting ())
		{
			value_type::block_type* writeData = data.m_bits;
			if (transfer.ConvertEndianess ())
			{
				writeData = (value_type::block_type*)UNITY_MALLOC (kMemTempAlloc, byteSize);
				memcpy (writeData, data.m_bits, byteSize);
				SwapEndianArray (writeData, sizeof (value_type::block_type), byteSize / 4);
			}
			
			AssertIf (data.num_blocks () != byteSize / 4);
			transfer.TransferTypelessData (byteSize, writeData);
			
			if (transfer.ConvertEndianess ())
				UNITY_FREE (kMemTempAlloc, writeData);
		}
		else
			transfer.TransferTypelessData (byteSize, NULL);
	}
	// Deque<bool> converter
	template<class TransferFunction>
	static bool Convert (value_type& data, TransferFunction& transfer)
	{
		const TypeTree& oldTypeTree = transfer.GetActiveOldTypeTree ();
		const std::string& oldType = transfer.GetActiveOldTypeTree ().m_Type;
		if ((oldType == "vector" || oldType == "deque") && GetElementTypeFromContainer (oldTypeTree).m_Type == "bool")
		{
			std::deque<bool> dequeBool;
			transfer.TransferSTLStyleArray (dequeBool);
			data.resize (dequeBool.size ());

			std::deque<bool>::iterator d = dequeBool.begin ();
			for (int i=0;i<data.size ();i++)
			{
				data[i] = *d;
				d++;
			}
			return true;
		}
		else	
			return false;
	}
};
