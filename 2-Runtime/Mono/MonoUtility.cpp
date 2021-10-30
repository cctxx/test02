#include "UnityPrefix.h"
#include "MonoIncludes.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/BaseClasses/RefCounted.h"
#include "Runtime/BaseClasses/GameObject.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Utilities/PathNameUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Scripting/Scripting.h"

#if UNITY_EDITOR
static UNITY_TLS_VALUE(void*) gStackLimit;

static void* GetStackLimit()
{
#if UNITY_WIN
    MEMORY_BASIC_INFORMATION mbi;
    VirtualQuery(&mbi, &mbi, sizeof(mbi));

    return mbi.AllocationBase;
#elif UNITY_OSX
	pthread_t self = pthread_self();
	void* addr = pthread_get_stackaddr_np(self);
	size_t size = pthread_get_stacksize_np(self);
	return (void*)((char*)addr - (char*)size);
#elif UNITY_LINUX
	pthread_attr_t attr;
	size_t stacksize = 0;
	void *stackaddr = NULL;
	pthread_t self = pthread_self ();
	int ret = pthread_getattr_np (self, &attr);

	if (ret != 0)
	{
		printf_console ("pthread_getattr_np ret=%d\n", ret);
		return 0;
	}

	ret = pthread_attr_getstack (&attr, &stackaddr, &stacksize);

	if (ret != 0)
	{
		printf_console ("pthread_attr_getstack ret=%d\n", ret);
		return 0;
	}

	return (void*)((char*)stackaddr - (char*)stacksize);
#else
#error Platform does not have stack checking implemented.
#endif
}

#define REQUIRED_SCRIPTING_STACK_SIZE (16*1024)


// Functions to check whether we have REQUIRED_SCRIPTING_STACK_SIZE (64K) of stack space available
// before calling into native code. This ensures a StackOverflowException will *not* occur in mono runtime
// or engine code.
bool IsStackLargeEnough ()
{
	// Note, we assume stack grows DOWN
	void* stackLimit = gStackLimit;
	if (stackLimit == NULL)
		gStackLimit = stackLimit = GetStackLimit();

	if (((char*)&stackLimit-(char*)stackLimit) < REQUIRED_SCRIPTING_STACK_SIZE)
		return false;
	else
		return true;
}

void AssertStackLargeEnough ()
{	
	if (!IsStackLargeEnough ())
	{
		Scripting::RaiseManagedException ("System", "StackOverflowException", "");
	}
}

#endif

std::string ErrorMessageForUnsupportedEnumField(MonoType* enumType, MonoType* classType, const char * fieldName)
{
	char* enumTypeName = mono_type_get_name (enumType);
	char* classTypeName = mono_type_get_name (classType);

	std::string message = Format("Unsupported enum type '%s' used for field '%s' in class '%s'", 
		enumTypeName,
		fieldName,
		classTypeName);

	g_free(enumTypeName);
	g_free(classTypeName);

	return message;
}

#if MONO_QUALITY_ERRORS
MonoObject* MonoObjectNULL (ScriptingClassPtr klass, ScriptingStringPtr error)
{
	AssertMsg (klass, "NULL scripting class!");
	if (NULL == klass)
		return NULL;

	if (mono_class_is_subclass_of (klass, GetScriptingManager ().GetCommonClasses ().monoBehaviour, 0))
		return NULL;
	if (mono_class_is_subclass_of (klass, GetScriptingManager ().GetCommonClasses ().scriptableObject, 0))
		return NULL;

	if (!mono_class_is_subclass_of (klass, GetScriptingManager ().GetCommonClasses ().unityEngineObject, 0))
		return NULL;

	if (mono_unity_class_is_abstract (klass) || mono_unity_class_is_interface (klass))
		return NULL;

	ScriptingObjectPtr scriptingobject = mono_object_new (mono_domain_get (), klass);
	if (scriptingobject == NULL)
		return NULL;

	ScriptingObjectOfType<Object> object (scriptingobject);
	object.SetInstanceID (0);

	if (error != NULL)
		object.SetError (error);

	return scriptingobject;
}

MonoObject* MonoObjectNULL (int classID, MonoString* error)
{
	AssertIf (classID == -1);
	if (classID == ClassID (MonoBehaviour))
		return NULL;

	ScriptingObjectPtr scriptingobject = Scripting::InstantiateScriptingWrapperForClassID(classID);
	if (scriptingobject == NULL)
		return NULL;

	ScriptingObjectOfType<Object> object(scriptingobject);
	object.SetInstanceID(0);
	
	if (error != NULL)
		object.SetError(error);

	return scriptingobject;
}

MonoString* MissingComponentString (GameObject& go, const char* klassName)
{
		return MonoStringFormat(
							"MissingComponentException:There is no '%s' attached to the \"%s\" game object, but a script is trying to access it.\n"
							"You probably need to add a %s to the game object \"%s\". Or your script needs to check if the component is attached before using it.",
							klassName, go.GetName(), klassName, go.GetName());
}

MonoString* MissingComponentString (GameObject& go, int classID)
{
	const string& className = Object::ClassIDToString(classID);
	return MissingComponentString(go,className.c_str());
}

MonoString* MissingComponentString (GameObject& go, ScriptingTypePtr klass)
{
	return MissingComponentString(go,scripting_class_get_name(klass));
}

#endif

int mono_array_length (MonoArray* array)
{
	char* raw = sizeof(uintptr_t)*3 + (char*)array;
	return *reinterpret_cast<uintptr_t*> (raw);
}

int mono_array_length_safe (MonoArray* array)
{
	if (array)
	{
		char* raw = sizeof(uintptr_t)*3 + (char*)array;
		return *reinterpret_cast<uintptr_t*> (raw);
	}
	else
	{
		return 0;
	}
}

ScriptingClassPtr GetBuiltinScriptingClass(const char* name,bool optional)
{
	return GetMonoManager().GetBuiltinMonoClass(name,optional);
}


#if USE_MONO_AOT && !(UNITY_XENON || UNITY_PS3)

// Flag defined in mono, when AOT libraries are built with -ficall option
// But that is not available in mono/consoles
extern "C" int mono_ficall_flag;

void* ResolveMonoMethodPointer(MonoDomain* domain, MonoMethod* method)
{
	return mono_ficall_flag && method ?  mono_aot_get_method(domain, method) : NULL;	
}
#else
void* ResolveMonoMethodPointer(MonoDomain* domain, MonoMethod* method)
{
	return NULL;
}
#endif

void mono_runtime_object_init_exception (MonoObject *thiss, MonoException** exception)
{
	MonoClass *klass = mono_object_get_class (thiss);

	MonoMethod *method;
	void* iter = NULL;
	while ((method = mono_class_get_methods (klass, &iter))) {
		MonoMethodSignature *signature = mono_method_signature (method);
		if (!signature) {
			ErrorString (Format ("Error looking up signature for method %s.%s", mono_class_get_name (klass), mono_method_get_name (method)));
			continue;
		}
		int paramCount = mono_signature_get_param_count (signature);
		if (!strcmp (".ctor", mono_method_get_name (method)) && signature && paramCount == 0)
			break;
	}

	if (method)
	{
		AssertIf (mono_class_is_valuetype (mono_method_get_class (method)));
		mono_runtime_invoke_profiled (method, thiss, NULL, exception);
	}
	else
	{
		*exception = NULL;	
	}
}

void mono_runtime_object_init_log_exception (MonoObject *thiss)
{
	if (!thiss)
		return;
	MonoException* exc = NULL;
	mono_runtime_object_init_exception(thiss, &exc);
	if (exc)
		::Scripting::LogException(exc, 0);
}

/*
mono_enumerator_next (MonoObject* enumerable, gconstpointer pointer)
{

}*/

bool IsUtf16InAsciiRange( gunichar2 const* str, int length )
{
	gunichar2 const* strEnd = str + length;
	while( str != strEnd )
	{ //length-- ) {
		if( (*str & ~((gunichar2)0x7f)) != 0 )
			return false;
		++str;
	}
	return true;
}

bool FastTestAndConvertUtf16ToAscii( char* destination, gunichar2 const* str, int length )
{
	gunichar2 const* strEnd = str + length;
	while( str != strEnd ) { //length-- ) {
		if( (*str & ~((gunichar2)0x7f)) != 0 )
			return false;
		*destination = (char)*str;
		++destination;
		++str;
	}
	return true;
}

// converts symbols in the range 0x00-0x7f from unicode16 to ascii (excluding the terminating 0 character)
void fastUtf16ToAscii( char* destination, gunichar2 const* str, int length )
{
	gunichar2 const* strEnd = str + length;
	while( str != strEnd ) {
		*destination = (char)*str;
		++destination;
		++str;
	}
}

#if UNITY_WIN || UNITY_XENON
std::wstring MonoStringToWideCpp (MonoString* monoString)
{
	if (monoString)
	{
		wchar_t* buf = (wchar_t*)mono_string_to_utf16(monoString);
		std::wstring temp (buf);
		g_free (buf);
		return temp;
	}
	else
		return std::wstring ();
}
#endif

std::string MonoStringToCpp (MonoString* monoString)
{
	if (!monoString)
		return string ();
	
	char buff[256];
	if(monoString->length <= 256 && FastTestAndConvertUtf16ToAscii (buff,mono_string_chars(monoString), mono_string_length(monoString)) )
		return string((char const*)buff,mono_string_length (monoString));
		
	char* buf = mono_string_to_utf8 (monoString);
	string temp (buf);
	g_free (buf);
	return temp;
}

MonoArray *mono_array_new_2d (int size0, int size1, MonoClass *klass) {
	guint32 sizes[] = {size0, size1};
	MonoClass* ac = mono_array_class_get (klass, 2);

	return mono_array_new_full(mono_domain_get (), ac, sizes, NULL);
}

MonoArray *mono_array_new_3d (int size0, int size1, int size2, MonoClass *klass) {
	guint32 sizes[] = {size0, size1, size2};
	MonoClass* ac = mono_array_class_get (klass, 3);

	return mono_array_new_full(mono_domain_get (), ac, sizes, NULL);
}

std::string MonoStringToCppChecked (MonoObject* monoString)
{
	if (monoString && mono_type_get_type(mono_class_get_type(mono_object_get_class(monoString))) == MONO_TYPE_STRING)
	{
		char* buf = mono_string_to_utf8 ((MonoString*)monoString);
		string temp (buf);
		g_free (buf);
		return temp;
	}
	else
		return string ();
}

inline bool ExtractLineAndPath (const string& exception, string::size_type& pathBegin, int& line, string& path)
{
	// Extract line and path from exception ...
	// Format is: in [0x00031] (at /Users/.../filename.cs:51)
	
	pathBegin = exception.find ("(at ", pathBegin);
	
	if (pathBegin != string::npos)
	{
		pathBegin += 4;

		// On Windows, there's a ':' right at the beginning as part of drive
		#if UNITY_WIN && UNITY_EDITOR
		std::string::size_type pathEnd = exception.find (':', exception.size() > pathBegin+2 ? pathBegin+2 : pathBegin);
		#else
		std::string::size_type pathEnd = exception.find (':', pathBegin);
		#endif
		if (pathEnd != string::npos)
		{
			path.assign (exception.begin () + pathBegin, exception.begin () + pathEnd);
			line = atoi (exception.c_str () + pathEnd + 1);
			pathBegin = pathEnd;
			ConvertSeparatorsToUnity(path);
			return true;
		}	
	}
	return false;
}

inline bool IsScriptAssetPath (const string& path)
{
	const string& projectDir = File::GetCurrentDirectory ();
	// C# returns absolute path names
	if (path.find (projectDir) == 0)
		return true;
	// Boo returns relative path names
	if (!IsAbsoluteFilePath(path) )
		return true;
	return false;
}

bool ExceptionToLineAndPath (const string& stackTrace, int& line, string& path)
{
	// Extract line and path from exception...
	// We want the topmost exception function trace that is in the project folder. 
	// If there is nothing in the project folder we return the topmost.
	// Format is: in [0x00031] (at /Users/.../filename.cs:51)
	string::size_type searchStart = 0;
	
	if (ExtractLineAndPath (stackTrace, searchStart, line, path) && !IsScriptAssetPath (path))
	{
		string tempPath;
		int tempLine;
		while (ExtractLineAndPath (stackTrace, searchStart, tempLine, tempPath))
		{
			if (!IsAbsoluteFilePath(tempPath))
			{
				path = tempPath;
				line = tempLine;
				break;
			}
		}
		return true;
	}
	else
		return false;
}


string SimpleGetExceptionString(MonoException* exception)
{
	MonoClass* klass = mono_object_get_class((MonoObject*)exception);
	if (!klass)
		return "";
	
	MonoMethod* toString = mono_class_get_method_from_name(mono_get_exception_class(), "ToString", 0);
	if (!toString)
		return "";

	MonoString* exceptionString = (MonoString*)mono_runtime_invoke_profiled(toString, (MonoObject*)exception, NULL, NULL);
	if (!exceptionString)
		return "";

	return mono_string_to_utf8(exceptionString);
}

MonoString* MonoStringNew (const std::string& in)
{
	return MonoStringNew (in.c_str ());
}

MonoString* MonoStringNew (const char* in)
{
	Assert (in != NULL);
	MonoString* mono = mono_string_new_wrapper (in);
	if (mono != NULL)
		return mono;
	else
	{
		// This can happen when conversion fails eg. converting utf8 to ascii or something i guess.
		mono = mono_string_new_wrapper ("");
		Assert (mono != NULL);
		return mono;
	}
}

MonoString* MonoStringNewUTF16 (const wchar_t* in)
{
	Assert (in != NULL);
	MonoString* mono = mono_string_from_utf16 ( (const gunichar2*)in );
	if (mono != NULL)
		return mono;
	else
	{
		// See MonoStringNew
		mono = mono_string_new_wrapper ("");
		Assert (mono != NULL);
		return mono;
	}
}

MonoString* MonoStringNewLength (const char* in, int length)
{
	Assert (in != NULL);
	Assert (length >= 0);
	MonoDomain* domain = mono_domain_get ();
	Assert (domain != NULL);
	MonoString* mono = mono_string_new_len (domain, in, length);
	if (mono != NULL)
		return mono;
	else
	{
		// This can happen when conversion fails eg. converting utf8 to ascii or something i guess.
		mono = mono_string_new_wrapper ("");
		Assert (mono != NULL);
		return mono;
	}
}

bool MonoSetObjectField(MonoObject* target, const char* fieldname, MonoObject* value)
{
	MonoClass* klass = mono_object_get_class(target);
	MonoClassField* field = mono_class_get_field_from_name(klass,fieldname);
	if (!field) return false;
	mono_field_set_value(target,field,value);
	return true;
}

bool MonoObjectToBool (MonoObject* value)
{
	if (value && mono_type_get_type (mono_class_get_type (mono_object_get_class (value))) == MONO_TYPE_BOOLEAN)
		return ExtractMonoObjectData<char> (value);
	else
		return false;
}

int MonoObjectToInt (MonoObject* value)
{
	if (value && mono_type_get_type (mono_class_get_type (mono_object_get_class (value))) == MONO_TYPE_I4)
		return ExtractMonoObjectData<int> (value);
	else
		return -1;
}

MonoAssembly* mono_load_assembly_from_any_monopath(const char* assemblyname)
{
	MonoDomain* domain = mono_domain_get();
	std::vector<string>& monoPaths = MonoPathContainer::GetMonoPaths();
	for (int i=0; i!=monoPaths.size(); i++)
	{
		MonoAssembly* ass = mono_domain_assembly_open(domain, AppendPathName(monoPaths[i],assemblyname).c_str());
		if (ass) return ass;
	}
	return NULL;
}

MonoMethod* mono_unity_find_method(const char* assemblyname, const char* ns, const char* klass, const char* methodname)
{
//todo: be less stupid about always trying to load an assembly that will be already loaded 99% of the time
	MonoAssembly* ass = mono_load_assembly_from_any_monopath(assemblyname);
	if (!ass) return NULL;
	MonoImage* img = mono_assembly_get_image(ass);
	if (!img) return NULL;
	MonoMethod* method = FindStaticMonoMethod(img,klass,ns,methodname);
	if (!method) return NULL;
	return method;
}

MonoMethod* mono_reflection_method_get_method (MonoObject* ass)
{
	return ExtractMonoObjectData<MonoMethod*>(ass);
}

MonoString* MonoStringFormat (const char* format, ...)
{
	using namespace std;
	va_list vl;
	va_start( vl, format );
	char buffer[1024 * 5];
	vsnprintf (buffer, 1024 * 5, format, vl);
	va_end (vl);
	return mono_string_new_wrapper(buffer);
}

void StringMonoArrayToVector (MonoArray* arr, std::vector<UnityStr>& container)
{
	container.resize(mono_array_length_safe(arr));
	for (int i=0;i<container.size();i++)
	{
		container[i] = MonoStringToCpp(GetMonoArrayElement<MonoString*> (arr, i));
	}
}

void StringMonoArrayToVector (MonoArray* arr, std::vector<std::string>& container)
{
	container.resize(mono_array_length_safe(arr));
	for (int i=0;i<container.size();i++)
	{
		container[i] = MonoStringToCpp(GetMonoArrayElement<MonoString*> (arr, i));
	}
}


void SetReferenceDataOnScriptingWrapper(MonoObject* wrapper, const UnityEngineObjectMemoryLayout& data)
{
	UnityEngineObjectMemoryLayout* wrapperdata = reinterpret_cast<UnityEngineObjectMemoryLayout*> (((char*)wrapper) + kMonoObjectOffset);
	memcpy(wrapperdata,&data,sizeof(UnityEngineObjectMemoryLayout));
}

MonoObject* mono_class_get_object (MonoClass* klass)
{
	if (klass == NULL)
		return NULL;
	
	MonoType* type = mono_class_get_type(klass);
	if (type)
		return mono_type_get_object (mono_domain_get(), type);
	else
		return NULL;
}

MonoClass* mono_type_get_class_or_element_class (MonoType* type)
{
#if MONO_2_12
	MonoClass* klass = mono_class_from_mono_type (type);
	if (mono_class_get_rank (klass) > 0)
	{
		klass = mono_class_get_element_class (klass);
	}

	return klass;
#else
	return mono_type_get_class (type);
#endif
}

int mono_array_length_safe_wrapper(MonoArray* array)
{
	return mono_array_length_safe(array);
}

#if MONO_QUALITY_ERRORS
MonoString* UnassignedReferenceString (MonoObject* instance, int classID, MonoClassField* field, int instanceID)
{
	MonoClass* klass = NULL;
	if (instance == NULL)
		return NULL;
	// Transfer sometimes provides us with non-object derived instances so we simply ignore those
	klass = mono_object_get_class(instance);
	if (!mono_class_is_subclass_of(klass, GetMonoManager().GetCommonClasses().unityEngineObject, false))
		return NULL;
	
	const char* fieldName = mono_field_get_name(field);
	const char* klassName = mono_class_get_name(mono_object_get_class(instance));
	
	if (instanceID == 0)
	{
		return MonoStringFormat(
								"UnassignedReferenceException:The variable %s of '%s' has not been assigned.\n"
								"You probably need to assign the %s variable of the %s script in the inspector.",
								fieldName, klassName, fieldName, klassName);
	}
	else
	{
		return MonoStringFormat(
								"MissingReferenceException:The variable %s of '%s' doesn't exist anymore.\n"
								"You probably need to reassign the %s variable of the '%s' script in the inspector.",
								fieldName, klassName, fieldName, klassName);
	}
}
#endif

MonoClassField* GetMonoArrayFieldFromList (int type, MonoType* monoType, MonoClassField* field)
{
	if (type != MONO_TYPE_GENERICINST)
		return NULL;
	
	MonoClass* elementClass = mono_class_from_mono_type(monoType);
	
	// Check that we have a Generic List class
	const char* className = mono_class_get_name(elementClass);
	if (strcmp(className, "List`1") != 0 || mono_class_get_image(elementClass) != mono_get_corlib())
		return NULL;
	
	MonoClassField *arrayField;
	void* iter_list = NULL;
	
	// List<> first element is something called Default Capacity
	// Second is the actual array
	// But, Mono 2.12 reordered the fields
#if !MONO_2_12
	mono_class_get_fields (elementClass, &iter_list);
#endif
	arrayField = mono_class_get_fields (elementClass, &iter_list);
	
#if !UNITY_RELEASE
	AssertIf(strcmp(mono_field_get_name(arrayField), "_items") != 0);
	AssertIf(mono_field_get_offset(arrayField) != kMonoObjectOffset);
	
	MonoClassField* sizeField = mono_class_get_fields (elementClass, &iter_list);
	AssertIf(strcmp(mono_field_get_name(sizeField), "_size") != 0);
	AssertIf(mono_field_get_offset(sizeField) != kMonoObjectOffset + sizeof(intptr_t));
#endif
	
	return arrayField;
}

static int currentDomainId = 0;

int MonoDomainGetUniqueId()
{
	return currentDomainId;
}

void MonoDomainIncrementUniqueId()
{
	currentDomainId++;
}
