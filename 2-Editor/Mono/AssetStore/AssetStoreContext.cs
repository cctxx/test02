using UnityEngine;
using UnityEditor;
using UnityEditorInternal;
using System.Collections;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.IO;

namespace UnityEditor
{
	
    internal partial class AssetStoreContext : ScriptableObject
    {
		private static Regex standardPackageRe = new Regex(@"/Standard Packages/(Character\ Controller|Glass\ Refraction\ \(Pro\ Only\)|Image\ Effects\ \(Pro\ Only\)|Light\ Cookies|Light\ Flares|Particles|Physic\ Materials|Projectors|Scripts|Standard\ Assets\ \(Mobile\)|Skyboxes|Terrain\ Assets|Toon\ Shading|Tree\ Creator|Water\ \(Basic\)|Water\ \(Pro\ Only\))\.unitypackage$", RegexOptions.IgnoreCase);
		private static Regex generatedIDRe = new Regex(@"^\{(.*)\}$");
		private static Regex invalidPathChars = new Regex(@"[^a-zA-Z0-9() _-]");
				
		public bool docked;
		public string initialOpenURL;
		public AssetStoreWindow window;
		
		// One should use  ScriptableObject.CreateInstance<AssetStoreContext> to create instances of AssetStoreContext
		private AssetStoreContext()
		{
		}
		
		// Convenience to fetch the current WebView's main WebScriptObject
		private WebScriptObject JS {
			get {
				return window.scriptObject;
			}
		}
		
    	public string GetInitialOpenURL() 
		{
			if (initialOpenURL != null )
			{
				string tmp=initialOpenURL;
				initialOpenURL=null;
				return tmp;
			}
			else 
			{
				return "";
			}
		}
		
		public string GetAuthToken()
		{
    		return UnityEditorInternal.InternalEditorUtility.GetAuthToken();
    	}
    	
		public int[] GetLicenseFlags()
		{
			return UnityEditorInternal.InternalEditorUtility.GetLicenseFlags();
		}

		public void Download(WebScriptObject package, WebScriptObject downloadInfo)
		{
			string url = downloadInfo.Get("url");
			string key = downloadInfo.Get("key");

			string package_id = downloadInfo.Get("id");
			string package_name = package.Get("title");
			
			string publisher_name = package.Get("publisher.label");
			string category_name = package.Get("category.label");

			Download(package_id, url, key, package_name, 
						     publisher_name, category_name, null);
			
			/*
			string[] dest = {publisher_name, category_name, package_name};
			for(int i = 0; i < 3 ; i++)
			{
				dest[i]=invalidPathChars.Replace(dest[i],"");
			}
			
			// If package name cannot be stored as a valid file name, use the package id
			if (dest[2] == "")
				dest[2]=invalidPathChars.Replace(package_id,"");
			
			// If still no valid chars use a mangled url as the file name
			if (dest[2] == "")
				dest[2]=invalidPathChars.Replace(url,"");
			
			WebScriptObject existing = JS.ParseJSON(AssetStoreUtils.CheckDownload(package_id, url, dest, key));
			WebScriptObject inProgress = existing.Get("in_progress");
			if ( inProgress != null && inProgress )
			{
				Debug.Log("Will not download " + package_name + ". Download is already in progress.");
				return;
			}
			
			bool resumeOK = false;
			string existingUrl = existing.Get("download.url");
			string existingKey = existing.Get("download.key");
			resumeOK = (existingUrl == url && existingKey == key);

			WebScriptObject parameters = JS.ParseJSON("{}");
			//parameters.Set("content", package);
			parameters.Set("download", downloadInfo);
			Debug.Log(parameters.ToJSON());
			
			//Debug.Log((resumeOK?"Resuming":"Initiating")+" a download request of package " + package_name + " from url " + url);
			AssetStoreUtils.Download(package_id, url, dest, key, parameters.ToJSON(4), resumeOK);
    		*/
    	}
    	   	
    	/*
    	 * Create an array consisting of publisherName, categoryName and packageName
    	 * This is to be used by AssetStoreUtils.*Download functions
    	 */
		public static string[] PackageStorePath(string publisher_name,	
												string category_name,
												string package_name,
												string package_id,
												string url)
		{
			string[] dest = {publisher_name, category_name, package_name};
			for(int i = 0; i < 3 ; i++)
				dest[i] = invalidPathChars.Replace(dest[i], "");
			
			// If package name cannot be stored as a valid file name, use the package id
			if (dest[2] == "")
				dest[2] = invalidPathChars.Replace(package_id,"");
			
			// If still no valid chars use a mangled url as the file name
			if (dest[2] == "")
				dest[2] = invalidPathChars.Replace(url,"");
			return dest;
		}
		
		public static void Download(string package_id, string url, string key, string package_name, 
						     		string publisher_name, string category_name, AssetStoreUtils.DownloadDoneCallback doneCallback)
		{
			string[] dest = PackageStorePath(publisher_name, category_name,
											 package_name, package_id, url);
			
			JSONValue existing = JSONParser.SimpleParse(AssetStoreUtils.CheckDownload(package_id, url, dest, key));

			// If the package is actively being downloaded right now just return
			if ( existing.Get("in_progress").AsBool(true) )
			{
				Debug.Log("Will not download " + package_name + ". Download is already in progress.");
				return;
			}
			
			// The package is not being dowloaded.
			// If the package has previously been partially downloaded then
			// resume that download.
			string existingUrl = existing.Get("download.url").AsString(true);
			string existingKey = existing.Get("download.key").AsString(true);
			bool resumeOK = (existingUrl == url && existingKey == key);
			
			JSONValue download = new JSONValue();
			download["url"] = url;
			download["key"] = key;
			JSONValue parameters = new JSONValue();
			parameters["download"] = download;
						
			//Debug.Log((resumeOK?"Resuming":"Initiating")+" a download request of package " + package_name + " from url " + url);
			AssetStoreUtils.Download(package_id, url, dest, key, parameters.ToString(), resumeOK, doneCallback);
    	}

		
		public string GetString(string key)
    	{
			return EditorPrefs.GetString(key);
    	}
    	
    	public int GetInt(string key)
    	{
    		return EditorPrefs.GetInt(key);
    	}
    	
    	public float GetFloat(string key)
    	{
    		return EditorPrefs.GetFloat(key);
    	}
    	
    	public void SetString(string key, string value)
    	{
			EditorPrefs.SetString(key, value);
    	}
    	
    	public void SetInt(string key, int value)
    	{
    		EditorPrefs.SetInt(key, value);
    	}

    	public void SetFloat(string key, float value)
    	{
    		EditorPrefs.SetFloat(key, value);
    	}
    	
    	public bool HasKey(string key)
    	{
    		return EditorPrefs.HasKey(key);
    	}
		
		public void DeleteKey(string key)
		{
			EditorPrefs.DeleteKey(key);
		}

		private bool IsBuiltinStandardAsset(string path)
		{
			return standardPackageRe.IsMatch(path);
		}
		
		public WebScriptObject packages {
			get {
				WebScriptObject res = GetPackageList();
				return res.Get("results");
			}
			
		}
		
		public WebScriptObject GetPackageList()
		{
			
			Dictionary<string, WebScriptObject> uniq = new Dictionary<string, WebScriptObject>();
			WebScriptObject res = JS.ParseJSON("{}");
			WebScriptObject array = JS.ParseJSON("[]");
			PackageInfo[] packages = PackageInfo.GetPackageList();
			foreach (PackageInfo p in packages) 
			{
				WebScriptObject item ;
				
				if ( p.jsonInfo == "" )
				{
					item = JS.ParseJSON("{}");
					item.Set("title", System.IO.Path.GetFileNameWithoutExtension(p.packagePath) );
					
					item.Set("id", "{"+p.packagePath+"}");
					
					if ( IsBuiltinStandardAsset(p.packagePath) )
					{
						item.Set("publisher", JS.ParseJSON(@"{""label"": ""Unity Technologies"",""id"": ""1""}"));
						item.Set("category", JS.ParseJSON(@"{""label"": ""Prefab Packages"",""id"": ""4""}"));
						item.Set("version", "3.5.0.0");
						item.Set("version_id", "-1");
					}
					
				}
				else
				{
					item = JS.ParseJSON(p.jsonInfo);
					if ( item.Get("id") == null )
					{
						//Debug.Log("Getting id from link item");
						WebScriptObject link = item.Get("link");
						if (link != null)
						{
							item.Set("id",(string)link.Get("id"));
						}
						else
						{
							item.Set("id","{"+p.packagePath+"}");
						}
					}
				}
				string id = item.Get("id");
				item.Set("local_icon", p.iconURL);
				item.Set("local_path", p.packagePath);
				
				if ( ! uniq.ContainsKey(id) || uniq[id].Get("version_id") == null || 
					item.Get("version_id") != null && (int)uniq[id].Get("version_id") <= (int)item.Get("version_id") ) 
				{
					uniq[id]=item;
				}
				
			}
			int i = 0;
			foreach(  KeyValuePair<string, WebScriptObject> item in uniq )
				array.Set(i++, item.Value);
				
			res.Set("results", array);
			
			return res;
		}

		//TODO: WTF are we exposing the raw skinindex?
		public int GetSkinIndex()
		{
			return EditorGUIUtility.isProSkin ? 1 : 0;
		}
		
		public bool GetDockedStatus()
		{
			return docked;
		}
		
		public bool OpenPackage(string id)
		{
			return OpenPackage(id, "default");
		}
		
		
		public bool OpenPackage(string id, string action)
		{
			return OpenPackageInternal(id);
			/*
			Match match = generatedIDRe.Match(id); 
			if (match.Success && File.Exists(match.Groups[1].Value) ) // If id looks like a path name, just try to open that
			{
				AssetDatabase.ImportPackage(match.Groups[1].Value, true);
				return true;
			}
			else
			{
				foreach (PackageInfo package in PackageInfo.GetPackageList())
				{
					if ( package.jsonInfo != "")
					{
						WebScriptObject item = JS.ParseJSON(package.jsonInfo);
						WebScriptObject itemID =  item.Get("id") ;
						if ( itemID != null && itemID == id && File.Exists(package.packagePath) )
						{
							AssetDatabase.ImportPackage(package.packagePath, true);
							return true;
						}
					}
				}
			}
			Debug.LogError("Unknown package ID "+id);
			return false;
			*/
		}
		
		public static bool OpenPackageInternal(string id)
		{
			Match match = generatedIDRe.Match(id); 
			if (match.Success && File.Exists(match.Groups[1].Value) ) // If id looks like a path name, just try to open that
			{
				AssetDatabase.ImportPackage(match.Groups[1].Value, true);
				return true;
			}
			else
			{
				foreach (PackageInfo package in PackageInfo.GetPackageList())
				{
					if ( package.jsonInfo != "")
					{
						JSONValue item = JSONParser.SimpleParse(package.jsonInfo);
						string itemID = item.Get("id").IsNull() ? null : item["id"].AsString(true);
						if ( itemID != null && itemID == id && File.Exists(package.packagePath) )
						{
							AssetDatabase.ImportPackage(package.packagePath, true);
							return true;
						}
					}
				}
			}
			Debug.LogError("Unknown package ID " + id);
			return false;
		}

		public void MakeMenu(WebScriptObject contextMenu)
		{
			MakeMenu(contextMenu, 0f, 0f);
		}
		
		public void MakeMenu(WebScriptObject contextMenu, float deltaX, float deltaY)
		{
			WebScriptObject commands = contextMenu.Get("commands");
			int commandsLength = commands.Get("length");
			string[] menuItemLabels = new string[commandsLength];
			string[] menuItemActions = new string[commandsLength];
			for (int i = 0; i < commands.Get("length"); i++)
			{
				WebScriptObject command = commands.Get(i);
				menuItemLabels[i] = command.Get("label");
				menuItemActions[i] = command.Get("action");
			}
			Vector2 mousePos = Event.current.mousePosition;
			Rect r = new Rect(mousePos.x + deltaX, mousePos.y + deltaY, 0f, 0f);
			EditorUtility.DisplayCustomMenu(r, menuItemLabels, null, ContextMenuClick, menuItemActions);
		}
		
		public void OpenBrowser(string url)
		{
			Application.OpenURL(url);
		}
		
		private void ContextMenuClick(object userData, string[] options, int selected)
		{
			if (selected >= 0)
			{
				string[] menuItemActions = userData as string[];
				JS.EvalJavaScript(menuItemActions[selected]);
			}
		}
		
    }
	
}
