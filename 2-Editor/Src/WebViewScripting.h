
#include "Editor/Src/WebViewWrapper.h"

#if ENABLE_ASSET_STORE
#if WEBVIEW_IMPL_WEBKIT

#if UNITY_LINUX
#include "JavaScriptCore/JavaScript.h"
#else
#include "JavaScriptCore/JavaScriptCore.h"
#endif

#include "Runtime/Mono/MonoBehaviour.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Mono/MonoTypeSignatures.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/tabledefs.h"
#include "Runtime/Utilities/LogAssert.h"
#endif
#endif // ENABLE_ASSET_STORE

#ifndef __WEBVIEW_SCRIPTING_H__
#define __WEBVIEW_SCRIPTING_H__

#if WEBVIEW_IMPL_WEBKIT

void WebViewScriptingCheck();

#if ENABLE_ASSET_STORE
struct WrappedMonoBehaviour {
	PPtr<MonoBehaviour> m_MonoBehaviour;
	string m_MethodName;

	WrappedMonoBehaviour( ScriptingObjectOfType<MonoBehaviour> bh );
	WrappedMonoBehaviour( const WrappedMonoBehaviour& orig, string method );
	~WrappedMonoBehaviour();
	
	static JSClassDefinition s_ClassDef ;
	static JSClassDefinition s_MethodDef ;
	
	static void FinalizeCallback(JSObjectRef object);
	static JSValueRef GetPropertyCallback(JSContextRef ctx, JSObjectRef object, JSStringRef propertyName, JSValueRef* exception);
	static JSValueRef InvokeMethodCallback(JSContextRef ctx, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception);

	static void InitClassDefinitions();	
	
};
#endif // ENABLE_ASSET_STORE
#endif // WEBVIEW_IMPL_WEBKIT



class WebScriptObjectWrapper
{
private:
#if WEBVIEW_IMPL_WEBKIT && ENABLE_ASSET_STORE
	JSContextRef m_ScriptContext;
	JSObjectRef m_ScriptObject;
	
	JSClassRef m_WrappedMonoBehaviour;
	JSClassRef m_WrappedMonoBehaviourMethod;

	JSValueRef m_LastException;
	bool m_RetainContext;
#endif

public:
#if WEBVIEW_IMPL_WEBKIT && ENABLE_ASSET_STORE
	WebScriptObjectWrapper(JSGlobalContextRef context);
	WebScriptObjectWrapper(JSContextRef context, JSObjectRef scriptObject, bool retainContext);
	~WebScriptObjectWrapper();
	JSContextRef GetContext() { return m_ScriptContext; }
	JSObjectRef GetWrappedObject() { return m_ScriptObject; }

	MonoObject* MonoObjectFromJS( JSValueRef value ) ;
	MonoObject* MonoObjectFromJS( JSValueRef value, MonoType* type ) ;
	MonoArray* MonoArgsFromJS(MonoMethod* method, const JSValueRef jsValues[]); 
	JSObjectRef MonoArrayToJS(MonoArray* aray); 
	JSObjectRef MonoStructToJS(MonoObject* o );
	JSValueRef MonoArrayElementToJS(MonoArray* ary, MonoType* elementType, size_t index);
	JSValueRef MonoFieldToJS( MonoObject* instance, MonoClassField* field);
	JSValueRef MonoObjectToJS( MonoObject* o );

	JSValueRef MakeMethodWrapper( const WrappedMonoBehaviour& wrapper, std::string name);
	JSValueRef MakeObjectWrapper( ScriptingObjectOfType<MonoBehaviour> bh );
	void CheckJSException();

#endif

	void DeleteOnMainThread();

	MonoObject* EvalJavaScript( const std::string& scriptText ) ;
	MonoObject* InvokeMethod( const std::string& methodName, MonoArray* argumentList);

	MonoObject* GetKey( const std::string& key );
	void SetKey( const std::string& key, MonoObject* value );
	void RemoveKey( const std::string& key );

	MonoObject* GetIndex( int index);
	void SetIndex(int index, MonoObject* value);
	
	MonoObject* MonoObjectFromJSON( const std::string& jsonString );
	
	std::string ToJSON( int indent = 0 );
	std::string ToString();
	MonoObject* ToMonoType(MonoType* t);

};

#endif
