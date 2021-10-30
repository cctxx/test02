#pragma once

template<class T>
class SerializeTraitsBase
{
	public:

	typedef T value_type;

	static int GetByteSize ()	{return sizeof (value_type);}
	static size_t GetAlignOf()	{return ALIGN_OF(value_type);}

	static void resource_image_assign_external (value_type& /*data*/, void* /*begin*/, void* /*end*/)
	{
		AssertString("Unsupported");
	}
};

template<class T>
class SerializeTraitsBaseForBasicType : public SerializeTraitsBase<T>
{
public:
	typedef T value_type;

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		transfer.TransferBasicData (data);
	}
};

template<class T>
class SerializeTraits : public SerializeTraitsBase<T>
{
public:

	typedef T value_type;

	inline static const char* GetTypeString (void* /*ptr*/) { return value_type::GetTypeString (); }
	inline static bool MightContainPPtr () { return value_type::MightContainPPtr (); }
	/// Returns whether or not a this type is to be treated as a seperate channel in the animation system
	static bool IsAnimationChannel () { return T::IsAnimationChannel (); }

	/// AllowTransferOptimization can be used for type that have the same memory format as serialized format.
	/// Eg. a float or a Vector3f.
	/// StreamedBinaryRead will collapse the read into a direct read when reading an array with values that have AllowTransferOptimization.
	static bool AllowTransferOptimization () { return T::AllowTransferOptimization (); }

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		data.Transfer (transfer);
	}

};

#define DEFINE_GET_TYPESTRING_CONTAINER(x)						\
inline static const char* GetTypeString (void*)	{ return #x; } \
inline static bool IsAnimationChannel ()	{ return false; } \
inline static bool MightContainPPtr ()	{ return SerializeTraits<T>::MightContainPPtr(); } \
inline static bool AllowTransferOptimization ()	{ return false; }
