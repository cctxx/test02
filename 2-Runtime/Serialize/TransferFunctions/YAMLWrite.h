#ifndef YAMLWRITE_H
#define YAMLWRITE_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "YAMLSerializeTraits.h"
#include "Editor/Src/Utility/YAMLNode.h"
#include "External/yaml/include/yaml.h"
#include "Runtime/Serialize/FloatStringConversion.h"

class CachedWriter;
struct YAMLConverterContext;

class YAMLWrite : public TransferBase
{
private:

	struct MetaParent
	{
		int node;
		std::string name;
	};

	std::vector<MetaParent> m_MetaParents;
	std::vector<int> m_MetaFlags;
	yaml_document_t m_Document;
	int m_CurrentNode;
	bool m_Error;
	std::string m_DebugFileName;

	void TransferStringToCurrentNode (const char* str);
	int NewMapping ();
	int NewSequence ();
	int GetNode ();
	void AppendToNode(int parentNode, const char* keyStr, int valueNode);
	static int StringOutputHandler(void *data, unsigned char *buffer, size_t size);
	static int CacheOutputHandler(void *data, unsigned char *buffer, size_t size);

	void OutputToHandler (yaml_write_handler_t *handler, void *data);

public:

	YAMLWrite (int flags, std::string *debugFileName = NULL);
	~YAMLWrite();

	void OutputToCachedWriter (CachedWriter* writer);
	void OutputToString (std::string& str);

	// Sets the "version of the class currently transferred"
	void SetVersion (int version);

	bool HasError ()                  { return m_Error; }
	bool IsWriting ()                 { return true; }
	bool IsWritingPPtr ()             { return true; }
	bool NeedsInstanceIDRemapping ()          { return m_Flags & kNeedsInstanceIDRemapping; }
	bool AssetMetaDataOnly ()         { return m_Flags & kAssetMetaDataOnly; }
	bool IsSerializingForGameRelease ()   { return false; }

	void SetFlowMappingStyle (bool on);

	yaml_document_t* GetDocument () { return &m_Document; }
	int GetCurrentNodeIndex () { return m_CurrentNode; }

	void PushMetaFlag (int flag) { m_MetaFlags.push_back(flag | m_MetaFlags.back());}
	void PopMetaFlag () { m_MetaFlags.pop_back(); }
	void AddMetaFlag(int mask) { m_MetaFlags.back() |= mask; }
		
	void BeginMetaGroup (std::string name);
	void EndMetaGroup ();
	void StartSequence ();

	template<class T>
	void Transfer (T& data, const char* name, int metaFlag = 0);
	template<class T>
	void TransferWithTypeString (T& data, const char*, const char*, int metaFlag = 0);

	void TransferTypeless (unsigned* value, const char* name, int metaFlag = 0)
	{
		Transfer(*value, name, metaFlag);
	}

	void TransferTypelessData (unsigned size, void* data, int metaFlag = 0);

	template<class T>
	void TransferBasicData (T& data);

	template<class T>
	void TransferPtr (bool, ReduceCopyData*){}

	template<class T>
	void TransferStringData (T& data);

	template<class T>
	void TransferSTLStyleArray (T& data, int metaFlag = 0);

	template<class T>
	void TransferSTLStyleMap (T& data, int metaFlag = 0);

	template<class T>
	void TransferSTLStyleSet (T& data, int metaFlag = 0);

	template<class T>
	void TransferPair (T& data, int metaFlag = 0, int parent = -1);
};

template<>
inline void YAMLWrite::TransferBasicData<SInt64> (SInt64& data) 
{
	char valueStr[17];
	BytesToHexString (&data, sizeof(SInt64), valueStr);
	valueStr[16] = '\0';
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<UInt64> (UInt64& data) 
{
	char valueStr[17];
	BytesToHexString (&data, sizeof(UInt64), valueStr);
	valueStr[16] = '\0';
	TransferStringToCurrentNode (valueStr);
}

// This are the definitions of std::numeric_limits<>::max_digits10, which we cannot use
// because it is only in the C++11 standard.
const int kMaxFloatDigits = std::floor(std::numeric_limits<float>::digits * 3010.0/10000.0 + 2);
const int kMaxDoubleDigits = std::floor(std::numeric_limits<double>::digits * 3010.0/10000.0 + 2);

template<>
inline void YAMLWrite::TransferBasicData<float> (float& data)
{
	char valueStr[64];
	if (FloatToStringAccurate(data, valueStr, 64))
		TransferStringToCurrentNode (valueStr);
	else
		TransferStringToCurrentNode ("error");
}

template<>
inline void YAMLWrite::TransferBasicData<double> (double& data)
{
	char valueStr[64];
	if (DoubleToStringAccurate (data, valueStr, 64))
		TransferStringToCurrentNode (valueStr);
	else
		TransferStringToCurrentNode ("error");
}

template<>
inline void YAMLWrite::TransferBasicData<char> (char& data)
{
	char valueStr[16];
	snprintf(valueStr, 16, "%hhd", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<SInt8> (SInt8& data)
{
	char valueStr[16];
	snprintf(valueStr, 16, "%hhd", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<UInt8> (UInt8& data) 
{
	char valueStr[16];
	snprintf(valueStr, 16, "%hhu", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<SInt32> (SInt32& data) 
{
	char valueStr[16];
	snprintf(valueStr, 16, "%d", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<UInt32> (UInt32& data) 
{
	char valueStr[16];
	snprintf(valueStr, 16, "%u", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<SInt16> (SInt16& data) 
{
	char valueStr[16];
	snprintf(valueStr, 16, "%hd", data);
	TransferStringToCurrentNode (valueStr);
}

template<>
inline void YAMLWrite::TransferBasicData<UInt16> (UInt16& data) 
{
	char valueStr[16];
	snprintf(valueStr, 16, "%hu", data);
	TransferStringToCurrentNode (valueStr);
}

template<class T>
inline void YAMLWrite::TransferStringData (T& data)
{
	TransferStringToCurrentNode (data.c_str());
}

template<class T>
void YAMLWrite::Transfer (T& data, const char* _name, int metaFlag)
{
	if (m_Error)
		return;

	if (metaFlag & kIgnoreInMetaFiles)
		return;

	std::string name = YAMLSerializeTraits<T>::ParseName(_name, AssetMetaDataOnly());

	PushMetaFlag(0);

	int parent = GetNode();
	m_CurrentNode = -1;

	YAMLSerializeTraits<T>::Transfer (data, *this);

	if (m_CurrentNode != -1)
		AppendToNode (parent, name.c_str(), m_CurrentNode);

	PopMetaFlag();

	m_CurrentNode = parent;
}

template<class T>
void YAMLWrite::TransferWithTypeString (T& data, const char* name, const char*, int metaFlag)
{
	Transfer(data, name, metaFlag);
}


template<class T>
void YAMLWrite::TransferSTLStyleArray (T& data, int metaFlag)
{
	typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;
	if (YAMLSerializeTraits<non_const_value_type>::ShouldSerializeArrayAsCompactString())
	{
#if UNITY_BIG_ENDIAN
#error "Needs swapping to be implemented to work on big endian platforms!"
#endif
		std::string str;
		size_t numElements = data.size();
		size_t numBytes = numElements * sizeof(non_const_value_type);
		str.resize (numBytes*2);

		typename T::iterator dataIterator = data.begin ();
		for (size_t i=0; i<numElements; i++)
		{
			BytesToHexString ((void*)&*dataIterator, sizeof(non_const_value_type), &str[i*2*sizeof(non_const_value_type)]);
			++dataIterator;
		}

		TransferStringData (str);
	}
	else
	{
		m_CurrentNode = NewSequence ();

		typename T::iterator i = data.begin ();
		typename T::iterator end = data.end ();
		while (i != end)
		{
			Transfer (*i, "data", metaFlag);
			++i;
		}
	}
}

template<class T>
void YAMLWrite::TransferSTLStyleMap (T& data, int metaFlag)
{
	m_CurrentNode = NewMapping ();

	typename T::iterator i = data.begin ();
	typename T::iterator end = data.end ();


	// maps value_type is: pair<const First, Second>
	// So we have to write to maps non-const value type
	typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;
	typedef typename non_const_value_type::first_type first_type;
	while (i != end)
	{
		non_const_value_type& p = (non_const_value_type&)(*i);
		if (YAMLSerializeTraits<first_type>::IsBasicType())
			TransferPair (p, metaFlag, m_CurrentNode);
		else
			Transfer (p, "data", metaFlag);
		i++;
	}
}

template<class T>
void YAMLWrite::TransferSTLStyleSet (T& data, int metaFlag)
{
	m_CurrentNode = NewSequence ();

	typename T::iterator i = data.begin ();
	typename T::iterator end = data.end ();

	typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;
	while (i != end)
	{
		non_const_value_type& p = (non_const_value_type&)(*i);
		Transfer (p, "data", metaFlag);
		++i;
	}
}

template<class T>
void YAMLWrite::TransferPair (T& data, int /*metaFlag*/, int parent)
{
	typedef typename T::first_type first_type;
	typedef typename T::second_type second_type;
	if (parent == -1)
		parent = NewMapping ();

	m_CurrentNode = -1;
	YAMLSerializeTraits<first_type>::Transfer (data.first, *this);
	int key = m_CurrentNode;

	m_CurrentNode = -1;
	YAMLSerializeTraits<second_type>::Transfer (data.second, *this);

	yaml_document_append_mapping_pair(&m_Document, parent, key, m_CurrentNode);

	m_CurrentNode = parent;
}
#endif
