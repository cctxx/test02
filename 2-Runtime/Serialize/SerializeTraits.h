#ifndef SERIALIZETRAITS_H
#define SERIALIZETRAITS_H

#include "TypeTree.h"
#include "Runtime/Utilities/LogAssert.h"
#include "SerializeUtility.h"
#include "SerializationMetaFlags.h"
#include "Runtime/Utilities/vector_utility.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/vector_set.h"
#include "Runtime/Utilities/dense_hash_map.h"
#include "Runtime/Utilities/dynamic_array.h"
#include <map>
#include <set>
#include <list>
#include <deque>
#include "SerializeTraitsBase.h"

class SerializedFile;
class SafeBinaryRead;

typedef void TransferTypelessCallback (UInt8* data, int byteSize, int instanceID, int userdata);

/*

	You can use SerializeTraits to setup transfer functions for classes where you can't change to code, eg. the STL.
	You might also want to use it when writing custom converters.
	
	template<>
	class SerializeTraits<Vector4f> : public SerializeTraitsBase<Vector4f>
	{
		public:
		
		typedef Vector4f value_type;

		inline static const char* GetTypeString () { return value_type::GetTypeString (); }

		template<class TransferFunction> inline
		static void Transfer (value_type& data, TransferFunction& transfer)
		{		
			data.Transfer (transfer);
		}
		
		template<class TransferFunction>
		static void Convert (value_type& data, TransferFunction& transfer)
		{
			const TypeTree& oldTypeTree = transfer.GetActiveOldTypeTree ();
			const std::string& oldType = transfer.GetActiveOldTypeTree ().m_Type;
			if (oldType == "Vector3f")
			{
				Vector3f temp = data;
				temp.Transfer (transfer);
				data = temp;
				return true;
			}
			else
				return false;
		}
		
		/// Returns whether or not a this type is to be treated as a seperate channel in the animation system
		static bool IsAnimationChannel () { return T::IsAnimationChannel (); }
	};

*/


#define DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS(x)		\
	inline static const char* GetTypeString (void* p = 0) { return #x; } \
	inline static bool IsAnimationChannel ()	{ return true; } \
	inline static bool MightContainPPtr ()	{ return false; } \
	inline static bool AllowTransferOptimization ()	{ return true; }

template<>
struct SerializeTraits<float> : public SerializeTraitsBaseForBasicType<float>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (float)
};

template<>
struct SerializeTraits<double> : public SerializeTraitsBaseForBasicType<double>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (double)
};

template<>
struct SerializeTraits<SInt32> : public SerializeTraitsBaseForBasicType<SInt32>
{
	// We use "int" rather than "SInt32" here for backwards-compatibility reasons.
	// "SInt32" and "int" used to be two different types (as were "UInt32" and "unsigned int")
	// that we now serialize through same path.  We use "int" instead of "SInt32" as the common
	// identifier as it was more common.
    DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (int)
};

template<>
struct SerializeTraits<UInt32> : public SerializeTraitsBaseForBasicType<UInt32>
{
    DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (unsigned int) // See definition of "int" above.
};

template<>
struct SerializeTraits<SInt64> : public SerializeTraitsBaseForBasicType<SInt64>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (SInt64)
};
template<>
struct SerializeTraits<UInt64> : public SerializeTraitsBaseForBasicType<UInt64>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (UInt64)
};

template<>
struct SerializeTraits<SInt16> : public SerializeTraitsBaseForBasicType<SInt16>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (SInt16)
};

template<>
struct SerializeTraits<UInt16> : public SerializeTraitsBaseForBasicType<UInt16>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (UInt16)
};

template<>
struct SerializeTraits<SInt8> : public SerializeTraitsBaseForBasicType<SInt8>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (SInt8)
};

template<>
struct SerializeTraits<UInt8> : public SerializeTraitsBaseForBasicType<UInt8>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (UInt8)
};

template<>
struct SerializeTraits<char> : public SerializeTraitsBaseForBasicType<char>
{
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (char)
};

template<>
struct SerializeTraits<bool> : public SerializeTraitsBase<bool>
{
	typedef bool value_type;
	DEFINE_GET_TYPESTRING_IS_ANIMATION_CHANNEL_TRAITS (bool)
	
	static int GetByteSize ()	{ return 1; }

    template<class TransferFunction> inline
    static void Transfer (value_type& data, TransferFunction& transfer)
    {
		#if (defined __ppc__) && !UNITY_WII
		AssertIf (sizeof(bool) != 4);
        UInt8& temp = *(reinterpret_cast<UInt8*>(&data) + 3);
		
        transfer.TransferBasicData (temp);
        
        // When running in debug mode in OS X (-O0 in gcc), 
		// bool values which are not exactly 0x01 are treated as false.
		// We don't want this. Cast UInt8 to bool to fix this.
		if (transfer.IsReading())
			data = temp;
        #if DEBUGMODE
		AssertIf((transfer.IsReading() || transfer.IsWriting()) && (reinterpret_cast<int&> (data) != 0 && reinterpret_cast<int&> (data) != 1));
        #endif
		#else
		AssertIf (sizeof(bool) != 1);
        UInt8& temp = reinterpret_cast<UInt8&>(data);
        transfer.TransferBasicData (temp);
        
        // When running in debug mode in OS X (-O0 in gcc), 
		// bool values which are not exactly 0x01 are treated as false.
		// We don't want this. Cast UInt8 to bool to fix this.
        #if DEBUGMODE
		if (transfer.IsReading())
			data = temp;
		// You constructor or Reset function is not setting the bool value to a defined value!
		AssertIf((transfer.IsReading() || transfer.IsWriting()) && (temp != 0 && temp != 1));
        #endif
		#endif
    }
};



#define DEFINE_GET_TYPESTRING_MAP_CONTAINER(x)						\
inline static const char* GetTypeString (void*)	{ return #x; } \
inline static bool IsAnimationChannel ()	{ return false; } \
inline static bool MightContainPPtr ()	{ return SerializeTraits<FirstClass>::MightContainPPtr() || SerializeTraits<SecondClass>::MightContainPPtr(); } \
inline static bool AllowTransferOptimization ()	{ return false; }

template<>
class SerializeTraits<UnityStr> : public SerializeTraitsBase<UnityStr>
{
public:

	typedef UnityStr	value_type;
	inline static const char* GetTypeString (value_type* x = NULL)	{ return "string"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return false; }
	inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
		static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data, kHideInEditorMask);
		transfer.Align();
	}

	static bool IsContinousMemoryArray ()	{ return true; }

	static void ResizeSTLStyleArray (value_type& data, int rs)
	{
		data.resize (rs, 1);
	}

};

// Do not add this serialization function. All serialized strings should use UnityStr instead of std::string
//template<class Traits, class Allocator>
//class SerializeTraits<std::basic_string<char,Traits,Allocator> > : public SerializeTraitsBase<std::basic_string<char,Traits,Allocator> >

template<class T, class Allocator>
class SerializeTraits<std::vector<T, Allocator> > : public SerializeTraitsBase<std::vector<T, Allocator> >
{
	public:

	typedef std::vector<T,Allocator>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}
		
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ resize_trimmed (data, rs); }
};

template<class Allocator>
class SerializeTraits<std::vector<UInt8,Allocator> > : public SerializeTraitsBase<std::vector<UInt8,Allocator> >
{
	public:

	typedef std::vector<UInt8,Allocator>	value_type;

	inline static const char* GetTypeString (void* x = NULL)	{ return "vector"; }
	inline static bool IsAnimationChannel ()	{ return false; } 
	inline static bool MightContainPPtr ()	{ return false; }
	inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
		transfer.Align();
	}
		
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ resize_trimmed (data, rs); }
};

template<class T, class Allocator>
class SerializeTraits<std::list<T,Allocator> > : public SerializeTraitsBase<std::list<T,Allocator> >
{
	public:

	typedef std::list<T,Allocator>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}

	static bool IsContinousMemoryArray ()	{ return false; }
	static void ResizeSTLStyleArray (value_type& data, int rs)	{ data.resize (rs); }
};

template<class FirstClass, class SecondClass>
class SerializeTraits<std::pair<FirstClass, SecondClass> > : public SerializeTraitsBase<std::pair<FirstClass, SecondClass> >
{
	public:

	typedef std::pair<FirstClass, SecondClass>	value_type;
	inline static const char* GetTypeString (void* x = NULL)	{ return "pair"; }
	inline static bool IsAnimationChannel ()	{ return false; }
	inline static bool MightContainPPtr ()	{ return SerializeTraits<FirstClass>::MightContainPPtr() || SerializeTraits<SecondClass>::MightContainPPtr(); }
//	inline static bool AllowTransferOptimization ()	{ return SerializeTraits<FirstClass>::AllowTransferOptimization() || SerializeTraits<SecondClass>::AllowTransferOptimization(); }
		inline static bool AllowTransferOptimization ()	{ return false; }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.Transfer (data.first, "first");
		transfer.Transfer (data.second, "second");
	}
};

template<class FirstClass, class SecondClass, class Compare, class Allocator>
class SerializeTraits<std::map<FirstClass, SecondClass, Compare, Allocator> > : public SerializeTraitsBase<std::map<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef std::map<FirstClass, SecondClass, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_MAP_CONTAINER(map)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && SerializeTraits<FirstClass>::MightContainPPtr() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};

template<class FirstClass, class SecondClass, class HashFunction, class Compare, class Allocator>
class SerializeTraits<dense_hash_map<FirstClass, SecondClass, HashFunction, Compare, Allocator> > : public SerializeTraitsBase<dense_hash_map<FirstClass, SecondClass, HashFunction, Compare, Allocator> >
{
	public:

	typedef dense_hash_map<FirstClass, SecondClass, HashFunction, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_MAP_CONTAINER(map)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && SerializeTraits<FirstClass>::MightContainPPtr() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};

template<class FirstClass, class SecondClass, class Compare, class Allocator>
class SerializeTraits<std::multimap<FirstClass, SecondClass, Compare, Allocator> > : public SerializeTraitsBase<std::multimap<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef std::multimap<FirstClass, SecondClass, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_MAP_CONTAINER(map)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && SerializeTraits<FirstClass>::MightContainPPtr() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};


template<class T, class Compare, class Allocator>
class SerializeTraits<std::set<T, Compare, Allocator> > : public SerializeTraitsBase<std::set<T, Compare, Allocator> >
{
	public:

	typedef std::set<T, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (set)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};

template<class FirstClass, class SecondClass, class Compare, class Allocator>
class SerializeTraits<vector_map<FirstClass, SecondClass, Compare, Allocator> > : public SerializeTraitsBase<vector_map<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef vector_map<FirstClass, SecondClass, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_MAP_CONTAINER (map)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleArray (data);
	}
	
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.get_vector ().resize (rs); }
};



template<class FirstClass, class SecondClass, class Compare, class Allocator>
class SerializeTraits<us_vector_map<FirstClass, SecondClass, Compare, Allocator> > : public SerializeTraitsBase<vector_map<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef vector_map<FirstClass, SecondClass, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_MAP_CONTAINER (map)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}
	
	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.get_vector ().resize (rs); }
};

template<class T, class Compare, class Allocator>
class SerializeTraits<vector_set<T, Compare, Allocator> > : public SerializeTraitsBase<vector_set<T, Compare, Allocator> >
{
	public:

	typedef vector_set<T, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (set)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleArray (data);
	}

	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.get_vector ().resize (rs); }
};

template<class T, class Compare, class Allocator>
class SerializeTraits<us_vector_set<T, Compare, Allocator> > : public SerializeTraitsBase<vector_set<T, Compare, Allocator> >
{
	public:

	typedef vector_set<T, Compare, Allocator>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (set)
	
	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}

	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.get_vector ().resize (rs); }
};


// Vector<bool> serialization is not allowed
template<class Allocator>
class SerializeTraits<std::vector<bool, Allocator> > : public SerializeTraitsBase<std::vector<bool, Allocator> >
{
	public:
	// disallow vector<bool> serialization	
};


template<class T, size_t align>
class SerializeTraits<dynamic_array<T, align> > : public SerializeTraitsBase<dynamic_array<T, align> >
{
public:

	typedef dynamic_array<T, align>	value_type;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)

		template<class TransferFunction> inline
		static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
	}

	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.resize_initialized(rs); }

	static void resource_image_assign_external (value_type& data, void* begin, void* end)
	{
		data.assign_external(reinterpret_cast<T*> (begin), reinterpret_cast<T*> (end));
	}
};

template<>
class SerializeTraits<dynamic_array<UInt8> > : public SerializeTraitsBase<dynamic_array<UInt8> >
{
public:

	typedef dynamic_array<UInt8>	value_type;
	typedef UInt8	T;
	DEFINE_GET_TYPESTRING_CONTAINER (vector)

		template<class TransferFunction> inline
		static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferSTLStyleArray (data);
		transfer.Align();
	}

	static bool IsContinousMemoryArray ()	{ return true; }
	static void ResizeSTLStyleArray (value_type& data, int rs)		{ data.resize_initialized(rs); }

	static void resource_image_assign_external (value_type& data, void* begin, void* end)
	{
		data.assign_external(reinterpret_cast<UInt8*> (begin), reinterpret_cast<UInt8*> (end));
	}
};


template<class T>
struct NonConstContainerValueType
{
	typedef typename T::value_type value_type;
};

template<class T>
struct NonConstContainerValueType<std::set<T> >
{
	typedef T value_type;
};

template<class T0, class T1, class Compare, class Allocator>
struct NonConstContainerValueType<std::map<T0, T1, Compare, Allocator> >
{
	typedef std::pair<T0, T1> value_type;
};

template<class T0, class T1, class Compare, class Allocator>
struct NonConstContainerValueType<std::multimap<T0, T1, Compare, Allocator> >
{
	typedef std::pair<T0, T1> value_type;
};

template<class T0, class T1, class HashFunction, class Compare, class Allocator>
struct NonConstContainerValueType<dense_hash_map<T0, T1, HashFunction, Compare, Allocator> >
{
	typedef std::pair<T0, T1> value_type;
};

#endif
