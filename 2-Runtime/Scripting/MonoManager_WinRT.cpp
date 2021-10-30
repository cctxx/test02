#include "UnityPrefix.h"

#include "MonoManager_WinRT.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/BaseClasses/ManagerContext.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"

#include "Runtime/Scripting/Backend/Metro/ScriptingTypeProvider_Metro.h"
#include "Runtime/Scripting/Backend/Metro/ScriptingMethodFactory_Metro.h"

#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"

#include "Runtime/Mono/MonoBehaviourSerialization_ByCodeGeneration.h"

#include "PlatformDependent/MetroPlayer/MetroUtils.h"

MonoManager::MonoManager (MemLabelId label, ObjectCreationMode mode)
	: ScriptingManager(label, mode, UNITY_NEW( ScriptingTypeProvider_Metro(), kMemManager), UNITY_NEW( ScriptingMethodFactory_Metro(), kMemManager))
{
	METRO_DBG_MARK_TIME("MonoManager::ctor");
	InputString data;
	const char* managedAssemblies = "managedAssemblies.txt";
	if (ReadStringFromFile(&data, PathToAbsolutePath(managedAssemblies)))
	{
		GetWinRTUtils()->LoadAssemblies(ConvertUtf8ToString(data.c_str()));
	}
	else
	{
		FatalErrorMsg("Failed to load %s", managedAssemblies);
	}
	METRO_DBG_MARK_TIME("MonoManager::FillCommonScriptingClasses begin...");
	FillCommonScriptingClasses(m_CommonScriptingClasses);
	METRO_DBG_MARK_TIME("MonoManager::FillCommonScriptingClasses end...");
	SetupExceptionHandler();
	SetupMarshalCallbacks();
	void RegisterAllInternalCalls();
	RegisterAllInternalCalls();

#	if ENABLE_SERIALIZATION_BY_CODEGENERATION

		s_WinRTBridge->SetupSerializationReader(
				(long long)(NativeExt_MonoBehaviourSerialization_ReaderAlign),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadBuffer),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadUnityEngineObject),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadGUIStyle),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadRectOffset),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadAnimationCurve),
				(long long)(NativeExt_MonoBehaviourSerialization_ReadGradient));

		s_WinRTBridge->SetupSerializationWriter(
				(long long)(NativeExt_MonoBehaviourSerialization_WriterAlign),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteBuffer),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteUnityEngineObject),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteGUIStyle),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteRectOffset),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteAnimationCurve),
				(long long)(NativeExt_MonoBehaviourSerialization_WriteGradient));

		s_WinRTBridge->SetupSerializationRemapper(
				(long long)(NativeExt_MonoBehaviourSerialization_GetNewInstanceToReplaceOldInstance));

		ScriptingInvocation initManagedAnalysis(GetScriptingMethodRegistry().GetMethod("UnityEngine.Serialization","ManagedLivenessAnalysis","Init"));
		initManagedAnalysis.Invoke();
#	endif
	// It tries to get classes like LevelGameManager which doesn't exist in managed land, is this intended?
	RebuildClassIDToScriptingClass();

#	if !ENABLE_WINRT_PINVOKE
	METRO_DBG_MARK_TIME("MonoManager::SetupDelegates begin...");
	GetWinRTUtils()->SetupDelegates();
	METRO_DBG_MARK_TIME("MonoManager::SetupDelegates end...");
#	else
	if (GetWinRTUtils()->IsPlatformInvokeSupported() == false)
		FatalErrorMsg("WinRTBridge: Platform Invoke should be supported on this platform.");
	if (UnityEngineDelegates::PlatformInvoke::IsSupported() == false)
		FatalErrorMsg("UnityEngineDelegates: Platform Invoke should be supported on this platform.");
#	endif

}

MonoManager::~MonoManager ()
{

}

template<class TransferFunction>
void MonoManager::Transfer (TransferFunction& transfer)
{
	Super::Transfer (transfer);
	transfer.SetVersion(2);
	transfer.Transfer (m_MonoScriptManager.m_RuntimeScripts, "m_Scripts");

	// ToDo: Temporary fix if we're deserializing for metro see MonoManager::Transfer function
	std::vector<UnityStr> assemblyNames;
	transfer.Transfer (assemblyNames, "m_AssemblyNames");
}


IMPLEMENT_OBJECT_SERIALIZE (MonoManager)
IMPLEMENT_CLASS (MonoManager)
GET_MANAGER (MonoManager)
GET_MANAGER_PTR (MonoManager)
