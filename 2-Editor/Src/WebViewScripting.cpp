#include "UnityPrefix.h"


#include "WebViewScripting.h"

#if ENABLE_ASSET_STORE
#include "Runtime/Threads/Mutex.h"

#if WEBVIEW_IMPL_WEBKIT
static string JSStringToCPP(JSStringRef js );
static void JSStringReleaseChecked( JSStringRef s );
static void SetException(JSContextRef ctx, JSValueRef* exception, const string& reason);

//#pragma mark WebScriptObjectWrapper

std::set<WebScriptObjectWrapper*> s_WrappersToDestroy;
Mutex s_Mutex;

void WebViewScriptingCheck()
{
	s_Mutex.Lock();
	for( std::set<WebScriptObjectWrapper*>::iterator i = s_WrappersToDestroy.begin() ; i != s_WrappersToDestroy.end(); i++ )
	{
		delete *i;
	}
	s_WrappersToDestroy.clear();
	s_Mutex.Unlock();
}
	
void WebScriptObjectWrapper::DeleteOnMainThread() {
	s_Mutex.Lock();
	s_WrappersToDestroy.insert(this);
	s_Mutex.Unlock();
}

WebScriptObjectWrapper::WebScriptObjectWrapper( JSContextRef context, JSObjectRef scriptObject, bool retainContext ) 
	: m_ScriptContext(context)
	, m_ScriptObject(scriptObject)
	, m_WrappedMonoBehaviour(NULL)
	, m_WrappedMonoBehaviourMethod(NULL)
	, m_LastException(NULL)
	, m_RetainContext(retainContext)
{
	if(m_RetainContext) {
		JSGlobalContextRetain((JSGlobalContextRef)context);
		if(m_ScriptObject)
			JSValueProtect(m_ScriptContext, m_ScriptObject);
	}
}

WebScriptObjectWrapper::WebScriptObjectWrapper( JSGlobalContextRef context ) 
	: m_ScriptContext((JSContextRef)context)
	, m_ScriptObject(JSContextGetGlobalObject(context))
	, m_WrappedMonoBehaviour(NULL)
	, m_WrappedMonoBehaviourMethod(NULL)
	, m_LastException(NULL)
	, m_RetainContext(true)
{
	JSGlobalContextRetain(context);
	if(m_ScriptObject)
		JSValueProtect(m_ScriptContext, m_ScriptObject);
}

WebScriptObjectWrapper::~WebScriptObjectWrapper()
{
	if(m_RetainContext) {
		if(m_ScriptObject) {
			JSValueUnprotect(m_ScriptContext, m_ScriptObject);
			m_ScriptObject = NULL;
		}
		JSGlobalContextRelease((JSGlobalContextRef)m_ScriptContext);
	}
}

JSValueRef WebScriptObjectWrapper::MakeMethodWrapper( const WrappedMonoBehaviour& wrapper, string name)
{
	if (! m_WrappedMonoBehaviourMethod)
	{
		WrappedMonoBehaviour::InitClassDefinitions();
		m_WrappedMonoBehaviourMethod = JSClassCreate(&WrappedMonoBehaviour::s_MethodDef);
	}
	WrappedMonoBehaviour* methodWrapperData = new WrappedMonoBehaviour( wrapper, name );
	return JSObjectMake(m_ScriptContext, m_WrappedMonoBehaviourMethod, (void*) methodWrapperData);
}

JSValueRef WebScriptObjectWrapper::MakeObjectWrapper( ScriptingObjectOfType<MonoBehaviour> bh )
{
	if (! m_WrappedMonoBehaviour)
	{
		WrappedMonoBehaviour::InitClassDefinitions();
		m_WrappedMonoBehaviour = JSClassCreate(&WrappedMonoBehaviour::s_ClassDef);
	}
	WrappedMonoBehaviour* methodWrapperData = new WrappedMonoBehaviour( bh );
	return JSObjectMake(m_ScriptContext, m_WrappedMonoBehaviour, (void*) methodWrapperData);
}

MonoObject* WebScriptObjectWrapper::MonoObjectFromJS( JSValueRef value ) 
{
	if ( value == NULL )
		return NULL;
	
	JSObjectRef jsObject = JSValueToObject(m_ScriptContext, value, &m_LastException);
	CheckJSException();
	if ( jsObject == NULL )
		return NULL;
				
	MonoObject* obj;
	obj = mono_object_new (mono_domain_get(), GetMonoManager().GetBuiltinEditorMonoClass("WebScriptObject"));
	ExtractMonoObjectData<WebScriptObjectWrapper*> (obj) = new WebScriptObjectWrapper( m_ScriptContext, jsObject, m_RetainContext );

	return obj;
}

MonoObject* WebScriptObjectWrapper::MonoObjectFromJS( JSValueRef value, MonoType* monoType ) 
{
	if ( value == NULL )
		return NULL;
	
	int t = mono_type_get_type ( monoType );
	MonoObject* obj = NULL;
	switch (t) {
		case MONO_TYPE_BOOLEAN:
		{
			obj = mono_object_new(mono_domain_get(), mono_class_from_name (mono_get_corlib (), "System", "Boolean"));
			UInt8& i = ExtractMonoObjectData<UInt8>(obj);
			i = ((int)JSValueToNumber(m_ScriptContext, value, &m_LastException)) != 0 ;
			CheckJSException();
			break;
		}
		case MONO_TYPE_CHAR:
		{
			obj = mono_object_new(mono_domain_get(), mono_class_from_name (mono_get_corlib (), "System", "Char"));
			unsigned short& i = ExtractMonoObjectData<unsigned short>(obj);
			JSStringRef jsString = JSValueToStringCopy(m_ScriptContext, value, &m_LastException);
			if ( jsString != NULL && JSStringGetLength(jsString) > 0 )
				i = (unsigned short)(JSStringGetCharactersPtr(jsString)[0]);
			else 
				i = 0;
			CheckJSException();
			JSStringReleaseChecked(jsString);
			break;
		}
		case MONO_TYPE_I1:
		{
			obj = mono_object_new(mono_domain_get(),  mono_class_from_name (mono_get_corlib (), "System", "SByte"));
			ExtractMonoObjectData<signed char>(obj) = (signed char)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_U1:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().byte);
			ExtractMonoObjectData<unsigned char>(obj) = (unsigned char)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_I2:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().int_16);
			ExtractMonoObjectData<short>(obj) = (short)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_U2:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().uInt_16);
			ExtractMonoObjectData<unsigned short>(obj) = (unsigned short)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_I4:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().int_32);
			ExtractMonoObjectData<int>(obj) = (int)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_U4:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().uInt_32);
			ExtractMonoObjectData<unsigned int>(obj) = (unsigned int)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_I8:
		{
			obj = mono_object_new(mono_domain_get(), mono_class_from_name (mono_get_corlib (), "System", "Int64"));
			ExtractMonoObjectData<long long>(obj) = (long long)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_U8:
		{
			obj = mono_object_new(mono_domain_get(), mono_class_from_name (mono_get_corlib (), "System", "UInt64"));
			ExtractMonoObjectData<unsigned long long>(obj) = (unsigned long long)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_R4:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().floatSingle);
			ExtractMonoObjectData<float>(obj) = (float)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_R8:
		{
			obj = mono_object_new(mono_domain_get(), GetMonoManager ().GetCommonClasses ().floatDouble);
			ExtractMonoObjectData<double>(obj) = (double)JSValueToNumber(m_ScriptContext, value, &m_LastException);
			CheckJSException();

			break;
		}
		case MONO_TYPE_STRING:
		{
			JSStringRef s = JSValueToStringCopy(m_ScriptContext, value, &m_LastException);
			obj = (MonoObject*)MonoStringNew( JSStringToCPP(s) );
			JSStringReleaseChecked(s);
			CheckJSException();
			break;
		}
		case MONO_TYPE_VALUETYPE:
			Scripting::RaiseMonoException("Converting javascript objects to mono structs is not implemented");
			break;
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
		{
			MonoClass* klass = mono_type_get_class_or_element_class(monoType);
			if ( mono_class_is_subclass_of(klass, GetMonoManager().GetBuiltinEditorMonoClass("WebScriptObject"), true ) )
				obj = MonoObjectFromJS(value);
			else
				Scripting::RaiseMonoException("Converting javascript objects to random mono objects is not implemented");
			break;
		}	
		case MONO_TYPE_ARRAY:
		case MONO_TYPE_SZARRAY:
		{
			JSObjectRef jsObj = JSValueToObject(m_ScriptContext, value, &m_LastException);
			CheckJSException();
			int size = 0;
			
			JSStringRef jsLengthProp = JSStringCreateWithUTF8CString("length");
			JSValueRef jsLength = JSObjectGetProperty(m_ScriptContext, jsObj, jsLengthProp, &m_LastException);
			JSStringReleaseChecked(jsLengthProp);
			CheckJSException();
			if (jsLength != NULL)
			{
				size = (int) JSValueToNumber(m_ScriptContext, value, &m_LastException);
				CheckJSException();
			}
			
			MonoClass* klass = mono_type_get_class_or_element_class(monoType);
			MonoType* elementType = mono_class_get_type(klass);
			obj = (MonoObject*)mono_array_new (mono_domain_get (), klass, size);

			for (int i=0; i<size; i++)
			{
				JSValueRef element = JSObjectGetPropertyAtIndex(m_ScriptContext, jsObj, i, &m_LastException);
				CheckJSException();
				Scripting::SetScriptingArrayElement((MonoArray*)obj, i, MonoObjectFromJS( element , elementType ));
			}
		}
		default:
			Scripting::RaiseMonoException("Unsupported mono type when casting from Javascript to Mono");
			break;
	}

	return obj;
	
}

MonoArray* WebScriptObjectWrapper::MonoArgsFromJS(MonoMethod* method, const JSValueRef jsValues[])
{
	MonoMethodSignature* sig = mono_method_signature(method);
	int argCount = mono_signature_get_param_count (sig);

	void* iterator = NULL;

	MonoClass* klass = mono_class_from_name (mono_get_corlib (), "System", "Object");
	MonoArray* array = mono_array_new (mono_domain_get (), klass, argCount);

	for (int i=0; i<argCount; i++)
	{
		MonoType* argumentType = mono_signature_get_params (sig, &iterator);
		Scripting::SetScriptingArrayElement(array, i, MonoObjectFromJS( jsValues[i] , argumentType) );
	}
	return array;
}

// Creates a script object and populates is with the fields of the struct member
JSObjectRef WebScriptObjectWrapper::MonoStructToJS( MonoObject* o )
{
	JSObjectRef jsObject = JSObjectMake(m_ScriptContext, NULL, NULL);
	
	MonoClass* klass = mono_object_get_class(o);
	MonoClassField *field;
	void* iter = NULL;
	while ((field = mono_class_get_fields (klass, &iter)))
	{
		int flags = mono_field_get_flags (field);
			
		// Ignore static or non-public
		if ((flags &  FIELD_ATTRIBUTE_STATIC) || ! (flags & FIELD_ATTRIBUTE_PUBLIC))
			continue;
		const char* name = mono_field_get_name(field);
		JSStringRef propName = JSStringCreateWithUTF8CString(name);
		JSObjectSetProperty(m_ScriptContext, jsObject, propName, MonoFieldToJS(o, field), 0, &m_LastException);
		CheckJSException();
		
	}
	return jsObject;
}

JSValueRef WebScriptObjectWrapper::MonoObjectToJS( MonoObject* o ) 
{
	if (! o )
		return JSValueMakeUndefined(m_ScriptContext);
	
	MonoClass* klass = mono_object_get_class(o);
	MonoType* monoType = mono_class_get_type (klass);
	int t = mono_type_get_type ( monoType );
	
	switch (t) {
		case MONO_TYPE_BOOLEAN:
			return JSValueMakeBoolean(m_ScriptContext, ExtractMonoObjectData<short>(o));
		case MONO_TYPE_CHAR:
		{
			JSStringRef tmp = JSStringCreateWithCharacters(&ExtractMonoObjectData<JSChar>(o), 1);
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_I1:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<signed char>(o));
		case MONO_TYPE_U1:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<unsigned char>(o));
		case MONO_TYPE_I2:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<signed short>(o));
		case MONO_TYPE_U2:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<unsigned short>(o));
		case MONO_TYPE_I4:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<signed int>(o));
		case MONO_TYPE_U4:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<unsigned int>(o));
		case MONO_TYPE_I8:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<signed long long>(o));
		case MONO_TYPE_U8:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<unsigned long long>(o));
		case MONO_TYPE_R4:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<float>(o));
		case MONO_TYPE_R8:
			return JSValueMakeNumber(m_ScriptContext, (double) ExtractMonoObjectData<double>(o));
		case MONO_TYPE_STRING:
		{
			JSStringRef tmp = JSStringCreateWithUTF8CString(mono_string_to_utf8((MonoString*)o));
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_VALUETYPE:
			return MonoStructToJS((MonoObject*)o);
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
			if ( mono_class_is_subclass_of(klass, GetMonoManager().GetBuiltinEditorMonoClass("WebScriptObject"), true ) ) 
			{
				return ExtractMonoObjectData<WebScriptObjectWrapper*> (o)->GetWrappedObject();
			}
			else if ( mono_class_is_subclass_of(klass, GetMonoManager ().GetCommonClasses ().scriptableObject, true ) ) 
			{
				ScriptingObjectOfType<MonoBehaviour> value(o);
				return MakeObjectWrapper(value);
			}
			
			else
			{
				break; // Everything else is unsupported
			}
		case MONO_TYPE_ARRAY:
		case MONO_TYPE_SZARRAY:
		{
			return MonoArrayToJS((MonoArray*)o);
		}
		default:
			break;
	}

	//NSLog(@"Unsupported type id: %02x\n", t);

	return JSValueMakeUndefined(m_ScriptContext);
	
}


JSValueRef WebScriptObjectWrapper::MonoFieldToJS(MonoObject* instance, MonoClassField* field)
{ 

	MonoType* monoType = mono_field_get_type(field) ;
	int t = mono_type_get_type ( monoType );
	
	
	switch (t) {
		case MONO_TYPE_BOOLEAN:
		{
			short o=0;
			mono_field_get_value(instance, field, &o);

			return  JSValueMakeBoolean(m_ScriptContext, o);
		}
		case MONO_TYPE_CHAR:
		{
			JSChar o=0;
			mono_field_get_value(instance, field, &o);
			JSStringRef tmp = JSStringCreateWithCharacters(&o, 1);
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_I1:
		{
			signed char o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_U1:
		{
			unsigned char o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_I2:
		{
			signed short o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_U2:
		{
			unsigned short o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_I4:
		{
			signed int o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_U4:
		{
			unsigned int o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_I8:
		{
			unsigned long long o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_R4:
		{
			float o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_R8:
		{
			double o=0;
			mono_field_get_value(instance, field, &o);
			return JSValueMakeNumber(m_ScriptContext, (double) o);
		}
		case MONO_TYPE_STRING:
		{
			MonoString* o=NULL;
			mono_field_get_value(instance, field, &o);
			char* utf8 = mono_string_to_utf8(o);

			JSStringRef tmp = JSStringCreateWithUTF8CString(utf8);
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_VALUETYPE:
		{
			/* value type */ 
			MonoClass* klass = mono_class_from_mono_type (mono_field_get_type(field)); 
			MonoObject* boxed = ScriptingInstantiateObject (klass);
			int offset = mono_field_get_offset(field);
			size_t size = mono_class_instance_size(klass);
			memcpy(ExtractMonoObjectDataPtr<void>(boxed), offset + (char*) instance, size);
			return MonoStructToJS(boxed);
		}
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
		{
			MonoObject* o=NULL;
			mono_field_get_value(instance, field, &o);

			MonoClass* klass = mono_object_get_class(o);
			if ( mono_class_is_subclass_of(klass, GetMonoManager().GetBuiltinEditorMonoClass("WebScriptObject"), true ) ) 
			{
				return ExtractMonoObjectData<WebScriptObjectWrapper*> (o)->GetWrappedObject();
			}
			else if ( mono_class_is_subclass_of(klass, GetMonoManager ().GetCommonClasses ().scriptableObject, true ) ) 
			{
				return MakeObjectWrapper(ScriptingObjectOfType<MonoBehaviour>(o)) ;
			}
			else if ( mono_class_is_valuetype(klass) ) // boxed object
			{
				return MonoObjectToJS(o);
			}
			else
			{
				break; // Everything else is unsupported
			}
		}
		case MONO_TYPE_ARRAY:
		case MONO_TYPE_SZARRAY:
		{
			MonoArray* o=NULL;
			mono_field_get_value(instance, field, &o);
			return MonoArrayToJS(o);
		}
		default:
			break;
	}

	//NSLog(@"Unsupported type id: %02x\n", t);

	return JSValueMakeUndefined(m_ScriptContext);
}


JSValueRef WebScriptObjectWrapper::MonoArrayElementToJS(MonoArray* ary, MonoType* monoType, size_t index)
{ 

	int elementType = mono_type_get_type(monoType);
	switch (elementType) {
		case MONO_TYPE_BOOLEAN:
			return  JSValueMakeBoolean(m_ScriptContext, GetMonoArrayElement<short>(ary, index));
		case MONO_TYPE_CHAR:
		{
			JSChar o= GetMonoArrayElement<JSChar>(ary, index);
			JSStringRef tmp = JSStringCreateWithCharacters(&o, 1);
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_I1:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<signed char>(ary, index));
		case MONO_TYPE_U1:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<unsigned char>(ary, index));
		case MONO_TYPE_I2:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<signed short>(ary, index));
		case MONO_TYPE_U2:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<unsigned short>(ary, index));
		case MONO_TYPE_I4:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<signed int>(ary, index));
		case MONO_TYPE_U4:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<unsigned int>(ary, index));
		case MONO_TYPE_I8:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<unsigned long long>(ary, index));
		case MONO_TYPE_R4:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<float>(ary, index));
		case MONO_TYPE_R8:
			return JSValueMakeNumber(m_ScriptContext, (double) GetMonoArrayElement<double>(ary, index));
		case MONO_TYPE_STRING:
		{
			MonoString* o=GetMonoArrayElement<MonoString*>(ary, index);
			JSStringRef tmp = JSStringCreateWithUTF8CString(mono_string_to_utf8(o));
			JSValueRef v =  JSValueMakeString(m_ScriptContext, tmp);
			JSStringReleaseChecked(tmp);
			return v;
		}
		case MONO_TYPE_VALUETYPE:
		{
			/* value type */ 
			MonoClass* klass = mono_class_from_mono_type (monoType); 
			MonoObject* boxed = ScriptingInstantiateObject (klass);
			size_t size = mono_class_array_element_size (klass);
			memcpy(ExtractMonoObjectDataPtr<void>(boxed), kMonoArrayOffset + index*size + (char*)ary, size);
			return MonoStructToJS(boxed);
		}
		case MONO_TYPE_CLASS:
		case MONO_TYPE_OBJECT:
		{
			MonoObject* o=GetMonoArrayElement<MonoObject*>(ary, index);
			MonoClass* klass = mono_object_get_class(o);
			if ( mono_class_is_subclass_of(klass, GetMonoManager().GetBuiltinEditorMonoClass("WebScriptObject"), true ) ) 
			{
				return ExtractMonoObjectData<WebScriptObjectWrapper*> (o)->GetWrappedObject();
			}
			else if ( mono_class_is_subclass_of(klass, GetMonoManager ().GetCommonClasses ().scriptableObject, true ) ) 
			{
				return MakeObjectWrapper(ScriptingObjectOfType<MonoBehaviour>(o)) ;
			}
			else if ( mono_class_is_valuetype(klass) ) // boxed object
			{
				return MonoObjectToJS(o);
			}
			else
			{
				break; // Everything else is unsupported
			}
		}
		case MONO_TYPE_ARRAY:
		case MONO_TYPE_SZARRAY:
		{
			return MonoArrayToJS(GetMonoArrayElement<MonoArray*>(ary, index));
		}
		default:
			break;
	}

	//NSLog(@"Unsupported type id: %02x\n", t);

	return JSValueMakeUndefined(m_ScriptContext);
}

JSObjectRef WebScriptObjectWrapper::MonoArrayToJS( MonoArray* ary ) 
{
	
	int len = mono_array_length_safe(ary);
	JSObjectRef res = JSObjectMakeArray(m_ScriptContext, 0, NULL, NULL);
	
	MonoClass* klass = mono_object_get_class((MonoObject*)ary);
	MonoType* monoType = mono_class_get_type (klass);
	MonoClass* elementClass = mono_type_get_class_or_element_class(monoType);
	MonoType* elementType = mono_class_get_type (elementClass);
	for ( int i = 0; i<len; i++) 
	{
		JSObjectSetPropertyAtIndex(m_ScriptContext, res, i, MonoArrayElementToJS(ary, elementType, i), &m_LastException);
		CheckJSException();
	}
	return res;
}

MonoObject* WebScriptObjectWrapper::MonoObjectFromJSON( const std::string& jsonString )
{
	JSStringRef json = JSStringCreateWithUTF8CString(jsonString.c_str());
	JSValueRef res = JSValueMakeFromJSONString(m_ScriptContext, json);
	JSStringReleaseChecked(json);

	if (! res) 
		Scripting::RaiseMonoException("Invalid JSON string");
		
	
	return MonoObjectFromJS(res);
}

MonoObject* WebScriptObjectWrapper::EvalJavaScript( const std::string& scriptText ) 
{
	JSStringRef jsScriptText = JSStringCreateWithUTF8CString(scriptText.c_str());
	JSValueRef res = JSEvaluateScript(m_ScriptContext, jsScriptText, m_ScriptObject, NULL, 0, &m_LastException);
	JSStringReleaseChecked(jsScriptText);
	CheckJSException();
	
	return MonoObjectFromJS(res);
}

MonoObject* WebScriptObjectWrapper::InvokeMethod( const std::string& methodName, MonoArray* argumentList ) 
{
	// First find the method:
	JSStringRef jsMethodName = JSStringCreateWithUTF8CString(methodName.c_str());
	JSValueRef method = JSObjectGetProperty(m_ScriptContext, m_ScriptObject, jsMethodName, &m_LastException);
	JSStringReleaseChecked(jsMethodName);
	CheckJSException();
	
	if (method == NULL || JSValueGetType(m_ScriptContext, method) != kJSTypeObject ) {
		// Error method either not found nor an object
		return NULL;
	}
	
	int argCount = mono_array_length_safe(argumentList);
	JSValueRef* jsArgs = new JSValueRef[argCount];
	for ( int i = 0; i<argCount; i++) 
	{
		jsArgs[i]=MonoObjectToJS(GetMonoArrayElement<MonoObject*>(argumentList, i));
	}
	
	JSValueRef res = JSObjectCallAsFunction(m_ScriptContext, (JSObjectRef) method, m_ScriptObject, argCount, jsArgs, &m_LastException);
	delete[] jsArgs;
	CheckJSException();
	
	return MonoObjectFromJS(res);
}

MonoObject* WebScriptObjectWrapper::GetKey( const std::string& key )
{
	JSStringRef jsPropName = JSStringCreateWithUTF8CString(key.c_str());
	JSValueRef val = JSObjectGetProperty(m_ScriptContext, m_ScriptObject, jsPropName, &m_LastException);
	JSStringReleaseChecked(jsPropName);
	CheckJSException();
	
	return MonoObjectFromJS(val);
}

void WebScriptObjectWrapper::SetKey( const std::string& key, MonoObject* value )
{
	JSStringRef jsPropName = JSStringCreateWithUTF8CString(key.c_str());

	JSObjectSetProperty(m_ScriptContext, m_ScriptObject, jsPropName, MonoObjectToJS(value), 0, &m_LastException);
	JSStringReleaseChecked(jsPropName);
	
	CheckJSException();
}

void WebScriptObjectWrapper::RemoveKey( const std::string& key )
{
	JSStringRef jsPropName = JSStringCreateWithUTF8CString(key.c_str());
	JSObjectDeleteProperty(m_ScriptContext, m_ScriptObject, jsPropName,  &m_LastException);
	JSStringReleaseChecked(jsPropName);
	CheckJSException();
}

MonoObject* WebScriptObjectWrapper::GetIndex( int index)
{
	JSValueRef value = JSObjectGetPropertyAtIndex(m_ScriptContext, m_ScriptObject, index,  &m_LastException );
	CheckJSException();
	return MonoObjectFromJS( value );
}


void WebScriptObjectWrapper::SetIndex(int index, MonoObject* value)
{
	JSObjectSetPropertyAtIndex(m_ScriptContext, m_ScriptObject, index, MonoObjectToJS(value), &m_LastException);
	CheckJSException();
}

string WebScriptObjectWrapper::ToJSON(int indent)
{
	JSStringRef json = JSValueCreateJSONString(m_ScriptContext, m_ScriptObject, indent, &m_LastException );
	string res = JSStringToCPP(json);
	JSStringReleaseChecked(json);
	CheckJSException();
	return res;

}

string WebScriptObjectWrapper::ToString()
{
	JSStringRef s = JSValueToStringCopy(m_ScriptContext, m_ScriptObject, &m_LastException);
	string res = JSStringToCPP(s);
	JSStringReleaseChecked(s);
	CheckJSException();
	return res;
}

MonoObject* WebScriptObjectWrapper::ToMonoType(MonoType* t)
{
	return MonoObjectFromJS((JSValueRef)m_ScriptObject, t);
}

void WebScriptObjectWrapper::CheckJSException()
{
	if (m_LastException == NULL)
		return;
	
	string cerror="Unknown exception";
	
	JSStringRef s = JSValueToStringCopy(m_ScriptContext, m_LastException, NULL);
	cerror = JSStringToCPP(s);
	JSStringReleaseChecked(s);
	
	m_LastException = NULL;
	if (! cerror.empty())
		Scripting::RaiseMonoException("Caught exception in Javascript code: %s", cerror.c_str());
}

//#pragma mark WrappedMonoBehaviour

WrappedMonoBehaviour::WrappedMonoBehaviour (ScriptingObjectOfType<MonoBehaviour> bh )
{
	InitClassDefinitions();
	m_MonoBehaviour = bh.GetPtr();
}

WrappedMonoBehaviour::WrappedMonoBehaviour (const WrappedMonoBehaviour& orig, string method)
{
	InitClassDefinitions();
	m_MonoBehaviour = orig.m_MonoBehaviour;
	m_MethodName = method;
}

WrappedMonoBehaviour::~WrappedMonoBehaviour ()
{
}

static bool s_ClassDefsInitialized = false;
JSClassDefinition WrappedMonoBehaviour::s_ClassDef = kJSClassDefinitionEmpty;
JSClassDefinition WrappedMonoBehaviour::s_MethodDef = kJSClassDefinitionEmpty;

void WrappedMonoBehaviour::InitClassDefinitions()
{
	if ( s_ClassDefsInitialized )
		return;
	s_ClassDef = kJSClassDefinitionEmpty;
	s_MethodDef = kJSClassDefinitionEmpty;
	
	s_ClassDef.finalize = FinalizeCallback;
	s_ClassDef.getProperty = GetPropertyCallback;
	s_MethodDef.finalize = FinalizeCallback;
	s_MethodDef.callAsFunction = InvokeMethodCallback;
	
	s_ClassDefsInitialized = true;
}

void WrappedMonoBehaviour::FinalizeCallback(JSObjectRef object)
{
	WrappedMonoBehaviour* thisP = (WrappedMonoBehaviour*)JSObjectGetPrivate(object);
	if ( thisP )
		delete thisP;

}

JSValueRef WrappedMonoBehaviour::InvokeMethodCallback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef args[], JSValueRef* jsException)
{
	
	WebScriptObjectWrapper js = WebScriptObjectWrapper(ctx, thisObject, false);
	WrappedMonoBehaviour* thisP = (WrappedMonoBehaviour*)JSObjectGetPrivate(function);

	if (! thisP )
		return JSValueMakeUndefined(ctx);
	
	if (! thisP->m_MonoBehaviour || thisP->m_MethodName.empty() )
		return JSValueMakeUndefined(ctx);
		
	MonoObject* instance = thisP->m_MonoBehaviour->GetInstance();
	if ( instance == NULL ) 
		return JSValueMakeUndefined(ctx);
		
	MonoClass* klass = thisP->m_MonoBehaviour->GetClass();
	if ( klass == NULL )
		return JSValueMakeUndefined(ctx);
	
	string name = thisP->m_MethodName;
	
	MonoMethod* method = NULL;
	MonoClass* currentClass = klass;
	while ( method == NULL && currentClass != NULL ) {
		method = mono_class_get_method_from_name( currentClass, name.c_str(), argumentCount);
		//if (method && currentClass)
		//	printf_console("Looking for method %s in class %s, found as %s:%s\n",name.c_str(),mono_class_get_name(klass),mono_class_get_name(currentClass),name.c_str());
		
		currentClass = mono_class_get_parent(currentClass);
	}
	
	
	if ( method == NULL )
	{
		//SetException(ctx, jsException, Format("Could not find Mono method %s.%s taking %d arguments", mono_class_get_name(klass),thisP->m_MethodName.c_str(),argumentCount));
		WarningString(Format("Could not find Mono method %s.%s taking %u arguments", mono_class_get_name(klass),name.c_str(),(unsigned int)argumentCount));
		return JSValueMakeUndefined(ctx);
	}
	
	MonoArray* monoArgs = js.MonoArgsFromJS(method, args);
	
	AssertIf( mono_array_length_safe(monoArgs) != argumentCount);

	MonoException* exception = NULL;
	MonoObject* res = mono_runtime_invoke_array(method, instance, monoArgs, &exception);
	if ( exception != NULL )
	{
		MonoException* tempException = NULL;
		MonoString* monoStringMessage = NULL;
		MonoString* monoStringTrace = NULL;
		void* args[] = { exception, &monoStringMessage, &monoStringTrace };

		mono_runtime_invoke (GetMonoManager ().GetCommonClasses ().extractStringFromException->monoMethod, exception, args, &tempException);
		if ( tempException )
		{
			SetException(ctx, jsException, "Caught exception while running Mono code, but could not extract error message from the exception" );
		}
		else
		{
			SetException(ctx, jsException, Format("Caught exception while running Mono code: %s\nstack trace: %s",  mono_string_to_utf8(monoStringMessage), mono_string_to_utf8(monoStringTrace)));
		}
		return JSValueMakeUndefined(ctx);
		
	}
		
	if ( res != NULL)
		return js.MonoObjectToJS(res);
	
	return JSValueMakeUndefined(ctx);

}

JSValueRef WrappedMonoBehaviour::GetPropertyCallback(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception)
{
	WebScriptObjectWrapper js = WebScriptObjectWrapper(ctx, object, false);
	WrappedMonoBehaviour* thisP = (WrappedMonoBehaviour*)JSObjectGetPrivate(object);
	if (! thisP )
		return JSValueMakeUndefined(ctx);
	
	if (! thisP->m_MonoBehaviour || ! thisP->m_MethodName.empty() )
		return JSValueMakeUndefined(ctx);
		
	MonoObject* instance = thisP->m_MonoBehaviour->GetInstance();
	if ( instance == NULL ) 
		return JSValueMakeUndefined(ctx);
		
	MonoClass* klass = thisP->m_MonoBehaviour->GetClass();
	if ( klass == NULL )
		return JSValueMakeUndefined(ctx);

	string key=JSStringToCPP(propertyName);
	
	// Try to find a class field matching the name
	MonoClassField* field = NULL;
	MonoClass* currentClass ;
	for (currentClass = klass; field == NULL && currentClass != NULL; currentClass = mono_class_get_parent(currentClass)) 
		field = mono_class_get_field_from_name(currentClass, key.c_str());
		
	if ( field )
	{
		// Ignore static or non-public
		int flags = mono_field_get_flags (field);
		if ( (flags &  FIELD_ATTRIBUTE_STATIC) || ! (flags & FIELD_ATTRIBUTE_PUBLIC) )
		{
			//SetException(ctx, exception, Format("Can't access static or non-public field %s",  key.c_str()));
			return JSValueMakeUndefined(ctx);
		}
			
		return js.MonoFieldToJS(instance, field);
	}

	MonoProperty* property = NULL;
	for (currentClass = klass; property == NULL && currentClass != NULL; currentClass = mono_class_get_parent(currentClass)) 
		property = mono_class_get_property_from_name(currentClass, key.c_str());
	
	if ( property )
	{
		MonoMethod* getter = mono_property_get_get_method(property);
		
		MonoException* monoException = NULL;
		MonoObject* res = mono_runtime_invoke(getter, instance, NULL, &monoException);
		if ( monoException != NULL )
		{
			MonoException* tempException = NULL;
			MonoString* monoStringMessage = NULL;
			MonoString* monoStringTrace = NULL;
			void* args[] = { monoException, &monoStringMessage, &monoStringTrace };

			mono_runtime_invoke (GetMonoManager ().GetCommonClasses ().extractStringFromException->monoMethod, monoException, args, &tempException);
			if ( tempException )
			{
				SetException(ctx, exception, "Caught exception while running Mono code, but could not extract error message from the exception" );
			}
			else
			{
				SetException(ctx, exception, Format("Caught exception while running Mono code: %s\nstack trace: %s",  mono_string_to_utf8(monoStringMessage), mono_string_to_utf8(monoStringTrace)));
			}
			return JSValueMakeUndefined(ctx);
			
		}
		
		return js.MonoObjectToJS(res);

	}
	
	// We assume it's a method if it's not a field nor a property...
	
	// Special case so that converting wrapped mono objects to string will end up calling the .Net ToString method.
	if ( key == "toString" || key == "valueOf" )
		key = "ToString";
		
	MonoMethodDesc* desc = mono_method_desc_new((":"+key).c_str(), false);
	bool found = false;
	// As we don't know how many arguments we will be passing, we only check whether there is a method by the correct name
	for (currentClass = klass; !found && currentClass != NULL; currentClass = mono_class_get_parent(currentClass))
		found = ( mono_method_desc_search_in_class(desc, currentClass) != NULL ) ;	
	mono_method_desc_free(desc);
	
	if (found)
		return js.MakeMethodWrapper(*thisP, key);
	
	//printf_console("Could not find a property nor method named %s on class %s\n", key.c_str(), mono_class_get_name(klass));
	return JSValueMakeUndefined(ctx);
	
}

//#pragma mark Utility functions

static void SetException(JSContextRef ctx, JSValueRef* exception, const string& reason)
{
	JSValueRef args[1];

	JSStringRef exceptionStr = JSStringCreateWithUTF8CString( reason.c_str() );
	args[0] = JSValueMakeString(ctx, exceptionStr);
	JSStringReleaseChecked(exceptionStr);
	(*exception) = (JSValueRef)JSObjectMakeError(ctx, 1, args, NULL);
}


static string JSStringToCPP(JSStringRef js)
{
	if ( js == NULL )
		return string();
		
	string res;
	res.resize(JSStringGetMaximumUTF8CStringSize(js));
	char * buffer = (char *)&(res[0]);
	JSStringGetUTF8CString(js, buffer, res.size());
	
	// JSStringGetMaximumUTF8CStringSize might return a number larger than the actual number of bytes, so we have to trim the string afterwards to the first zero character
	int end = res.find(char());
	if ( end != string::npos )
		res.resize(end);
	
	return res;
}

static void JSStringReleaseChecked( JSStringRef s)
{
	if (s != NULL )
		JSStringRelease(s);
}

#endif


#else // ENABLE_ASSET_STORE

void WebScriptObjectWrapper::DeleteOnMainThread() { }

MonoObject* WebScriptObjectWrapper::EvalJavaScript( const std::string& scriptText ) { return NULL; }
MonoObject* WebScriptObjectWrapper::InvokeMethod( const std::string& methodName, MonoArray* argumentList) { return NULL; }

MonoObject* WebScriptObjectWrapper::GetKey( const std::string& key ) { return NULL; }
void WebScriptObjectWrapper::SetKey( const std::string& key, MonoObject* value ) { }
void WebScriptObjectWrapper::RemoveKey( const std::string& key ) { }

MonoObject* WebScriptObjectWrapper::GetIndex( int index) { return NULL; }
void WebScriptObjectWrapper::SetIndex(int index, MonoObject* value) { }

MonoObject* WebScriptObjectWrapper::MonoObjectFromJSON( const std::string& jsonString ) { return NULL; }

std::string WebScriptObjectWrapper::ToJSON(int indent) { return std::string(); }
std::string WebScriptObjectWrapper::ToString() { return std::string(); }
MonoObject* WebScriptObjectWrapper::ToMonoType(MonoType* t) { return NULL; }

void WebViewScriptingCheck() { }

#endif // ENABLE_ASSET_STORE

