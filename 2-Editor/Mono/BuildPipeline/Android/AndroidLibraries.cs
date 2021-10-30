using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

using UnityEngine;

namespace UnityEditor.Android
{
internal class AndroidLibraries : HashSet<string>
{
	private const string PROJECT_PROPERTIES = "project.properties";
	private const string DEFAULT_PROPERTIES = "default.properties";
	private const string RESOURCES_DIR = "res";
	private const string LIBRARY_DIR = "libs";
	private const string COMPILED_JAR_FILES = "bin/*.jar";

	private const RegexOptions REGEXOPT_MULTILINEIGNORECASE	= RegexOptions.Multiline | RegexOptions.IgnoreCase;
	private static readonly Regex PROP_ISLIBRARYPROJECT	= new Regex(@"^android\.library.*=.*true", REGEXOPT_MULTILINEIGNORECASE);
	private static readonly Encoding UTF8_ENCODING = new UTF8Encoding(false);

	public string[] GetManifestFiles()
	{
		return Find(AndroidManifest.AndroidManifestFile);
	}

	public string[] GetResourceDirectories()
	{
		return Find(RESOURCES_DIR);
	}

	public string[] GetLibraryDirectories()
	{
		return Find(LIBRARY_DIR);
	}

	public string[] GetCompiledJarFiles()
	{
		return Find(COMPILED_JAR_FILES);
	}

	public string[] GetPackageNames()
	{
		List<string> packageNames = new List<string>();
		foreach (string manifestFile in GetManifestFiles())
			packageNames.Add(new AndroidManifest(manifestFile).GetPackageName());
		return packageNames.ToArray();
	}

	public int FindAndAddLibraryProjects(string searchPattern)
	{
		int numberOfProjectAdded = 0;
		string[] patterns = new string[] {
			Path.Combine(searchPattern, PROJECT_PROPERTIES),
			Path.Combine(searchPattern, DEFAULT_PROPERTIES)
		};
		foreach (string pattern in patterns)
			foreach (string candidate in AndroidFileLocator.Find(pattern))
				if (AddLibraryProject(candidate))
					++numberOfProjectAdded;
		return numberOfProjectAdded;
	}

	public bool AddLibraryProject(string projectPropertiesPath)
	{
		if (!IsLibraryProject(projectPropertiesPath))
		{
			Debug.LogWarning(string.Format("Project '{0}' is not an android library.", Directory.GetParent(projectPropertiesPath)));
			return false;
		}
		Add(Directory.GetParent(projectPropertiesPath).FullName);
		return true;
	}

	public static bool IsLibraryProject(string projectPropertiesPath)
	{
		if (!File.Exists(projectPropertiesPath))
			return false;

		string projectProperties = File.ReadAllText(projectPropertiesPath, UTF8_ENCODING);
		return PROP_ISLIBRARYPROJECT.IsMatch(projectProperties);
	}

	private string[] Find(string searchPattern)
	{
		List<string> files = new List<string>();
		foreach (var libraryPath in this)
			AndroidFileLocator.Find(Path.Combine(libraryPath, searchPattern), files);
		return files.ToArray();
	}
}
}