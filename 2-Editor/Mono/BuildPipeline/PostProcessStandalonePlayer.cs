using System;
using System.IO;
using System.Collections.Generic;
using UnityEditor;

internal class PostProcessStandalonePlayer
{
	public static string subDir32Bit
	{
		get
		{
			return "x86";
		}
	}

	public static string subDir64Bit
	{
		get
		{
			return "x86_64";
		}
	}

	public static void PostProcessStandaloneOSXPlayer (BuildTarget target, BuildOptions options, string installPath, string stagingAreaData, string stagingArea, string playerPackage, string companyName, string productName)
	{
		string stagingAreaContents = stagingArea + "/UnityPlayer.app/Contents";
		
		PostprocessBuildPlayer.InstallStreamingAssets (stagingAreaData);

		// Build player directly in builds folder for debugging purposes
		if ((options & BuildOptions.InstallInBuildFolder) != 0)
		{
			PostprocessBuildPlayer.PostprocessStandalonePlayerInBuildFolder (stagingAreaData);
			return;
		}

		FileUtil.CopyDirectoryRecursive (Path.Combine (playerPackage, "UnityPlayer.app"), Path.Combine (stagingArea, "UnityPlayer.app"));

		// Replace bundle identifier and bundle name.
		string identifierName = PostprocessBuildPlayer.GenerateBundleIdentifier (companyName, productName);

		FileUtil.ReplaceText (stagingAreaContents + "/Info.plist", "UNITY_BUNDLE_IDENTIFIER", identifierName, ">UnityPlayer<", ">" + productName + "<");

		// Strip arch
		if (BuildTarget.StandaloneOSXUniversal != target)
		{
#if INCLUDE_MONO_SGEN
			RemoveArch (stagingAreaContents + "/Frameworks/MonoEmbedRuntime/osx/libmonosgen-2.0.0.dylib", target);
#else
			RemoveArch (stagingAreaContents + "/Frameworks/MonoEmbedRuntime/osx/libmono.0.dylib", target);
#endif
			RemoveArch (stagingAreaContents + "/MacOS/UnityPlayer", target);
		}

		// Rename Executable
		File.Move (stagingAreaContents + "/MacOS/UnityPlayer", stagingAreaContents + "/MacOS/" + productName);

		// Move data folder
		FileUtil.MoveFileOrDirectory (stagingAreaData, stagingAreaContents + "/Data");

		// Install plugins	
		PostprocessBuildPlayer.InstallPlugins (stagingAreaContents + "/Plugins", target);

		// Move to final location
		FileUtil.DeleteFileOrDirectory (installPath);
		FileUtil.MoveFileOrDirectory (stagingArea + "/UnityPlayer.app", installPath);
	}
	
	public static void PostProcessStandaloneWindowsPlayer (BuildTarget target, BuildOptions options, string installPath, string stagingAreaData, string stagingArea, string playerPackage)
	{
		var files = new List<string> { "player_win.exe", "Data/Mono" };
#if INCLUDE_MONO_2_12
		files.Add ("Data/MonoBleedingEdge");
#endif
		if ((options & BuildOptions.Development) != 0)
			files.Add ("UnityPlayer_Symbols.pdb");
		if (target == BuildTarget.StandaloneGLESEmu)
		{
			Directory.CreateDirectory (stagingArea + "/gles2");
			Directory.CreateDirectory (stagingArea + "/gles3");
			files.AddRange (new List<string>
				{
					"gles2/libEGL.dll",
					"gles2/libgles_cl.dll",
					"gles2/libgles_cm.dll",
					"gles2/libGLESv2.dll",
					"gles3/libEGL.dll",
					"gles3/libGLESv2.dll",
					"gles3/libMaliT6xxSC.dll",
					"gles3/mali_essl_checker.exe"
				});
		}
		foreach (var file in files)
		{
			FileUtil.CopyFileOrDirectory (playerPackage + "/" + file, stagingArea + "/" + file);
		}

		// Copy the right built-in resources file
		Directory.CreateDirectory (stagingArea + "/Data/Resources");
		string resourcesFileName;
		if (target == BuildTarget.StandaloneGLESEmu)
			resourcesFileName = "ResourcesGLES/unity default resources";
		else
			resourcesFileName = "Resources/unity default resources";
		FileUtil.CopyFileOrDirectory (playerPackage + "/Data/" + resourcesFileName, stagingArea + "/Data/Resources/unity default resources");

		// Install plugins
		PostprocessBuildPlayer.InstallPlugins (stagingArea + "/Data/Plugins", target);

		PostprocessBuildPlayer.InstallStreamingAssets (stagingAreaData);

		// Build player directly in builds folder for debugging purposes
		if ((options & BuildOptions.InstallInBuildFolder) != 0)
		{
			PostprocessBuildPlayer.PostprocessStandalonePlayerInBuildFolder (stagingAreaData);
			return;
		}

		// Clear the final destination path.
		string installName = FileUtil.UnityGetFileNameWithoutExtension (installPath);
		string installDirectory = FileUtil.UnityGetDirectoryName (installPath);
		string dataFolderName = installName + "_Data";
		string installDataFolder = Path.Combine (installDirectory, dataFolderName);
		string pdbFileName = Path.Combine (installDirectory, "UnityPlayer_Symbols.pdb");
		FileUtil.DeleteFileOrDirectory (installPath);
		FileUtil.DeleteFileOrDirectory (installDataFolder);
		FileUtil.DeleteFileOrDirectory (pdbFileName);

		//and copy the new files in there.
		FileUtil.MoveFileOrDirectory (stagingArea + "/Data", installDataFolder);
		FileUtil.MoveFileOrDirectory (stagingArea + "/player_win.exe", installPath);
		if ((options & BuildOptions.Development) != 0)
			FileUtil.MoveFileOrDirectory (stagingArea + "/UnityPlayer_Symbols.pdb", pdbFileName);

		if (target == BuildTarget.StandaloneGLESEmu)
		{
			FileUtil.MoveFileOrDirectory (stagingArea + "/gles2", Path.Combine (installDirectory,"gles2"));
			FileUtil.MoveFileOrDirectory (stagingArea + "/gles3", Path.Combine (installDirectory, "gles3"));
		}
	}

	public static void PostProcessStandaloneLinuxPlayer (BuildTarget target, BuildOptions options, string installPath, string stagingAreaData, string stagingArea, string playerPackage)
	{
		// Copy needed files
		foreach (string path in new[] { "LinuxPlayer", "Data/Resources/unity default resources", "Data/Mono", "Data/Plugins" })
		{
			if ((Directory.Exists(Path.Combine(playerPackage, path))) || (File.Exists(Path.Combine(playerPackage, path))))
				CopyUsingRelativePath(playerPackage, stagingArea, path);
		}

		string arch = GetArchitectureForTarget (target);
		// Move mono plugin to platform-specific directory
		string platformSpecificMonoDir = Path.Combine (stagingArea,
		                                 Path.Combine ("Data/Mono", arch)
		                                 );
		FileUtil.CreateOrCleanDirectory (platformSpecificMonoDir);
		FileUtil.MoveFileOrDirectory (Path.Combine (stagingArea, "Data/Mono/libmono.so"),
		                              Path.Combine (platformSpecificMonoDir, "libmono.so"));

		// Install plugins if they exist (headless player does not bundle any plugins)
		string pluginDir = Path.Combine(Path.Combine(stagingArea, "Data/Plugins"), arch);
		if (Directory.Exists(pluginDir))
			PostprocessBuildPlayer.InstallPlugins(pluginDir, target);
		PostprocessBuildPlayer.InstallStreamingAssets (stagingAreaData);
/*
		// Build player directly in builds folder for debugging purposes
		if ((options & BuildOptions.InstallInBuildFolder) != 0)
		{
			PostprocessBuildPlayer.PostprocessStandalonePlayerInBuildFolder (stagingAreaData);
			return;
		}
*/
		// Clear the final destination path.
		string installName = FileUtil.UnityGetFileNameWithoutExtension (installPath);
		string installDirectory = FileUtil.UnityGetDirectoryName (installPath);
		string dataFolderName = installName + "_Data";
		string installDataFolder = Path.Combine (installDirectory, dataFolderName);
		FileUtil.DeleteFileOrDirectory (installPath);
		FileUtil.DeleteFileOrDirectory (installDataFolder);

		//and copy the new files in there.
		FileUtil.MoveFileOrDirectory (stagingArea + "/Data", installDataFolder);
		FileUtil.MoveFileOrDirectory (stagingArea + "/LinuxPlayer", installPath);
	}

	public static void PostProcessUniversalLinuxPlayer (BuildTarget target, BuildOptions options, string installPath, string stagingAreaData, string stagingArea, string playerPackage)
	{
        // Strip any extensions off the install path
        installPath = Path.Combine (Path.GetDirectoryName (installPath), Path.GetFileNameWithoutExtension (installPath));

//		// Copy needed files
		CopyUsingRelativePath (BuildPipeline.GetPlaybackEngineDirectory (BuildTarget.StandaloneLinux, options), stagingArea, "Data/Resources/unity default resources");
		FileUtil.CreateOrCleanDirectory (Path.Combine (stagingArea, "Data/Plugins"));

		foreach (BuildTarget arch in new[]{ BuildTarget.StandaloneLinux, BuildTarget.StandaloneLinux64 })
		{
			string archPlayerPackage = BuildPipeline.GetPlaybackEngineDirectory (arch, options);
			string archString = GetArchitectureForTarget (arch);
			FileUtil.CopyFileOrDirectory (Path.Combine (archPlayerPackage, "LinuxPlayer"), Path.Combine (stagingArea, "LinuxPlayer." + archString));

			string monoStagingDir = Path.Combine (stagingArea, Path.Combine ("Data/Mono", archString));
			FileUtil.CreateOrCleanDirectory (monoStagingDir);
			FileUtil.CopyFileOrDirectory (Path.Combine (archPlayerPackage, "Data/Mono/libmono.so"), Path.Combine (monoStagingDir, "libmono.so"));

			string pluginsDir = Path.Combine ("Data/Plugins", archString);
			CopyUsingRelativePath (archPlayerPackage, stagingArea, pluginsDir);
		}

		// Install plugins
		PostprocessBuildPlayer.InstallPlugins (stagingArea + "/Data/Plugins", target);
		PostprocessBuildPlayer.InstallStreamingAssets (stagingAreaData);
/*
		// Build player directly in builds folder for debugging purposes
		if ((options & BuildOptions.InstallInBuildFolder) != 0)
		{
			PostprocessBuildPlayer.PostprocessStandalonePlayerInBuildFolder (stagingAreaData);
			return;
		}
*/
		// Clear the final destination path.
		string installDirectory = FileUtil.UnityGetDirectoryName (installPath);
		string dataFolderName = FileUtil.UnityGetFileName (installPath) + "_Data";
		string installDataFolder = Path.Combine (installDirectory, dataFolderName);
        FileUtil.DeleteFileOrDirectory (installPath + "." + GetArchitectureForTarget (BuildTarget.StandaloneLinux));
        FileUtil.DeleteFileOrDirectory (installPath + "." + GetArchitectureForTarget (BuildTarget.StandaloneLinux64));
		FileUtil.DeleteFileOrDirectory (installDataFolder);

		//and copy the new files in there.
		FileUtil.MoveFileOrDirectory (stagingArea + "/Data", installDataFolder);
        FileUtil.MoveFileOrDirectory (stagingArea + "/LinuxPlayer." + GetArchitectureForTarget (BuildTarget.StandaloneLinux), installPath + "." + GetArchitectureForTarget (BuildTarget.StandaloneLinux));
        FileUtil.MoveFileOrDirectory (stagingArea + "/LinuxPlayer." + GetArchitectureForTarget (BuildTarget.StandaloneLinux64), installPath + "." + GetArchitectureForTarget (BuildTarget.StandaloneLinux64));
	}
	
	static void RemoveArch (string path, BuildTarget target)
	{
		Unsupported.StripFatMacho (path, BuildTarget.StandaloneOSXIntel64 == target);
	}
	
	static void CopyUsingRelativePath (string sourceRoot, string destinationRoot, string relativePath)
	{
		FileUtil.CopyFileOrDirectory (Path.Combine (sourceRoot, relativePath),
		                              Path.Combine (destinationRoot, relativePath));
	}
	
	internal static string GetArchitectureForTarget (BuildTarget target)
	{
		switch (target)
		{
		case BuildTarget.StandaloneLinux64:
		case BuildTarget.StandaloneWindows64:
			return "x86_64";
		case BuildTarget.StandaloneLinux:
		case BuildTarget.StandaloneLinuxUniversal:
		case BuildTarget.StandaloneOSXIntel:
		case BuildTarget.StandaloneWindows:
			return "x86";
		default:
			return string.Empty;
		}
	}
}
