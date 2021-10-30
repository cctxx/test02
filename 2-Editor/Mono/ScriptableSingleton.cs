using UnityEngine;
using System;
using System.IO;
using UnityEditorInternal;
using Object = UnityEngine.Object;


namespace UnityEditor
{
	// Use the FilePathAttribute when you want to have your scriptable singleton to persist between unity sessions. 
	// Example: [FilePathAttribute("Library/SearchFilters.ssf", FilePathAttribute.Location.ProjectFolder)]
	// Ensure to call Save() from client code (derived instance) 
	[AttributeUsage(AttributeTargets.Class)]
	class FilePathAttribute : Attribute
	{
		public enum Location {PreferencesFolder, ProjectFolder}
		public string filepath {get;set;}
		public FilePathAttribute (string relativePath, Location location)
		{
			if (string.IsNullOrEmpty(relativePath))
			{
				Debug.LogError ("Invalid relative path! (its null or empty)");
				return;
			}

			// We do not want a slash as first char
			if (relativePath[0] == '/')
				relativePath = relativePath.Substring (1);				

			if (location == Location.PreferencesFolder)
				filepath = InternalEditorUtility.unityPreferencesFolder + "/" + relativePath;
			else //location == Location.ProjectFolder
				filepath = relativePath;
		}
	}
	

	internal class ScriptableSingleton<T> : ScriptableObject where T : ScriptableObject
	{
		static T s_Instance;

		internal static T instance 
		{
			get {
				if (s_Instance == null) 
					CreateAndLoad ();

				return s_Instance;
			}
		}
	
		// On domain reload ScriptableObject objects gets reconstructed from a backup. We therefore set the s_Instance here
		protected ScriptableSingleton ()
		{
			if (s_Instance != null)
			{
				Debug.LogError("ScriptableSingleton already exists. Did you query the singleton in a constructor?");
			}
			else
			{
				object casted = this;
				s_Instance = casted as T;
				NUnit.Framework.Assert.That(s_Instance != null);
			}
		}

		private static void CreateAndLoad ()
		{
			NUnit.Framework.Assert.That (s_Instance == null);

			// Load
			string filePath = GetFilePath ();
			if (!string.IsNullOrEmpty (filePath))
			{
				// If a file exists the 
				InternalEditorUtility.LoadSerializedFileAndForget (filePath);
			}

			if (s_Instance == null)
			{
				// Create
				T t = CreateInstance<T>();
				t.hideFlags = HideFlags.HideAndDontSave;
			}

			NUnit.Framework.Assert.That(s_Instance != null);
		}

		protected virtual void Save (bool saveAsText)
		{
			if (s_Instance == null)
			{
				Debug.Log ("Cannot save ScriptableSingleton: no instance!");
				return;
			}

			string filePath = GetFilePath ();
			if (!string.IsNullOrEmpty (filePath))
			{
				string folderPath = Path.GetDirectoryName(filePath);
				if (!Directory.Exists (folderPath))
					Directory.CreateDirectory (folderPath);

				InternalEditorUtility.SaveToSerializedFileAndForget(new[] { s_Instance }, filePath, saveAsText);
			}
		}

		private static string GetFilePath ()
		{
			Type type = typeof(T);
			object[] atributes = type.GetCustomAttributes (true);
			foreach (object attr in atributes)
			{
				if (attr is FilePathAttribute)
				{
					FilePathAttribute f = attr as FilePathAttribute;
					return f.filepath;
				}
			}	
			return null;
		}
	}
}
