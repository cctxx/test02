#include "UnityPrefix.h"
#include "MonoScriptCache.h"

#if ENABLE_SCRIPTING

#include "Runtime/BaseClasses/MessageHandler.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Misc/MessageParameters.h"
#include "Runtime/Scripting/Backend/ScriptingMethodRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingTypeRegistry.h"
#include "Runtime/Scripting/Backend/ScriptingBackendApi.h"
#include "MonoManager.h"

const char* kMethodNames[] = {
	"Update",
	"LateUpdate",
	"FixedUpdate",
	"Awake",
	"Start",
	"Main",
	"OnRenderObject",
	"OnEnable",
	"OnDisable",
	"OnDisableINTERNAL",
	"Start",
	"Main",
	"OnRenderImage",
	"OnDrawGizmos",
	"OnGUI",
	"OnValidate",
	"OnSerializeNetworkView",
	"OnNetworkInstantiate",
	"OnDestroy",
	"OnAudioFilterRead",
	NULL
};


typedef MonoScriptCache::MethodCache MethodCache;
static void ClearMethodCache (MonoScriptCache::MethodCache& methods)
{
	for (MonoScriptCache::MethodCache::iterator i=methods.begin ();i != methods.end ();i++)
		delete[] const_cast<char*> (i->first);
	methods.clear ();
}


// These errors get removed as soon as the assembly gets reloaded. (MonoManager::ReloadAssemblies)
#define LogScriptError(x,script) DebugStringToFile (x, 0, __FILE__, __LINE__, kLog | kScriptCompileError, script ? script->GetInstanceID() : 0, manager.GetInstanceID ());

static bool Check2MethodParameters (ScriptingMethodPtr method, MonoClass* klass0, MonoClass* klass1, Object* script, MonoManager& manager);
static bool Check1MethodParameters (ScriptingMethodPtr method, MonoClass* klass0, Object* script, MonoManager& manager);

static RegisterMonoRPCCallback* gRegisterMonoRPC = NULL;

void RegisterMonoRPC (RegisterMonoRPCCallback* callback)
{
	gRegisterMonoRPC = callback;
}

#if UNITY_FLASH
static MethodCache* methodCacheToInsertInto;

extern "C" void Ext_InsertAllMethodsInMethodCacheForType(ScriptingClassPtr klass);

void BuildMethodCache (MethodCache& methods, ScriptingTypePtr klass, bool staticMethod)
{
	AssertIf (klass == NULL);
	methodCacheToInsertInto = &methods;
	Ext_InsertAllMethodsInMethodCacheForType(klass);
}

extern "C" void InsertMethodInMethodCache(const char* name, const char* mappedName, ScriptingClassPtr klass)
{
	const char* namecpy = strcpy(new char[strlen(name) + 1],name);
	ScriptingMethodPtr method = new ScriptingMethod(namecpy,mappedName,"",klass);
	methodCacheToInsertInto->insert (std::make_pair (namecpy, method));
}
#endif

#if !UNITY_FLASH
static void BuildMethodCache (MethodCache& methods, ScriptingClassPtr klass, bool staticMethod)
{
	AssertIf (klass == NULL);
	
	std::vector<ScriptingMethodPtr> foundMethods;
	GetScriptingMethodRegistry().AllMethodsIn(klass, foundMethods, ScriptingMethodRegistry::kInstanceOnly);
	
	for (std::vector<ScriptingMethodPtr>::iterator methodIterator = foundMethods.begin(); methodIterator != foundMethods.end(); methodIterator++)
	{
		ScriptingMethodPtr method = *methodIterator;
		
		std::string curName = scripting_method_get_name (method);
		if (methods.find (curName.c_str()) != methods.end ())
			continue;
		
		methods.insert (std::make_pair (strcpy (new char[curName.length() + 1], curName.c_str()), method));
	}
}
#endif

#if ENABLE_MONO
static bool IsCoroutine (MonoMethod* method, const CommonScriptingClasses& common)
{
	MonoType* returnType = mono_signature_get_return_type (mono_method_signature (method));
	if (returnType == NULL)
		return false;
	
	MonoClass* returnClass = mono_class_from_mono_type (returnType);
	
	// C# iterators return iEnumerator
	return returnClass == common.iEnumerator;
}

static bool CheckMethodParameters (ScriptingMethodPtr method, MonoClass* klass, MonoClass** klassArray, unsigned numParams, Object* errorContext, MonoManager& manager)
{
	MonoMethodSignature* signature = mono_method_signature (method->monoMethod);
	int paramCount = mono_signature_get_param_count (signature);
	if (paramCount != numParams)
	{
		const char* foundClass = mono_class_get_name (klass);
		string prefix = Format ("Script error (%s): %s.\n", foundClass, mono_method_get_name (method->monoMethod));
		string postfix = "The function will be ignored.";
		string message = Format("%sThe function must have exactly %i parameters.\n%s", prefix.c_str(), numParams, postfix.c_str());
		LogScriptError (message, errorContext);
		return false;
	}
	
	void* iterator = NULL;
	
	bool success = true;
	
	for (int i=0;i<numParams;++i)
	{
		MonoClass* monoParameterClass = mono_class_from_mono_type (mono_signature_get_params (signature, &iterator));		
		
		if (monoParameterClass != mono_get_object_class () && !mono_class_is_subclass_of (klassArray[i], monoParameterClass, true))
		{
			success = false;
			break;
		}
	}
	
	if (!success)
	{
		const char* foundClass = mono_class_get_name (klass);
		string prefix = Format ("Script error(%s): %s.\n", foundClass, mono_method_get_name (method->monoMethod));
		string postfix = "The function will be ignored.";
		string message;
		for (int i=0;i<numParams;++i)
		{
			message += mono_class_get_name (klassArray[i]);
			message += (i<numParams-1)?" and ":".";
		}
		LogScriptError (prefix + "The function parameters have to be of type: " + message + "\n" + postfix, errorContext);
		return false;
	}
	else
		return true;
}

static bool Check1MethodParameters (ScriptingMethodPtr method, MonoClass* klass, MonoClass* klass0, Object* errorContext, MonoManager& manager)
{
	MonoClass* klassArray[1];
	klassArray[0] = klass0;
	return CheckMethodParameters(method, klass, klassArray, 1, errorContext, manager);
}

static bool Check2MethodParameters (ScriptingMethodPtr method, MonoClass* klass, MonoClass* klass0, MonoClass* klass1, Object* errorContext, MonoManager& manager)
{
	MonoClass* klassArray[2];
	klassArray[0] = klass0;
	klassArray[1] = klass1;
	return CheckMethodParameters(method, klass, klassArray, 2, errorContext, manager);
}

// A messages has to:
// - return either void or bool.
// - have 1 or zero arguments
// - the argument has to be derived from Object or be a builtin type
// otherwise the method gets ignored as a message receiver
// This is done to simplify the MessageHandling at runtime.
static bool CheckMessageParameters (MonoMethod* method, int messageIndex, Object* errorContext, MonoManager& manager)
{
	MessageHandler& msgHandler = GameObject::GetMessageHandler ();
	string messageName = msgHandler.MessageIDToName (messageIndex);
	
	string prefix = "Script error: " + string (messageName) + "\n";
	string postfix = "The message will be ignored.";
	
	
	// Needs to have zero or one parameter
	MonoMethodSignature* signature = mono_method_signature (method);
	int paramCount = mono_signature_get_param_count (signature);
	if (paramCount != 0 && paramCount != 1)
	{
		LogScriptError (prefix + "The message must have 0 or 1 parameters.\n" + postfix, errorContext);
		return false;
	}
	
	MonoClass* monoObjectClass = mono_get_object_class ();
	
	MonoImage* engineImage = manager.GetEngineImage();
	
	if (paramCount == 1)
	{
		// Passed msg needs to have a parameter
		if (msgHandler.MessageIDToParameter (messageIndex) == 0)
		{
			LogScriptError (prefix + "The message may not have any parameters.\n" + postfix, errorContext);
			return false;
		}
		
		void* iterator = NULL;
		MonoType* monoParameterType = mono_signature_get_params (signature, &iterator);
		MonoClass* monoParameterClass = mono_class_from_mono_type (monoParameterType);
		int typeType = mono_type_get_type (monoParameterType);
		MessageIdentifier msg  = msgHandler.MessageIDToMessageIdentifier (messageIndex);
		
		const char* formattedMsgParameter = "";
		
		// Specific c to mono remap
		if (msg.scriptParameterName)
		{
			MonoClass* scriptParameterClass = mono_class_from_name(engineImage, "UnityEngine", msg.scriptParameterName);
			AssertIf(scriptParameterClass == NULL);
			if (monoParameterClass == scriptParameterClass && scriptParameterClass)
				return true;
			else
				formattedMsgParameter = msg.scriptParameterName;
		}
		
		if (msg.parameterClassId != -1) // Check when derived from Object
		{
			// javascript style
			if (monoParameterClass == monoObjectClass)
				return true;
			
			// Check custom data passing.
			// Generalize this!
			if (msg.parameterClassId == ClassID(Collision))
			{
				if (monoParameterClass == manager.GetCommonClasses().collision)
					return true;
				formattedMsgParameter = "Collision";
			}
			else if (msg.parameterClassId == ClassID(Collision2D))
			{
				if (monoParameterClass == manager.GetCommonClasses().collision2D)
					return true;
				formattedMsgParameter = "Collision2D";
			}
			else
			{
				// Allow anything that is a super class
				if (monoParameterClass)
				{
					int monoParameterClassID = Scripting::GetClassIDFromScriptingClass (monoParameterClass);
					
					if (monoParameterClassID != -1 && Object::IsDerivedFromClassID (msg.parameterClassId, monoParameterClassID))
						return true;
				}
				
				// Format the error nicer
				formattedMsgParameter = Object::ClassIDToString (msg.parameterClassId).c_str ();
			}
		}
		
		// Check for MonoObject passed directly
		///@TODO: REMOVE THIS AND DO A PROPER SendMessage for the character controller
		if (msg.parameterClassId == ClassID(MonoObject))
			return true;
		
		// Check built in types
		if (typeType == MONO_TYPE_BOOLEAN && msg.parameterClassId == ClassID (bool))
			return true;
		if (typeType == MONO_TYPE_I4 && msg.parameterClassId == ClassID (int))
			return true;
		if (typeType == MONO_TYPE_R4 && msg.parameterClassId == ClassID (float))
			return true;
		
		// Should have already returned if everything was OK...
		LogScriptError (prefix + "This message parameter has to be of type: " + formattedMsgParameter + "\n" + postfix, errorContext);
		return false;
	}
	return true;
}

#endif//ENABLE_MONO

ScriptingMethodPtr FindMethod (const MonoScriptCache& cache, const char* name)
{
	MonoScriptCache::MethodCache::const_iterator found = cache.methodCache.find (name);
	if (found != cache.methodCache.end ())
		return found->second;
	else
		return SCRIPTING_NULL;
}


static void PopulateMethods(MonoScriptCache& cache, MonoClass* klass, Object* errorContext)
{
	int messageCount = GameObject::GetMessageHandler ().GetMessageCount ();
	cache.methods.resize_initialized (MonoScriptCache::kMethodCount + messageCount, SCRIPTING_NULL);
	
	// Check the methods we support (Eg. Update, Render ...)		
	DebugAssertIf (kMethodNames[MonoScriptCache::kMethodCount] != NULL);
	for (int i=0;i<MonoScriptCache::kMethodCount;i++)
	{
		DebugAssertIf(kMethodNames[i] == NULL);
		
		ScriptingMethodPtr method = FindMethod (cache, kMethodNames[i]);
#if ENABLE_MONO		
		MonoManager& manager = GetMonoManager();
		const CommonScriptingClasses& common = manager.GetCommonClasses();
		
		if (method)
		{
			int parameterCount = mono_signature_get_param_count (mono_method_signature (method->monoMethod));
#if ENABLE_NETWORK
			if (i == MonoScriptCache::kSerializeNetView)
			{
				if (parameterCount == 1)
				{
					if (!Check1MethodParameters (method, klass, common.bitStream, errorContext, manager))
						method = NULL;
				}
				else
				{
					if (!Check2MethodParameters (method, klass, common.bitStream, common.networkMessageInfo, errorContext, manager))
						method = NULL;		
				}
			}
			else if (i == MonoScriptCache::kNetworkInstantiate)
			{
				if (!Check1MethodParameters (method, klass, common.networkMessageInfo, errorContext, manager))
					method = NULL;
			}
			else
#endif
				if (i == MonoScriptCache::kRenderImageFilter)
				{
					if (!Check2MethodParameters (method, klass, common.renderTexture, common.renderTexture, errorContext, manager))
						method = NULL;
				}
				else if (i == MonoScriptCache::kAudioFilterRead)
				{
					if (!Check2MethodParameters (method, klass, common.floatSingleArray, common.int_32, errorContext, manager))
						method = NULL;
				}
				else if (parameterCount != 0)
				{
					method = NULL;
					const char* foundClass = mono_class_get_name (klass);
					LogScriptError (string ("Script error (") + foundClass + "): " + kMethodNames[i] + "() can not take parameters.", errorContext);
				}
				else if (IsCoroutine (method->monoMethod, common))
				{
					if (i == MonoScriptCache::kStart || i == MonoScriptCache::kMain)
						method = NULL;
					else if (i != MonoScriptCache::kCoroutineStart && i != MonoScriptCache::kCoroutineMain)
					{
						method = NULL;
						const char* foundClass = mono_class_get_name (klass);
						LogScriptError (string ("Script error (") + foundClass + "): " + kMethodNames[i] + "() can not be a coroutine.", errorContext);
					}	
				}
		}
#endif
		cache.methods[i] = method;
	}
	
	// Check which messages we support
	for (int i=0;i<messageCount;i++)
	{
		MessageHandler& msgHandler = GameObject::GetMessageHandler ();
		if ((msgHandler.MessageIDToMessageIdentifier (i).options & MessageIdentifier::kSendToScripts) == 0)
			continue;
		
		const char* messageName = msgHandler.MessageIDToName (i);
		ScriptingMethodPtr method = FindMethod (cache, messageName);
		
#if ENABLE_MONO
		if (method)
		{
			if (!CheckMessageParameters (method->monoMethod, i, errorContext, GetMonoManager()))
				method = NULL;
		}
		
#endif		
		cache.methods[i + MonoScriptCache::kMethodCount] = method;
	}
	
}

static void RegisterNetworkRPC(MonoScriptCache& cache, const CommonScriptingClasses& common)
{
#if ENABLE_NETWORK
	if (gRegisterMonoRPC && common.RPC)
	{
		for (MethodCache::iterator m=cache.methodCache.begin();m != cache.methodCache.end();m++)
		{
			MonoMethod* meth = m->second->monoMethod;
			MonoCustomAttrInfo* attr = mono_custom_attrs_from_method(meth);
			if (attr != NULL)
			{
				if (mono_custom_attrs_has_attr (attr, common.RPC))
					gRegisterMonoRPC (mono_method_get_name(meth));
				mono_custom_attrs_free(attr);
			}
		}
	}
#endif
}

MonoScriptCache::MonoScriptCache ()
{
	scriptType = kScriptTypeNotInitialized;
	klass = SCRIPTING_NULL;
	className = NULL;
	#if UNITY_EDITOR
	scriptTypeWasJustCreatedFromComponentMenu = false;
	runInEditMode = false;
	#endif
}


MonoScriptCache::~MonoScriptCache ()
{
#if ENABLE_SCRIPTING
	ClearMethodCache (methodCache);
#endif
}

void MonoScriptCache::Release () const
{
	MonoScriptCache* cache = const_cast<MonoScriptCache*> (this);
	if (cache->refCount.Release())
	{
		UNITY_DELETE(cache, kMemScriptManager);
	}
}

void MonoScriptCache::Retain () const
{
	const_cast<AtomicRefCounter&> (refCount).Retain();
}

MonoScriptCache* CreateMonoScriptCache (ScriptingTypePtr klass, bool isEditorScript, Object* errorContext)
{
	MonoScriptCache* cache = UNITY_NEW (MonoScriptCache, kMemScriptManager);

	// Class still needs to be assigned, even on things not derived from MonoBehaviour or ScriptableObject (e.g. for 
	// interfaces etc.). So that any GetComponent(string) calls will be able to find us.
	cache->klass = klass;

	if (klass == NULL)
	{
		cache->scriptType = kScriptTypeClassNotFound;
		return cache;
	}	

	MonoManager& manager = GetMonoManager();
	const CommonScriptingClasses& common = manager.GetCommonClasses();
	cache->className = scripting_class_get_name (klass);
	
	#if ENABLE_MONO
	if (mono_class_get_flags (klass) & MONO_TYPE_ATTR_ABSTRACT)
	{
		cache->scriptType = kScriptTypeClassIsAbstract;
		return cache;
	}

	#endif

	#if ENABLE_MONO && (!UNITY_PEPPER)
	// @TODO: NACL should support mono_class_is_generic
	if (mono_class_is_generic (klass) || mono_class_is_inflated (klass))
	{
		cache->scriptType = kScriptTypeClassIsGeneric;
		return cache;
	}
	#endif
	
	
	if (scripting_class_is_subclass_of(klass, common.monoBehaviour))
		cache->scriptType = kScriptTypeMonoBehaviourDerived;
	else if (scripting_class_is_subclass_of (klass, common.scriptableObject))
		cache->scriptType = kScriptTypeScriptableObjectDerived;
	else
	{
		cache->scriptType = kScriptTypeNothingDerived;
		return cache;
	}

	#if UNITY_EDITOR		
	if (isEditorScript)
	{
		if (cache->scriptType == kScriptTypeScriptableObjectDerived)
		{
			cache->scriptType = kScriptTypeEditorScriptableObjectDerived;
		}
		else
		{
			cache->scriptType = kScriptTypeNothingDerived;
			return cache;
		}
	}
	#endif

	ClearMethodCache (cache->methodCache);
	BuildMethodCache (cache->methodCache, cache->klass, false);

	PopulateMethods(*cache, cache->klass, errorContext);
	RegisterNetworkRPC (*cache, common);

	#if UNITY_EDITOR
	// Is this an edit mode script?
	MonoObject* monoScriptObject = mono_type_get_object(mono_domain_get(), mono_class_get_type(klass));
	ScriptingInvocation invocation(common.checkIsEditMode);
	invocation.AddObject(monoScriptObject);

	cache->runInEditMode = MonoObjectToBool(invocation.Invoke()) || isEditorScript;
	#endif
	
	return cache;
}

bool IsValidScriptType(MonoScriptType type)
{
	return (type == kScriptTypeMonoBehaviourDerived ||
		type == kScriptTypeEditorScriptableObjectDerived ||
		type == kScriptTypeScriptableObjectDerived);
}

std::string FormatScriptTypeError(MonoScriptType type, const std::string& fileName)
{
	if (type == kScriptTypeClassNotFound)
		return Format("The class defined in script file named '%s' does not match the file name!", fileName.c_str());
	if (type == kScriptTypeNothingDerived)
		return Format("The class defined in the script file named '%s' is not derived from MonoBehaviour or ScriptableObject!", fileName.c_str());
	if (type == kScriptTypeClassIsAbstract)
		return Format("The class in script file named '%s' is abstract. The script class can't be abstract!", fileName.c_str());
	if (type == kScriptTypeClassIsInterface)
		return Format("The class in script file named '%s' is an interface. The script can't be an interface!", fileName.c_str());
	if (type == kScriptTypeClassIsGeneric)
		return Format("The class in script file named '%s' is generic. Generic MonoBehaviours are not supported!", fileName.c_str());
	if (type == kScriptTypeNotInitialized)
		return Format("The class in script file named '%s' is not yet initialized!", fileName.c_str());
	if (type == kScriptTypeScriptMissing)
		return "The referenced script on this Behaviour is missing!";

	return "";
}

#endif