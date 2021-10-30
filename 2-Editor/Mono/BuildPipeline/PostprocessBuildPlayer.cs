using System.Linq;
using UnityEngine;
using System.Collections.Generic;
using System.IO;
using System.Xml.XPath;
using UnityEditorInternal;
using System;
using System.Text.RegularExpressions;
using Mono.Cecil;
using UnityEditor.Modules;

namespace UnityEditor
{
internal class PostprocessBuildPlayer
	{
	
	internal const string StreamingAssets = "Assets/StreamingAssets";

	///@TODO: This should be moved to C++ since playerprefs has it there already
	internal static string GenerateBundleIdentifier(string companyName, string productName)
	{
		return "unity" + "." + companyName + "." + productName;
	}

	internal static void InstallPlugins (string destPluginFolder, BuildTarget target)
	{
		string basePluginSourceFolder = "Assets/Plugins";
		IBuildPostprocessor postprocessor = ModuleManager.GetBuildPostProcessor(target);

		if (postprocessor != null)
		{
			bool shouldRetainStructure;
			string[] files = postprocessor.FindPluginFilesToCopy(basePluginSourceFolder, out shouldRetainStructure);
			// If null - fallback to non-modular code.
			if (files != null)
			{
			if (files.Length > 0 && !Directory.Exists(destPluginFolder))
				Directory.CreateDirectory(destPluginFolder);

			foreach (var file in files)
			{
				if (Directory.Exists(file))
				{
					string targetPath = Path.Combine(destPluginFolder, file);
					FileUtil.CopyDirectoryRecursive(file, targetPath);
				}
				else
				{
					string fileName = Path.GetFileName(file);
					if (shouldRetainStructure)
					{
						string dirName = Path.GetDirectoryName(file.Substring(basePluginSourceFolder.Length + 1));
						string targetDir = Path.Combine(destPluginFolder, dirName);
						string targetPath = Path.Combine(targetDir, fileName);
						if (!Directory.Exists(targetDir))
							Directory.CreateDirectory(targetDir);
						FileUtil.UnityFileCopy(file, targetPath);
					}
					else
					{
						string targetPath = Path.Combine(destPluginFolder, fileName);
						FileUtil.UnityFileCopy(file, targetPath);
					}
				}
			}
			return;
		}
		}

		// -=[ Non-modular code below. Please refactor ]=-

		// Modularization: set true in module impl if retaining structure (will duplicate subdirs in output folder).
		// In some cases (i.e, universal builds for Linux) we will want to copy the plugins
		// to a sub-directory in the build-output -- for other cases (i.e, Android), we don't
		// want to do this
		bool duplicateSubDirsInOutput = false;

		// We build up a list of directories to look for plugins in dynamically, in
		// preparation for "Universal" 32-bit and 64-bit player builds, that will need
		// to contain both 32-bit and 64-bit versions of a plugin
		List<string> pluginSourceFolderSubDirectories = new List<string>();

		bool isOSXStandalone = (target == BuildTarget.StandaloneOSXIntel || 
		                        target == BuildTarget.StandaloneOSXIntel64 ||
		                        target == BuildTarget.StandaloneOSXUniversal);

		bool copyDirectories = isOSXStandalone;
		string extension = string.Empty;
		string debugExtension = string.Empty;

		if (isOSXStandalone)
		{
			extension = ".bundle";
			pluginSourceFolderSubDirectories.Add ("");
		}
		else if (target == BuildTarget.StandaloneWindows)
		{
			extension = ".dll";
			debugExtension = ".pdb";
			AddPluginSubdirIfExists (pluginSourceFolderSubDirectories, basePluginSourceFolder, PostProcessStandalonePlayer.subDir32Bit);
		}
		else if (target == BuildTarget.StandaloneWindows64)
		{
			extension = ".dll";
			debugExtension = ".pdb";
			AddPluginSubdirIfExists (pluginSourceFolderSubDirectories, basePluginSourceFolder, PostProcessStandalonePlayer.subDir64Bit);
		}
		else if (target == BuildTarget.StandaloneGLESEmu)
		{
			extension = ".dll";
			debugExtension = ".pdb";
			pluginSourceFolderSubDirectories.Add ("");
		}
		else if (target == BuildTarget.StandaloneLinux)
		{
			extension = ".so";
			AddPluginSubdirIfExists (pluginSourceFolderSubDirectories, basePluginSourceFolder, PostProcessStandalonePlayer.subDir32Bit);
		}
		else if (target == BuildTarget.StandaloneLinux64)
		{
			extension = ".so";
			AddPluginSubdirIfExists (pluginSourceFolderSubDirectories, basePluginSourceFolder, PostProcessStandalonePlayer.subDir64Bit);
		}
		else if (target == BuildTarget.StandaloneLinuxUniversal)
		{
			extension = ".so";
			pluginSourceFolderSubDirectories.Add (PostProcessStandalonePlayer.subDir32Bit);
			pluginSourceFolderSubDirectories.Add (PostProcessStandalonePlayer.subDir64Bit);
			duplicateSubDirsInOutput = true;
		}
		else if (target == BuildTarget.PS3)
		{
			extension = ".sprx";
			pluginSourceFolderSubDirectories.Add ("");
		}
		else if (target == BuildTarget.Android)
		{
			extension = ".so";
			pluginSourceFolderSubDirectories.Add ("Android");
		}
		else if( target == BuildTarget.BB10)
		{
			extension = ".so";
			pluginSourceFolderSubDirectories.Add("BlackBerry");
		}
		foreach (string directory in pluginSourceFolderSubDirectories)
		{
			if (duplicateSubDirsInOutput)
			{
				InstallPluginsByExtension(Path.Combine(basePluginSourceFolder, directory), extension, debugExtension,
				                          Path.Combine(destPluginFolder, directory), copyDirectories);
			}
			else
			{
				InstallPluginsByExtension(Path.Combine(basePluginSourceFolder, directory), extension, debugExtension,
				                          destPluginFolder, copyDirectories);
			}
		}
	}
	
	private static void AddPluginSubdirIfExists (List<string> subdirs, string basedir, string subdir)
	{
		if (Directory.Exists (Path.Combine (basedir, subdir)))
			subdirs.Add (subdir);
		else
			subdirs.Add (string.Empty);
	}
	
	internal static bool IsPlugin(string path, string targetExtension)
	{
		return string.Compare(Path.GetExtension(path), targetExtension, true) == 0 || string.Compare(Path.GetFileName(path), targetExtension, true) == 0;
	}

	internal static bool InstallPluginsByExtension(string pluginSourceFolder, string extension, string debugExtension, string destPluginFolder, bool copyDirectories)
	{
		bool installedPlugins = false;

		if (!Directory.Exists(pluginSourceFolder))
			return installedPlugins;

		string[] contents = Directory.GetFileSystemEntries(pluginSourceFolder);
		foreach (string path in contents)
		{
			string fileName = Path.GetFileName (path);
			string fileExtension = Path.GetExtension (path);

			bool filenameMatch =	fileExtension.Equals (extension, StringComparison.OrdinalIgnoreCase) ||
									fileName.Equals (extension, StringComparison.OrdinalIgnoreCase);
			bool debugMatch =		debugExtension != null && debugExtension.Length != 0 &&
								(	fileExtension.Equals (debugExtension, StringComparison.OrdinalIgnoreCase) ||
									fileName.Equals (debugExtension, StringComparison.OrdinalIgnoreCase)	);
			
			// Do we really need to check the file name here?
			if (filenameMatch || debugMatch)
			{
				if (!Directory.Exists (destPluginFolder))
					Directory.CreateDirectory (destPluginFolder);

				string targetPath = Path.Combine(destPluginFolder, fileName);
				if (copyDirectories)
					FileUtil.CopyDirectoryRecursive (path, targetPath);
				else if (!Directory.Exists (path))
					FileUtil.UnityFileCopy(path, targetPath);

				installedPlugins = true;
			}
		}
		return installedPlugins;
	}
	
	internal static void InstallStreamingAssets (string stagingAreaDataPath)
	{
		if (Directory.Exists (StreamingAssets))
			FileUtil.CopyDirectoryRecursiveForPostprocess (StreamingAssets, Path.Combine (stagingAreaDataPath, "StreamingAssets"), true);
	}

	static public string GetExtensionForBuildTarget (BuildTarget target)
	{
		IBuildPostprocessor postprocessor = ModuleManager.GetBuildPostProcessor(target);
		if (postprocessor != null)
		{
			return postprocessor.GetExtension();
		}

		switch (target)
		{
			case BuildTarget.StandaloneOSXIntel:
			case BuildTarget.StandaloneOSXIntel64:
			case BuildTarget.StandaloneOSXUniversal:
				return "app";
			case BuildTarget.WebPlayer:
			case BuildTarget.WebPlayerStreamed:
				return string.Empty;
			case BuildTarget.StandaloneGLESEmu:
			case BuildTarget.StandaloneWindows:
			case BuildTarget.StandaloneWindows64: 
				return "exe";
			case BuildTarget.StandaloneLinux:
			case BuildTarget.StandaloneLinux64:
			case BuildTarget.StandaloneLinuxUniversal:
				return PostProcessStandalonePlayer.GetArchitectureForTarget (target);
			case BuildTarget.Android:
				return "apk";
			case BuildTarget.FlashPlayer:
				return "swf";
			default:
				return string.Empty;
		}
	}

	static public bool SupportsInstallInBuildFolder(BuildTarget target)
	{
		IBuildPostprocessor postprocessor = ModuleManager.GetBuildPostProcessor(target);
		if (postprocessor != null)
		{
			return postprocessor.SupportsInstallInBuildFolder();
		}

		switch (target)
		{
			case BuildTarget.PS3:
			case BuildTarget.StandaloneWindows:
			case BuildTarget.StandaloneWindows64:
			case BuildTarget.StandaloneOSXIntel:
			case BuildTarget.StandaloneOSXIntel64:
			case BuildTarget.StandaloneOSXUniversal:
			case BuildTarget.Android:
			case BuildTarget.StandaloneGLESEmu:
			case BuildTarget.MetroPlayer:
#if INCLUDE_WP8SUPPORT
            case BuildTarget.WP8Player:
#endif
				return true;
			default:
				return false;
		}
	}
	
	static public void Launch (BuildTarget target, string path, string productName, BuildOptions options)
	{
			IBuildPostprocessor postprocessor = ModuleManager.GetBuildPostProcessor(target);
			if (postprocessor != null)
			{
				BuildLaunchPlayerArgs args;
				args.target = target;
				args.playerPackage = BuildPipeline.GetPlaybackEngineDirectory(target, options);
				args.installPath = path;
				args.productName = productName;
				args.options = options;
				postprocessor.LaunchPlayer(args);
			}
			else if (target == BuildTarget.NaCl)
			{
				PostProcessNaclPlayer.Launch(target, path);
			}
			else
				throw new UnityException (string.Format("Launching {0} build target via mono is not supported", target));
	}

	static public void Postprocess (BuildTarget target, string installPath, string companyName, string productName, 
									int width, int height, string downloadWebplayerUrl, string manualDownloadWebplayerUrl, BuildOptions options,
									RuntimeClassRegistry usedClassRegistry)
	{
		string stagingArea = "Temp/StagingArea";
		string stagingAreaData = "Temp/StagingArea/Data";
		string stagingAreaDataManaged = "Temp/StagingArea/Data/Managed"; stagingAreaDataManaged += "";
		string playerPackage = BuildPipeline.GetPlaybackEngineDirectory(target, options);
		
		// Disallow providing an empty string as the installPath
		bool willInstallInBuildFolder = (options & BuildOptions.InstallInBuildFolder) != 0 && SupportsInstallInBuildFolder(target);
		if (installPath == string.Empty && !willInstallInBuildFolder)
			throw new System.Exception (installPath + " must not be an empty string");
		
		IBuildPostprocessor postprocessor = ModuleManager.GetBuildPostProcessor(target);
		if (postprocessor != null) {
			BuildPostProcessArgs args;
			args.target = target;
			args.stagingAreaData = stagingAreaData;
			args.stagingArea = stagingArea;
			args.stagingAreaDataManaged = stagingAreaDataManaged;
			args.playerPackage = playerPackage;
			args.installPath = installPath;
			args.companyName = companyName;
			args.productName = productName;
			args.productGUID = PlayerSettings.productGUID;
			args.options = options;
			args.usedClassRegistry = usedClassRegistry;
			postprocessor.PostProcess(args);
			return;
		}
			
		switch (target)
		{
			case BuildTarget.StandaloneOSXIntel:
			case BuildTarget.StandaloneOSXIntel64:
			case BuildTarget.StandaloneOSXUniversal:
				PostProcessStandalonePlayer.PostProcessStandaloneOSXPlayer(target, options, installPath, stagingAreaData, stagingArea, playerPackage, companyName, productName);
				break;
			case BuildTarget.StandaloneWindows:
			case BuildTarget.StandaloneWindows64:
			case BuildTarget.StandaloneGLESEmu:
				PostProcessStandalonePlayer.PostProcessStandaloneWindowsPlayer(target, options, installPath, stagingAreaData, stagingArea, playerPackage);
				break;
			case BuildTarget.StandaloneLinuxUniversal:
				PostProcessStandalonePlayer.PostProcessUniversalLinuxPlayer(target, options, installPath, stagingAreaData, stagingArea, playerPackage);
				break;
			case BuildTarget.Android:
				Android.PostProcessAndroidPlayer.PostProcess (target, stagingAreaData, stagingArea,
				                          playerPackage, installPath, companyName,
				                          productName, options);
				break;
			case BuildTarget.WebPlayerStreamed:
			case BuildTarget.WebPlayer:
				PostProcessWebPlayer.PostProcess(options, installPath, downloadWebplayerUrl, width, height);
				break;
			//case BuildTarget.PS3:
			//	PostProcessPS3Player.PostProcess(target, options, installPath, stagingAreaData, stagingArea, playerPackage, stagingAreaDataManaged, usedClassRegistry);
			//	break;
			case BuildTarget.NaCl:
				PostProcessNaclPlayer.PostProcess(options, installPath, downloadWebplayerUrl, width, height);
				break;				
			default:
				throw new UnityException ("Build target not supported");
		}
	}

	internal static string ExecuteSystemProcess(string command, string args, string workingdir)
	{
		var psi = new System.Diagnostics.ProcessStartInfo()
		{
			FileName = command,
			Arguments = args,
			WorkingDirectory = workingdir,
			CreateNoWindow = true
		};
		var p = new UnityEditor.Utils.Program(psi);
		p.Start();
		while (!p.WaitForExit(100))
			;

		string output = p.GetStandardOutputAsString();
		p.Dispose();
		return output;
	}

	internal static void PostprocessStandalonePlayerInBuildFolder (string stagingAreaData)
	{
		string targetDirectory = null;
		if (Application.platform == RuntimePlatform.WindowsEditor)
			targetDirectory = Unsupported.GetBaseUnityDeveloperFolder() + "/build/WindowsStandalonePlayer/Data";
		else if (Application.platform == RuntimePlatform.OSXEditor)
			targetDirectory = Unsupported.GetBaseUnityDeveloperFolder() + "/build/MacStandalonePlayer/Data";
		else
			throw new System.Exception ("Unsupported");
		
		FileUtil.DeleteFileOrDirectory(targetDirectory);
		FileUtil.MoveFileOrDirectory(stagingAreaData, targetDirectory);
		
		//CopyDirectoryRecursive("", stagingAreaData + "/..");
	}
}
}
