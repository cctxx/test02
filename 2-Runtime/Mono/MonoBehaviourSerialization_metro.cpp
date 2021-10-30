#include "UnityPrefix.h"

#include "MonoBehaviour.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "MonoScript.h"
#include "MonoManager.h"
#include "Runtime/Mono/MonoBehaviourSerialization.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/IMGUI/GUIStyle.h"

#if ENABLE_SCRIPTING && UNITY_WINRT && !ENABLE_SERIALIZATION_BY_CODEGENERATION
std::stack<StreamedBinaryWrite<false>*> currentTransferWrite;
std::stack<StreamedBinaryRead<false>*>  currentTransferRead;
RemapPPtrTransfer*						currentRemapper = NULL;

static void SerializationReader_Read(int destination, int size, bool align)
{
	StreamedBinaryRead<false>* read = currentTransferRead.top();
	read->ReadDirect((void*)destination, size);
	if (align) 
		read->Align();
}
static long long SerializationReader_ReadPPtr()
{
	StreamedBinaryRead<false>* transfer = currentTransferRead.top();

	PPtr<Object> pptr;
	transfer->Transfer (pptr, "does_not_matter", kNoTransferFlags);
	
	return TransferPPtrToMonoObjectUnChecked(pptr.GetInstanceID(), transfer->GetFlags() & kThreadedSerialization);
}


void SerializationReader_ReadGUIStyle(int dstPtr)
{
	StreamedBinaryRead<false>* transfer = currentTransferRead.top();
	transfer->Transfer(*(GUIStyle*)dstPtr, "does_not_matter", kNoTransferFlags);
}
void SerializationReader_ReadRectOffset(int dstPtr)
{
	StreamedBinaryRead<false>* transfer = currentTransferRead.top();
	transfer->Transfer(*(RectOffset*)dstPtr, "does_not_matter", kNoTransferFlags);
}
int SerializationReader_ReadAnimationCurve()
{
	StreamedBinaryRead<false>* transfer = currentTransferRead.top();
	AnimationCurve* curve = new AnimationCurve();
	transfer->Transfer (*curve, "does_not_matter", kNoTransferFlags);
	return (int)curve;
}


static void SetupReader()
{
	static BridgeInterface::ISerializationReader^ reader = nullptr;
	if (reader == nullptr)
	{
		reader = s_WinRTBridge->CreateSerializationReader(
			ref new BridgeInterface::SerializationReader_Read(SerializationReader_Read),
			ref new BridgeInterface::SerializationReader_ReadPPtr(SerializationReader_ReadPPtr),
			ref new BridgeInterface::SerializationReader_ReadStruct(SerializationReader_ReadGUIStyle),
			ref new BridgeInterface::SerializationReader_ReadStruct(SerializationReader_ReadRectOffset),
			ref new BridgeInterface::SerializationReader_ReadAnimationCurve(SerializationReader_ReadAnimationCurve));
		GetWinRTMonoBehaviourSerialization()->SetNativeReader(reader);
	}
}


static void SerializationWriter_Write(int source, int size, bool align)
{
	StreamedBinaryWrite<false>* writer = currentTransferWrite.top();
	writer->GetCachedWriter().Write((void*)source, size);
	if (align)
		writer->Align();
}

void SerializationWriter_WriteGUIStyle(int srcPtr)
{
	currentTransferWrite.top()->Transfer(*(GUIStyle*)srcPtr, "does_not_matter", kNoTransferFlags);
}
void SerializationWriter_WriteRectOffset(int srcPtr)
{
	currentTransferWrite.top()->Transfer(*(RectOffset*)srcPtr, "does_not_matter", kNoTransferFlags);
}
void SerializationWriter_WriteAnimationCurve(int srcAnimationCurvePtr)
{
	AnimationCurve* curve = (AnimationCurve*)srcAnimationCurvePtr;
	currentTransferWrite.top()->Transfer(*curve,"doesntmatter", kNoTransferFlags);
}


static void SerializationWriter_WritePPtr(int instanceID)
{
	StreamedBinaryWrite<false>* transfer = currentTransferWrite.top();
	PPtr<Object> pptr;
	pptr.SetInstanceID(instanceID);
	currentTransferWrite.top()->Transfer(pptr, "doesntmatter", kNoTransferFlags);
}

static void SetupWriter()
{
	static BridgeInterface::ISerializationWriter^ writer = nullptr;
	if (writer == nullptr)
	{
		writer = s_WinRTBridge->CreateSerializationWriter(
			ref new BridgeInterface::SerializationWriter_Write(SerializationWriter_Write),
			ref new BridgeInterface::SerializationWriter_WritePPtr(SerializationWriter_WritePPtr),
			ref new BridgeInterface::SerializationWriter_WriteStruct(SerializationWriter_WriteGUIStyle),
			ref new BridgeInterface::SerializationWriter_WriteStruct(SerializationWriter_WriteRectOffset),
			ref new BridgeInterface::SerializationWriter_WriteAnimationCurve(SerializationWriter_WriteAnimationCurve));
		GetWinRTMonoBehaviourSerialization()->SetNativeWriter(writer);
	}
}


template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryWrite<false> > (StreamedBinaryWrite<false>& transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	currentTransferWrite.push(&transfer);

	SetupWriter();
	GetWinRTMonoBehaviourSerialization()->Serialize(GetInstance());
	
	StreamedBinaryWrite<false>* remove = currentTransferWrite.top();
	Assert(remove == &transfer);
	currentTransferWrite.pop();
}

template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryRead<false> > (StreamedBinaryRead<false> & transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	
	currentTransferRead.push(&transfer);
	SetupReader();
	GetWinRTMonoBehaviourSerialization()->Deserialize(GetInstance());
	
	StreamedBinaryRead<false>* remove = currentTransferRead.top();
	Assert(remove == &transfer);
	currentTransferRead.pop();
}

static long long SerializationRemapper_GetScriptingWrapper(int oldInstanceID)
{
	SInt32 newInstanceID = currentRemapper->GetNewInstanceIDforOldInstanceID(oldInstanceID);
	//printf_console("GNITRO called. old instanceid: %i, new instanceid: %i", oldInstanceID, newInstanceID);
	return Scripting::GetScriptingWrapperForInstanceID(newInstanceID);
}

static void SetupRemapper()
{
	static BridgeInterface::SerializationRemapper_GetScriptingWrapper^ remapper = nullptr;
	if (remapper == nullptr)
	{
		remapper = ref new BridgeInterface::SerializationRemapper_GetScriptingWrapper(SerializationRemapper_GetScriptingWrapper);
		GetWinRTMonoBehaviourSerialization()->SetNativeRemapper(remapper);
	}

}

template<>
void MonoBehaviour::TransferWithInstance<RemapPPtrTransfer> (RemapPPtrTransfer& transfer)
{
	AssertIf (GetInstance() == SCRIPTING_NULL);
	currentRemapper = &transfer;
	SetupRemapper();
	GetWinRTMonoBehaviourSerialization()->Remap(GetInstance());
}

void MonoBehaviour::DoLivenessCheck(RemapPPtrTransfer& transfer)
{
}
#endif
