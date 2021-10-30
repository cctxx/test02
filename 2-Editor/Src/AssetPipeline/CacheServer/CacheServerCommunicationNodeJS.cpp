#include "UnityPrefix.h"
#include "CacheServerCommunicationNodeJS.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/PlayerPrefs.h"
#include "Runtime/Network/Sockets.h"
#include "Runtime/Network/SocketStreams.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Serialize/SwapEndianBytes.h"
#include "Editor/Src/AssetPipeline/MdFourGenerator.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Editor/Src/EditorSettings.h"

using namespace std;


const char* kCacheServerTempFolder = "Temp/CacheServerIntegration";
const char* kCacheServerIPPrefsKey = "CacheServerIPAddress";
const char* kCacheServerEnableKey = "CacheServerEnabled";
const char* kCacheServerDB = "UnityCacheServerDefault";

const char* kMetaTag = "meta";
const char* kAssetInfoTag = "assetInfo";

enum
{ 
	kReadWriteBufferSize = 128 * 1024,
	kCacheServerPort = 8125,
	kCacheServerConnectionTimeoutMS = 2000
};

SocketStream* gSocket = NULL;

bool CanConnectToCacheServer()
{
	if (IsConnectedToCacheServer())
		DisconnectFromCacheServer();
	CacheServerError result = ConnectToCacheServer();
	if (IsConnectedToCacheServer())	
		DisconnectFromCacheServer();
	return result == kCacheServerSuccess;
}

bool IsConnectedToCacheServer ()
{
	return gSocket ? gSocket->IsConnected() : false;
}

enum CacheServerConnectionStatus
{
	NoStatus = 0,
	Connected = 1,
	NotResolveHost = 2,
	NotConnectToHost = 3,
	NotSendRequest = 4,
	NotReceiveReply = 5,
	VersionMismatch = 6
};

void CacheServerConnectError (const std::string& error, CacheServerConnectionStatus& currentStatus, CacheServerConnectionStatus status)
{
	// Only warn when there's a status change.
	if (currentStatus == status )
		return;

	DebugStringToFile ("Couldn't connect to to Cache Server.\n" + error, 0, __FILE__, __LINE__, kScriptingError, 0, GetEditorSettings().GetInstanceID());
	currentStatus = status;
}

CacheServerError ConnectToCacheServer()
{	
	static CacheServerConnectionStatus status = NoStatus;
	static bool socketConnectionError = false;

	if (IsConnectedToCacheServer())
	{
		ErrorString("Already connected to cache server");
		return kCacheServerConnectionFailure;
	}
	
	// Silently ignore cache server
	if (!LicenseInfo::Flag (lf_maint_client))
		return kCacheServerConnectionFailure;

	if (!EditorPrefs::GetBool(kCacheServerEnableKey))
		return kCacheServerConnectionFailure;
	
	string ip = EditorPrefs::GetString(kCacheServerIPPrefsKey);

	// XXX Move
	// Should be moved to network util or smth similar
	struct hostent *host = gethostbyname (ip.c_str());
	if (host == NULL)
	{
		CacheServerConnectError (Format("Could not resolve host %s.", ip.c_str()), status, NotResolveHost);
		return kCacheServerConnectionFailure;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(kCacheServerPort);
	memcpy (&addr.sin_addr, host->h_addr_list[0], host->h_length);
	// XXX

	int socketHandle = Socket::Connect( (const sockaddr*) &addr, sizeof(addr), kCacheServerConnectionTimeoutMS,
		false, !socketConnectionError);

	if (socketHandle < 0)
	{
		socketConnectionError = true;
		CacheServerConnectError (Format("Unable to connect to host %s:%d.", ip.c_str(), kCacheServerPort), status, NotConnectToHost);
		return kCacheServerConnectionFailure;
	}
	
	socketConnectionError = false;

	gSocket = new SocketStream(socketHandle, true);
	
	if (!gSocket->SendAll ("ff", 2))
	{
		CacheServerConnectError ("Could not send request to cache server.", status, NotSendRequest);
		DisconnectFromCacheServer();
		return kCacheServerConnectionFailure;
	}
	char reply[8];
	if (!gSocket->RecvAll (reply, sizeof(reply)))
	{
		CacheServerConnectError ("Could not receive reply from cache server.", status, NotReceiveReply);
		DisconnectFromCacheServer();
		return kCacheServerConnectionFailure;
	}
	
	if (memcmp (reply, "000000ff", sizeof(reply)) != 0)
	{
		CacheServerConnectError ("Cache server version mismatch.", status, VersionMismatch);
		DisconnectFromCacheServer();
		return kCacheServerConnectionFailure;
	}
	
	// remove errors upon successful connection.
	RemoveErrorWithIdentifierFromConsole (GetEditorSettings().GetInstanceID());
	status = Connected;

	return kCacheServerSuccess;
}

void DisconnectFromCacheServer()
{
	if (gSocket)
	{
		gSocket->Shutdown();
		delete gSocket;
		gSocket = NULL;
	}
}

bool DownloadDataHeader (UInt64 &offset, UInt64 &length, std::string& outputName)
{
	if (!gSocket->RecvAll (&length, sizeof(length)))
	{
		printf_console ("Could not read length from socket\n");
		return false;
	}
	offset += sizeof(length);

	UInt8 nameLength;
	if (!gSocket->RecvAll (&nameLength, sizeof(nameLength)))
	{
		printf_console ("Could not read nameLength from socket\n");
		return false;
	}
	offset += sizeof(nameLength);
	
	outputName.resize(nameLength);
	
	if (nameLength > 0 && !gSocket->RecvAll (&outputName[0], nameLength))
	{
		printf_console ("Could not read file name from socket\n");
		return false;
	}
	offset += nameLength;
	return true;
}

bool DownloadDataFromCombineFile (UInt64 &offset, UInt64 length, string &data)
{		
	data.clear ();
	
	data.resize (length);
	if (!gSocket->RecvAll (&data[0], length))
	{
		printf_console ("Could not read file data from socket\n");
		return false;
	}
	offset += length;
	
	return true;
}

bool DownloadFile (UInt64 &offset, UInt64 length, const std::string& path)
{
	if (length == 0)
	{
		printf_console ("Cached file is empty\n");
		return false;
	}
	
	File f;
	if (f.Open(path, File::kWritePermission))
	{
		char* recvBuffer;
		ALLOC_TEMP (recvBuffer, char, kReadWriteBufferSize);
				
		while (length > 0)
		{
			size_t bytesToRead = kReadWriteBufferSize;
			if (length < bytesToRead)
				bytesToRead = length;
			if (!gSocket->RecvAll (recvBuffer, bytesToRead))
			{
				f.Close ();
				printf_console ("Could not read file data from socket\n");
				return false;
			}
			f.Write(recvBuffer, bytesToRead);
			length -= bytesToRead;
			offset += bytesToRead;
		}
		f.Close ();
		return true;
	}
	printf_console ("Could not open file for writing\n");
	return false;
}

bool UploadFile (const std::string& path, size_t length)
{
	if (length == 0)
	{
		printf_console ("Cached file is empty\n");
		return false;
	}
	
	File f;
	if (!f.Open(path, File::kReadPermission))
		return false;
	
	char* writeBuffer;
	ALLOC_TEMP (writeBuffer, char, kReadWriteBufferSize);

	while (length != 0)
	{
		size_t bytesRead = f.Read(writeBuffer, kReadWriteBufferSize);
		if (bytesRead <= 0 || !gSocket->Send (writeBuffer, bytesRead))
		{
			printf_console ("Failed sending data to cache server.\n");			
			f.Close();		
			return false;
		}
		length -= bytesRead;
	}
	
	f.Close();
	
	return true;
}
	
#define USE_TEMP_DIR 0

CacheServerError DownloadCacheServerAsset (size_t len, CacheServerData& data, const std::string& baseName, const UnityGUID& guid)
{	
#if USE_TEMP_DIR
	string metaDataPath = baseName;
#else
	string metaDataPath = AppendPathNameExtension(GetMetaDataPathFromGUID(guid), "tmp_cache");
#endif
	
	UInt64 offset = 0;
	
	while (len > offset)
	{
		string outputName;
		UInt64 length;
		if (!DownloadDataHeader (offset, length, outputName))
			return kCacheServerInvalidDataFileFailure;

		// Zero sized files are not allowed...
		if (length == 0)
			return kCacheServerInvalidDataFileFailure;
		
		#define DOWNLOAD_SIMPLE(x) { data.x = x; if (!DownloadFile (offset, length, data.x)) return kCacheServerConnectionFailure; }
		if (outputName == kMetaTag)
			DOWNLOAD_SIMPLE(metaDataPath)
		else if (outputName == kAssetInfoTag)
		{
			if (!DownloadDataFromCombineFile (offset, length, data.assetInfoData))
				return kCacheServerInvalidDataFileFailure;
		}
		else
		{
			return kCacheServerInvalidDataFileFailure;
		}
	}
		
	data.downloadSize += offset;
	
	if (len != offset)
		return kCacheServerInvalidDataFileFailure;
	
	return kCacheServerSuccess;
}

CacheServerError ReadRequestedFilesFromDB(const string &baseName, CacheServerData& data, const UnityGUID &guid, const MdFour &hash)
{	
	char* recvBuffer;
	ALLOC_TEMP (recvBuffer, char, kReadWriteBufferSize);
	
	if (!gSocket->RecvAll(recvBuffer, 1))
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerConnectionFailure;
	}	

	if (recvBuffer[0] == '-')
	{
		if (!gSocket->RecvAll(recvBuffer, sizeof(UnityGUID) + sizeof(MdFour)))
		{
			printf_console ("Did not get expected reply from cache server\n");
			return kCacheServerConnectionFailure;
		}
		return kCacheServerNotCached;
	}

	if (recvBuffer[0] != '+')
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerConnectionFailure;
	}
	
	if (!gSocket->RecvAll(recvBuffer, 16))
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerConnectionFailure;
	}	

	UInt64 size;
	HexStringToBytes (recvBuffer, sizeof(UInt64), &size);
	SwapEndianBytesBigToNative(size);

	if (!gSocket->RecvAll(recvBuffer, sizeof(UnityGUID)))
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerConnectionFailure;
	}
	
	if (memcmp (guid.data, recvBuffer, sizeof(UnityGUID)) != 0)
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerInvalidDataFileFailure;
	}
	
	if (!gSocket->RecvAll(recvBuffer, sizeof(MdFour)))
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerInvalidDataFileFailure;
	}
	
	if (memcmp (hash.bytes, recvBuffer, sizeof(MdFour)) != 0)
	{
		printf_console ("Did not get expected reply from cache server\n");
		return kCacheServerInvalidDataFileFailure;
	}
	
	return DownloadCacheServerAsset (size, data, baseName, guid);
}

CacheServerError DownloadFromCacheServer (const UnityGUID& guid, const MdFour& hash, CacheServerData& output)
{
	if (!IsConnectedToCacheServer())
		return kCacheServerConnectionFailure;
	
	// Copy cache file to temporary location (Cache file might not exist - this is valid)
	return ReadRequestedFilesFromDB(AppendPathName(kCacheServerTempFolder, GUIDToString(guid)), output, guid, hash);
}

CacheServerError RequestFromCacheServer (const UnityGUID& guid, const MdFour& hash)
{
	if (!IsConnectedToCacheServer())
		return kCacheServerConnectionFailure;
	
	char command[1 + sizeof(UnityGUID) + sizeof(MdFour)];
	command[0] = 'g';
	memcpy (command + 1, guid.data, sizeof(UnityGUID));
	memcpy (command + 1 + sizeof(UnityGUID), hash.bytes, sizeof(MdFour));
	if (!gSocket->SendAll (command, sizeof(command)))
		return kCacheServerConnectionFailure;
	
	return kCacheServerSuccess; 
}

static bool UploadHeader (const char* name, UInt64 length)
{
	Assert (strlen (name) < 256);
	UInt8 nameLength = strlen (name);
	
	if (!gSocket->Send (&length, sizeof(length))) 
		return false;
	
	if (!gSocket->Send (&nameLength, sizeof(nameLength))) 
		return false;
	
	if (!gSocket->Send (name, nameLength))
		return false;
	
	return true;
}

static size_t CalculateHeaderSize (const char* name)
{
	return sizeof(UInt64) + sizeof(UInt8) + strlen (name);
}

size_t CalculateCachedFileSize (const CacheServerData& data, size_t metaDataFileSize)
{
	size_t metaDataSize = metaDataFileSize + CalculateHeaderSize (kMetaTag);
	size_t assetInfoSize = data.assetInfoData.size() + CalculateHeaderSize (kAssetInfoTag);
	
	return metaDataSize + assetInfoSize;
}

bool UploadCachedFile (const CacheServerData& data, size_t metaDataFileSize)
{
	// MetaData file
	if (!UploadHeader(kMetaTag, metaDataFileSize))
		return false;
	if (!UploadFile(data.metaDataPath, metaDataFileSize))
		return false;
	
	// asset Info header
	if (!UploadHeader (kAssetInfoTag, data.assetInfoData.size()))
		return false;
	if (!gSocket->Send(&data.assetInfoData[0], data.assetInfoData.size()))
		return false;
	
	return true;
}

CacheServerError UploadToCacheServer(const UnityGUID& guid, const MdFour& hash, const CacheServerData& data)
{
	if (!IsConnectedToCacheServer())
		return kCacheServerConnectionFailure;

	size_t metaDataFileSize = ::GetFileLength(data.metaDataPath);
	UInt64 size = CalculateCachedFileSize(data, metaDataFileSize);
	
	// GUID & hash
	char command[1 + 16 + sizeof(UnityGUID) + sizeof(MdFour)];
	command[0] = 'p';
	SwapEndianBytesBigToNative(size);
	BytesToHexString (&size, sizeof(UInt64), command + 1);
	memcpy (command + 17, guid.data, sizeof(UnityGUID));
	memcpy (command + 17 + sizeof(UnityGUID), hash.bytes, sizeof(MdFour));
	
	if (!gSocket->Send (command, sizeof(command)))
	{
		printf_console ("Failed sending request to cache server.\n");	
		return kCacheServerConnectionFailure;
	}
	
	if (!UploadCachedFile(data, metaDataFileSize))
		return kCacheServerConnectionFailure;

	return kCacheServerSuccess;
}