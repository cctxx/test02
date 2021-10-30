#ifndef MESSAGEIDENTIFIER_H
#define MESSAGEIDENTIFIER_H

#include <list>
#include <typeinfo>
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Misc/Allocator.h"
#include <map>
#include "Runtime/Allocator/MemoryMacros.h"
#include "Runtime/Scripting/Backend/ScriptingTypes.h"
#include "Runtime/BaseClasses/ObjectDefines.h"
#include "Runtime/BaseClasses/ClassIDs.h"

struct MessageData
{
	int type;
private:
	// Note: on Metro WinRT types cannot be located in union, so don't use union!
	intptr_t data;
	ScriptingObjectPtr scriptingObjectData;
public:
	MessageData () : scriptingObjectData(SCRIPTING_NULL)
	{ 
		data = 0; type = 0; 
	}
	
	template<class T>
	void SetData (T inData, int classId)
	{
		// Check if SetData is used instead of SetScriptingObjectData 
		Assert (type != ClassID (MonoObject));
		AssertIf (sizeof (T) > sizeof (data)); // increase the data size
		*reinterpret_cast<T*> (&data) = *reinterpret_cast<T*> (&inData);
		type = classId;
	}
	
	template<class T>
	T GetData ()
	{
		// Check if GetData is used instead of GetScriptingObjectData 
		Assert (type != ClassID (MonoObject));
		return *reinterpret_cast<T*> (&data);
	}

	intptr_t& GetGenericDataRef ()
	{
		// Check if GetGenericDataRef is used instead of GetScriptingObjectData 
		Assert (type != ClassID (MonoObject));
		return data;
	}

	void SetScriptingObjectData (ScriptingObjectPtr inData)
	{
		scriptingObjectData = inData;
		type = ClassID (MonoObject);
	}
	
	ScriptingObjectPtr GetScriptingObjectData ()
	{
		Assert (type == ClassID (MonoObject));
		return scriptingObjectData;
	}

};

// usage:
// MessageIdentifier kOnTransformChanged ("OnTransformChanged", MessageIdentifier::kDontSendToScripts);
// MessageIdentifier kOnTransformChanged ("OnTransformChanged", MessageIdentifier::kDontSendToScripts, ClassID (int));


class MessageIdentifier
{
	public:
	
	enum Options { kDontSendToScripts = 0, kSendToScripts = 1 << 0, kUseNotificationManager = 1 << 1, kDontSendToDisabled = 1 << 2 };
	
	const char* messageName;
	const char* scriptParameterName;
	int			messageID;
	int			parameterClassId;
	int         options;
	
	/// Place the MessageIdentifier as a global variable!
	/// The constructor must be called before InitializeEngine
	explicit MessageIdentifier (const char* name, Options opt, int classId = 0, const char* scriptParamName = NULL);
		
	typedef std::list<MessageIdentifier*, STL_ALLOCATOR(kMemPermanent, MessageIdentifier*) > RegisteredMessages;
	typedef std::map<std::string, MessageIdentifier> SortedMessages;
	
	// All registered message identifiers
	static RegisteredMessages& GetRegisteredMessages ();
	// Sorted list of message identifiers
	// - Duplicate message identifiers are ignored
	// - Only notification messages are returned if onlyNotificationMessages otherwise only normal messages are returned (See kUseNotificationManager)
	static SortedMessages GetSortedMessages (bool onlyNotificationMessages);

	static void Cleanup ();
	
	// Only to be used by subclasses
	MessageIdentifier ()
	{
		messageID = -1; scriptParameterName = NULL; messageName = NULL;
		options = kSendToScripts;
	}
};

#include "MessageIdentifiers.h"

#endif
