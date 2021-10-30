#include "UnityPrefix.h"

#include "MonoBehaviour.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "MonoScript.h"
#include "MonoManager.h"
#include "Runtime/Mono/MonoBehaviourSerialization.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include <stack>

#include "MonoBehaviourSerialization_ByCodeGeneration.h"

#if ENABLE_SCRIPTING

static StreamedBinaryRead<false>*	currentTransferRead;
static CachedReader*				currentCachedReader;
static std::stack< StreamedBinaryRead<false>* > currentTranferReadStack;
static StreamedBinaryWrite<false>* currentTransferWrite;
static RemapPPtrTransfer* currentRemapPPTRTransfer;

// ToDo: On windows standalone, we're getting :
//(Filename: E:/Projects/win8-new-serialization/Runtime/ExportGenerated/StandalonePlayer/UnityEngineDebug.cpp Line: 54)
//
//ptr == NULL || GET_CURRENT_ALLOC_OWNER() == GET_DEFAULT_OWNER() || GET_CURRENT_ALLOC_OWNER() == s_MonoDomainContainer

int SERIALIZATION_SCRIPT_CALL_CONVENTION GetCurrentSerializationStreamPosition()
{
	return (int)currentCachedReader->GetAbsoluteMemoryPosition();
}

SCRIPTINGOBJECTWRAPPER SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_GetNewInstanceToReplaceOldInstance(int oldInstanceID)
{
	SInt32 newInstanceID = currentRemapPPTRTransfer->GetNewInstanceIDforOldInstanceID(oldInstanceID);
	return Scripting::GetScriptingWrapperForInstanceID(newInstanceID);
}

#if ENABLE_SERIALIZATION_BY_CODEGENERATION

template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryWrite<false> >(StreamedBinaryWrite<false>& transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	
	//TODO: find a way to serialize a monobehaviour that is both fast and does not have a fixed buffersize.
	currentTransferWrite = &transfer;
#if UNITY_WINRT
	static ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetScriptingTypeRegistry().GetType("UnityEngine", "IUnitySerializable"), "Unity_Serialize");
#else
	// Don't know why above code doesn't work on Mono, says something about invalid header, but this code works
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetClass(), "Serialize");
#endif
	ScriptingInvocationNoArgs invoke(method);
	invoke.object = GetInstance();
	invoke.Invoke();
}

template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryRead<false> >(StreamedBinaryRead<false> & transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	
	currentTranferReadStack.push(&transfer);
	
	currentTransferRead = currentTranferReadStack.top();
	if(currentTransferRead != NULL){
		currentCachedReader = &currentTransferRead->GetCachedReader();
	}

#if UNITY_WINRT
	static ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetScriptingTypeRegistry().GetType("UnityEngine", "IUnitySerializable"), "Unity_Deserialize");
#else
	// Don't know why above code doesn't work on Mono, says something about invalid header, but this code works
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetClass(), "Deserialize");
#endif
	ScriptingInvocationNoArgs invoke(method);
	invoke.object = GetInstance();
	invoke.Invoke();

	Assert(currentTransferRead == &transfer);
	currentTranferReadStack.pop();
	
	if (!currentTranferReadStack.empty()){
		currentTransferRead = currentTranferReadStack.top();
		currentCachedReader = &currentTransferRead->GetCachedReader();
	}
}

template<>
void MonoBehaviour::TransferWithInstance<RemapPPtrTransfer>(RemapPPtrTransfer& transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL );
	
	currentRemapPPTRTransfer = &transfer;
#if UNITY_WINRT
	static ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetScriptingTypeRegistry().GetType("UnityEngine", "IUnitySerializable"), "Unity_RemapPPtrs");
#else
	// Don't know why above code doesn't work on Mono, says something about invalid header, but this code works
	ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetClass(), "RemapPPtrs");
#endif
	ScriptingInvocationNoArgs invoke(method);
	invoke.object = GetInstance();
	invoke.Invoke();
}

#if ENABLE_SERIALIZATION_BY_CODEGENERATION
void MonoBehaviour::DoLivenessCheck(RemapPPtrTransfer& transfer)
{
	if (GetInstance() == SCRIPTING_NULL) return;
	
	currentRemapPPTRTransfer = &transfer;

	static ScriptingMethodPtr method = GetScriptingMethodRegistry().GetMethod(GetScriptingTypeRegistry().GetType("UnityEngine", "IUnityAssetsReferenceHolder"), "Unity_LivenessCheck");

	ScriptingInvocationNoArgs invoke(method);
	invoke.object = GetInstance();
	invoke.Invoke();
}
#endif

#endif

unsigned char SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadByte()
{
	unsigned char retVal = 0;
	currentCachedReader->Read(&retVal,1);
	return retVal;
}

char SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadBool()
{
	return NativeExt_MonoBehaviourSerialization_ReadByte();
}

int SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadInt()
{
	int retVal = 0;
	currentCachedReader->Read(&retVal,4);
	return retVal;
}

float SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadFloat()
{
	float retVal = 0;
	currentCachedReader->Read(&retVal,4);
	return retVal;
}

double SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadDouble()
{
	double retVal = 0;
	currentCachedReader->Read(&retVal,8);
	return retVal;
}
SCRIPT_BINDINGS_EXPORT_DECL void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadBuffer(int ptr, int size)
{
	currentCachedReader->Read((void*)ptr, size);
}
ScriptingStringPtr SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadString()
{	
	UnityStr stdString;
	currentTransferRead->Transfer(stdString, "does_not_matter", kNoTransferFlags);
	return scripting_string_new(stdString);
}

SCRIPTINGOBJECTWRAPPER SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadUnityEngineObject()
{
	PPtr<Object> pptr;
	currentTransferRead->Transfer (pptr, "does_not_matter", kNoTransferFlags);
	return TransferPPtrToMonoObjectUnChecked(pptr.GetInstanceID(), currentTransferRead->GetFlags() & kThreadedSerialization);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadGUIStyle(GUIStyle* style)
{
	currentTransferRead->Transfer(*style,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadGradient(GradientNEW* gradient)
{
	currentTransferRead->Transfer(*gradient,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadAnimationCurve(AnimationCurve* animation_curve)
{
	currentTransferRead->Transfer(*animation_curve,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReadRectOffset(RectOffset* offset)
{
	currentTransferRead->Transfer(*offset,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_ReaderAlign()
{
	currentCachedReader->Align4Read();
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriterAlign()
{
	currentTransferWrite->GetCachedWriter().Align4Write();
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteByte(unsigned char value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteBool(int value)
{
	currentTransferWrite->GetCachedWriter().Write((char)(value ? 1 : 0));
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteInt(int value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteFloat(float value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteDouble(double value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteBuffer(int ptr, int size)
{
	currentTransferWrite->GetCachedWriter().Write((void*)ptr, size);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteString(char* value, int size)
{
	// TODO: optimize this, probably creating a new UnityStr is not needed
	UnityStr tmpStr(value, size);
	currentTransferWrite->Transfer(tmpStr, "does_not_matter", kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteUnityEngineObject(int instance_id)
{
	PPtr<Object> pptr;
	pptr.SetInstanceID(instance_id);
	currentTransferWrite->Transfer(pptr, "does_not_matter", kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteGradient(GradientNEW* value)
{
	currentTransferWrite->Transfer(*value,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteAnimationCurve(AnimationCurve* value)
{
	currentTransferWrite->Transfer(*value,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteGUIStyle(GUIStyle* value)
{
	currentTransferWrite->Transfer(*value,"does_not_matter",kNoTransferFlags);
}

void SERIALIZATION_SCRIPT_CALL_CONVENTION NativeExt_MonoBehaviourSerialization_WriteRectOffset(RectOffset* value)
{
	currentTransferWrite->Transfer(*value,"does_not_matter",kNoTransferFlags);
}
#endif