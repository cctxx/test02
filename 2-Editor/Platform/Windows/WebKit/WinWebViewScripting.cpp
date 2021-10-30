#include "UnityPrefix.h"
#include "WebViewScripting.h"

WebScriptObjectWrapper* WebViewWrapper::GetWindowScriptObject()
{
	return NULL;//new WebScriptObjectWrapper( [ m_WebView windowScriptObject ] );
}

WebScriptObjectWrapper::WebScriptObjectWrapper( /*WebScriptObject*/ void* o ) : m_ScriptObject(o)
{
	//[ m_ScriptObject retain ];
}

WebScriptObjectWrapper::~WebScriptObjectWrapper()
{
	//[ m_ScriptObject release ];
}

MonoObject* WebScriptObjectWrapper::EvalJavaScript( const std::string& scriptText ) 
{
	return NULL;//MonoObjectFromCocoa( [ m_ScriptObject evaluateWebScript:[ NSString stringWithUTF8String: scriptText.c_str() ] ]);
}

MonoObject* WebScriptObjectWrapper::InvokeMethod( const std::string& methodName, MonoArray* argumentList ) 
{
	return NULL;//MonoObjectFromCocoa( [ m_ScriptObject callWebScriptMethod: [ NSString stringWithUTF8String: methodName.c_str() ] withArguments: MonoArrayToCocoa( argumentList) ]);
}

MonoObject* WebScriptObjectWrapper::GetKey( const std::string& key )
{
	return NULL;
}

void WebScriptObjectWrapper::SetKey( const std::string& key, MonoObject* value )
{
}

void WebScriptObjectWrapper::RemoveKey( const std::string& key )
{
}

MonoObject* WebScriptObjectWrapper::GetIndex( int index)
{
	return NULL;
}

void WebScriptObjectWrapper::SetIndex(int index, MonoObject* value)
{
}
