using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections;
using System.Collections.Generic;

using Mono.Cecil;
using UnityEngine;

namespace UnityEditor
{

public partial class AssemblyHelper
{
	// Check if assmebly internal name doesn't match file name, and show the warning.
	static public void CheckForAssemblyFileNameMismatch(string assemblyPath)
	{
		string fileName = Path.GetFileNameWithoutExtension(assemblyPath);
		string assemblyName = ExtractInternalAssemblyName(assemblyPath);
		if (fileName != assemblyName)
		{
			Debug.LogWarning("Assembly '" + assemblyName + "' has non matching file name: '" + Path.GetFileName(assemblyPath) + "'. This can cause build issues on some platforms.");
		}
	}

	static public string[] GetNamesOfAssembliesLoadedInCurrentDomain()
	{
		var assemblies = AppDomain.CurrentDomain.GetAssemblies();
		var locations = new List<string>();
		foreach (var a in assemblies)
		{
			try
			{
				locations.Add(a.Location);
			}
			catch (NotSupportedException)
			{
			   //we have some "dynamic" assmeblies that do not have a filename
			}
		}
		return locations.ToArray();
	}

	static public string ExtractInternalAssemblyName (string path)
	{
		AssemblyDefinition definition = AssemblyDefinition.ReadAssembly(path);
		return definition.Name.Name;
	}

	static AssemblyDefinition GetAssemblyDefinitionCached (string path, Dictionary<string, AssemblyDefinition> cache)
	{
		if (cache.ContainsKey(path))	
			return cache[path];
		else
		{
			//float time = Time.realtimeSinceStartup;
			AssemblyDefinition definition = AssemblyDefinition.ReadAssembly(path);
			cache[path] = definition;
			// Debug.Log("Loading " + path + " : " + (Time.realtimeSinceStartup - time));
			return definition;
		}
	}
	static private bool IgnoreAssembly(string assemblyPath, BuildTarget target)
	{
		switch (target)
		{
			case BuildTarget.MetroPlayer:
				if (assemblyPath.IndexOf("mscorlib.dll") != -1 ||
					assemblyPath.IndexOf("System.") != -1 ||
					assemblyPath.IndexOf("Windows.dll") != -1)
					return true;
				break;
			#if INCLUDE_WP8SUPPORT
			case BuildTarget.WP8Player:
				if (assemblyPath.IndexOf("mscorlib.dll") != -1 ||
					assemblyPath.IndexOf("System.") != -1 ||
					assemblyPath.IndexOf("Windows.dll") != -1 ||
					assemblyPath.IndexOf("Microsoft.") != -1)
					return true;
				break;
			#endif
		}
		return IsInternalAssembly (assemblyPath);
	}
	static private void AddReferencedAssembliesRecurse (string assemblyPath, List<string> alreadyFoundAssemblies, string[] allAssemblyPaths, string[] foldersToSearch, Dictionary<string, AssemblyDefinition> cache, BuildTarget target)
	{
        if (IgnoreAssembly(assemblyPath, target)) return;

		AssemblyDefinition assembly = GetAssemblyDefinitionCached(assemblyPath, cache);
		if (assembly == null)
			throw new System.ArgumentException("Referenced Assembly " + Path.GetFileName(assemblyPath) + " could not be found!");
				
		// Ignore it if we already added the assembly
		if (alreadyFoundAssemblies.IndexOf (assemblyPath) != -1)
			return;
		
		alreadyFoundAssemblies.Add (assemblyPath);
		
		// Go through all referenced assemblies
		foreach (AssemblyNameReference referencedAssembly in assembly.MainModule.AssemblyReferences)		
		{
			// Special cases for Metro
			if (referencedAssembly.Name == "BridgeInterface") continue;
			if (referencedAssembly.Name == "WinRTBridge") continue;
			if (referencedAssembly.Name == "UnityEngineProxy") continue;
            if (IgnoreAssembly(referencedAssembly.Name + ".dll", target)) continue;

			string foundPath = FindAssemblyName(referencedAssembly.FullName, referencedAssembly.Name, allAssemblyPaths, foldersToSearch, cache);

			if (foundPath == "")
			{
				throw new System.ArgumentException(string.Format("The Assembly {0} is referenced by {1}. But the dll is not allowed to be included or could not be found.", referencedAssembly.Name, assembly.MainModule.Assembly.Name.Name));
			}

			AddReferencedAssembliesRecurse(foundPath, alreadyFoundAssemblies, allAssemblyPaths, foldersToSearch, cache, target);
		}
	}
	
	static string FindAssemblyName (string fullName, string name, string[] allAssemblyPaths, string[] foldersToSearch, Dictionary<string, AssemblyDefinition> cache)	
	{
			
		// Search in provided assemblies
		for (int i=0;i<allAssemblyPaths.Length;i++)
		{
			AssemblyDefinition definition = GetAssemblyDefinitionCached(allAssemblyPaths[i], cache);
			if (definition.MainModule.Assembly.Name.Name == name)
				return allAssemblyPaths[i];
		}
			
		// Search in GAC
		foreach (string folder in foldersToSearch)
		{
			string pathInGacFolder = Path.Combine(folder, name + ".dll");
			if (File.Exists(pathInGacFolder))
				return pathInGacFolder;
		}
		return "";
	}	

	static public string[] FindAssembliesReferencedBy (string[] paths, string[] foldersToSearch, BuildTarget target)
	{
		List<string> unique = new List<string> ();
		string[] allAssemblyPaths = paths;

		Dictionary<string, AssemblyDefinition> cache = new Dictionary<string, AssemblyDefinition>();
		for (int i=0;i<paths.Length;i++)
			AddReferencedAssembliesRecurse(paths[i], unique, allAssemblyPaths, foldersToSearch, cache, target);

		for (int i=0;i<paths.Length;i++)
			unique.Remove(paths[i]);
	
		return unique.ToArray();
	}

	static public string[] FindAssembliesReferencedBy(string path, string[] foldersToSearch, BuildTarget target)
	{
		string[] tmp = new string[1];
		tmp[0] = path;
		return FindAssembliesReferencedBy(tmp, foldersToSearch, target);
	}
	
	static bool IsTypeMonoBehaviourOrScriptableObject (AssemblyDefinition assembly, TypeReference type)
	{
		if (type == null)
			return false;

		// Early out
		if (type.FullName == "System.Object")
			return false;
		
		// Look up the type in UnityEngine.dll or UnityEditor.dll
		Assembly builtinAssembly = null;
		if (type.Scope.Name == "UnityEngine")
			builtinAssembly = typeof(MonoBehaviour).Assembly;
		else if (type.Scope.Name == "UnityEditor")
			builtinAssembly = typeof(EditorWindow).Assembly;
		
		if (builtinAssembly != null)
		{		
			Type engineType = builtinAssembly.GetType(type.FullName);
			if (engineType == typeof(MonoBehaviour) || engineType.IsSubclassOf(typeof(MonoBehaviour)))
				return true;
			if (engineType == typeof(ScriptableObject) || engineType.IsSubclassOf(typeof(ScriptableObject)))
				return true;
		}
		
		// Look up the type in the assembly we are importing
		foreach (ModuleDefinition module in assembly.Modules)
		{
			TypeDefinition typeDefinition = module.GetType(type.FullName);
			if (typeDefinition != null)
				return IsTypeMonoBehaviourOrScriptableObject(assembly, typeDefinition.BaseType);
		}
		return false;
	}
		
	static public void ExtractAllClassesThatInheritMonoBehaviourAndScriptableObject (string path, out string[] classNamesArray, out string[] classNameSpacesArray)
	{
		classNamesArray = null;
		classNameSpacesArray = null;
			
		List<string> classNames = new List<string>();
		List<string> nameSpaces = new List<string>();
		AssemblyDefinition assembly = AssemblyDefinition.ReadAssembly(path);
		foreach (ModuleDefinition module in assembly.Modules)
		{
			foreach (TypeDefinition type in module.Types)
			{
				TypeReference baseType = type.BaseType;
				
				if (IsTypeMonoBehaviourOrScriptableObject (assembly, baseType))
				{
					classNames.Add(type.Name);
					nameSpaces.Add(type.Namespace);
				}
			}
		}
		
		classNamesArray = classNames.ToArray();
		classNameSpacesArray = nameSpaces.ToArray();	
	}
		
	static public AssemblyTypeInfoGenerator.ClassInfo[] ExtractAssemblyTypeInfo(string assemblyPathName)
	{
		AssemblyTypeInfoGenerator gen = new AssemblyTypeInfoGenerator (assemblyPathName);
		gen.gatherClassInfo ();
		return gen.ClassInfoArray;
	}

	internal static Type[] GetTypesFromAssembly(Assembly assembly)
	{
		try
		{
			return assembly.GetTypes ();
		}
		catch (ReflectionTypeLoadException)
		{
			return new Type[]{};
		}
	}

	internal static IEnumerable<T> FindImplementors<T> (Assembly assembly) where T: class
	{
		Type interfaze = typeof(T);
		foreach (Type type in AssemblyHelper.GetTypesFromAssembly (assembly)) {
			if (/*type.IsNotPublic - future! ||*/type.IsInterface || type.IsAbstract || !interfaze.IsAssignableFrom (type))
				continue;
			T module = null;

			if (typeof (ScriptableObject).IsAssignableFrom (type))
				module = ScriptableObject.CreateInstance (type) as T;
			else
				module = Activator.CreateInstance (type) as T;
			if (module != null)
				yield return module;
		}
	}

	public static bool IsManagedAssembly (string file)
	{
		if (!".dll".Equals (Path.GetExtension (file), StringComparison.OrdinalIgnoreCase))
			return false;

		try {
			using (Stream fs = new FileStream (file, FileMode.Open, FileAccess.Read, FileShare.ReadWrite)) {
				BinaryReader reader = new BinaryReader(fs);
				
				//The 4-byte PE Header indicator is at 0x3C
				fs.Position = 0x3C;
				uint peHeader = reader.ReadUInt32();
				
				//Jump to the CLR header start location at 0xE8 past the PE header 
				fs.Position = peHeader + 0xe8;
				
				// These two bytes will be nonzero for managed assemblies, 
				// and zero for native dlls
				return (0UL != reader.ReadUInt64 ());
			}// using fs
		} catch { } // Don't care
		return false;
	}

	public static bool IsInternalAssembly (string file)
	{
		return UnityEditor.Modules.ModuleManager.IsRegisteredModule (file);
	}

	const int kDefaultDepth = 10;
	internal static ICollection<string> FindAssemblies (string basePath)
	{
		return FindAssemblies (basePath, kDefaultDepth);
	}

	internal static ICollection<string> FindAssemblies (string basePath, int maxDepth)
	{
			var assemblies = new List<string> ();

			if (0 == maxDepth)
				return assemblies;

			try {
				DirectoryInfo directory = new DirectoryInfo (basePath);
				assemblies.AddRange (directory.GetFiles ()
				                     .Where (file => IsManagedAssembly (file.FullName))
				                     .Select (file => file.FullName));
				foreach (DirectoryInfo subdirectory in directory.GetDirectories ())
					assemblies.AddRange (FindAssemblies (subdirectory.FullName, maxDepth-1));
			} catch (Exception) {
				// Return what we have now
			}

			return assemblies;
	}
}

}
