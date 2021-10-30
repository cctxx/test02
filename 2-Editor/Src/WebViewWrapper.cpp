#include "UnityPrefix.h"


#include "Runtime/Utilities/File.h"
#include "Runtime/Graphics/Image.h"
#include "Runtime/Utilities/LogAssert.h"
#include "Runtime/Utilities/BitUtility.h"
#include "Runtime/Utilities/URLUtility.h"
#include "Runtime/Scripting/ScriptingUtility.h"
#include "Runtime/Mono/MonoManager.h"
#include "Runtime/Mono/MonoScript.h"
#include "Runtime/Profiler/TimeHelper.h"
#include "Editor/Src/Undo/UndoManager.h"

#include "Editor/Src/WebViewWrapper.h"

TextureFormat g_WebKitTextureFormat = kTexFormatRGBA32;
float g_WebKitFlipYScale = -1;

WebViewWrapper* WebViewWrapper::s_FocusedWebView = NULL;

#if ENABLE_ASSET_STORE

bool WebViewWrapper::UpdateTexture() 
{
	if (!IsDirty() )
		return false;

	DoRenderImpl();

	return true;
}

void WebViewWrapper::Resize(int width, int height) 
{
	if (width != m_Width || height != m_Height) 
	{
		AutoDisableEmulation guard;

		m_Width=width;
		m_Height=height;
		
		int textureWidth=NextPowerOfTwo(width);
		int textureHeight=NextPowerOfTwo(height);
		bool textureSizeWasChanged = false;

		if (textureWidth != m_TargetTexture->GetGLWidth() || textureHeight != m_TargetTexture->GetGLHeight())
		{
			m_TargetTexture->ResizeWithFormat(textureWidth, textureHeight, g_WebKitTextureFormat, Texture2D::kNoMipmap | Texture2D::kOSDrawingCompatible);
			textureSizeWasChanged = true;
		}

		m_TargetTexture->SetUVScale( width/(float)textureWidth, g_WebKitFlipYScale*height/(float)textureHeight );

		DoResizeImpl(width, height, textureSizeWasChanged);
	}
}

void WebViewWrapper::InitTexture(int width, int height)
{
	AutoDisableEmulation guard;
	Assert( m_TargetTexture == NULL );
	
	m_TargetTexture = NEW_OBJECT_MAIN_THREAD (Texture2D);
	m_TargetTexture->Reset();
	m_TargetTexture->AwakeFromLoad( kInstantiateOrCreateFromCodeAwakeFromLoad );
	m_TargetTexture->SetHideFlags( Object::kHideAndDontSave );

	int textureWidth = NextPowerOfTwo(width);
	int textureHeight = NextPowerOfTwo(height);

	if (m_TargetTexture->InitTexture(textureWidth, textureHeight, g_WebKitTextureFormat, Texture2D::kNoMipmap | Texture2D::kOSDrawingCompatible, 1))
	{
		m_TargetTexture->SetFilterMode( kTexFilterNearest );
		m_TargetTexture->SetUVScale(width/(float)textureWidth, g_WebKitFlipYScale*height/(float)textureHeight);
		PostInitTexture(width, height);
	}
	else
	{
		DestroySingleObject (m_TargetTexture);
		m_TargetTexture = NULL;
		Scripting::RaiseMonoException("Failed to create texture because of invalid parameters.");
	}

}

bool WebViewWrapper::InvokeDelegateMethod(MonoMethodDesc* desc, void** argumentList, int inArgCount){
	if ( ! m_DelegateMonoObject )
		return false;
	
	MonoObject* instance = m_DelegateMonoObject->GetInstance();
	if ( instance == NULL ) 
		return false;
	MonoClass* klass = m_DelegateMonoObject->GetClass();
	if ( klass == NULL )
		return false;
	
	
	//MonoMethod* method = m_DelegateMonoObject->FindMethod(methodName.c_str());
	MonoMethod* method = mono_method_desc_search_in_class( desc, klass);
		
				
	if ( method == NULL )
		return false;
		
	MonoException* exception = 0;
	MonoMethodSignature* sig = mono_method_signature (method);
	int argCount = mono_signature_get_param_count (sig);
	
	if ( argCount == inArgCount )
	{
		mono_runtime_invoke(method, instance, argumentList, &exception);
		return true;
	}
	return false;
}


void WebViewWrapper::OnBeginNavigation(  const std::string& url, const std::string& frameName)
{
	void* argumentList[2];
	argumentList[0]=MonoStringNew(url);
	argumentList[1]=MonoStringNew(frameName);

	MonoMethodDesc* desc = mono_method_desc_new(":OnBeginNavigation(string,string)", false);
	InvokeDelegateMethod(desc, argumentList, 2);
	mono_method_desc_free(desc);
}

void WebViewWrapper::OnBeginLoading( const std::string& url, const std::string& frameName,  const std::string& mimeType)
{
	void* argumentList[3];
	argumentList[0]=MonoStringNew(url);
	argumentList[1]=MonoStringNew(frameName);
	argumentList[2]=MonoStringNew(mimeType);
	MonoMethodDesc* desc = mono_method_desc_new(":OnBeginLoading(string,string,string)", false);
	InvokeDelegateMethod(desc, argumentList, 3);
	mono_method_desc_free(desc);
}

void WebViewWrapper::OnFinishLoading( )
{
	MonoMethodDesc* desc = mono_method_desc_new(":OnFinishLoading()", false);
	InvokeDelegateMethod(desc, NULL, 0);
	mono_method_desc_free(desc);
}

void WebViewWrapper::OnReceiveTitle( const std::string& title, const std::string& frameName )
{
	if( m_TargetTexture != NULL)
		m_TargetTexture->SetName( title.c_str() );
		
	void* argumentList[2];
	argumentList[0]=MonoStringNew(title);
	argumentList[1]=MonoStringNew(frameName);
	
	MonoMethodDesc* desc = mono_method_desc_new(":OnReceiveTitle(string,string)", false);
	InvokeDelegateMethod(desc, argumentList, 2);
	mono_method_desc_free(desc);
	
}

void WebViewWrapper::OnLoadError( const std::string& frameName )
{
	if (mono_runtime_is_shutting_down())
		return;

	void* argumentList[1];
	argumentList[0]=MonoStringNew(frameName);
	
	MonoMethodDesc* desc = mono_method_desc_new(":OnLoadError(string)", false);
	InvokeDelegateMethod(desc, argumentList, 1);
	mono_method_desc_free(desc);
	
}

void WebViewWrapper::OnWebViewDirty ()
{
	MonoMethodDesc* desc = mono_method_desc_new(":OnWebViewDirty()", false);
	InvokeDelegateMethod(desc, NULL, 0);
	mono_method_desc_free(desc);
}

void WebViewWrapper::OnOpenExternalLink( const std::string& url, const std::string& source )
{
	void* argumentList[2];
	argumentList[0]=MonoStringNew(url);
	argumentList[1]=MonoStringNew(source);
	
	MonoMethodDesc* desc = mono_method_desc_new(":OnOpenExternalLink(string,string)", false);
	bool res = InvokeDelegateMethod(desc, argumentList, 2 );
	mono_method_desc_free(desc);
	
	if (! res )
		OpenURL(url);
}

void WebViewWrapper::SetDelegateMonoObject(MonoBehaviour* object) 
{
	m_DelegateMonoObject = object;
}

void WebViewWrapper::SetCursor( int cursor )
{
	void* argumentList[1];
	argumentList[0]=&cursor;
	
	MonoMethodDesc* desc = mono_method_desc_new(":SetCursor(int)", false);
	InvokeDelegateMethod(desc, (void**)argumentList, 1 );
	mono_method_desc_free(desc);
	
}

void WebViewWrapper::Focus()
{
	s_FocusedWebView = this;
	GetUndoManager().UpdateUndoName ();
}

void WebViewWrapper::UnFocus()
{
	s_FocusedWebView = NULL;
	GetUndoManager().UpdateUndoName ();
}

void WebViewWrapper::DestroyCommon()
{
	if ( m_TargetTexture != NULL )
		DestroySingleObject(m_TargetTexture);
	if ( s_FocusedWebView == this )
		s_FocusedWebView = NULL;
	m_TargetTexture=NULL;
}

#else // ENABLE_ASSET_STORE

bool WebViewWrapper::UpdateTexture() { return false; }
bool WebViewWrapper::IsDirty() { return false; }
void WebViewWrapper::Resize(int width, int height) { }
void WebViewWrapper::SetDelegateMonoObject(MonoBehaviour* object) { }

void WebViewWrapper::InjectMouseMove(int x, int y) { }
void WebViewWrapper::InjectMouseDrag(int x, int y, int button) { }
void WebViewWrapper::InjectMouseDown(int x, int y, int button, int clickCount) { }
void WebViewWrapper::InjectMouseUp(int x, int y, int button, int clickCount) { }
void WebViewWrapper::InjectMouseWheel(int x, int y, float deltaX, float deltaY) { }
void WebViewWrapper::InjectKeyboardEvent(const WebKeyboardEvent& keyboardEvent) { }
void WebViewWrapper::Focus() { }
void WebViewWrapper::UnFocus() { }
void WebViewWrapper::LoadURL(const string& url) { }
void WebViewWrapper::LoadFile(const string& path) { }

void WebViewWrapper::OnBeginNavigation(  const std::string& url, const std::string& frameName) { }
void WebViewWrapper::OnBeginLoading( const std::string& url, const std::string& frameName, const std::string& mimeType) { }
void WebViewWrapper::OnFinishLoading( ) { }
void WebViewWrapper::OnReceiveTitle( const std::string& title, const std::string& frameName) { }
void WebViewWrapper::OnLoadError( const std::string& frameName) { }
void WebViewWrapper::OnOpenExternalLink( const std::string& url, const std::string& source ) { }
void WebViewWrapper::SetCursor( int cursor) { }
void WebViewWrapper::OnWebViewDirty() { }
void WebViewWrapper::Copy () { }
void WebViewWrapper::Cut () { }
void WebViewWrapper::Paste () { }
void WebViewWrapper::SelectAll () { }
void WebViewWrapper::Undo () { }
void WebViewWrapper::Redo () { }
bool WebViewWrapper::HasUndo () { return false; }
bool WebViewWrapper::HasRedo () { return false; }
void WebViewWrapper::SetWebkitControlsGlobalLocation(int x, int y) { }
WebScriptObjectWrapper* WebViewWrapper::GetWindowScriptObject() { return NULL; }
WebKeyboardEvent WebViewWrapper::GetCurrentKeyboardEvent() { return NULL; }
#if UNITY_WIN
std::wstring WebViewWrapper::GetWindowClassName() { return std::wstring(L"UnpossibleWebViewClass11eleven"); }
void WebViewWrapper::Unparent() { }
void WebViewWrapper::Reparent() { }
#endif


#endif // ENABLE_ASSET_STORE
