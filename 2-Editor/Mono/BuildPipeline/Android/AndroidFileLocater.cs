using UnityEngine;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;

namespace UnityEditor.Android
{

internal class AndroidFileLocator
{
	private static string s_JDKLocation;

	public static string LocateJDKbin()
	{
		string jdkHome = LocateJDKHome();
		if (jdkHome.Length > 0)
			return Path.Combine(jdkHome, "bin");

		// Returning empty string in hope that we at least have the JDK added to the PATH.
		return "";
	}

	public static string LocateJDKHome()
	{
		if (s_JDKLocation != null)
			return s_JDKLocation;

		string javaHome = System.Environment.GetEnvironmentVariable("JAVA_HOME");
		if (IsValidJavaHome(javaHome))
			return (s_JDKLocation = javaHome);

		string programFiles = System.Environment.GetEnvironmentVariable("ProgramFiles");
		string programFilesx86 = System.Environment.GetEnvironmentVariable("ProgramFiles(x86)");

		string[] searchPatterns = new string[]
		{
			"/System/Library/Frameworks/JavaVM.framework/Versions/CurrentJDK/Home/",
			"/System/Library/Frameworks/JavaVM.framework/Versions/Current/Home/",
			"/System/Library/Frameworks/JavaVM.framework/Versions/*/Home/",
			programFiles + @"\Java*\jdk*\",
			programFilesx86 + @"\Java*\jdk*\",
			programFiles + @"\Java*\jre*\",
			programFilesx86 + @"\Java*\jre*\",
		};

		foreach (string str in searchPatterns)
		{
			javaHome = FindAny(str);
			if (IsValidJavaHome(javaHome))
				return (s_JDKLocation = javaHome);
		}

		if (Application.platform == RuntimePlatform.WindowsEditor)
		{
			string[] javaHomeRegistryLocations = new string[]
			{
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Development Kit\1.6",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Development Kit\1.6",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Development Kit\1.7",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Development Kit\1.7",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Development Kit\1.8",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Development Kit\1.8",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Runtime Environment\1.6",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Runtime Environment\1.6",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Runtime Environment\1.7",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Runtime Environment\1.7",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\JavaSoft\Java Runtime Environment\1.8",
				@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\JavaSoft\Java Runtime Environment\1.8"
			};

			foreach (string javaHomeRegistryLocation in javaHomeRegistryLocations)
			{
				javaHome = Microsoft.Win32.Registry.GetValue(javaHomeRegistryLocation, "JavaHome", "").ToString();
				if (IsValidJavaHome(javaHome))
					return (s_JDKLocation = javaHome);
			}
		}

		// Is there anywhere else this is stored? Returning empty string in hope that we at least have the JDK added to the PATH.
		bool win = Application.platform == RuntimePlatform.WindowsEditor;
		if (AndroidSDKTools.VerifyJava(win ? "java.exe" : "java"))
			return (s_JDKLocation = "");

		string title = "Unable to find suitable jdk installation.";
		string message = "Please make sure you have a suitable jdk installation.";
		message += " Android development requires at least JDK 6 (1.6).";
		message += " The latest JDK can be obtained from the Oracle ";
		message += "\nhttp://www.oracle.com/technetwork/java/javase/downloads/index.html";
		EditorUtility.DisplayDialog (title, message, "Ok");

		throw new UnityException(title + " " + message);
	}

	private static bool IsValidJavaHome(string javaHome)
	{
		if (javaHome == null || javaHome.Length == 0)
			return false;

		bool win = Application.platform == RuntimePlatform.WindowsEditor;
		string javaBin = Path.Combine(javaHome == null ? "" : javaHome, "bin");
		string javaExe = Path.Combine(javaBin, win ? "java.exe" : "java");
		return File.Exists(javaExe)	&& AndroidSDKTools.VerifyJava(javaExe);
	}

	public static string FindAny(string searchPattern)
	{
		List<string> result = new List<string>();
		return Find(searchPattern, result, true) ? result[0] : null;
	}

	public static string[] Find(string searchPattern)
	{
		List<string> result = new List<string>();
		Find(searchPattern, result, false);
		return result.ToArray();
	}

	public static bool Find(string searchPattern, List<string> result)
	{
		return Find(searchPattern, result, false);
	}

	public static bool Find(string searchPattern, List<string> result, bool findFirst)
	{
		return Find(searchPattern, result, findFirst, 256);
	}

	public static bool Find(string searchPattern, List<string> result, bool findFirst, int maxdepth)
	{
		if (maxdepth < 0)
			return false;

		char[] dir_separators = { '/', '\\' };
		char[] wildcards = { '*', '?' };
		int firstWildcard = searchPattern.IndexOfAny(wildcards);

		if (firstWildcard >= 0)
		{
			int remainingPathIndex = searchPattern.IndexOfAny(dir_separators, firstWildcard);
			if (remainingPathIndex == -1) remainingPathIndex = searchPattern.Length;

			string remainingPath	= searchPattern.Substring(remainingPathIndex);
			string pathWithWildcard	= searchPattern.Substring(0, remainingPathIndex);
			string resolvedPath		= Path.GetDirectoryName(pathWithWildcard);
			if ("" == resolvedPath) // if searchPattern starts with wildcard
				resolvedPath = Directory.GetCurrentDirectory();

			DirectoryInfo resolvedDirectory = new DirectoryInfo(resolvedPath);
			if (!resolvedDirectory.Exists)
				return false;

			string pattern = Path.GetFileName(pathWithWildcard);
			foreach (var match in resolvedDirectory.GetFileSystemInfos(pattern))
				if (Find(match.FullName + remainingPath, result, findFirst, maxdepth - 1) && findFirst)
					return true;
		}
		else if (File.Exists(searchPattern) || Directory.Exists(searchPattern))
			result.Add(searchPattern);

		return result.Count > 0;
	}
}
}