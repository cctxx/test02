#ifndef YAMLSERIALIZETRAITS_H
#define YAMLSERIALIZETRAITS_H

class Object;
template<class T>
class PPtr;
template<class T>
class ImmediatePtr;
struct UnityGUID;
class YAMLRead;
class YAMLWrite;

template<class T>
class YAMLSerializeTraitsBase
{
	public:
	inline static std::string ParseName (const char* _name, bool stripNames)
	{
		std::string name = _name;

		if (name == "Base")
			name = SerializeTraits<T>::GetTypeString (NULL);
		else if (stripNames)
		{
			if (name.rfind(".") != std::string::npos)
				name = name.substr(name.rfind(".") + 1);
			if (name.length() >= 3 && name.find("m_") == 0)
				name = (char)tolower(name[2]) + name.substr(3);
		}
		return name;
	}

	inline static bool ShouldSerializeArrayAsCompactString ()
	{
		return false;
	}

	inline static bool IsBasicType ()
	{
		return false;
	}

	template<class TransferFunction> inline
	static void Transfer (T& data, TransferFunction& transfer)
	{
		SerializeTraits<T>::Transfer (data, transfer);
	}

	inline static void TransferStringToData (T& /*data*/, std::string& /*str*/)
	{
	}
};

template<class T>
class YAMLSerializeTraits : public YAMLSerializeTraitsBase<T> {};

template<class T>
class YAMLSerializeTraitsForBasicType : public YAMLSerializeTraitsBase<T>
{
	public:
	inline static bool ShouldSerializeArrayAsCompactString ()
	{
		return true;
	}

	inline static bool IsBasicType ()
	{
		return true;
	}
};

template<>
class YAMLSerializeTraits<UInt16> : public YAMLSerializeTraitsForBasicType<UInt16> {};

template<>
class YAMLSerializeTraits<SInt16> : public YAMLSerializeTraitsForBasicType<SInt16> {};

template<>
class YAMLSerializeTraits<UInt32> : public YAMLSerializeTraitsForBasicType<UInt32> {};

template<>
class YAMLSerializeTraits<SInt32> : public YAMLSerializeTraitsForBasicType<SInt32> {};

template<>
class YAMLSerializeTraits<UInt64> : public YAMLSerializeTraitsForBasicType<UInt64> {};

template<>
class YAMLSerializeTraits<SInt64> : public YAMLSerializeTraitsForBasicType<SInt64> {};

template<>
class YAMLSerializeTraits<UInt8> : public YAMLSerializeTraitsForBasicType<UInt8> {};

template<>
class YAMLSerializeTraits<SInt8> : public YAMLSerializeTraitsForBasicType<SInt8> {};

template<>
class YAMLSerializeTraits<char> : public YAMLSerializeTraitsForBasicType<char> {};

template<>
class YAMLSerializeTraits<bool> : public YAMLSerializeTraitsForBasicType<bool> {};

template<>
class YAMLSerializeTraits<UnityStr> : public YAMLSerializeTraitsBase<UnityStr >
{
public:

	template<class TransferFunction> inline
	static void Transfer (UnityStr& data, TransferFunction& transfer)
	{
		transfer.TransferStringData (data);
	}

	inline static void TransferStringToData (UnityStr& data, std::string &str)
	{
		data = str.c_str();
	}

	inline static bool IsBasicType ()
	{
		return true;
	}
};

// Do not add this serialization function. All serialized strings should use UnityStr instead of std::string
//template<class Traits, class Allocator>
//class YAMLSerializeTraits<std::basic_string<char,Traits,Allocator> > : public YAMLSerializeTraitsBase<std::basic_string<char,Traits,Allocator> >

template<class FirstClass, class SecondClass>
class YAMLSerializeTraits<std::pair<FirstClass, SecondClass> > : public YAMLSerializeTraitsBase<std::pair<FirstClass, SecondClass> >
{
	public:


	template<class TransferFunction> inline
	static void Transfer (std::pair<FirstClass, SecondClass>& data, TransferFunction& transfer)
	{
		if (YAMLSerializeTraits<FirstClass>::IsBasicType())
			transfer.TransferPair (data);
		else
		{
			transfer.Transfer (data.first, "first");
			transfer.Transfer (data.second, "second");
		}
	}
};

template<class FirstClass, class SecondClass, class Compare, class Allocator>
class YAMLSerializeTraits<std::map<FirstClass, SecondClass, Compare, Allocator> > : public YAMLSerializeTraitsBase<std::map<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef std::map<FirstClass, SecondClass, Compare, Allocator>	value_type;

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && SerializeTraits<FirstClass>::MightContainPPtr() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};

template<class FirstClass, class SecondClass, class Compare, class Allocator>
class YAMLSerializeTraits<std::multimap<FirstClass, SecondClass, Compare, Allocator> > : public YAMLSerializeTraitsBase<std::multimap<FirstClass, SecondClass, Compare, Allocator> >
{
	public:

	typedef std::multimap<FirstClass, SecondClass, Compare, Allocator>	value_type;

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && SerializeTraits<FirstClass>::MightContainPPtr() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleMap (data);
	}
};


template<class T, class Compare, class Allocator>
class YAMLSerializeTraits<std::set<T, Compare, Allocator> > : public YAMLSerializeTraitsBase<std::set<T, Compare, Allocator> >
{
	public:

	typedef std::set<T, Compare, Allocator>	value_type;

	template<class TransferFunction> inline
	static void Transfer (value_type& data, TransferFunction& transfer)
	{
		AssertIf(transfer.IsRemapPPtrTransfer() && transfer.IsReadingPPtr());
		transfer.TransferSTLStyleSet (data);
	}
};


template<class TransferFunction>
void TransferYAMLPPtr (PPtr<Object> &data, TransferFunction& transfer);
template<class TransferFunction>
void TransferYAMLPPtr (ImmediatePtr<Object> &data, TransferFunction& transfer);

template<class T>
class YAMLSerializeTraits<PPtr<T> > : public YAMLSerializeTraitsBase<PPtr<T> >
{
	public:

	template<class TransferFunction> inline
	static void Transfer (PPtr<T>& data, TransferFunction& transfer)
	{
		TransferYAMLPPtr ((PPtr<Object>&)data, transfer);
	}
};

template<class T>
class YAMLSerializeTraits<ImmediatePtr<T> > : public YAMLSerializeTraitsBase<ImmediatePtr<T> >
{
	public:

	template<class TransferFunction> inline
	static void Transfer (ImmediatePtr<T>& data, TransferFunction& transfer)
	{
		TransferYAMLPPtr ((ImmediatePtr<Object>&)data, transfer);
	}
};

template<>
class YAMLSerializeTraits<UnityGUID> : public YAMLSerializeTraitsBase<UnityGUID>
{
	public:

	template<class TransferFunction>
	static void Transfer (UnityGUID& data, TransferFunction& transfer);

	inline static bool IsBasicType ()
	{
		return true;
	}
};
#endif
