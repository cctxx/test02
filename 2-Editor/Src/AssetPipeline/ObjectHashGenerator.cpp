#include "UnityPrefix.h"
#include "ObjectHashGenerator.h"
#include "Runtime/Serialize/TransferUtility.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Serialize/SerializedFile.h"
#include "Runtime/BaseClasses/EditorExtension.h"

using std::vector;
using std::string;

//#define DEBUG_MD4_GENERATOR
#ifdef DEBUG_MD4_GENERATOR
#define DebugFormat printf_console
#else
#define DebugFormat
#endif


struct ObjectHashGeneratorIterator
{
	SerializedFile*  m_File;
	UnityGUID        m_SelfGUID;
	MdFourGenerator* m_Generator;

	ObjectHashGeneratorIterator(MdFourGenerator* g) : m_Generator (g), m_File (NULL) {
	}
	
	// Maps need to be treated a bit special.
	// Material hashes use FastPropertyName and are sorted by that which yields almost completely random sort order
	void HashMap (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if ( arrayTypeTree.m_Children.back().m_Children.size() != 2)
			return;
		const TypeTree& key = arrayTypeTree.m_Children.back ().m_Children.front();
		const TypeTree& value = arrayTypeTree.m_Children.back ().m_Children.back();

	
		MdFourGenerator* oldGenerator = m_Generator;

		// - Go through the map and create seperate key and value string pairs
		// - Sort them by key
		// - Add to the hash
		int arraySize = *reinterpret_cast<SInt32*> (&data[bytePosition]);
		bytePosition += sizeof(SInt32);
		
		std::multimap<std::string, std::string> mdfours;
		
		for (int i=0;i<arraySize;i++)
		{
			// Hash key
			MdFourGenerator keyGen;
			m_Generator = &keyGen;
			IterateTypeTree (key, data, &bytePosition, *this);
			string keyGenString = MdFourToString(keyGen.Finish());

			// hash value
			MdFourGenerator valueGen;
			m_Generator = &valueGen;
			IterateTypeTree (value, data, &bytePosition, *this);
			string valueGenString = MdFourToString(valueGen.Finish());
			
			// insert into map
			mdfours.insert(make_pair(keyGenString, valueGenString));
		}

		m_Generator = oldGenerator;
		
		// hash sorted map
		for (std::multimap<std::string,std::string>::const_iterator i=mdfours.begin();i != mdfours.end();i++)
		{
//			DebugFormat("%sEND MAP (sz: %d)\n", string(mapDepth.top(), ' ').c_str(), mapStack.top().size());
			m_Generator->Feed(i->first); 
			m_Generator->Feed(i->second);
		}
	}
	
	bool FastPathHashArray (const TypeTree& arrayTypeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		if ( arrayTypeTree.m_Children.back().m_Children.size() != 2 && !arrayTypeTree.m_Children.back ().IsBasicDataType())
			return false;
        const TypeTree& typeTree = arrayTypeTree.m_Children.back ();
		int arraySize = *reinterpret_cast<SInt32*> (&data[bytePosition]);
		bytePosition += sizeof(SInt32);
        
        if (typeTree.m_ByteSize == 4)
        {
            if(typeTree.m_Type == "float")
            {
                float* dataPtr = reinterpret_cast<float*>(&data[bytePosition]);
                for (int i=0;i<arraySize;i++)
                    m_Generator->Feed(dataPtr[i]);
                return true;
            }
            else if(typeTree.m_Father->m_Type == "ColorRGBA") 
            {
                ColorRGBA32* dataPtr = reinterpret_cast<ColorRGBA32*>(&data[bytePosition]);
                for (int i=0;i<arraySize;i++)
                    m_Generator->Feed(dataPtr[i]);
                return true;
            }
            else
            {
                int* dataPtr = reinterpret_cast<int*>(&data[bytePosition]);
                for (int i=0;i<arraySize;i++)
                    m_Generator->Feed(dataPtr[i]);
                return true;
            }
        }
        else if (typeTree.m_ByteSize == 1 && typeTree.m_Type == "UInt8")
		{
			UInt8* dataPtr = reinterpret_cast<UInt8*>(&data[bytePosition]);
                for (int i=0;i<arraySize;i++)
                    m_Generator->Feed(dataPtr[i]);
            return true;
		}
        return false;
	}
	
	bool operator () (const TypeTree& typeTree, dynamic_array<UInt8>& data, int bytePosition)
	{
		// hidden and ignore
//		if ((m_Flags & kIgnoreHiddenProperties) != 0 && (typeTree.m_MetaFlag & kHideInEditorMask) != 0)
//			return false;
		
//		DebugFormat("%04d%s %s : %s\n", depth, string(depth, ' ').c_str(), typeTree.m_Name.c_str(), typeTree.m_Type.c_str());

		// Hash basic types
		if ( typeTree.IsBasicDataType() )
		{
			if (typeTree.m_ByteSize == 1)
				m_Generator->Feed((int)Get<UInt8>(data, bytePosition));
			else if (typeTree.m_ByteSize == 2)
				m_Generator->Feed((int)Get<UInt16>(data, bytePosition));
			else if (typeTree.m_ByteSize == 4)
			{
				if(typeTree.m_Type == "float")
					m_Generator->Feed(Get<float>(data, bytePosition));
				else if(typeTree.m_Father->m_Type == "ColorRGBA") 
					m_Generator->Feed(Get<ColorRGBA32>(data, bytePosition));
				else
					m_Generator->Feed((int)Get<UInt32>(data, bytePosition));
			}
			else if (typeTree.m_ByteSize == 8)
			{
				if (typeTree.m_Type == "double")
					m_Generator->Feed(Get<double>(data, bytePosition));
				else
					m_Generator->Feed(Get<UInt64>(data, bytePosition));
			}
			else if (typeTree.m_ByteSize == 8)
				m_Generator->Feed(Get<UInt64>(data, bytePosition));
			else
			{
				ErrorString("Unexpected byte size!");
			}
		}
		// Maps need to be sorted to keep ordering consistent between architectures
		// Materials use map<FastPropertyName, SomeData>
		else if ( typeTree.m_IsArray)
		{
			if (typeTree.m_Father->m_Type == "map")
			{
				HashMap(typeTree, data, bytePosition);
				return false;
			}
			else
			{
				if (FastPathHashArray(typeTree, data, bytePosition))
					return false;
			}	
		}
		else if ( IsTypeTreePPtr( typeTree ) )
		{
			// Reading from file 
			if (m_File)
			{
				SInt32 pathID = *reinterpret_cast<SInt32*> (&data[bytePosition]);
				SInt32 fileID = *reinterpret_cast<SInt32*> (&data[bytePosition + sizeof(SInt32)]);

				if( fileID != 0 )
				{
					UnityGUID guid;
					if (pathID != 0)
						guid = m_File->GetExternalRefs()[pathID - 1].guid;
					else
						guid = m_SelfGUID;
					
				
					m_Generator->Feed((int)fileID);
					m_Generator->Feed(GUIDToString(guid));
				}
			}
			// Normal in memory serialization
			else
			{
				SInt32 instanceID = ExtractPPtrInstanceID(data, bytePosition);
				HashInstanceID(*m_Generator, instanceID);
			}
			return false;
		}
		return true;

	}
	
	template <class T> T Get(dynamic_array<UInt8>& data, int bytePosition) {
		return * (reinterpret_cast<T*>(&data[bytePosition]));
	}
};

void FeedHashWithObject(MdFourGenerator& generator, Object& o, int flags)
{
	TypeTree tree;
	dynamic_array<UInt8> data(kMemTempAlloc);
	
	GenerateTypeTree(o, &tree);
	WriteObjectToVector(o, &data);
	
	ObjectHashGeneratorIterator func(&generator);
	IterateTypeTree (tree, data, func);
}

// Collects all dependencies for the object and maintains them in the order they are serialized.
class DependencyCollectorForHash : public GenerateIDFunctor
{
public:	
	std::set<SInt32> m_IDs;
	std::vector<Object*> m_Objects;
	int m_Flags;
	
	virtual ~DependencyCollectorForHash () {}
	
	virtual SInt32 GenerateInstanceID (SInt32 oldInstanceID, TransferMetaFlags metaFlags = kNoTransferFlags)
	{
		EditorExtension* extendable = dynamic_instanceID_cast<EditorExtension*> (oldInstanceID);
		if (extendable == NULL)
			return oldInstanceID;
		
		if (!m_IDs.insert (oldInstanceID).second)
			return oldInstanceID;
		
		m_Objects.push_back(extendable);
		
		RemapPPtrTransfer transferFunction (m_Flags, false);
		transferFunction.SetGenerateIDFunctor (this);
		extendable->VirtualRedirectTransfer (transferFunction);
		
		return oldInstanceID;
	}
};

void FeedHashWithObjectAndAllDependencies(MdFourGenerator& generator, vector<Object*>& objects, int flags)
{
	DependencyCollectorForHash collector;
	collector.m_Flags = flags;
	
	for (int i=0;i<objects.size();i++)
	{
		if (objects[i])
			collector.GenerateInstanceID(objects[i]->GetInstanceID());
	}
	
	for (int i=0;i<collector.m_Objects.size();i++)
	{
		if (collector.m_Objects[i])
			FeedHashWithObject(generator, *collector.m_Objects[i], flags);
	}
}

void HashInstanceID (MdFourGenerator& generator, SInt32 instanceID)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	pm.Lock();
	
	SerializedObjectIdentifier identifier;
	if (pm.InstanceIDToSerializedObjectIdentifier(instanceID, identifier))
	{
		UnityGUID guid = pm.PathIDToFileIdentifierInternal(identifier.serializedFileIndex).guid;
		
		generator.Feed(identifier.localIdentifierInFile);
		generator.Feed(GUIDToString(guid));
	}
	pm.Unlock();
}