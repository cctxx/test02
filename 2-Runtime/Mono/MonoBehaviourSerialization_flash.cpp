#include "UnityPrefix.h"

#include "MonoBehaviour.h"
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Math/Gradient.h"
#include "Runtime/Serialize/TransferFunctions/SerializeTransfer.h"
#include "Runtime/Serialize/FileCache.h"
#include "Runtime/Serialize/IterateTypeTree.h"
#include "MonoScript.h"
#include "MonoManager.h"
#include "Runtime/Mono/MonoBehaviourSerialization.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/CommonScriptingClasses.h"
#include "Runtime/IMGUI/GUIStyle.h"
#include <stack>

#if UNITY_FLASH

#define FLASH_SERIALIZATION_STRING_BUFFERSIZE 4*1024

static StreamedBinaryRead<false>*	currentTransferRead;
static CachedReader*				currentCachedReader;
static std::stack< StreamedBinaryRead<false>* > currentTranferReadStack;
static StreamedBinaryWrite<false>* currentTransferWrite;
static RemapPPtrTransfer* currentRemapPPTRTransfer;

static unsigned char temporaryStringBuffer[FLASH_SERIALIZATION_STRING_BUFFERSIZE];

//since there is so much code involved in reading a PPtr<T> from a serializedstream, and getting the ScriptingObject
//belonging to that, we do not do that deserialization in as3, but in c instead. as3 will call this function
//when it wants to deserialize something that enherits from UnityEngine.Object
extern "C" ScriptingObjectPtr ReadPPtrAndReturnScriptingWrapper()
{
	PPtr<Object> pptr;
	currentTransferRead->Transfer (pptr, "does_not_matter", kNoTransferFlags);
	return TransferPPtrToMonoObjectUnChecked(pptr.GetInstanceID(), currentTransferRead->GetFlags() & kThreadedSerialization);
}

extern "C" GradientNEW* ReadGradientAndReturnPtr()
{
	GradientNEW* gradient = new GradientNEW();
	currentTransferRead->Transfer (*gradient, "does_not_matter", kNoTransferFlags);
	return gradient;
}

extern "C" AnimationCurve* ReadAnimationCurveAndReturnPtr()
{
	AnimationCurve* curve = new AnimationCurve();
	currentTransferRead->Transfer(*curve, "does_not_matter", kNoTransferFlags);
	return curve;
}

extern "C" int GetCurrentSerializationStreamPosition()
{
	return (int)currentCachedReader->GetAbsoluteMemoryPosition();
}

extern "C" void WriteGradient(GradientNEW* gradient)
{
	currentTransferWrite->Transfer(*gradient, "doesntmatter", kNoTransferFlags);
}

extern "C" void WriteAnimationCurve(AnimationCurve* curve)
{
	currentTransferWrite->Transfer(*curve, "doesntmatter", kNoTransferFlags);
}

extern "C" void WritePPtrWithInstanceID(int instanceid)
{
	PPtr<Object> pptr;
	pptr.SetInstanceID(instanceid);
	currentTransferWrite->Transfer(pptr, "doesntmatter", kNoTransferFlags);
}

extern "C" ScriptingObjectPtr GetNewInstanceToReplaceOldInstance(int oldInstanceID)
{
	SInt32 newInstanceID = currentRemapPPTRTransfer->GetNewInstanceIDforOldInstanceID(oldInstanceID);
	return Scripting::GetScriptingWrapperForInstanceID(newInstanceID);
}

template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryWrite<false> > (StreamedBinaryWrite<false>& transfer)
{
	AssertIf (GetInstance() == NULL);
	currentTransferWrite = &transfer;
	Ext_SerializeMonoBehaviour(GetInstance());
}

template<>
void MonoBehaviour::TransferWithInstance<StreamedBinaryRead<false> > (StreamedBinaryRead<false> & transfer)
{
	AssertIf (GetInstance() == NULL);
	
	currentTranferReadStack.push(&transfer);
	
	currentTransferRead = currentTranferReadStack.top();
	if(currentTransferRead != NULL){
		currentCachedReader = &currentTransferRead->GetCachedReader();
	}

	Ext_DeserializeMonoBehaviour(GetInstance());

	Assert(currentTransferRead == &transfer);
	currentTranferReadStack.pop();
	
	if(!currentTranferReadStack.empty()){
		currentTransferRead = currentTranferReadStack.top();
		currentCachedReader = &currentTransferRead->GetCachedReader();
	}
}

template<>
void MonoBehaviour::TransferWithInstance<RemapPPtrTransfer> (RemapPPtrTransfer& transfer)
{
	AssertIf (GetInstance() == NULL);
	currentRemapPPTRTransfer = &transfer;
	Ext_RemapPPtrs(GetInstance());
}

extern "C" unsigned char NativeExt_MonoBehaviourSerialization_ReadByte()
{
	unsigned char retVal = 0;
	currentCachedReader->Read(&retVal,1);
	return retVal;
}

extern "C" unsigned char NativeExt_MonoBehaviourSerialization_WriteByte(unsigned char value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

extern "C" char NativeExt_MonoBehaviourSerialization_ReadBool()
{
	return NativeExt_MonoBehaviourSerialization_ReadByte();
}

extern "C" char NativeExt_MonoBehaviourSerialization_WriteBool(bool value)
{
	currentTransferWrite->GetCachedWriter().Write((char)(value ? 1 : 0));
}

extern "C" int NativeExt_MonoBehaviourSerialization_ReadInt()
{
	int retVal = 0;
	currentCachedReader->Read(&retVal,4);
	return retVal;
}

extern "C" char NativeExt_MonoBehaviourSerialization_WriteInt(int value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

extern "C" float NativeExt_MonoBehaviourSerialization_ReadFloat()
{
	float retVal = 0;
	currentCachedReader->Read(&retVal,4);
	return retVal;
}

extern "C" char NativeExt_MonoBehaviourSerialization_WriteFloat(float value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

extern "C" double NativeExt_MonoBehaviourSerialization_ReadDouble()
{
	double retVal = 0;
	currentCachedReader->Read(&retVal,8);
	return retVal;
}

extern "C" char NativeExt_MonoBehaviourSerialization_WriteDouble(double value)
{
	currentTransferWrite->GetCachedWriter().Write(value);
}

extern "C" void NativeExt_MonoBehaviourSerialization_ReadString()
{
	int length = 0;
	currentCachedReader->Read(&length, 4);//Length of the string
	void* tempBuffer;
	bool usesLocalBuffer;
	usesLocalBuffer = !(length < FLASH_SERIALIZATION_STRING_BUFFERSIZE);
	if(usesLocalBuffer)
		tempBuffer = malloc(length);
	else
		tempBuffer = temporaryStringBuffer;
	
	
	currentCachedReader->Read(tempBuffer,length);

	__asm __volatile__("var bpos:int = heap.position;");
	__asm __volatile__("heap.position = %0;"::"r"(tempBuffer));
	__asm __volatile__("string_g0 = heap.readUTFBytes(%0);"::"r"(length));
	__asm __volatile__("heap.position = bpos;");

	if(usesLocalBuffer)
		free(tempBuffer);
	
	currentCachedReader->Align4Read();
}

extern "C" void NativeExt_MonoBehaviourSerialization_WriteString(char* value, int size)
{
	UnityStr tmpStr(value, size);
	currentTransferWrite->Transfer(tmpStr, "does_not_matter", kNoTransferFlags);
}

extern "C" void NativeExt_MonoBehaviourSerialization_ReadGUIStyle(GUIStyle* style)
{
	currentTransferRead->Transfer(*style,"does_not_matter",kNoTransferFlags);
}

extern "C" void NativeExt_MonoBehaviourSerialization_WriteGUIStyle(GUIStyle* style)
{
	currentTransferWrite->Transfer(*style,"does_not_matter",kNoTransferFlags);
}

extern "C" void NativeExt_MonoBehaviourSerialization_ReadRectOffset(RectOffset* offset)
{
	currentTransferRead->Transfer(*offset,"does_not_matter",kNoTransferFlags);
}

extern "C" void NativeExt_MonoBehaviourSerialization_WriteRectOffset(RectOffset* offset)
{
	currentTransferWrite->Transfer(*offset,"does_not_matter",kNoTransferFlags);
}

extern "C" void NativeExt_MonoBehaviourSerialization_WriteAlign()
{
	currentTransferWrite->GetCachedWriter().Align4Write();
}

extern "C" void NativeExt_MonoBehaviourSerialization_ReadAlign()
{
	currentCachedReader->Align4Read();
}
#endif
