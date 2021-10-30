#include "UnityPrefix.h"
#include "Configuration/UnityConfigure.h"

#if ENABLE_NETWORK
#include "NetworkManager.h"
#include "PackMonoRPC.h"
#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Allocator/LinearAllocator.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "External/RakNet/builds/include/StringCompressor.h"
#include "BitStreamPacker.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#if UNITY_WIN
#include <malloc.h> // alloca
#endif

#include "Runtime/Scripting/Scripting.h"

enum RPCFindResult { kRPCFailure = -1, kRPCNotFound = 0, kRPCFound = 1 };

static RPCFindResult FindRPCMethod (Object* target, const char* function, MonoMethod** outMethod, Object* netview);

static RPCFindResult FindRPCMethod (Object* target, const char* function, MonoMethod** outMethod, Object* netview)
{
	// We need the target to be a script
	MonoBehaviour* behaviour = dynamic_pptr_cast<MonoBehaviour*> (target);
	if (behaviour == NULL)
	{
		ErrorStringObject ("RPC call failed because the observed object is not a script.", netview);
		return kRPCFailure;
	}

	// We need the rpc method to exist	
	ScriptingMethodPtr method = behaviour->FindMethod (function);
	if (method == NULL)
	{
		if (behaviour->GetInstance() != NULL)
		{
			return kRPCNotFound;
		}
		else
		{
			ErrorStringObject (Format("RPC call failed because the script couldn't be loaded. The function was '%s'.", function), netview);
			return kRPCFailure;
		}
	}

	bool hasRPCAttribute = scripting_method_has_attribute (method, GetMonoManager().GetCommonClasses().RPC);
	if (!hasRPCAttribute)
	{
		const char* className = mono_class_get_name(mono_method_get_class(method->monoMethod));
		ErrorStringObject (Format("RPC call failed because the function '%s' in '%s' does not have the RPC attribute. You need to add the RPC attribute in front of the function declaration", function, className), netview);
		return kRPCFailure;
	}
	
	*outMethod = method->monoMethod;
	
	return kRPCFound;
}

template<class T>
void UnpackBuiltinValue (BitstreamPacker& stream, ForwardLinearAllocator& fwdallocator, void*& outData)
{
	outData = fwdallocator.allocate(sizeof(T));
	stream.Serialize (*reinterpret_cast<T*>(outData));
}

template<class T>
void UnpackBuiltinMaxError (BitstreamPacker& stream, ForwardLinearAllocator& fwdallocator, void*& outData)
{
	outData = fwdallocator.allocate(sizeof(T));
	stream.Serialize (*reinterpret_cast<T*>(outData), 0.0F);
}


bool UnpackString (RakNet::BitStream& stream, void*& outData)
{
	char rawOut[4096];
	
	if (StringCompressor::Instance()->DecodeString(rawOut, 4096, &stream))
	{
		outData = MonoStringNew(rawOut);	
		return true;
	}
	else
	{
		outData = NULL;
		return false;
	}
}

void PackString (RakNet::BitStream& stream, MonoObject* str)
{
	std::string cppStr = MonoStringToCpp((MonoString*)str);
	if (cppStr.size() >= 4096)
	{
		ErrorString("Strings sent via RPC calls may not be larger than 4096 UTF8 characters");
	}
	
	StringCompressor::Instance()->EncodeString(cppStr.c_str(), 4096, &stream);
}

bool UnpackAndInvokeRPCMethod (GameObject& target, const char* name, RakNet::BitStream& parameters, SystemAddress sender, NetworkViewID viewID, RakNetTime timestamp, Object* netview)
{
	int readoffset = parameters.GetReadOffset();
	bool invokedAny = false;
	for (int i=0;i<target.GetComponentCount();i++)
	{
		if (target.GetComponentClassIDAtIndex(i) != ClassID(MonoBehaviour))
			continue;
		
		MonoBehaviour* targetBehaviour = static_cast<MonoBehaviour*> (&target.GetComponentAtIndex(i));
		MonoMethod* method;
		RPCFindResult findResult = FindRPCMethod (targetBehaviour, name, &method, netview);
		if (findResult == kRPCNotFound)
			;
		else if (findResult == kRPCFound)
		{
			parameters.SetReadOffset(readoffset);
			if (UnpackAndInvokeRPCMethod (*targetBehaviour, method, parameters, sender, viewID, timestamp, netview))
				return false;
			
			invokedAny = true;
		}
		else
			return false;
	}
	
	if (!invokedAny)
	{
		ErrorStringObject (Format("RPC call failed because the function '%s' does not exist in any script attached to'%s'", name, target.GetName()), netview);
	}

	return invokedAny;
}

bool PackRPCParameters (GameObject& target, const char* name, RakNet::BitStream& inStream, MonoArray* data, Object* netview)
{
	bool invokedAny = false;
	for (int i=0;i<target.GetComponentCount();i++)
	{
		if (target.GetComponentClassIDAtIndex(i) != ClassID(MonoBehaviour))
			continue;
		
		MonoBehaviour* targetBehaviour = static_cast<MonoBehaviour*> (&target.GetComponentAtIndex(i));
		MonoMethod* method;
		RPCFindResult findResult = FindRPCMethod (targetBehaviour, name, &method, netview);

		if (findResult == kRPCNotFound)
			;
		else if (findResult == kRPCFound)
		{
			if (!PackRPCParameters (*targetBehaviour, method, inStream, data, netview, !invokedAny))
				return false;
			invokedAny = true;
		}
		else
		{
			return false;
		}
	}
	
	if (!invokedAny)
	{
		ErrorStringObject (Format("RPC call failed because the function '%s' does not exist in the any script attached to'%s'", name, target.GetName()), netview);
	}

	return invokedAny;
}

static bool IsValidRPCParameterArrayType(MonoClass* arrayClass)
{
	MonoClass* elementClass = mono_class_get_element_class(arrayClass);
	MonoType* elementType = mono_class_get_type(elementClass);
	int elementTypeCode = mono_type_get_type(elementType);
	
	switch (elementTypeCode)
	{
		case MONO_TYPE_U1:
		case MONO_TYPE_BOOLEAN:
		case MONO_TYPE_I4:
		case MONO_TYPE_R4:
		case MONO_TYPE_STRING:
			return true;
		case MONO_TYPE_VALUETYPE:
			{
				const CommonScriptingClasses& commonClasses = MONO_COMMON;
				return (elementClass == commonClasses.networkPlayer || 
					elementClass == commonClasses.networkViewID ||
					elementClass == commonClasses.vector3 ||
					elementClass == commonClasses.quaternion);
			}
			break;
	}
	return false;
}

bool UnpackAndInvokeRPCMethod (MonoBehaviour& targetBehaviour, MonoMethod* method, RakNet::BitStream& parameters, SystemAddress sender, NetworkViewID viewID, RakNetTime timestamp, Object* netview)
{
	DebugAssertIf(method == NULL);
	MonoMethodSignature* sig = mono_method_signature(method);
	int paramCount = mono_signature_get_param_count(sig);
	ForwardLinearAllocator fwdallocator (1024, kMemTempAlloc);
	#if ENABLE_THREAD_CHECK_IN_ALLOCS
	fwdallocator.SetThreadIDs(Thread::GetCurrentThreadID(), Thread::GetCurrentThreadID());
	#endif

	void** variables = (void** )alloca(paramCount * sizeof(void*));
	
	const CommonScriptingClasses& commonClasses = MONO_COMMON;
	
	BitstreamPacker stream(parameters, NULL, NULL, 0, true);

	NetworkMessageInfo msgInfo;
	
	// Loop through the parameters specified by the rpc function and extract it out of the param array
	MonoType* methodType;
	void* iter = NULL;
	int i = 0;
	bool success = true;
	while ((methodType = mono_signature_get_params (sig, &iter)))
	{
		int expectedTypecode = mono_type_get_type (methodType);
					
		switch (expectedTypecode)
		{
		// byte array
		case MONO_TYPE_SZARRAY:
			{
				MonoClass* arrayClass = mono_class_from_mono_type (methodType);
				if (IsValidRPCParameterArrayType(arrayClass))
				{
					MonoClass* elementClass = mono_class_get_element_class(arrayClass);
					int elementSize = mono_class_array_element_size(elementClass);
					int arrayLength;
					stream.Serialize(reinterpret_cast<SInt32&>(arrayLength));
					MonoArray* array = mono_array_new (mono_domain_get (), elementClass, arrayLength);
				char* ptr = Scripting::GetScriptingArrayStart<char>(array);
					int charLength = arrayLength * elementSize;
					stream.Serialize (ptr, charLength);
				
					variables[i] = array;
				}
				else
				{
					success = false;
				}
			}
			break;
		case MONO_TYPE_BOOLEAN:
			{
				bool extractedValue;
				stream.Serialize (extractedValue);
				char* writeHere = reinterpret_cast<char*>(fwdallocator.allocate(sizeof(char)));
				*writeHere = extractedValue;
				variables[i] = writeHere;
			}
			break;
		case MONO_TYPE_I4:
			UnpackBuiltinValue<SInt32>(stream, fwdallocator, variables[i]);
			break;
		case MONO_TYPE_R4:
			UnpackBuiltinMaxError<float>(stream, fwdallocator, variables[i]);
			break;
		case MONO_TYPE_STRING:
			UnpackString (parameters, variables[i]);
			break;
		case MONO_TYPE_VALUETYPE:
			{
				MonoClass* structClass = mono_type_get_class_or_element_class (methodType);
				if (structClass == commonClasses.networkPlayer)
					UnpackBuiltinValue<NetworkPlayer>(stream, fwdallocator, variables[i]);
				else if (structClass == commonClasses.networkViewID)
					UnpackBuiltinValue<NetworkViewID>(stream, fwdallocator, variables[i]);
				else if (structClass == commonClasses.vector3)
					UnpackBuiltinMaxError<Vector3f>(stream, fwdallocator, variables[i]);
				else if (structClass == commonClasses.quaternion)
					UnpackBuiltinMaxError<Quaternionf>(stream, fwdallocator, variables[i]);
				else if (structClass == commonClasses.networkMessageInfo)
				{
					msgInfo.timestamp = TimestampToSeconds(timestamp);
					msgInfo.sender = GetNetworkManager().GetIndexFromSystemAddress ( sender );
					msgInfo.viewID = viewID;
					AssertIf(mono_class_array_element_size(structClass) != sizeof(NetworkMessageInfo));
					variables[i] = &msgInfo;
				}
				else
					success = false;
			}
			break;
		default:
			success = false;
			break;
		}

		i++;
	}
	
	if (stream.HasReadOutOfBounds())	
		success = false;
	
	if (!success)
	{
		const char* classname = mono_class_get_name(mono_method_get_class(method));
		ErrorStringObject(Format ("Failed to invoke arriving RPC method because the parameters didn't match the function declaration. '%s' of '%s'.", mono_method_get_name(method), classname), netview);
		
		return false;
	}

	MonoObject* instance = targetBehaviour.GetInstance();
	// Invoke the method
	MonoException* exception;
	MonoObject* returnValue = mono_runtime_invoke_profiled (method, instance, variables, &exception);
	if (returnValue && exception == NULL)
	{
		ScriptingMethodPtr scriptingMethod = GetScriptingMethodRegistry().GetMethod(method);
		targetBehaviour.HandleCoroutineReturnValue (scriptingMethod, returnValue);
	}	

	
	if (exception == NULL)
		return true;
	else
	{
		Scripting::LogException(exception, Scripting::GetInstanceIDFromScriptingWrapper(instance));
		return false;
	}
}

bool PackRPCParameters (MonoBehaviour& targetBehaviour, MonoMethod* method, RakNet::BitStream& inStream, MonoArray* data, Object* netview, bool doPack)
{
	DebugAssertIf(method == NULL);
	bool requireTimeStamp = false;
	
	int arraySize = mono_array_length_safe(data);
	
	BitstreamPacker stream(inStream, NULL, NULL, 0, false);
		
	// Check the number of parameters
	// Handle message info as last element case
	MonoMethodSignature* sig = mono_method_signature(method);
	if (arraySize != mono_signature_get_param_count(sig) && arraySize + 1 != mono_signature_get_param_count(sig))
	{
		ErrorStringObject(Format( "Sending RPC '%s' failed because the number of supplied parameters doesn't match the rpc declaration. Expected %d but got %d parameters.", mono_method_get_name(method), mono_signature_get_param_count(sig), mono_array_length_safe(data)), netview);
		return false;
	}
	
	const CommonScriptingClasses& commonClasses = MONO_COMMON;

	// Loop through the parameters specified by the rpc function and extract it out of the param array
	MonoType* methodType;
	void* iter = NULL;
	int i = 0;
	while ((methodType = mono_signature_get_params (sig, &iter)))
	{
		int expectedTypecode = mono_type_get_type (methodType);
		
		// Handle message info as last element case
		if (i == arraySize)
		{
			if (expectedTypecode == MONO_TYPE_VALUETYPE && mono_type_get_class_or_element_class(methodType) == commonClasses.networkMessageInfo)
			{
				requireTimeStamp = true;
				return true;
			}
			else
			{
				ErrorStringObject(Format( "Sending RPC '%s' failed because the number of supplied parameters doesn't match the RPC declaration. Expected %d but got %d parameters.", mono_method_get_name(method), mono_signature_get_param_count(sig), mono_array_length_safe(data)), netview);
				return false;
			}
		}
		
		// @TODO check element against null
		///@TODO What if you intentionally want to pass in null. Eg. a null reference to a game object
		MonoObject* element = GetMonoArrayElement<MonoObject*>(data, i);
		if (element == NULL)
		{
			ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d was null", mono_method_get_name(method), i), netview);
			return false;
		}
		MonoClass* elementClass = mono_object_get_class(element);
		MonoType* elementType = mono_class_get_type(elementClass);
		int elementTypeCode = mono_type_get_type(elementType);
		if (elementTypeCode == expectedTypecode)
		{
			switch (elementTypeCode)
			{
			// byte array
			case MONO_TYPE_SZARRAY:
				if (!IsValidRPCParameterArrayType(elementClass))
				{
					ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d (%s) is not supported.", mono_method_get_name(method), i, mono_type_get_name(elementType)), netview);			
					return false;
				}
				if (doPack)
				{
					MonoArray* array = GetMonoArrayElement<MonoArray*>(data, i);
					MonoClass* arrayElementClass = mono_class_get_element_class(elementClass);
					int elementSize = mono_class_array_element_size(arrayElementClass);
					int length = mono_array_length(array);
					char* ptr = Scripting::GetScriptingArrayStart<char>(array);
					stream.Serialize (reinterpret_cast<SInt32&>(length));
					int charLength = length * elementSize;
					stream.Serialize (ptr, charLength);
				}
				break;
			case MONO_TYPE_BOOLEAN:
				if (doPack)
				{
					bool boolValue = ExtractMonoObjectData<UInt8> (element);
					stream.Serialize (boolValue);
				}
				break;
			case MONO_TYPE_I4:
				if (doPack)
					stream.Serialize (ExtractMonoObjectData<SInt32> (element));					
				break;
			case MONO_TYPE_R4:
				if (doPack)
					stream.Serialize (ExtractMonoObjectData<float> (element), 0);
				break;
			case MONO_TYPE_STRING:
				if (doPack)
					PackString (inStream, element);
				break;
			case MONO_TYPE_VALUETYPE:
				{
					MonoClass* methodClass = mono_type_get_class_or_element_class(methodType);
					if (elementClass == methodClass)
					{
						if (elementClass == commonClasses.networkPlayer)
						{
							if (doPack)
								stream.Serialize (ExtractMonoObjectData<NetworkPlayer> (element));
						}
						else if (elementClass == commonClasses.networkViewID)
						{
							if (doPack)
								stream.Serialize (ExtractMonoObjectData<NetworkViewID> (element));
						}
						else if (elementClass == commonClasses.vector3)
						{
							if (doPack)
								stream.Serialize (ExtractMonoObjectData<Vector3f> (element), 0.0F);
						}
						else if (elementClass == commonClasses.quaternion)
						{
							if (doPack)
								stream.Serialize (ExtractMonoObjectData<Quaternionf> (element), 0.0F);
						}
						else
						{
							ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d (%s) is not supported.", mono_method_get_name(method), i, mono_type_get_name(elementType)), netview);			
							return false;
						}
					}
					else
					{
						const char* expectedTypeName = mono_type_get_name(methodType);
						const char* gotTypeName = mono_type_get_name(elementType);
						ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d didn't match the RPC declaration. Expected '%s' but got '%s'", mono_method_get_name(method), i, expectedTypeName, gotTypeName), netview);
						return false;
					}
				}
				break;

			default:
				ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d (%s) is not supported.", mono_method_get_name(method), i, mono_type_get_name(elementType)), netview);			
				return false;
				break;
			}
		}
		else
		{
			const char* expectedTypeName = mono_type_get_name(methodType);
			const char* gotTypeName = mono_type_get_name(elementType);
			ErrorStringObject(Format("Sending RPC failed because '%s' parameter %d didn't match the RPC declaration. Expected '%s' but got '%s'", mono_method_get_name(method), i, expectedTypeName, gotTypeName), netview);
			return false;
		}
		i++;
	}
	
	return true;
}

#endif
