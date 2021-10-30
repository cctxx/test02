#ifndef SERIALIZEDFILE_H
#define SERIALIZEDFILE_H

#include <map>
#include <string>
#include "TypeTree.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "FileCache.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "Runtime/Utilities/vector_map.h"
#include "Runtime/Utilities/dynamic_block_vector.h"
#if !GAMERELEASE
#include "Runtime/Utilities/GUID.h"
#endif

extern const char* kUnityTextMagicString;

using std::map;
using std::string;
using std::list;
using std::vector;

class CachedWriter;
enum ObjectCreationMode;
struct ResourceImageGroup;

struct FileIdentifier
{
	enum { kNonAssetType = 0, kDeprecatedCachedAssetType = 1, kSerializedAssetType = 2, kMetaAssetType = 3 };

	UnityStr pathName;
	SInt32 type;

	#if GAMERELEASE
	struct GUIDPlaceHolder { UInt32 data[4]; };
	GUIDPlaceHolder guid;

	void CheckValidity () {}

	#else

	UnityGUID guid;
	
	FileIdentifier (const string& p, const UnityGUID& g, int t)
		: pathName (p), guid (g), type (t)
	{
		CheckValidity ();
	}
	
	void CheckValidity ();
	void Fix_3_5_BackwardsCompatibility ();
	
	bool operator < (const FileIdentifier &other) const
	{
		if (guid < other.guid)
			return true;
		else if (guid != other.guid)
			return false;
		else return type < other.type;
	}
	#endif

	FileIdentifier () { type = 0; }

	#if SUPPORT_TEXT_SERIALIZATION
	DECLARE_SERIALIZE (FileIdentifier);
	#endif
};

class SerializedFile
{
	struct ObjectInfo
	{
		SInt32 byteStart;
		SInt32 byteSize;
		SInt32 typeID;
		SInt16 classID;
		UInt16 isDestroyed;
#if SUPPORT_TEXT_SERIALIZATION
		SInt32 debugLineStart;
#endif
	};
	
	typedef vector_map<LocalIdentifierInFileType, ObjectInfo> 	ObjectMap;
	
 
#if SUPPORT_TEXT_SERIALIZATION
	bool								m_IsTextFile;
#endif
 	
	unsigned							m_ReadOffset;

	#if SUPPORT_SERIALIZED_TYPETREES
	typedef map<SInt32, Type>			TypeMap;
	TypeMap								m_Type;
	#endif
	ObjectMap							m_Object;
	UInt8								m_FileEndianess;
	
	bool								m_IsDirty;
	bool                                m_MemoryStream;
	bool                                m_CachedFileStream;
	bool								m_HasErrors;
	UInt32								m_Options;
	UInt32								m_TargetPlatform; // enum BuildTargetPlatform
	int									m_SubTarget;
	UInt32								m_WriteDataOffset;

	dynamic_block_vector<FileIdentifier>m_Externals;
	
	CacheReaderBase*					m_ReadFile;
	ResourceImageGroup					m_ResourceImageGroup;
	
	#if ENABLE_PROFILER || UNITY_EDITOR
	std::string                         m_DebugPath;
	#endif
	
	#if SUPPORT_SERIALIZE_WRITE
	CachedWriter*						m_CachedWriter;

	//unsigned                            m_ObjectBufferStart;
	#endif
	public:

	enum { kSectionAlignment = 16 };

	enum
	{
		kLittleEndian = 0,
		kBigEndian = 1,
		
		#if UNITY_BIG_ENDIAN
		kActiveEndianess = kBigEndian,
		kOppositeEndianess = kLittleEndian
		#else
		kActiveEndianess = kLittleEndian,
		kOppositeEndianess = kBigEndian
		#endif
	};
	
	SerializedFile ();

	// options: kSerializeGameRelease, kSwapEndianess, kBuildPlayerOnlySerializeBuildProperties	
	bool InitializeWrite (CachedWriter& cachedWriter, BuildTargetSelection target, int options);
	bool InitializeRead (const std::string& path, ResourceImageGroup& resourceImage, unsigned cacheSize, unsigned cacheCount, int options, int readOffset = 0);
	bool InitializeMemory (const string& path, UInt8* buffer, unsigned size, int options);
	bool InitializeMemoryBlocks (const string& path, UInt8** buffer, unsigned size, unsigned offset, int options);
	
	~SerializedFile ();
		
	static void DeleteNoFlush (SerializedFile* file);
	
	#if ENABLE_PROFILER || UNITY_EDITOR
	void SetDebugPath(const std::string& path) { m_DebugPath = path; }
	const std::string& GetDebugPath() const { return m_DebugPath; }
	#endif
	
	// Writes an object with id to the file. (Possibly overriding any older versions)
	// Writing to a stream which includes objects with an older typetree version is not possible and false will be returned
	void WriteObject (Object& object, LocalIdentifierInFileType fileID, const BuildUsageTag& buildUsage);
	
	// Reads the object referenced by id from disk
	// Returns a pointer to the object. (NULL if no object was found on disk)
	// object is either PRODUCED or the object already in memory referenced by id is used
	// isMarkedDestroyed is a returned by value (non-NULL)
	// registerInstanceID should the instanceID be register with the ID To Object lookup (false for threaded loading)
	// And reports whether the object read was marked as destroyed or not
	void ReadObject (LocalIdentifierInFileType fileID, int instanceId, ObjectCreationMode mode, bool isPersistent, TypeTree** oldTypeTree, bool* didChangeTypeTree, Object** readObject);

	// Returns whether or not the object referenced by id was destroyed
	bool DestroyObject (LocalIdentifierInFileType id);

	// objects: On return, all fileIDs to all objects in this Serialze
	void GetAllFileIDs (vector<LocalIdentifierInFileType>* objects)const;
	void GetAllFileIDsUnchecked (vector<LocalIdentifierInFileType>* objects)const;	
	
	// Returns the biggest id of all the objects in the file.
	// if no objects are in the file 0 is returned
	LocalIdentifierInFileType GetHighestID () const;
	
	// Returns whether or not an object is available in the stream
	bool IsAvailable (LocalIdentifierInFileType id) const;
	// Returns the classID of the object at id
	int GetClassID (LocalIdentifierInFileType id) const;

	// Returns the size the object takes up on the disk
	int GetByteSize (LocalIdentifierInFileType id) const;

	// Returns the seek position in the file where the object with id is stored
	int GetByteStart (LocalIdentifierInFileType id) const;

	// Returns the seek position in the file where the object with id is stored
	const TypeTree* GetTypeTree (LocalIdentifierInFileType id);
	
	// Are there any objects stored in this serialize.
	bool IsEmpty () const;
	
	bool HasErrors () const { return m_HasErrors; }
	
	#if UNITY_EDITOR
	struct ObjectData
	{
		int classID;
		dynamic_array<UInt8> data;
		TypeTree* typeTree;
	};
	bool ExtractObjectData (LocalIdentifierInFileType fileID, ObjectData& data);
	#endif

	// Get/Set the list of FileIdentifiers this file uses
	const dynamic_block_vector<FileIdentifier>& GetExternalRefs ()const	{ return m_Externals; }
	
	// Add an external reference
	void AddExternalRef (const FileIdentifier& pathName);

	// Is the header of the file written?
	bool IsFileDirty () const { return m_IsDirty; }
	
	inline bool ShouldSwapEndian () const { return m_FileEndianess != kActiveEndianess; }

	bool IsMemoryStream () const { return m_MemoryStream; }
	bool IsCachedFileStream () const { return m_CachedFileStream;  }

	void SetIsCachedFileStream (bool cache) { m_CachedFileStream = cache; }	

	#if SUPPORT_SERIALIZE_WRITE
	bool FinishWriting ();
	#endif

	#if SUPPORT_TEXT_SERIALIZATION
	bool IsTextFile () const { return m_IsTextFile; }
	#endif
private:
	// Writes everything that is in the caches out to the disk.
	void Flush ();

	bool FinalizeInit (int options);
	bool ReadHeader ();

	template<bool kSwap> bool ReadMetadata (int version, unsigned dataOffset, UInt8 const* data, size_t length, size_t dataFileSize);
	
#if SUPPORT_SERIALIZE_WRITE
	template<bool kSwap> void BuildMetadataSection (SerializationCache& cache, unsigned dataOffsetInFile);
	template<bool kSwap> bool WriteHeader (SerializationCache& cache);
#endif
	
#if SUPPORT_TEXT_SERIALIZATION
	template<class T>
	void WriteTextSerialized (string& label, T &data, int options);
	bool IndexTextFile ();
	bool ReadHeaderText ();
#endif


};

#if SUPPORT_TEXT_SERIALIZATION
bool IsSerializedFileTextFile(string pathName);
#endif

void InitializeStdConverters ();
void CleanupStdConverters();

#endif
