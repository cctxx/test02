using UnityEngine;
using UnityEditor;
using System.Collections;

namespace UnityEditor
{
    internal class AssetStoreWindow : EditorWindow , IHasCustomMenu
	{
		private WebView webView;
		private AssetStoreContext contextScriptObject;
		private bool isProSkin;
		private bool isDocked;
		private bool isOffline;
        private MouseCursor m_SavedCursor;
        private int m_SavedCursorCountHack = 0;
		private Vector2 minUndockedSize;
		private Vector2 minDockedSize;

        // Use EditorWindow.GetWindow<AssetStoreWindow> to get/create an instance of this class;
		private AssetStoreWindow()
		{
			Resolution res = Screen.currentResolution;
			int width = res.width >= 1024 ? 1024 : res.width;
			int height = res.height >= 896 ? 800 : res.height-96;
			int left = (res.width - width) / 2;
			int top = (res.height - height) / 2;

			position = new Rect(left, top, width, height);
		}

		public static void OpenURL( string url )
		{
			AssetStoreWindow window = Init();

			window.InvokeJSMethod("document.AssetStore", "openURL", url);
			window.CreateContextObject();
			window.contextScriptObject.initialOpenURL = url;
		}

		// Use this for initialization
		public static AssetStoreWindow Init () 
		{
			AssetStoreWindow window = EditorWindow.GetWindow<AssetStoreWindow>();
			Resolution res = Screen.currentResolution;
			int minWidth = res.width >= 1024 ? 1024 : res.width;
			int minHeight = res.height >= 896 ? 800 : res.height-96;
			window.minUndockedSize = new Vector2 (minWidth, minHeight);
			window.minDockedSize = new Vector2 (512,256);
			window.minSize = window.docked?window.minDockedSize:window.minUndockedSize;
			window.maxSize = new Vector2 (2048,2048);
			window.Show ();
            window.m_SavedCursor = MouseCursor.Arrow;
			return window;
		}
		
		public virtual void AddItemsToMenu (GenericMenu menu)
		{
			menu.AddItem (new GUIContent ("Reload"), false, Reload);
		}
		
		// Forces user logout
		public void Logout()
		{
			InvokeJSMethod("document.AssetStore.login","logout");
		}
		
		// Reloads the web view
		public void Reload()
		{
			if ( isOffline )
			{
				InitWebView();
			}
			else
			{
				isProSkin = EditorGUIUtility.isProSkin;
				isDocked = docked;
				InvokeJSMethod("location","reload",true);
			}
		}
		
		void CreateContextObject()
		{
			if ( contextScriptObject == null )
			{
				contextScriptObject = ScriptableObject.CreateInstance<AssetStoreContext>();
				contextScriptObject.hideFlags=HideFlags.HideAndDontSave;
				contextScriptObject.window=this;
			}
		}
				
		void SetContextObject()
		{
			CreateContextObject();
			contextScriptObject.docked = docked;
			webView.windowScriptObject.Set("context", contextScriptObject);

		}
		
		// Returns the current web view's global JavaScript object.
		internal WebScriptObject scriptObject {
			get {
				return webView.windowScriptObject;
			}
		}
		
		void InitWebView() 
		{
			isProSkin = EditorGUIUtility.isProSkin;
			isDocked = docked;
			isOffline = false;
			if( ! webView )
			{
				webView = ScriptableObject.CreateInstance<WebView>();
				webView.InitWebView((int)position.width, (int)position.height, false);
				webView.hideFlags=HideFlags.HideAndDontSave;
				webView.LoadFile(AssetStoreUtils.GetLoaderPath());
			}
			else
			{
				webView.LoadFile(AssetStoreUtils.GetLoaderPath());
			}
				
			webView.SetDelegateObject(this);
			wantsMouseMove=true;
		}
		
		void OnLoadError(string frameName)
		{
			if ( ! webView )
				return;
			if ( isOffline ) 
			{
				Debug.LogError("Unexpected error: Failed to load offline Asset Store page");
				return;
			} 
			isOffline = true;

            webView.LoadFile(AssetStoreUtils.GetOfflinePath());
		}
		
		void OnEnable() 
		{
			
			AssetStoreUtils.RegisterDownloadDelegate(this);
		}
    	
		public void OnDisable() 
		{
			AssetStoreUtils.UnRegisterDownloadDelegate(this);
    	}

		private void InvokeJSMethod(string objectName, string name, params object[] args)
		{
			if ( !webView )
				return;
			WebScriptObject obj  = webView.windowScriptObject.Get(objectName);
			if (obj == null)
				return;
			obj.InvokeMethodArray(name, args);
		}

		void OnGUI () 
		{
			if(! webView )
               InitWebView();

			EditorGUIUtility.AddCursorRect(new Rect(0, 0, position.width, position.height), m_SavedCursor);
            // No need to check skin index and docked status on every event type
			if( Event.current.type == EventType.Layout)
			{
                
                // send an updated skin index to css
				if (webView && isProSkin != EditorGUIUtility.isProSkin)
				{
					isProSkin = EditorGUIUtility.isProSkin;
					InvokeJSMethod("document.AssetStore", "refreshSkinIndex");
					Repaint();
				}
				UpdateDockStatusIfNeeded();
			}
			
			// This window only contains a single web view
			webView.DoGUI(new Rect(0,0,position.width,position.height));
		}
		
		void UpdateDockStatusIfNeeded()
		{
			if (isDocked != docked)
			{
				minSize = docked?minDockedSize:minUndockedSize;
				isDocked = docked;
				
				if (contextScriptObject != null)
				{
					contextScriptObject.docked = docked;
					InvokeJSMethod("document.AssetStore", "updateDockStatus");
				}
				Repaint();
			}
		}

		void OnReceiveTitle(string iTitle, string frameName)
		{
			title=iTitle;
			SetContextObject();
		}
		
		void OnFocus()
		{
			if ( webView )
				webView.Focus();
		}

		void OnLostFocus()
		{
			if ( webView )
				webView.UnFocus();
		}
		
		public void OnDestroy()
		{
			DestroyImmediate( webView );
			if ( contextScriptObject != null)
				contextScriptObject.window=null;
		}
		
 		public void OnDownloadProgress(string id, string message, int bytes, int total) 
		{
			InvokeJSMethod("document.AssetStore.pkgs", "OnDownloadProgress", id, message, bytes, total);
    	}

		void OnWebViewDirty()
		{
			Repaint();
		}

        void SetCursor(int cursor)
        {
            if ((MouseCursor)cursor != m_SavedCursor)
            {
                if( cursor != 0 || m_SavedCursorCountHack-- <= 0 )
                {
                    m_SavedCursorCountHack = 1;
                    m_SavedCursor = (MouseCursor)cursor;
                    Repaint();
                }
            }
            else
            {
                m_SavedCursorCountHack = 1;
            }
        }
    
    }
}
