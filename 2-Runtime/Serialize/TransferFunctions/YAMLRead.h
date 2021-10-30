#ifndef YAMLREAD_H
#define YAMLREAD_H

#include "Runtime/Serialize/TransferFunctions/TransferBase.h"
#include "TransferNameConversions.h"
#include "YAMLSerializeTraits.h"
#include "Editor/Src/Utility/YAMLNode.h"
#include "External/yaml/include/yaml.h"
#include "Runtime/Serialize/FloatStringConversion.h"
#include "Runtime/Utilities/TypeUtilities.h"

class CacheReaderBase;
struct StreamingInfo;

class YAMLRead : public TransferBase
{
private:

	int m_CurrentVersion;
	std::string m_CurrentType;
	std::string m_NodeName;
	bool m_DidReadLastProperty;

	yaml_document_t m_Document;
	yaml_document_t* m_ActiveDocument;
	std::vector<yaml_node_t*> m_MetaParents;
	std::vector<int> m_Versions;
	yaml_node_t* m_CurrentNode;
	yaml_node_pair_t* m_CachedIndex;
	yaml_read_handler_t *m_ReadHandler;
	void* m_ReadData;
	size_t m_ReadOffset;
	size_t m_EndOffset;

	int GetDataVersion ();
	yaml_node_t *GetValueForKey(yaml_node_t* parentNode, const char* keystr);

	static int YAMLReadCacheHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read);
	static int YAMLReadStringHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read);

	void Init(int flags, yaml_read_handler_t *handler, std::string *debugFileName, int debugLineCount);

public:

	YAMLRead (const char* strBuffer, int size, int flags, std::string *debugFileName = NULL, int debugLineCount = 0);
	YAMLRead (const CacheReaderBase *input, size_t readOffset, size_t endOffset, int flags, std::string *debugFileName = NULL, int debugLineCount = 0);
	YAMLRead (yaml_document_t* yamlDocument, int flags);
	~YAMLRead();

	void SetVersion (int version)                { m_CurrentVersion = version; }
	bool IsCurrentVersion ()                     { return m_CurrentVersion == GetDataVersion(); }
	bool IsOldVersion (int version)              { return version == GetDataVersion(); }
	bool IsVersionSmallerOrEqual (int version)   { return version >= GetDataVersion(); }

	bool IsReading ()			                 { return true; }
	bool IsReadingPPtr ()                        { return true; }
	bool IsReadingBackwardsCompatible()          { return true; }
	bool NeedsInstanceIDRemapping ()                     { return m_Flags & kNeedsInstanceIDRemapping; }
	bool DirectStringTransfer ()                 { return true; }
	bool AssetMetaDataOnly ()                    { return m_Flags & kAssetMetaDataOnly; }

	bool IsSerializingForGameRelease ()              { return false; }

	bool DidReadLastProperty ()                  { return m_DidReadLastProperty; }
	bool DidReadLastPPtrProperty ()              { return m_DidReadLastProperty; }

	YAMLNode* GetCurrentNode ();
	YAMLNode* GetValueNodeForKey (const char* key);

	static int StringOutputHandler(void *data, unsigned char *buffer, size_t size) ;
	void BeginMetaGroup (std::string name);
	void EndMetaGroup ();

	template<class T>
	void Transfer (T& data, const char* name, int metaFlag = 0);
	template<class T>
	void TransferWithTypeString (T& data, const char*, const char*, int metaFlag = 0);

	void TransferTypeless (unsigned* value, const char* name, int metaFlag = 0)
	{
		Transfer(*value, name, metaFlag);
	}

	bool HasNode (const char* name);

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
	void TransferPair (T& data, int metaFlag = 0, yaml_node_pair_t* pair = NULL);
};

template<>
inline void YAMLRead::TransferBasicData<bool> (bool& data)
{
	int i;
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%d", &i);
	data = (i == 0);
}

template<>
inline void YAMLRead::TransferBasicData<char> (char& data) 
{
	//scanf on msvc does not support %hhd. read int instead
	int i;
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%d", &i);
	data = i;
}

template<>
inline void YAMLRead::TransferBasicData<SInt8> (SInt8& data)
{
	TransferBasicData (reinterpret_cast<char&> (data));
}

template<>
inline void YAMLRead::TransferBasicData<UInt8> (UInt8& data) 
{
	//scanf on msvc does not support %hhu. read unsigned int instead
	unsigned int i;
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%u", &i);
	data = i;
}

template<>
inline void YAMLRead::TransferBasicData<SInt16> (SInt16& data) 
{
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%hd", &data);
}

template<>
inline void YAMLRead::TransferBasicData<UInt16> (UInt16& data) 
{
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%hu", &data);
}

template<>
inline void YAMLRead::TransferBasicData<SInt32> (SInt32& data) 
{
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%d", &data);
}

template<>
inline void YAMLRead::TransferBasicData<UInt32> (UInt32& data) 
{
	sscanf ((char*)m_CurrentNode->data.scalar.value, "%u", &data);
}

template<>
inline void YAMLRead::TransferBasicData<SInt64> (SInt64& data) 
{
	// msvc does not like %lld. Just read hex data directly.
	Assert (strlen((char*)m_CurrentNode->data.scalar.value) == 16);
	HexStringToBytes ((char*)m_CurrentNode->data.scalar.value, sizeof(SInt64), &data);
}

template<>
inline void YAMLRead::TransferBasicData<UInt64> (UInt64& data) 
{
	// msvc does not like %lld. Just read hex data directly.
	Assert (strlen((char*)m_CurrentNode->data.scalar.value) == 16);
	HexStringToBytes ((char*)m_CurrentNode->data.scalar.value, sizeof(UInt64), &data);
}

template<>
inline void YAMLRead::TransferBasicData<double> (double& data)
{
	data = StringToDoubleAccurate((char*)m_CurrentNode->data.scalar.value);
}

template<>
inline void YAMLRead::TransferBasicData<float> (float& data)
{
	data = StringToFloatAccurate ((char*)m_CurrentNode->data.scalar.value);
}

template<class T>
inline void YAMLRead::TransferStringData (T& data) 
{
	data = (char*)m_CurrentNode->data.scalar.value;
}

template<class T>
void YAMLRead::Transfer (T& data, const char* _name, int metaFlag)
{
	m_DidReadLastProperty = false;

	if (metaFlag & kIgnoreInMetaFiles)
		return; 
	
	std::string name = YAMLSerializeTraits<T>::ParseName(_name, AssetMetaDataOnly());
	
	yaml_node_t *parentNode = m_CurrentNode;	
	m_CurrentNode = GetValueForKey(parentNode, name.c_str());
	if (!m_CurrentNode)
	{
		const AllowNameConversion::mapped_type* nameConversion = GetAllowedNameConversions (m_CurrentType.c_str(), _name);
		if (nameConversion)
		{
			for (AllowNameConversion::mapped_type::const_iterator i = nameConversion->begin(); i!=nameConversion->end();i++)
			{
				m_CurrentNode = GetValueForKey(parentNode, *i);
				if (m_CurrentNode)
					break;
			}
		}
		if (!m_CurrentNode)
		{
			if (strcmp (_name, "Base") == 0)
			{
				if (parentNode && parentNode->type == YAML_MAPPING_NODE)
				{
					if (parentNode->data.mapping.pairs.start != parentNode->data.mapping.pairs.top)
						m_CurrentNode = yaml_document_get_node(m_ActiveDocument, parentNode->data.mapping.pairs.start->value);
				}
			}
			else if (metaFlag & kTransferAsArrayEntryNameInMetaFiles)
			{
				YAMLSerializeTraits<T>::TransferStringToData (data, m_NodeName);
				m_CurrentNode = parentNode;
				return;
			}
		}
	}

	std::string parentType = m_CurrentType;
	m_CurrentType = SerializeTraits<T>::GetTypeString (&data);

	if (m_CurrentNode != NULL)
	{
		m_Versions.push_back(-1);
		YAMLSerializeTraits<T>::Transfer (data, *this);
		m_Versions.pop_back();
		m_DidReadLastProperty = true;
	}
	
	m_CurrentNode = parentNode;
	m_CurrentType = parentType;
}

template<class T>
void YAMLRead::TransferWithTypeString (T& data, const char* name, const char*, int metaFlag)
{
	Transfer(data, name, metaFlag);
}


template<class T>
void YAMLRead::TransferSTLStyleArray (T& data, int /*metaFlag*/)
{
	yaml_node_t *parentNode = m_CurrentNode;
	typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;

	switch (m_CurrentNode->type)
	{
		case YAML_SCALAR_NODE:
		{
#if UNITY_BIG_ENDIAN
#error "Needs swapping to be implemented to work on big endian platforms!"
#endif
			std::string str;
			TransferStringData (str);
			size_t byteLength = str.size() / 2;
			size_t numElements = byteLength / sizeof(non_const_value_type);
			SerializeTraits<T>::ResizeSTLStyleArray (data, numElements);
			typename T::iterator dataIterator = data.begin ();
			for (size_t i=0; i<numElements; i++)
			{
				HexStringToBytes (&str[i*2*sizeof(non_const_value_type)], sizeof(non_const_value_type), (void*)&*dataIterator);
				++dataIterator;
			}
		}
		break;

		case YAML_SEQUENCE_NODE:
		{
			yaml_node_item_t* start = m_CurrentNode->data.sequence.items.start;
			yaml_node_item_t* top   = m_CurrentNode->data.sequence.items.top;

			SerializeTraits<T>::ResizeSTLStyleArray (data, top - start);
			typename T::iterator dataIterator = data.begin ();

			for(yaml_node_item_t* i = start; i != top; i++)
			{
				m_CurrentNode = yaml_document_get_node(m_ActiveDocument, *i);
				YAMLSerializeTraits<non_const_value_type>::Transfer (*dataIterator, *this);
				++dataIterator;
			}
		}
		break;

		// Some stupid old-style meta data writing code unnecessarily used mappings
		// instead of sequences to encode arrays. So, we're able to read that as well.
		case YAML_MAPPING_NODE:
		{
			yaml_node_pair_t* start = m_CurrentNode->data.mapping.pairs.start;
			yaml_node_pair_t* top   = m_CurrentNode->data.mapping.pairs.top;

			SerializeTraits<T>::ResizeSTLStyleArray (data, top - start);
			typename T::iterator dataIterator = data.begin ();

			for(yaml_node_pair_t* i = start; i != top; i++)
			{
				yaml_node_t* key = yaml_document_get_node(m_ActiveDocument, i->key);
				Assert (key->type == YAML_SCALAR_NODE);

				m_NodeName = (std::string)(char*)key->data.scalar.value;
				m_CurrentNode = yaml_document_get_node(m_ActiveDocument, i->value);

				YAMLSerializeTraits<non_const_value_type>::Transfer (*dataIterator, *this);
				++dataIterator;
			}
		}
		break;

		default:
			ErrorString("Unexpected node type.");
	}

	m_CurrentNode = parentNode;
}

template<class T>
void YAMLRead::TransferSTLStyleMap (T& data, int metaFlag)
{
	if (m_CurrentNode->type == YAML_MAPPING_NODE)
	{
		yaml_node_pair_t* start = m_CurrentNode->data.mapping.pairs.start;
		yaml_node_pair_t* top   = m_CurrentNode->data.mapping.pairs.top;

		data.clear();

		yaml_node_t *parentNode = m_CurrentNode;

		for(yaml_node_pair_t* i = start; i != top; i++)
		{
			typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;
			typedef typename non_const_value_type::first_type first_type;
			non_const_value_type p;

			if (!YAMLSerializeTraits<first_type>::IsBasicType())
			{
				m_CurrentNode = yaml_document_get_node(m_ActiveDocument, i->value);

				YAMLSerializeTraits<non_const_value_type>::Transfer (p, *this);
			}
			else
				TransferPair (p, metaFlag, i);

			data.insert (p);
		}
		m_CurrentNode = parentNode;
	}
}

template<class T>
void YAMLRead::TransferSTLStyleSet (T& data, int /*metaFlag*/)
{
	if (m_CurrentNode->type == YAML_SEQUENCE_NODE)
	{
		yaml_node_item_t* start = m_CurrentNode->data.sequence.items.start;
		yaml_node_item_t* top   = m_CurrentNode->data.sequence.items.top;

		data.clear();

		yaml_node_t *parentNode = m_CurrentNode;

		for(yaml_node_item_t* i = start; i != top; i++)
		{
			typedef typename NonConstContainerValueType<T>::value_type non_const_value_type;
			non_const_value_type p;

			m_CurrentNode = yaml_document_get_node(m_ActiveDocument, *i);

			YAMLSerializeTraits<non_const_value_type>::Transfer (p, *this);

			data.insert (p);
		}
		m_CurrentNode = parentNode;
	}
}

template<class T>
void YAMLRead::TransferPair (T& data, int /*metaFlag*/, yaml_node_pair_t* pair)
{
	typedef typename T::first_type first_type;
	typedef typename T::second_type second_type;

	if (pair == NULL)
	{
		yaml_node_pair_t* start = m_CurrentNode->data.mapping.pairs.start;
		yaml_node_pair_t* top   = m_CurrentNode->data.mapping.pairs.top;
		if (start == top)
			return;
		pair = start;
	}

	yaml_node_t* parent = m_CurrentNode;
	m_CurrentNode = yaml_document_get_node(m_ActiveDocument, pair->key);
	YAMLSerializeTraits<first_type>::Transfer (data.first, *this);
	m_CurrentNode = yaml_document_get_node(m_ActiveDocument, pair->value);
	YAMLSerializeTraits<second_type>::Transfer (data.second, *this);
	m_CurrentNode = parent;
}
#endif
