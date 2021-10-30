#ifndef _PROFILERCONNECTION_H_
#define _PROFILERCONNECTION_H_

#if ENABLE_PROFILER

#include "Configuration/UnityConfigure.h"

#include "Runtime/Threads/Thread.h"
#include "ProfilerImpl.h"
#include "Runtime/Misc/SystemInfo.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

class ProfilerFrameData;

enum ConnectedProfiler
{
	kConnectedProfilerEditor,
	kConnectedProfilerStandalone,
	kConnectedProfilerWeb,
	kConnectedProfileriPhone,
	kConnectedProfilerAndroid,
	kConnectedProfilerXenon,
	kConnectedProfilerPS3,
	kConnectedProfilerWII,
	kConnectedProfilerPepper,
	kConnectedProfilerCount
};

class ProfilerConnection
{
public:
	static void Initialize();
	static void Cleanup();
	// Singleton accessor for ProfilerConnection
	static ProfilerConnection& Get() { return *ms_Instance; }

#if UNITY_EDITOR
	void GetAvailableProfilers (std::vector<UInt32>& values);

	void EnableConnectedProfiler ( bool enable );
	void SetConnectedProfiler (UInt32 guid, bool sendDisable = true);
	UInt32 GetConnectedProfiler ();
	static UInt32 GetEditorGuid();
	void DirectIPConnect(const std::string& IP);

	std::string GetConnectionIdentification(UInt32 guid);
	bool IsIdentifierConnectable(UInt32 guid);
	bool IsIdentifierOnLocalhost(UInt32 guid);
	bool IsConnectionEditor();
	void SetupTargetSpecificConnection(BuildTargetPlatform targetPlatform);

	static void HandleFileDataMessage (const void* data, UInt32 size, UInt32 guid);
	void SendCaptureHeapshotMessage();

	void SendGetObjectMemoryProfile();
#endif
	
	void SendFrameDataToEditor( ProfilerFrameData& data );


private:
	ProfilerConnection();

#if ENABLE_PLAYERCONNECTION 

	void PrepareConnections();
	void RemoveConnections();
	
	static void HandleProfilerDataMessage (const void* data, UInt32 size, UInt32 guid);
	static void HandleObjectMemoryProfileDataMessage (const void* data, UInt32 size, UInt32 guid);
	static void HandlePlayerConnectionMessage (const void* data, UInt32 size, UInt32 guid);
	static void HandleConnectionMessage (UInt32 guid);
	static void HandleDisconnectionMessage (UInt32 guid);
	static void EnableProfilerMessage ( const void* data, UInt32 size, UInt32 guid);
	static void GetObjectMemoryProfile( const void* data, UInt32 size, UInt32 guid);
	static void HandleCaptureHeapshotMessage ( const void* data, UInt32 size, UInt32 guid);
#endif

private:	
	UInt32 m_ConnectedProfiler;
	int	m_CurrentBuildTarget;

	// ProfilerConnection instance to use with singleton pattern
	static ProfilerConnection* ms_Instance;
	static UInt32 ms_EditorGuid;
	static UInt32 ms_CustomIPGuid;
};

#endif

#endif
