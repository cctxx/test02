#include "UnityPrefix.h"
#include "ProfilerConnection.h"

#if ENABLE_PROFILER

#if ENABLE_PLAYERCONNECTION
#include "ProfilerImpl.h"
#include "ProfilerFrameData.h"
#include "ProfilerStats.h"
#include "Runtime/Mono/MonoHeapShot.h"
#include "Runtime/Misc/BuildSettings.h"
#include "Runtime/Network/NetworkUtility.h"
#include "Runtime/Network/PlayerCommunicator/PlayerConnection.h"
#include "Runtime/Network/PlayerCommunicator/EditorConnection.h"
#include "ProfilerHistory.h"
#include "ObjectMemoryProfiler.h"
#endif

#if UNITY_EDITOR
#include "Editor/Src/ProjectWizardUtility.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include <ctime>
#endif

UInt32 ProfilerConnection::ms_EditorGuid = 0xFFFFFFFF;
UInt32 ProfilerConnection::ms_CustomIPGuid = 0xFFFFFFFE;
ProfilerConnection* ProfilerConnection::ms_Instance = NULL;

void ProfilerConnection::Initialize()
{
	Assert(ms_Instance == NULL);
	ms_Instance = new ProfilerConnection();
#if ENABLE_PLAYERCONNECTION 
	ms_Instance->PrepareConnections();
#endif
}

void ProfilerConnection::Cleanup()
{
	Assert(ms_Instance != NULL);
#if ENABLE_PLAYERCONNECTION 
	ms_Instance->RemoveConnections();
#endif
	delete ms_Instance;
	ms_Instance = NULL;
}

ProfilerConnection::ProfilerConnection() 
: m_ConnectedProfiler(ms_EditorGuid)
, m_CurrentBuildTarget(kBuildNoTargetPlatform)
{	
}

#if UNITY_EDITOR

void ProfilerConnection::DirectIPConnect(const std::string& IP)
{
	m_ConnectedProfiler = EditorConnection::Get().ConnectPlayerDirectIP(IP);
	if(m_ConnectedProfiler == PLAYER_DIRECT_IP_CONNECT_GUID)
		EnableConnectedProfiler(true);
}

void ProfilerConnection::GetAvailableProfilers ( std::vector<UInt32>& values )
{
	values.clear();
	values.push_back(ms_EditorGuid);
	EditorConnection::Get().GetAvailablePlayers(values);
}

void ProfilerConnection::EnableConnectedProfiler ( bool enable )
{
	int enabled = enable;
	EditorConnection::Get().SendMessage(m_ConnectedProfiler, GeneralConnection::kProfileStartupInformation, &enabled, 4);
}

std::string ProfilerConnection::GetConnectionIdentification(UInt32 guid)
{
	if (guid != ms_EditorGuid)
		return EditorConnection::Get().GetConnectionIdentifier(guid);
	return "Editor";
}

bool ProfilerConnection::IsIdentifierConnectable(UInt32 guid)
{
	if (guid != ms_EditorGuid)
		return EditorConnection::Get().IsIdentifierConnectable(guid);
	return true;
}

bool ProfilerConnection::IsIdentifierOnLocalhost(UInt32 guid)
{
	if (guid != ms_EditorGuid)
		return EditorConnection::Get().IsIdentifierOnLocalhost(guid);
	return true;
}

bool ProfilerConnection::IsConnectionEditor()
{
	return m_ConnectedProfiler == ms_EditorGuid;
}

void ProfilerConnection::SetConnectedProfiler( UInt32 guid, bool sendDisable)
{
	if (guid == ms_EditorGuid){
		if(m_ConnectedProfiler != guid && sendDisable)
			EnableConnectedProfiler(false);
		m_ConnectedProfiler = ms_EditorGuid;
		return;
	}
	
	m_ConnectedProfiler = EditorConnection::Get().ConnectPlayer(guid);
	if(m_ConnectedProfiler == guid)
		EnableConnectedProfiler(true);
}

void ProfilerConnection::SetupTargetSpecificConnection(BuildTargetPlatform targetPlatform)
{
	if (m_CurrentBuildTarget == targetPlatform)
		return;
	m_CurrentBuildTarget = targetPlatform;
	EditorConnection::Get().RemovePlayer(PLAYER_DIRECTCONNECT_GUID);
	switch (targetPlatform)
	{
		case kBuild_Android:
		{
			std::string localhost = "127.0.0.1";
			std::string hostName = Format("AndroidPlayer(ADB@%s:%i)", localhost.c_str(), PLAYER_DIRECTCONNECT_PORT);
			UInt32 guid = EditorConnection::Get().AddPlayer(hostName, localhost, PLAYER_DIRECTCONNECT_PORT, PLAYER_DIRECTCONNECT_GUID, GeneralConnection::kSupportsProfile);
			AssertMsg(guid == PLAYER_DIRECTCONNECT_GUID, "Unable to add Android direct profiler connection");
		}
	}
}

UInt32 ProfilerConnection::GetConnectedProfiler()
{
	return m_ConnectedProfiler;
}

UInt32 ProfilerConnection::GetEditorGuid()
{
	return ms_EditorGuid;
}

void ProfilerConnection::HandleDisconnectionMessage (UInt32 guid)
{
	Assert (UNITY_EDITOR);	
	ProfilerConnection::Get().SetConnectedProfiler(ms_EditorGuid, false);
}

#endif




// Network communication and serialization

#if ENABLE_PLAYERCONNECTION 

void ProfilerConnection::PrepareConnections()
{
#if UNITY_EDITOR
		EditorConnection::Get().RegisterConnectionHandler(&ProfilerConnection::HandleConnectionMessage);
		EditorConnection::Get().RegisterDisconnectionHandler(&ProfilerConnection::HandleDisconnectionMessage);
		EditorConnection::Get().RegisterMessageHandler(GeneralConnection::kProfileDataMessage, &ProfilerConnection::HandleProfilerDataMessage);
		EditorConnection::Get().RegisterMessageHandler(GeneralConnection::kObjectMemoryProfileDataMessage, &ProfilerConnection::HandleObjectMemoryProfileDataMessage);
		EditorConnection::Get().RegisterMessageHandler(GeneralConnection::kFileTransferMessage, &ProfilerConnection::HandleFileDataMessage);
#else
		PlayerConnection::Get().RegisterConnectionHandler(&ProfilerConnection::HandleConnectionMessage);
		PlayerConnection::Get().RegisterMessageHandler(GeneralConnection::kProfileStartupInformation, &ProfilerConnection::EnableProfilerMessage);
		PlayerConnection::Get().RegisterMessageHandler(GeneralConnection::kObjectMemoryProfileSnapshot, &ProfilerConnection::GetObjectMemoryProfile);
		PlayerConnection::Get().RegisterMessageHandler(GeneralConnection::kCaptureHeaphshotMessage, &ProfilerConnection::HandleCaptureHeapshotMessage);
#endif
}

void ProfilerConnection::RemoveConnections()
{
#if UNITY_EDITOR
		EditorConnection::Get().UnregisterConnectionHandler(&ProfilerConnection::HandleConnectionMessage);
		EditorConnection::Get().UnregisterDisconnectionHandler(&ProfilerConnection::HandleDisconnectionMessage);
		EditorConnection::Get().UnregisterMessageHandler(GeneralConnection::kProfileDataMessage, &ProfilerConnection::HandleProfilerDataMessage);
		EditorConnection::Get().UnregisterMessageHandler(GeneralConnection::kObjectMemoryProfileDataMessage, &ProfilerConnection::HandleObjectMemoryProfileDataMessage);
		EditorConnection::Get().UnregisterMessageHandler(GeneralConnection::kFileTransferMessage, &ProfilerConnection::HandleFileDataMessage);
#else
		PlayerConnection::Get().UnregisterConnectionHandler(&ProfilerConnection::HandleConnectionMessage);
		PlayerConnection::Get().UnregisterMessageHandler(GeneralConnection::kProfileStartupInformation, &ProfilerConnection::EnableProfilerMessage);
		PlayerConnection::Get().UnregisterMessageHandler(GeneralConnection::kObjectMemoryProfileSnapshot, &ProfilerConnection::GetObjectMemoryProfile);
		PlayerConnection::Get().UnregisterMessageHandler(GeneralConnection::kCaptureHeaphshotMessage, &ProfilerConnection::HandleCaptureHeapshotMessage);
#endif
}

void ProfilerConnection::HandleConnectionMessage (UInt32 guid)
{
	ProfilerConnection::Get().m_ConnectedProfiler = guid;
#if UNITY_EDITOR
	ProfilerConnection::Get().EnableConnectedProfiler(true);
#endif
}


void ProfilerConnection::EnableProfilerMessage ( const void* data, UInt32 size, UInt32 guid)
{
	// message sent from Editor to Player, to start and stop the profiler
	Assert(!UNITY_EDITOR);
	if(GetBuildSettingsPtr() && !GetBuildSettingsPtr()->hasAdvancedVersion)
	{
		ErrorString("Profiler is only supported with Pro License");
		return;
	}
	bool enable = *(int*)data != 0;
	if (enable)
		ProfilerConnection::Get().m_ConnectedProfiler = guid;
	if ( ProfilerConnection::Get().m_ConnectedProfiler == guid )
		UnityProfiler::Get().SetEnabled(enable);
}

void ProfilerConnection::GetObjectMemoryProfile( const void* data, UInt32 size, UInt32 guid)
{
	// message recieved on the player from Editor
	Assert(!UNITY_EDITOR);
	if ( ProfilerConnection::Get().m_ConnectedProfiler != guid )
		return;

	if(GetBuildSettingsPtr() && !GetBuildSettingsPtr()->hasAdvancedVersion)
	{
		ErrorString("Profiler is only supported with Pro License");
		return;
	}

#if ENABLE_MEM_PROFILER
	dynamic_array<int> buffer;
	ObjectMemoryProfiler::TakeMemorySnapshot(buffer);
	PlayerConnection::Get().SendMessage(ProfilerConnection::Get().m_ConnectedProfiler,PlayerConnection::kObjectMemoryProfileDataMessage, &buffer[0], buffer.size()*sizeof(int));
#endif	
}


void ProfilerConnection::HandleCaptureHeapshotMessage (const void* data, UInt32 size, UInt32 guid)
{
#if ENABLE_MONO_HEAPSHOT
	printf_console("Capturing heapshot\n");
	HeapShotData heapShotData;
	HeapShotDumpObjectMap(heapShotData);
	//bool WriteBytesToFile (const void *data, int byteLength, const string& pathName);
	//WriteBytesToFile(&data[0], data.size(), "game:\\test.dump");
	if (heapShotData.size() > 0)
		TransferFileOverPlayerConnection("testplayer.heapshot", &heapShotData[0], heapShotData.size()); 
#endif
}


void ProfilerConnection::SendFrameDataToEditor ( ProfilerFrameData& data )
{
	// TODO: Send partial data ( right now we double buffer the frame on the profiler side, to wait for GPU data)
	dynamic_array<int> buffer;
	
	UnityProfiler::SerializeFrameData(data, buffer);

	if(buffer.size()<128*1024)
		PlayerConnection::Get().SendMessage(m_ConnectedProfiler,PlayerConnection::kProfileDataMessage, &buffer[0], buffer.size()*sizeof(int));
}



#if UNITY_EDITOR

void ProfilerConnection::SendCaptureHeapshotMessage()
{
	int enabled = 1;
	//printf_console("Sending capture heapshot cmd");
	EditorConnection::Get().SendMessage(m_ConnectedProfiler, GeneralConnection::kCaptureHeaphshotMessage, &enabled, sizeof(int));
}

void ProfilerConnection::HandleFileDataMessage (const void* data, UInt32 size, UInt32 guid)
{
	UInt8* uData = (UInt8*) data;
	//printf_console("HandleFileDataMessage");

	UInt32 fileNameLength = *(UInt32*)uData;
	uData += sizeof(UInt32);

	char rawFileName[255];
	memcpy(rawFileName, uData, fileNameLength);
	rawFileName[fileNameLength] = '\0';
	uData += fileNameLength;

	UInt32 contentLength =  *(UInt32*)uData;
	uData += sizeof(UInt32);

	//printf_console("Name: %s Content length: %d\n", rawFileName, contentLength);

	time_t now;
	time(&now);
	struct tm nowTime;
	nowTime = *localtime(&now);

	std::string fileName = Format("%04d-%02d-%02d_%02dh%02dm%02ds.heapshot", nowTime.tm_year + 1900, nowTime.tm_mon + 1, nowTime.tm_mday,
		nowTime.tm_hour, nowTime.tm_min, nowTime.tm_sec);
	//printf_console(fileName.c_str());
	// ToDo: figure it out what we're saving, for now think always that it's a heapshot file
	std::string heapShotDirectory = AppendPathName (GetProjectPath (), "Heapshots");
	if (CreateDirectorySafe(heapShotDirectory))
	{
		std::string fullPath = AppendPathName (heapShotDirectory, fileName);
		WriteBytesToFile(uData, contentLength, fullPath);

		void* params[] = {scripting_string_new(fileName)};
		CallStaticMonoMethod ("HeapshotWindow", "EventHeapShotReceived", params);
	}
}
void ProfilerConnection::HandleProfilerDataMessage ( const void* data, UInt32 size, UInt32 guid )
{
	if (ProfilerConnection::Get().GetConnectedProfiler() != guid)
		return;

	if (!UnityProfiler::Get().GetEnabled())
		return;

	ProfilerFrameData* frame = UNITY_NEW(ProfilerFrameData, kMemProfiler) (1, 0);
	if( UnityProfiler::DeserializeFrameData(frame, data, size) )
		ProfilerHistory::Get().AddFrameDataAndTransferOwnership(frame, guid);
	else
		UNITY_DELETE(frame, kMemProfiler);	
}

void ProfilerConnection::SendGetObjectMemoryProfile()
{
#if ENABLE_MEM_PROFILER
	if(m_ConnectedProfiler == ms_EditorGuid)
		ObjectMemoryProfiler::SetDataFromEditor();
	else
		EditorConnection::Get().SendMessage(m_ConnectedProfiler, GeneralConnection::kObjectMemoryProfileSnapshot, NULL, 0);
#endif
}

void ProfilerConnection::HandleObjectMemoryProfileDataMessage ( const void* data, UInt32 size, UInt32 guid )
{
#if ENABLE_MEM_PROFILER
	if (ProfilerConnection::Get().GetConnectedProfiler() != guid)
		return;

	ObjectMemoryProfiler::DeserializeAndApply(data,size);
#endif
}

#endif



#endif
#endif
