#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"
#include "SerializedFile.h"
#include "Runtime/Utilities/Utility.h"
#include "SerializeConversion.h"
#include "TransferUtility.h"
#include "Runtime/Serialize/TransferFunctions/TransferNameConversions.h"
#include "CacheWrap.h"
#include "Runtime/Utilities/Word.h"
#include "Configuration/UnityConfigureVersion.h"
#include "BuildTargetVerification.h"
#include "Runtime/Utilities/FileUtilities.h"
#if UNITY_WII
#include "PlatformDependent/Wii/WiiUtility.h"
#include "PlatformDependent/Wii/WiiLoadingScreen.h"
#endif

#include "Runtime/Misc/Allocator.h"

/// Set this to 1 to dump type trees to the console when they don't match between
/// the runtime and the file that is being loaded.  This is most useful when debugging
/// loading issues in players (also enable DEBUG_FORCE_ALWAYS_WRITE_TYPETREES in
/// BuildPlayerUtility.cpp to have type trees included in player data).
#define DEBUG_LOG_TYPETREE_MISMATCHES 0

using namespace std;

enum { kCurrentSerializeVersion = 9 };

const char* kAssetBundleVersionNumber = "1";
const char* kUnityTextMagicString = "%YAML 1.1";
#define kUnityTextHeaderFileID -1

bool IsSerializedFileTextFile(string pathName)
{
	const int magiclen = strlen(kUnityTextMagicString);
	
	char compare[256];
	if (!ReadFromFile (pathName, compare, 0, magiclen))
		return false;
	
	compare[magiclen] = '\0';
	if (strcmp(compare, kUnityTextMagicString) == 0)
		return true;
	
	return false;
}


#if ENABLE_SECURITY
#define TEST_LEN(x) if (iterator + sizeof(x) > end) \
{\
	return false; \
}

#define TEST_READ_SIZE(x) if (iterator + x > end) \
{\
return false; \
}

#else
#define TEST_LEN(x)
#define TEST_READ_SIZE(x)
#endif

static const int kHeaderSize_Ver8 = 12;
static const int kPreallocateFront = 4096;

struct SerializedFileHeader
{
	// This header is always in BigEndian when in file
	// Metadata follows directly after the header
	UInt32	m_MetadataSize;
	UInt32	m_FileSize;
	UInt32	m_Version;
	UInt32	m_DataOffset;
	UInt8	m_Endianess;
	UInt8	m_Reserved[3];
	
	void SwapEndianess ()
	{
		SwapEndianBytes (m_MetadataSize);
		SwapEndianBytes (m_FileSize);
		SwapEndianBytes (m_Version);
		SwapEndianBytes (m_DataOffset);
	}
};

int RemapClassIDToNewClassID (int classID)
{
	switch(classID)
	{
		case 1012: return 1011; // AvatarSkeletonMask -> AvatarMask
		default: return classID;
	}
}

SerializedFile::SerializedFile ()
: m_Externals(1024,kMemSerialization)
{
	m_ReadOffset = 0;
	m_WriteDataOffset = 0;
	m_IsDirty = false;
	m_MemoryStream = false;
	m_HasErrors = false;
	m_CachedFileStream = false;
	m_TargetPlatform = kBuildNoTargetPlatform;
	m_SubTarget = 0;
	
	#if SUPPORT_TEXT_SERIALIZATION
	m_IsTextFile = false;
	#endif

	#if SUPPORT_SERIALIZE_WRITE
	m_CachedWriter = NULL;
	#endif

	m_ReadFile = NULL;
}


#if SUPPORT_SERIALIZE_WRITE
bool SerializedFile::InitializeWrite (CachedWriter& cachedWriter, BuildTargetSelection target, int options)
{
	SET_ALLOC_OWNER(this);
	m_TargetPlatform = target.platform;
	m_SubTarget = target.subTarget;

	m_CachedWriter = &cachedWriter;
	
	Assert (!((options & kAllowTextSerialization) && (options & kSerializeGameRelease)));
	m_IsTextFile = options & kAllowTextSerialization;
	
	if (!m_IsTextFile)
	{
		void* buffer = alloca (kPreallocateFront);
		memset (buffer, 0, kPreallocateFront);
		
		// Write header and reserve space for metadata. In case the resulting metadata will not fit
		// in the preallocated space we'll remove it and write it tightly packed in FinalizeWrite later.
		// In case it fits, we'll have a hole between meta and object data and that's fine.

		m_CachedWriter->Write(buffer, kPreallocateFront);
		m_WriteDataOffset = m_CachedWriter->GetPosition ();
	}
	
	return FinalizeInit(options);	
}
#endif

bool SerializedFile::InitializeRead (const string& path, ResourceImageGroup& resourceImage, unsigned cacheSize, unsigned cacheCount, int options, int readOffset)
{
	SET_ALLOC_OWNER(this);
	m_ReadOffset = readOffset;
	m_ReadFile = UNITY_NEW( FileCacherRead (path, cacheSize, cacheCount), kMemFile);
	m_ResourceImageGroup = resourceImage;

	return FinalizeInit(options);	
}

bool SerializedFile::InitializeMemoryBlocks (const string& path, UInt8** buffer, unsigned size, unsigned offset, int options)
{
	SET_ALLOC_OWNER(this);
	m_MemoryStream = true;
	m_ReadOffset = offset;
	m_ReadFile = UNITY_NEW( MemoryCacherReadBlocks (buffer, size, kCacheBlockSize), kMemFile);
	
	return FinalizeInit(options);	
}

bool SerializedFile::FinalizeInit (int options)
{
	m_Options = options;
	#if GAMERELEASE
	m_Options |= kSerializeGameRelease;
	#endif
	
	if (m_Options & kSwapEndianess)
		m_FileEndianess = kOppositeEndianess;
	else
		m_FileEndianess = kActiveEndianess;

	if (m_ReadFile)
	{
#if SUPPORT_TEXT_SERIALIZATION
		const int magiclen = strlen(kUnityTextMagicString);
		char compare[256];
		if (m_ReadFile->GetFileLength () >= magiclen)
		{
			ReadFileCache (*m_ReadFile, compare, 0 + m_ReadOffset, magiclen);
			compare[magiclen] = '\0';
			if (strcmp(compare, kUnityTextMagicString) == 0)
			{
				m_IsTextFile = true;
				m_ReadOffset += magiclen;
				return ReadHeaderText();
			}
		}
		m_IsTextFile = false;
#endif
		return ReadHeader();
	}
	else
	{
#if SUPPORT_TEXT_SERIALIZATION
		if (m_IsTextFile)
		{
			string label = kUnityTextMagicString;
			label += "\n%TAG !u! tag:unity3d.com,2011:\n";
			m_CachedWriter->Write (&label[0], label.length());
		}
#endif
		return true;
}
}

SerializedFile::~SerializedFile ()
{
	UNITY_DELETE( m_ReadFile, kMemFile);
}


#if SUPPORT_SERIALIZE_WRITE
bool SerializedFile::FinishWriting ()
{
	AssertIf(m_CachedWriter == NULL);
	
	if (m_CachedWriter != NULL)
	{
		if (!m_IsTextFile)
		{
			SerializationCache metadataBuffer;
			
			if (!ShouldSwapEndian())
			{
				BuildMetadataSection<false> (metadataBuffer, m_WriteDataOffset);
				return WriteHeader<false> (metadataBuffer);
			}
			else
			{
				BuildMetadataSection<true> (metadataBuffer, m_WriteDataOffset);
				return WriteHeader<true> (metadataBuffer);
			}
		}
		else 
		{
			bool success = m_CachedWriter->CompleteWriting();
			success &= m_CachedWriter->GetCacheBase().WriteHeaderAndCloseFile(NULL, 0, 0);
			return success;
		}
	}
	
	return false;
}

static void WriteAlignmentData (File& file, size_t misalignment)
{
	Assert (misalignment < SerializedFile::kSectionAlignment);
	UInt8 data[SerializedFile::kSectionAlignment];
	memset (data, 0, misalignment);
	file.Write(data, misalignment);
}

template<bool kSwap>
bool SerializedFile::WriteHeader (SerializationCache& metadata)
{
	bool success = true;
	
	// The aggregated metadata fits into the pre-written block, so write it directly.
	if (metadata.size () <= kPreallocateFront - sizeof (SerializedFileHeader))
	{
		UInt8* temp = (UInt8*)alloca (kPreallocateFront);

		SerializedFileHeader& header = *(SerializedFileHeader*)temp;
		header.m_MetadataSize = metadata.size ();
		header.m_FileSize = m_CachedWriter->GetPosition ();
		header.m_Version = kCurrentSerializeVersion;
		header.m_DataOffset = m_WriteDataOffset;
		header.m_Endianess = m_FileEndianess;
		memset (header.m_Reserved, 0, sizeof header.m_Reserved);
		
		if (kActiveEndianess != kBigEndian)
			header.SwapEndianess ();
		
		std::copy (metadata.begin (), metadata.end (), temp + sizeof (SerializedFileHeader));
		success &= m_CachedWriter->CompleteWriting();
		success &= m_CachedWriter->GetCacheBase ().WriteHeaderAndCloseFile (temp, 0, sizeof (SerializedFileHeader) + metadata.size ());
	}
	else
	{
		// metadata doesn't fit, therefore close the file, write header + metadata to another file
		// and copy data over from 'this' one.
	
		success &= m_CachedWriter->CompleteWriting();
		success &= m_CachedWriter->GetCacheBase ().WriteHeaderAndCloseFile (NULL, 0, 0);
		
		size_t dataFileSize = m_CachedWriter->GetPosition ();
		if (dataFileSize < kPreallocateFront)
			return false;
		
		size_t dataSize = dataFileSize - kPreallocateFront;
		size_t dataOffsetOriginal = sizeof (SerializedFileHeader) + metadata.size ();
		size_t dataOffset = RoundUp (dataOffsetOriginal, kSectionAlignment);
		
		std::string originalPath = m_CachedWriter->GetCacheBase().GetPathName ();
		std::string tempPath = GenerateUniquePathSafe (originalPath);
		
		SerializedFileHeader header =
		{
			metadata.size (), dataOffset + dataSize,
			kCurrentSerializeVersion,
			dataOffset,
			m_FileEndianess, 0, 0, 0
		};
		
		if (kActiveEndianess != kBigEndian)
			header.SwapEndianess ();
		
		File file;
		success &= file.Open(tempPath, File::kWritePermission);
		
		// header
		success &= file.Write (&header, sizeof (header));
		
		// metadata
		success &= file.Write (&*metadata.begin (), metadata.size ());
		if (dataOffset != dataOffsetOriginal)
			WriteAlignmentData (file, dataOffset - dataOffsetOriginal);
		FatalErrorIf (dataOffset != file.GetPosition ());
		
		{
			enum { kCopyChunck = 1 * 1024 * 1024 };
			
			UInt8* buffer;
			ALLOC_TEMP(buffer, UInt8, kCopyChunck);
			
			File srcFile;
			success &= srcFile.Open(originalPath, File::kReadPermission);
			
			size_t position = kPreallocateFront;
			size_t left = dataSize;
			while (left > 0 && success)
			{
				size_t toRead = (std::min)((size_t)kCopyChunck, left);
				int wasRead = srcFile.Read (position, buffer, toRead);
				success &= file.Write (buffer, wasRead);
				position += toRead;
				left -= toRead;
			}
			success &= srcFile.Close ();
			
			success &= file.Close ();
		}
			
		// move the temp file over to the destination
		success &= DeleteFile(originalPath);
		success &= MoveFileOrDirectory (tempPath, originalPath);
	}
	
	return success;
}
#endif // SUPPORT_SERIALIZE_WRITE

enum { kMaxTypeCount = 100000 };

bool SerializedFile::ReadHeader ()
{
	AssertIf (m_ReadFile == NULL);

	SerializedFileHeader header;
	
	if (m_ReadFile->GetFileLength () < sizeof (header))
		return false;

	ReadFileCache (*m_ReadFile, &header, m_ReadOffset, sizeof (header));
	
	if (kActiveEndianess == kLittleEndian)
		header.SwapEndianess ();
	
	// Consistency check if the file is a valid serialized file.
	if (header.m_MetadataSize == -1)
		return false;
	if (header.m_Version == 1)
		return false;
	if (header.m_Version > kCurrentSerializeVersion)
		return false;
	
	unsigned metadataSize, metadataOffset;
	unsigned dataSize, dataOffset;
	unsigned dataEnd;

	if (header.m_Version >= 9)
	{
		// If we're reading a stream file, m_ReadOffset + header.m_FileSize will not necessarilly be equal to m_ReadFile->GetFileLength(), 
		// because there can be few padding bytes which doesn't count into header.m_FileSize
		// See WriteStreamFile in BuildPlayerUtility.cpp
		if ((m_ReadOffset + header.m_FileSize) > m_ReadFile->GetFileLength () || header.m_DataOffset > header.m_FileSize)
			return false;
		
		// [header][metadata[...]][data]
		
		metadataOffset = sizeof header;
		metadataSize = header.m_MetadataSize;
		
		m_FileEndianess = header.m_Endianess;

		dataOffset = header.m_DataOffset;
		dataSize = header.m_FileSize - header.m_DataOffset;
		dataEnd = dataOffset + dataSize;
	}
	else
	{
		// [header][data][metadata]

		// We set dataOffset to zero, because offsets in object table are file-start based
		dataOffset = 0;
		dataSize = header.m_FileSize - header.m_MetadataSize - kHeaderSize_Ver8;
		dataEnd = header.m_FileSize - header.m_MetadataSize;
		
		// Offset by one, because we're reading the endianess flag right here
		metadataOffset = header.m_FileSize - header.m_MetadataSize + 1;
		metadataSize = header.m_MetadataSize - 1;
		
		if (metadataSize == -1 || (m_ReadOffset + header.m_FileSize) > m_ReadFile->GetFileLength () || dataEnd > header.m_FileSize)
			return false;	
		
		ReadFileCache (*m_ReadFile, &m_FileEndianess, m_ReadOffset + metadataOffset - 1, sizeof (m_FileEndianess));
	}
		
	// Check endianess validity
	if (m_FileEndianess != kBigEndian && m_FileEndianess != kLittleEndian)
		return false;
	
	SerializationCache metadataBuffer;
	metadataBuffer.resize (metadataSize);
	ReadFileCache (*m_ReadFile, &metadataBuffer[0], m_ReadOffset + metadataOffset, metadataSize);
	
	bool result;
	if (m_FileEndianess == kActiveEndianess)
	{
		result = ReadMetadata<false>(header.m_Version, dataOffset, &*metadataBuffer.begin (), metadataBuffer.size (), dataEnd);
	}
	else
	{
		result = ReadMetadata<true>(header.m_Version, dataOffset, &*metadataBuffer.begin (), metadataBuffer.size (), dataEnd);
	}
	
	if (!result)
	{
		ErrorString(Format("Failed to read file '%s' because it is corrupted.", m_ReadFile->GetPathName ().c_str()));
	}
	return result;
}

#if SUPPORT_TEXT_SERIALIZATION
bool SerializedFile::IndexTextFile()
{
	const size_t kBufferLength = 1024;
	const size_t kMaxLineLength = 256;
	bool hasMergeConflicts = false;
	string read;
	read.resize (kBufferLength);

	size_t readPos = 0;
	size_t lineStart = 0;
	int lineCount = 1; //We start counting lines at one, not zero.
	ObjectInfo *curInfo = NULL;
	
	const char *guidLabel = "guid: ";
	int guidLabelLen = strlen (guidLabel);
	int guidLabelPos = 0;
	bool lineContainsGUID = false;
	std::string prevLine;
	std::set <FileIdentifier> externals;
	while (readPos < m_ReadFile->GetFileLength() - m_ReadOffset)
	{
		size_t readBufferLength = std::min (m_ReadFile->GetFileLength() - m_ReadOffset - readPos, kBufferLength);
		ReadFileCache (*m_ReadFile, &read[0], m_ReadOffset + readPos, readBufferLength);
		
		for (size_t i=0; i<readBufferLength; i++)
		{
			if (read[i] == guidLabel[guidLabelPos])
			{
				guidLabelPos++;
				if (guidLabelPos == guidLabelLen)
					lineContainsGUID = true;
			}
			else 
				guidLabelPos = 0;
			
			if (read[i] == '\n')
			{
				lineCount++;
				size_t lineEnd = i+1+readPos;
				size_t lineLength = std::min(lineEnd - lineStart, kMaxLineLength);
				string line;
				if (lineStart < readPos)
				{
					line.resize (lineLength);
					ReadFileCache (*m_ReadFile, &line[0], m_ReadOffset + lineStart, lineLength);
				}
				else
					line = read.substr (lineStart - readPos, lineLength);
					
				if (line.length())
				{
					switch (line[0])
					{
						case ' ': 
							break; //fast path for most common case.
						case '-':
							{
								if (curInfo)
									curInfo->byteSize = lineStart - curInfo->byteStart;
								SInt32 fileID, classID;
								if (sscanf(line.c_str(), "--- !u!%d &%d", (int*)&classID, (int*)&fileID) == 2)
								{
									curInfo = &m_Object[fileID];
									curInfo->classID = RemapClassIDToNewClassID(classID); 
									curInfo->typeID = 0; 
									curInfo->byteStart = lineEnd; 
									curInfo->isDestroyed = false; 
									curInfo->debugLineStart = lineCount;
								}
							}
							break;
					#if UNITY_EDITOR
						case '>':
						case '<':
						case '=':
							if (!hasMergeConflicts)
								WarningStringMsg ("The file %s seems to have merge conflicts. Please open it in a text editor and fix the merge.\n", m_DebugPath.c_str());
							hasMergeConflicts = true;
							break;
					#endif
					}
					if (lineContainsGUID)
					{
						if (line.find ('}') == string::npos)
							prevLine = line;
						else
						{
							line = prevLine + line;
							line = line.substr(line.find ('{'));
							YAMLRead read (line.c_str(), line.size(), 0, &m_DebugPath, lineCount-1);
							
							FileIdentifier id;

							read.Transfer (id.guid, "guid");
							read.Transfer (id.type, "type");
							id.Fix_3_5_BackwardsCompatibility();
							
							if (id.guid != UnityGUID())
								externals.insert (id);
							else
								ErrorStringMsg ("Could not extract GUID in text file %s at line %d.", m_DebugPath.c_str(), lineCount-1);
							
							lineContainsGUID = false;
							prevLine = "";
						}
					}
				}
				lineStart = lineEnd;
			}
		}
				
		readPos += readBufferLength;
	}
	if (curInfo)
		curInfo->byteSize = readPos - curInfo->byteStart;
	
	m_Externals.assign (externals.begin(), externals.end());
	return !hasMergeConflicts;
}

bool SerializedFile::ReadHeaderText ()
{
	Assert (m_ReadFile != NULL);
	return IndexTextFile ();
}	

template<class T>
void SerializedFile::WriteTextSerialized (std::string &label, T &data, int options)
{
	Assert (m_CachedWriter != NULL);
	
	m_CachedWriter->Write (&label[0], label.length());

	YAMLWrite writeStream (options, &m_DebugPath);
	data.VirtualRedirectTransfer (writeStream);
	writeStream.OutputToCachedWriter(m_CachedWriter);
	if (writeStream.HasError())
		m_HasErrors = true;
}

#endif

// The header is put at the end of and is only allowed to be read at startup

template<bool kSwap>
bool SerializedFile::ReadMetadata (int version, unsigned dataOffset, UInt8 const* data, size_t length, size_t dataFileEnd)
{
	AssertIf(kSwap && kActiveEndianess == m_FileEndianess);
	AssertIf(!kSwap && kOppositeEndianess == m_FileEndianess);
	SET_ALLOC_OWNER(this);
	
	UInt8 const* iterator = data, *end = data + length;

	// Read Unity version file was built with
	UnityStr unityVersion;
	if (version >= 7)
	{
		if (!ReadString(unityVersion, iterator, end))
			return false;
	}
	
	// Build target platform verification
	if (version >= 8)
	{
		TEST_LEN(m_TargetPlatform);
		ReadHeaderCache<kSwap> (m_TargetPlatform, iterator);
		
		if (!CanLoadFileBuiltForTargetPlatform(static_cast<BuildTargetPlatform>(m_TargetPlatform)))
		{
			ErrorStringMsg(
				"The file can not be loaded because it was created for another build target that is not compatible with this platform.\n"
				"Please make sure to build asset bundles using the build target platform that it is used by.\n"
				"File's Build target is: %d\n",
				(int)m_TargetPlatform
			);
			return false;
		}
	}
	
	// Read number of types
	SInt32 typeCount;
	TEST_LEN(typeCount);
	ReadHeaderCache<kSwap> (typeCount, iterator);

	#if SUPPORT_SERIALIZED_TYPETREES
	// Read	types
	for (int i=0;i<typeCount;i++)
	{
		TypeTree* readType = UNITY_NEW (TypeTree, kMemTypeTree);
		TypeMap::key_type classID;
		TEST_LEN(classID);
		ReadHeaderCache<kSwap> (classID, iterator);
		if (!ReadTypeTree (*readType, iterator, end, version, kSwap))
			return false;
		classID = RemapClassIDToNewClassID(classID);

		m_Type[classID].SetOldType (readType);
	}
	#else
	if (typeCount != 0)
	{
		ErrorString("Serialized file contains typetrees but the target can't use them. Will ignore typetrees.");
	}
	#endif
	
	SInt32 bigIDEnabled = 0;
	if (version >= 7)
		ReadHeaderCache<kSwap> (bigIDEnabled, iterator);

	// Read number of objects
	SInt32 objectCount;
	TEST_LEN(objectCount);
	ReadHeaderCache<kSwap> (objectCount, iterator);
	
	// Check if the size is roughly out of bounds, we only want to prevent running out of memory due to insane objectCount value here.
	TEST_READ_SIZE(objectCount * 12)
	
	// Read Objects
	m_Object.reserve(objectCount);
	for (int i=0;i<objectCount;i++)
	{
		LocalIdentifierInFileType fileID;
		ObjectMap::mapped_type value;
		
		if (bigIDEnabled)
		{
//			AssertIf(fileID64 > LOCAL_IDENTIFIER_IN_FILE_SIZE);
			UInt64 fileID64;
			TEST_LEN(fileID64);
			ReadHeaderCache<kSwap> (fileID64, iterator);
			fileID = fileID64;
		}
		else
		{
			UInt32 fileID32;
			TEST_LEN(fileID32);
			ReadHeaderCache<kSwap> (fileID32, iterator);
			fileID = fileID32;
		}
		
		TEST_LEN(value);
		ReadHeaderCache<kSwap> (value.byteStart, iterator);
		ReadHeaderCache<kSwap> (value.byteSize, iterator);
		ReadHeaderCache<kSwap> (value.typeID, iterator);
		ReadHeaderCache<kSwap> (value.classID, iterator);
		ReadHeaderCache<kSwap> (value.isDestroyed, iterator);
		
		value.byteStart += dataOffset;

		// TODO check this with joachim
		value.typeID = RemapClassIDToNewClassID(value.typeID);
		value.classID = RemapClassIDToNewClassID(value.classID);
		
		AssertIf (value.byteStart + value.byteSize > dataFileEnd);
		if (value.byteStart < 0 || value.byteSize < 0 || value.byteStart + value.byteSize < value.byteStart || value.byteStart + value.byteSize > dataFileEnd)
			return false;

		m_Object.push_unsorted(fileID, value);
		//printf_console ("fileID: %d byteStart: %d classID: %d \n", fileID, value.byteStart, value.classID);
	}
	
	// If there's no type tree then Unity version must mach exactly.
	// 
	// For asset bundles we write the asset bundle serialize version and compare against that.
	// The asset bundle itself contains hashes of all serialized classes and uses it to figure out if an asset bundle can be loaded.
	bool needsVersionCheck = !m_Object.empty() && typeCount == 0 && (m_Options & kIsBuiltinResourcesFile) == 0;
	if (needsVersionCheck)
	{
		bool versionPasses;
		string::size_type newLinePosition = unityVersion.find('\n');
		// Compare Unity version number
		if (newLinePosition == string::npos)
			versionPasses = unityVersion == UNITY_VERSION;
		// Compare asset bundle serialize version
		else
			versionPasses = string (unityVersion.begin() + newLinePosition + 1, unityVersion.end()) == kAssetBundleVersionNumber;
		
		if (!versionPasses)
		{
			ErrorStringMsg("Invalid serialized file version. File: \"%s\". Expected version: " UNITY_VERSION ". Actual version: %s.", m_ReadFile->GetPathName().c_str(), unityVersion.c_str());
			return false;
		}
	}
	
	#if SUPPORT_SERIALIZED_TYPETREES
	if (unityVersion.find("3.5.0f5") == 0)
		m_Options |= kWorkaround35MeshSerializationFuckup;
	#endif

//	printf_console("file version: %s  - '%s'\n", unityVersion.c_str(), m_DebugPath.c_str());
	
	// Read externals/pathnames
	SInt32 externalsCount;
	TEST_LEN(externalsCount);

	ReadHeaderCache<kSwap> (externalsCount, iterator);
	// Check if the size is roughly out of bounds, we only want to prevent running out of memory due to insane externalsCount value here.
	TEST_READ_SIZE(externalsCount)

	m_Externals.resize (externalsCount);
	
	for (int i=0;i<externalsCount;i++)
	{
		if (version >= 5)
		{
			if (version >= 6)
			{
				///@TODO: Remove from serialized file format
				UnityStr tempEmpty;
				if (!ReadString(tempEmpty, iterator, end))
					return false;
			}
			
			TEST_LEN(m_Externals[i].guid.data);
			for (int g=0;g<4;g++)
				ReadHeaderCache<kSwap> (m_Externals[i].guid.data[g], iterator);
			
			TEST_LEN(m_Externals[i].type);
			ReadHeaderCache<kSwap> (m_Externals[i].type, iterator);
			if (!ReadString (m_Externals[i].pathName, iterator, end))
				return false;
		}
		else
		{
			if (!ReadString (m_Externals[i].pathName, iterator, end))
				return false;
		}
		
		#if UNITY_EDITOR
		m_Externals[i].Fix_3_5_BackwardsCompatibility ();
		#endif
		
		m_Externals[i].CheckValidity ();
	}
	
	// Read Userinfo string
	if (version >= 5)
	{
		UnityStr userInformation;
		if (!ReadString (userInformation, iterator, end))
			return false;
	}
	
	Assert (iterator == end);
	
	return true;
}

#if SUPPORT_SERIALIZE_WRITE

template<bool kSwap>
void SerializedFile::BuildMetadataSection (SerializationCache& cache, unsigned dataOffsetInFile)
{
	// Write Unity version file is being built with
	UnityStr version = UNITY_VERSION;
	if (m_Options & kSerializedAssetBundleVersion)
	{
		version += "\n";
		version += kAssetBundleVersionNumber;
	}
	WriteString (version, cache);
	
	WriteHeaderCache<kSwap> (m_TargetPlatform, cache);
	
	if ((m_Options & kDisableWriteTypeTree) == 0)
	{
		// Write number of types
		SInt32 typeCount = m_Type.size ();
		WriteHeaderCache<kSwap> (typeCount, cache);
		
		// Write type data
		for (TypeMap::iterator i = m_Type.begin ();i != m_Type.end ();i++)
		{	
			AssertIf (i->second.GetOldType () == NULL);
			WriteHeaderCache<kSwap> (i->first, cache);
			WriteTypeTree (*i->second.GetOldType (), cache, kSwap);
		}
	}
	else
	{
		SInt32 typeCount = 0;
		WriteHeaderCache<kSwap> (typeCount, cache);
	}

	SInt32 bigIDEnabled = LOCAL_IDENTIFIER_IN_FILE_SIZE > 32;
	WriteHeaderCache<kSwap> (bigIDEnabled, cache);
		
	// Write number of objects
	SInt32 objectCount = m_Object.size ();
	WriteHeaderCache<kSwap> (objectCount, cache);
	for (ObjectMap::iterator i = m_Object.begin ();i != m_Object.end ();i++)
	{	
		if (bigIDEnabled)
		{
			UInt64 bigID = i->first;
			WriteHeaderCache<kSwap> (bigID, cache);
		}
		else
		{
			UInt32 smallID = i->first;
			WriteHeaderCache<kSwap> (smallID, cache);
		}			
		
		WriteHeaderCache<kSwap> (i->second.byteStart - dataOffsetInFile, cache);
		WriteHeaderCache<kSwap> (i->second.byteSize, cache);
		WriteHeaderCache<kSwap> (i->second.typeID, cache);
		WriteHeaderCache<kSwap> (i->second.classID, cache);
		WriteHeaderCache<kSwap> (i->second.isDestroyed, cache);
		
		//printf_console ("fileID: %d byteStart: %d classID: %d \n", i->first, i->second.byteStart, i->second.classID);
	}

	// Write externals
	objectCount = m_Externals.size ();
	WriteHeaderCache<kSwap> (objectCount, cache);
	for (int i=0;i<objectCount;i++)
	{
		UnityStr tempEmpty;
		WriteString (tempEmpty, cache);
		for (int g=0;g<4;g++)
			WriteHeaderCache<kSwap> (m_Externals[i].guid.data[g], cache);
		WriteHeaderCache<kSwap> (m_Externals[i].type, cache);
		WriteString (m_Externals[i].pathName, cache);
	}
	
	// Write User info
	UnityStr tempUserInformation;
	WriteString (tempUserInformation, cache);	
}
#endif

bool SerializedFile::IsAvailable (LocalIdentifierInFileType id) const
{
	ObjectMap::const_iterator i = m_Object.find (id);
	if (i == m_Object.end ())
		return false;
	else
		return ! i->second.isDestroyed;
}

int SerializedFile::GetClassID (LocalIdentifierInFileType id) const
{
	ObjectMap::const_iterator i = m_Object.find (id);
	AssertIf (i == m_Object.end ());
	return i->second.classID;
}

int SerializedFile::GetByteStart (LocalIdentifierInFileType id) const
{
	ObjectMap::const_iterator i = m_Object.find (id);
	AssertIf (i == m_Object.end ());
	return i->second.byteStart;
}

int SerializedFile::GetByteSize (LocalIdentifierInFileType id) const
{
	ObjectMap::const_iterator i = m_Object.find (id);
	AssertIf (i == m_Object.end ());
	return i->second.byteSize;
}
#if SUPPORT_SERIALIZED_TYPETREES
const TypeTree* SerializedFile::GetTypeTree (LocalIdentifierInFileType id)
{
	ObjectMap::iterator found = m_Object.find(id);
	if (found == m_Object.end())
		return NULL;
	
	TypeMap::iterator type = m_Type.find (found->second.typeID);
	if (type == m_Type.end ())
		return NULL;
	return type->second.GetOldType ();
}
#endif

#if !UNITY_EXTERNAL_TOOL
// objects: On return, all fileIDs to all objects in this Serialze
void SerializedFile::GetAllFileIDs (vector<LocalIdentifierInFileType>* objects)const
{
	AssertIf (objects == NULL);
	
	objects->reserve (m_Object.size ());
	ObjectMap::const_iterator i;
	for (i=m_Object.begin ();i!=m_Object.end ();i++)
	{
		if (i->second.isDestroyed)
			continue;
		
		Object::RTTI* rtti = Object::ClassIDToRTTI(i->second.classID);
		if (rtti == NULL || rtti->factory == NULL)
			continue;

		objects->push_back (i->first);
	}
}
#endif
// objects: On return, all fileIDs to all objects in this Serialze
void SerializedFile::GetAllFileIDsUnchecked (vector<LocalIdentifierInFileType>* objects)const
{
	AssertIf (objects == NULL);
	
	objects->reserve (m_Object.size ());
	ObjectMap::const_iterator i;
	for (i=m_Object.begin ();i!=m_Object.end ();i++)
	{
		if (i->second.isDestroyed)
			continue;
		
		objects->push_back (i->first);
	}
}

LocalIdentifierInFileType SerializedFile::GetHighestID () const
{
	if (m_Object.empty ())
		return 0;
	else
		return m_Object.rbegin ()->first;
}

// Returns whether or not the object referenced by id was destroyed
bool SerializedFile::DestroyObject (LocalIdentifierInFileType id)
{
	#if SUPPORT_SERIALIZE_WRITE
	AssertIf(m_CachedWriter);
	#endif
	m_IsDirty = true;

	ObjectMap::iterator o;
	o = m_Object.find (id);
	if (o == m_Object.end ())
	{
		SET_ALLOC_OWNER(this);
		m_Object[id].isDestroyed = true;		
		return false;
	}
	else
	{
		o->second.isDestroyed = true;
		return true;
	}
}

#if SUPPORT_SERIALIZE_WRITE

void SerializedFile::WriteObject (Object& object, LocalIdentifierInFileType fileID, const BuildUsageTag& buildUsage)
{
	AssertIf (m_CachedWriter == NULL);
	SET_ALLOC_OWNER(this);

	bool perObjectTypeTree = object.GetNeedsPerObjectTypeTree ();
	
	int typeID = object.GetClassID ();

	int mask = kNeedsInstanceIDRemapping | m_Options;
	
#if UNITY_EDITOR
	object.SetFileIDHint (fileID);
#endif
	
#if SUPPORT_TEXT_SERIALIZATION
	if (!m_IsTextFile)
	{
#endif
	
	// Native C++ object typetrees share typetree by class id
	if (!perObjectTypeTree)
	{
		// Include Type
		Type& type = m_Type[typeID];

		// If we need a perObjectTypeTree there is only one object using it  (the one we are writing now)
		// Thus we can safely replace the old typeTree
		// If we have a per class typetree do not override the old typetree
		if (type.GetOldType () == NULL)
		{
			// Create new type and init it using a proxy transfer
			TypeTree* typeTree = UNITY_NEW (TypeTree, kMemTypeTree);
			GenerateTypeTree (object, typeTree, mask | kDontRequireAllMetaFlags);

			// Register the type tree
			type.SetOldType (typeTree);
		}
	}
	// Scripted objects we search the registered typetrees for duplicates and share
	// or otherwise allocate a new typetree.
	else
	{
		// Create new type and init it using a proxy transfer
		TypeTree* typeTree = UNITY_NEW (TypeTree, kMemTypeTree);
		
		GenerateTypeTree (object, typeTree, mask | kDontRequireAllMetaFlags);
		
		typeID = 0;
		
		// Find if there 
		for (TypeMap::iterator i=m_Type.begin();i!=m_Type.end();i++)		
		{
			if (i->first > 0)
				break;
			
			if (IsStreamedBinaryCompatbile(*typeTree, *i->second.GetOldType()))
			{
				typeID = i->first;
				UNITY_DELETE(typeTree, kMemTypeTree);
				break;
			}
		}
		
		// Allocate type id
		if (typeID == 0)
		{
			if (m_Type.empty())
				typeID = -1;
			else if (m_Type.begin()->first < 0)
				typeID = m_Type.begin()->first - 1;
			else	
				typeID = -1;
			
			Type& type = m_Type[typeID];

			// Create new type and init it using a proxy transfer
			// Register the type tree
			type.SetOldType (typeTree);
		}
	}

#if SUPPORT_TEXT_SERIALIZATION
	}
#endif

	// We are not taking care of fragmentation.	 		
	const unsigned kFileAlignment = 8;

	unsigned unalignedByteStart = m_CachedWriter->GetPosition();
	
	// Align the object to a kFileAlignment byte boundary
	unsigned alignedByteStart = unalignedByteStart;
	if (unalignedByteStart % kFileAlignment != 0)
		alignedByteStart += kFileAlignment - unalignedByteStart % kFileAlignment;
	

	AssertIf (m_Object.find (fileID) != m_Object.end () && m_Object.find (fileID)->second.classID != object.GetClassID ());
	ObjectInfo& info = m_Object[fileID];
	info.byteStart = alignedByteStart;
	info.classID = object.GetClassID ();
	info.isDestroyed = false;
	info.typeID = typeID;

/*	////// PRINT OUT serialized Data as ascii to console
	if (false && gPrintfDataHack)
	{
		printf_console ("\n\nPrinting object: %d\n", fileID);
	
		// Set write marker to end of file and register the objects position in file
		StreamedTextWrite writeStream;
		CachedWriter& cache = writeStream.Init (kNeedsInstanceIDRemapping);
		cache.Init (m_FileCacher, alignedByteStart, 0, false);
		
		// Write the object
		object.VirtualRedirectTransfer (writeStream);
		cache.End ();
	}
*/

#if SUPPORT_TEXT_SERIALIZATION
	if (m_IsTextFile)
	{
		string label = Format("--- !u!%d &%d\n", info.classID, (int)fileID);
		WriteTextSerialized (label, object, mask);
	}
	else
#endif
	if (!ShouldSwapEndian())
	{
		// Set write marker to end of file and register the objects position in file
		StreamedBinaryWrite<false> writeStream;
			
		CachedWriter& cache = writeStream.Init (*m_CachedWriter, mask, BuildTargetSelection(static_cast<BuildTargetPlatform>(m_TargetPlatform),m_SubTarget), buildUsage);
		char kZeroAlignment[kFileAlignment] = {0, 0, 0, 0, 0, 0, 0, 0};
		cache.Write (kZeroAlignment, alignedByteStart - unalignedByteStart);
		
		// Write the object
		object.VirtualRedirectTransfer (writeStream);
		
		*m_CachedWriter = cache;
	}
	else
	{
		// Set write marker to end of file and register the objects position in file
		StreamedBinaryWrite<true> writeStream;
		CachedWriter& cache = writeStream.Init (*m_CachedWriter, mask, BuildTargetSelection(static_cast<BuildTargetPlatform>(m_TargetPlatform),m_SubTarget), buildUsage);
		char kZeroAlignment[kFileAlignment] = {0, 0, 0, 0, 0, 0, 0, 0};
		cache.Write (kZeroAlignment, alignedByteStart - unalignedByteStart);

		// Write the object
		object.VirtualRedirectTransfer (writeStream);
		
		*m_CachedWriter = cache;
	}
	
	info.byteSize = m_CachedWriter->GetPosition() - info.byteStart;
}
#endif

#if !UNITY_EXTERNAL_TOOL

enum { kMonoBehaviourClassID = 114 };
static void OutOfBoundsReadingError (int classID, int expected, int was)
{
	if (classID == kMonoBehaviourClassID)
	{
		// This code should not access any member variables
		// because that might dereference pointers etc during threaded loading.
		ErrorString(Format("A script behaviour has a different serialization layout when loading. (Read %d bytes but expected %d bytes)\nDid you #ifdef UNITY_EDITOR a section of your serialized properties in any of your scripts?", was, expected));
	}
	else
	{
		ErrorString(Format("Mismatched serialization in the builtin class '%s'. (Read %d bytes but expected %d bytes)", Object::ClassIDToString(classID).c_str(), was, expected));
	}
}

void SerializedFile::ReadObject (LocalIdentifierInFileType fileID, int instanceId, ObjectCreationMode mode, bool isPersistent, TypeTree** oldTypeTree, bool* didChangeTypeTree, Object** outObjectPtr)
{
#if UNITY_WII
	wii::ProcessShutdownAndReset();
	wii::ProcessLoadingScreen();
#endif
//	printf_console("Reading instance: %d fileID: %d filePtr: %d\n", instanceId, fileID, this);
	
	ObjectMap::iterator iter = m_Object.find (fileID);

	// Test if the object is in stream
	if (iter == m_Object.end ())
		return;

	if (iter->second.isDestroyed)
	{
		return;
	}

	const ObjectInfo& info = iter->second;
	
	// Create empty object
	Object* objectPtr = *outObjectPtr;
	if (objectPtr == NULL)
	{
		*outObjectPtr = objectPtr = Object::Produce (info.classID, instanceId, kMemBaseObject, mode);
	}

	if (objectPtr == NULL)
	{
		ErrorString ("Could not produce class with ID " + IntToString (info.classID));
		return;
	}
	SET_ALLOC_OWNER(this);
#if UNITY_EDITOR
	(**outObjectPtr).SetFileIDHint (fileID);
#endif

	#if SUPPORT_SERIALIZED_TYPETREES
	AssertIf (objectPtr->GetClassID () != info.classID);
	bool perClassTypeTree = true;
	Type* type = NULL;
	if (!m_Type.empty() 
#if SUPPORT_TEXT_SERIALIZATION
	&& !m_IsTextFile
#endif
	)
	{
		// Find TypeTree
		type = &m_Type[info.typeID];
		AssertIf (type->GetOldType () == NULL);
	
		perClassTypeTree = !objectPtr->GetNeedsPerObjectTypeTree ();

		AssertIf (perClassTypeTree && objectPtr->GetClassID () != info.typeID);
		// If we have a per class type tree we do not generate a typetree before reading the object
		// That also means we always use SafeBinaryRead.
		
		// Setup new header type data
		// If we have a perObject TypeTree we dont need the new type since we safebinaryread anyway.
		// If we have a per class typetree we generate the typetree only once since it can't change
		// while the application is running
		if (type->GetNewType () == NULL && perClassTypeTree)
		{
			// Create new type and init it using a proxy transfer
			TypeTree* typeTree = UNITY_NEW (TypeTree, kMemTypeTree);
			
			int typeTreeOptions = kDontRequireAllMetaFlags | m_Options;
			GenerateTypeTree (*objectPtr, typeTree, typeTreeOptions);
		
			// Register the type tree
			type->SetNewType (typeTree);
			
			#if DEBUG_LOG_TYPETREE_MISMATCHES // Log when loaded typetree is out of sync.
			if (!type->EqualTypes ())
			{
				printf_console("Typetree mismatch in path: %s\n", m_DebugPath.c_str());
				printf_console("TYPETREE USED BY RUNTIME IS: \n");
				string buffer0;
				type->GetOldType()->DebugPrint(buffer0);
				printf_console(buffer0.c_str());

				printf_console("TYPETREE IN FILE IS:\n");
				string buffer1;
				type->GetNewType()->DebugPrint(buffer1);
				printf_console(buffer1.c_str());
			}
			#endif
		}
		AssertIf (type->EqualTypes () && !perClassTypeTree);
	}
	#endif

	int options = kNeedsInstanceIDRemapping | m_Options;
	if (ShouldSwapEndian())
		options |= kSwapEndianess;
	if (mode == kCreateObjectFromNonMainThread)
		options |= kThreadedSerialization;
	
	objectPtr->SetIsPersistent(isPersistent);
	
	int byteStart = info.byteStart + m_ReadOffset;

	#if SUPPORT_SERIALIZED_TYPETREES
	// Fill object with data
	
	// Never use the SafeBinaryRead code path for Meshes serialized with 3.5, because that needs a special workaround,
	// which requires using StreamedBinaryRead.
	if (type != NULL && !type->EqualTypes () && !((options & kWorkaround35MeshSerializationFuckup) && info.classID == ClassID(Mesh)))
	{
		SafeBinaryRead readStream;
		
		CachedReader& cache = readStream.Init (*type->GetOldType (), byteStart, info.byteSize , options);
		cache.InitRead (*m_ReadFile, byteStart, info.byteSize);
		Assert(m_ResourceImageGroup.resourceImages[0] == NULL);

		objectPtr->Reset ();
		
		// Read the object
		objectPtr->VirtualRedirectTransfer (readStream);
		int position = cache.End ();
		if (position - byteStart > info.byteSize)
			OutOfBoundsReadingError (info.classID, info.byteSize, position - byteStart);
		
		*didChangeTypeTree = perClassTypeTree;
	}
	else
	#endif
	{
		// we will read up that object - no need to call Reset as we will construct it fully
		objectPtr->HackSetResetWasCalled();
		
		///@TODO: Strip endianess!
#if SUPPORT_TEXT_SERIALIZATION
		if (m_IsTextFile)
		{
			YAMLRead readStream (m_ReadFile, byteStart, byteStart + info.byteSize, options, &m_DebugPath, info.debugLineStart);
			objectPtr->VirtualRedirectTransfer (readStream);		
			*didChangeTypeTree = false;
		}
		else
#endif
		if (!ShouldSwapEndian())
		{
			StreamedBinaryRead<false> readStream;
			CachedReader& cache = readStream.Init (options);
			cache.InitRead (*m_ReadFile, info.byteStart + m_ReadOffset, info.byteSize);
			cache.InitResourceImages (m_ResourceImageGroup);
			
			// Read the object
			objectPtr->VirtualRedirectTransfer (readStream);
			int position = cache.End ();
			if (position - byteStart != info.byteSize)
				OutOfBoundsReadingError (info.classID, info.byteSize, position - byteStart);
			
			*didChangeTypeTree = false;
		}
		else
		{
			#if SUPPORT_SERIALIZED_TYPETREES
			StreamedBinaryRead<true> readStream;
			CachedReader& cache = readStream.Init (options);

			cache.InitRead (*m_ReadFile, byteStart, info.byteSize);
			Assert(m_ResourceImageGroup.resourceImages[0] == NULL);

			// Read the object
			objectPtr->VirtualRedirectTransfer (readStream);
			int position = cache.End ();
			if (position - byteStart != info.byteSize)
				OutOfBoundsReadingError (info.classID, info.byteSize, position - byteStart);

			*didChangeTypeTree = false;
			#else
			AssertString("reading endian swapped is not supported");
			#endif
		}
	}

	#if SUPPORT_SERIALIZED_TYPETREES
	if (type)
		*oldTypeTree = type->GetOldType ();
	else
		*oldTypeTree = NULL;
	#endif
	
	// Setup hide flags when loading from a resource file
	if (m_Options & kIsBuiltinResourcesFile)
		objectPtr->SetHideFlagsObjectOnly(Object::kHideAndDontSave | Object::kHideInspector);

	return;
}	
					
#endif

bool SerializedFile::IsEmpty () const
{
	for (ObjectMap::const_iterator i = m_Object.begin ();
		i != m_Object.end (); ++i)
	{
		if (!i->second.isDestroyed)
			return false;
	}
	return true;
}

void SerializedFile::AddExternalRef (const FileIdentifier& pathName)
{
	// Dont' check for pathname here - it can be empty, if we are getting a GUID from
	// a text serialized file, and the file belonging to the GUID is missing. In that
	// case we just keep the GUID.
	#if SUPPORT_SERIALIZE_WRITE
	Assert (m_CachedWriter != NULL);
	#endif
	m_Externals.push_back (pathName);
	m_Externals.back().CheckValidity();
}

void InitializeStdConverters ()
{
	RegisterAllowTypeNameConversion ("PPtr<TransformComponent>", "PPtr<Transform>");
	RegisterAllowTypeNameConversion ("PPtr<FileTexture>", "PPtr<Texture2D>");
	RegisterAllowTypeNameConversion ("PPtr<FileTexture>", "PPtr<Texture>");
	RegisterAllowTypeNameConversion ("PPtr<Animation>", "PPtr<AnimationClip>");
	RegisterAllowTypeNameConversion ("PPtr<GOComponent>", "PPtr<Component>");
	RegisterAllowTypeNameConversion ("PPtr<Script>", "PPtr<TextAsset>");
	RegisterAllowTypeNameConversion ("UniqueIdentifier", "GUID");
	RegisterAllowTypeNameConversion ("Vector3f", "Vector2f");
	RegisterAllowTypeNameConversion ("Vector3f", "Vector4f");
	RegisterAllowTypeNameConversion ("vector_set", "set");
	RegisterAllowTypeNameConversion ("vector_map", "map");
	RegisterAllowTypeNameConversion ("map", "vector");
	RegisterAllowTypeNameConversion ("set", "vector");
	RegisterAllowTypeNameConversion ("list", "vector");
	RegisterAllowTypeNameConversion ("deque", "vector");
	RegisterAllowTypeNameConversion ("dynamic_array", "vector");
	RegisterAllowTypeNameConversion ("TypelessData", "dynamic_array");
	RegisterAllowTypeNameConversion ("tricky_vector", "vector");
	RegisterAllowTypeNameConversion ("PPtr<DataTemplate>", "PPtr<Prefab>");
	RegisterAllowTypeNameConversion ("PPtr<EditorExtension>", "PPtr<Object>");

	// Support for converting MonoBehaviour GUIStyle arrays to C++ native GUIStyle
	RegisterAllowTypeNameConversion ("GUIStyle", "vector");
	RegisterAllowTypeNameConversion ("Generic Mono", "GUIStyle");
	RegisterAllowTypeNameConversion ("Generic Mono", "RectOffset");
	RegisterAllowTypeNameConversion ("Generic Mono", "GUIStyleState");
	RegisterAllowTypeNameConversion ("PPtr<$Texture2D>", "PPtr<Texture2D>");
	RegisterAllowTypeNameConversion ("PPtr<$Font>", "PPtr<Font>");

	RegisterAllowTypeNameConversion ("PPtr<AvatarBodyMask>", "PPtr<AvatarMask>");		
	RegisterAllowTypeNameConversion ("PPtr<AnimationSet>", "PPtr<AnimatorOverrideController>");
	RegisterAllowTypeNameConversion ("AvatarSkeletonMaskElement", "TransformMaskElement");
	RegisterAllowTypeNameConversion ("HumanLayerConstant", "LayerConstant");	
	RegisterAllowTypeNameConversion ("PPtr<AnimatorController>", "PPtr<RuntimeAnimatorController>");
	
#if SUPPORT_SERIALIZED_TYPETREES 
	REGISTER_CONVERTER (float, double);
	REGISTER_CONVERTER (double, float);

	REGISTER_CONVERTER (int, float);
	
	#define REGISTER_BASIC_INTEGERTYPE_CONVERTER(x)	\
		REGISTER_CONVERTER (x, UInt64);				\
		REGISTER_CONVERTER (x, SInt64);				\
		REGISTER_CONVERTER (x, SInt32);				\
		REGISTER_CONVERTER (x, UInt32);				\
		REGISTER_CONVERTER (x, UInt16);				\
		REGISTER_CONVERTER (x, SInt16);				\
		REGISTER_CONVERTER (x, UInt8);				\
		REGISTER_CONVERTER (x, SInt8);				\
		REGISTER_CONVERTER (x, bool)
	
	
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(UInt64);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(SInt32);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(UInt32);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(UInt16);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(SInt16);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(UInt8);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(SInt8);
	REGISTER_BASIC_INTEGERTYPE_CONVERTER(bool);

	#undef REGISTER_BASIC_INTEGERTYPE_CONVERTER

#endif
}

void CleanupStdConverters()
{
	ClearTypeNameConversion();
	SafeBinaryRead::CleanupConverterTable();
}

#if UNITY_EDITOR
bool SerializedFile::ExtractObjectData (LocalIdentifierInFileType fileID, SerializedFile::ObjectData& data)
{
	ObjectMap::iterator iter = m_Object.find (fileID);
	if (iter == m_Object.end ())
		return false;

	const ObjectInfo& info = iter->second;
	SET_ALLOC_OWNER(this);
	// Find TypeTree
	if (info.typeID)
	{
	Type& type = m_Type[info.typeID];
	AssertIf (type.GetOldType () == NULL);
		data.typeTree = type.GetOldType ();
	}
	else
		data.typeTree = NULL;
	
	data.classID = info.classID;
	
	if(info.byteSize > 0)
	{
		CachedReader cache;
		cache.InitRead (*m_ReadFile, m_ReadOffset + info.byteStart, info.byteSize);
		
		data.data.resize_uninitialized(info.byteSize);
		cache.Read(data.data.begin(), info.byteSize);
		cache.End();
	}	
	return true;
}

void FileIdentifier::CheckValidity ()
{
	FatalErrorIf (type == kMetaAssetType && !pathName.empty());
	FatalErrorIf (type == kSerializedAssetType && !pathName.empty());
	FatalErrorIf (type == kDeprecatedCachedAssetType);
}

void FileIdentifier::Fix_3_5_BackwardsCompatibility ()
{
	// Backwards compatibility with 3.5 and before.
	// We no longer store the path for asset types. (It is implicit by the GUID)
	if (type == FileIdentifier::kDeprecatedCachedAssetType)
		type = FileIdentifier::kMetaAssetType;
	if (type == FileIdentifier::kSerializedAssetType || type == FileIdentifier::kMetaAssetType)
		pathName.clear();
}

#endif
