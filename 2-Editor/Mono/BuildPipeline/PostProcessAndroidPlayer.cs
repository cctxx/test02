using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Xml;
using UnityEditor;
using UnityEditor.Utils;
using UnityEditorInternal;
using UnityEngine;

namespace UnityEditor.Android
{

internal class ProgressHelper
{
	internal float currentBuildStep = 0f;
	internal float numBuildSteps = 0f;

	public void Reset (float numSteps)
	{
		currentBuildStep = 0f;
		numBuildSteps = numSteps;
	}

	public float Advance ()
	{
		return ++currentBuildStep / numBuildSteps;
	}

	public float Get ()
	{
		return currentBuildStep / numBuildSteps;
	}

	public float LastValue ()
	{
		return (currentBuildStep - 1f) / numBuildSteps;
	}

	public void Show (string title, string message)
	{
		if (EditorUtility.DisplayCancelableProgressBar (title, message, Get ()))
			throw new ProcessAbortedException(title);
	}

	public void Step (string title, string message)
	{
		Advance ();
		Show(title, message);
	}
}

internal class ProcessAbortedException : Exception
{
	public ProcessAbortedException(string message) : base(message) {}
}

internal class PostProcessAndroidPlayer
{
	private const string AndroidPlugins = "Assets/Plugins/Android";
	const string AndroidXmlNamespace = "http://schemas.android.com/apk/res/android";
	private const string consoleMsg = " See the Console for details.";

	// http://docs.oracle.com/javase/specs/jls/se7/html/jls-3.html#jls-3.9
	private static readonly string[] ReservedJavaKeywords = new string[] {
		"abstract", "continue", "for", "new", "switch", "assert", "default",
		"if", "package", "synchronized", "boolean", "do", "goto", "private",
		"this", "break", "double", "implements", "protected", "throw", "byte",
		"else", "import", "public", "throws", "case", "enum", "instanceof",
		"return", "transient", "catch", "extends", "int", "short", "try",
		"char", "final", "interface", "static", "void", "class", "finally",
		"long", "strictfp", "volatile", "const", "float", "native", "super",
		"while", "null", "false", "true"
	};

	private AndroidSDKTools sdk_tools;
	private string javac_exe;

	private string android_jar;
	private int platform_api_level;
	private ProgressHelper progress = new ProgressHelper ();

	PostProcessAndroidPlayer ()
	{
		progress.Reset(1);
		sdk_tools = AndroidSDKTools.GetInstance();
		if (sdk_tools == null)
			ShowErrDlgAndThrow("Build failure!", "Unable to locate Android SDK");
		sdk_tools.DumpDiagnostics();

		EnsureSDKToolsVersion(21);
		EnsureSDKPlatformToolsVersion(16);
		platform_api_level = EnsureSDKPlatformAPI("Android 4.0", 14);

		javac_exe = "javac";
		android_jar = "android.jar";

		if (Application.platform == RuntimePlatform.WindowsEditor)
			javac_exe += ".exe";

		var platform_root = GetAndroidPlatformPath(platform_api_level);
		if (platform_root.Length == 0)
		{
			// reset SDK setting since it is incompatible
			EditorPrefs.SetString("AndroidSdkRoot", "");
			string msg = "Android SDK does not include any platforms! "
			           + "Did you run Android SDK setup to install the platform(s)?\n"
			           + "Minimum platform required for build is Android 4.0 (API level 14)\n";
			ShowErrDlgAndThrow ("No platforms found", msg);
		}

		javac_exe = Path.Combine (AndroidFileLocator.LocateJDKbin(), javac_exe);
		android_jar = Path.Combine (platform_root, android_jar);

		// TODO: verify command paths
	}

	private int EnsureSDKToolsVersion(int minToolsMajorVersion)
	{
		string toolsVersion = sdk_tools.ToolsVersion(new Progress("Android SDK Detection", "Detecting current tools version", progress.Get()).Show);
		int toolsMajorVersion = int.Parse(Regex.Match(toolsVersion, @"\d+").Value);
		while (toolsMajorVersion < minToolsMajorVersion)
		{
			string title = "Android SDK is outdated";
			string message = string.Format("SDK Tools version {0} < {1}", toolsVersion, minToolsMajorVersion);
			if (!InternalEditorUtility.inBatchMode)
			{
				switch (AskUpdateSdk(title, message))
				{
					case 1: throw new UnityException(message);
					case 2: return toolsMajorVersion;
				}
			}

			int numberOfRetries = 16;
			while (toolsMajorVersion < minToolsMajorVersion && 0 < numberOfRetries--)
			{
				sdk_tools.UpdateSDK(new Progress("Updating Android SDK", "Updating Android SDK - Tools and Platform Tools...", progress.Get()).Show);
				toolsVersion = sdk_tools.ToolsVersion(new Progress("Android SDK Detection", "Detecting current tools version", progress.Get()).Show);
				toolsMajorVersion = int.Parse(Regex.Match(toolsVersion, @"\d+").Value);
			}
		}
		return toolsMajorVersion;
	}

	private int EnsureSDKPlatformToolsVersion(int minToolsMajorVersion)
	{
		string toolsVersion = sdk_tools.PlatformToolsVersion(new Progress("Android SDK Detection", "Detecting current platform tools version", progress.Get()).Show);
		int toolsMajorVersion = int.Parse(Regex.Match(toolsVersion, @"\d+").Value);
		while (toolsMajorVersion < minToolsMajorVersion)
		{
			string title = "Android SDK is outdated";
			string message = string.Format("SDK Platform Tools version {0} < {1}", toolsVersion, minToolsMajorVersion);
			if (!InternalEditorUtility.inBatchMode)
			{
				switch (AskUpdateSdk(title, message))
				{
					case 1: throw new UnityException(message);
					case 2: return toolsMajorVersion;
				}
			}

			int numberOfRetries = 16;
			while (toolsMajorVersion < minToolsMajorVersion && 0 < numberOfRetries--)
			{
				sdk_tools.UpdateSDK(new Progress("Updating Android SDK", "Updating Android SDK - Tools and Platform Tools...", progress.Get()).Show);
				toolsVersion = sdk_tools.PlatformToolsVersion(new Progress("Android SDK Detection", "Detecting current pltform tools version", progress.Get()).Show);
				toolsMajorVersion = int.Parse(Regex.Match(toolsVersion, @"\d+").Value);
			}
		}
		return toolsMajorVersion;
	}

	private int EnsureSDKPlatformAPI(string minPlatformName, int minPlatformApiLevel)
	{
		int	platformApiLevel = GetAndroidPlatform();
		while (platformApiLevel < minPlatformApiLevel)
		{
			string title = "Android SDK is missing required platform api";
			string message = string.Format("Minimum platform required is {0} (API level {1})", minPlatformName, minPlatformApiLevel);
			if (!InternalEditorUtility.inBatchMode)
			{
				switch (AskUpdateSdk(title, message))
				{
					case 1: throw new UnityException(message);
					case 2: return platformApiLevel;
				}
			}

			int numberOfRetries = 16;
			while (platformApiLevel < minPlatformApiLevel && 0 < numberOfRetries--)
			{
				sdk_tools.InstallPlatform(minPlatformApiLevel, new Progress("Updating Android SDK", string.Format("Updating Android SDK - {0} (API level {1})...", minPlatformName, minPlatformApiLevel), progress.Get()).Show);
				platformApiLevel = GetAndroidPlatform();
			}
		}
		return platformApiLevel;
	}

	private static int AskUpdateSdk(string title, string message)
	{
		return EditorUtility.DisplayDialogComplex(title, message, "Update SDK", "Cancel", "Continue");
	}

	static void CopyNativeLibs (BuildTarget target, string playerPackage, string stagingArea, string abi, string abi2)
	{
		EnsureLibraryIsAvailable(playerPackage, abi);

		// copy all unity libs
		Directory.CreateDirectory(stagingArea + "/libs");
		FileUtil.CopyFileOrDirectory (playerPackage + "/libs/" + abi, stagingArea + "/libs/" + abi);
		// copy generic plugin libs
		PostprocessBuildPlayer.InstallPlugins (stagingArea + "/libs/" + abi, target);
		// copy specialized plugin libs (armv7 if available, otherwise armv5)
		if (!PostprocessBuildPlayer.InstallPluginsByExtension (AndroidPlugins + "/libs/" + abi, ".so", string.Empty, stagingArea + "/libs/" + abi, false) && abi2 != null)
			PostprocessBuildPlayer.InstallPluginsByExtension (AndroidPlugins + "/libs/" + abi2, ".so", string.Empty, stagingArea + "/libs/" + abi, false);
		// copy "generic" gdbserver, and then armeabi-v7a gdbserver, if available
		if (!File.Exists(stagingArea + "/libs/" + abi + "/gdbserver"))
			if (!PostprocessBuildPlayer.InstallPluginsByExtension (AndroidPlugins, "gdbserver", string.Empty, stagingArea + "/libs/" + abi, false))
				PostprocessBuildPlayer.InstallPluginsByExtension (AndroidPlugins + "/libs/" + abi, "gdbserver", string.Empty, stagingArea + "/libs/" + abi, false);
	}

	internal static void EnsureLibraryIsAvailable(string playerPackage, string arch)
	{
		if (!File.Exists(playerPackage + "/libs/" + arch + "/libunity.so"))
			ShowErrDlgAndThrow ("Unable to package apk", "Missing unity library missing for the selected architecture '" + arch + "'!");
	}

	internal static void PostProcess(BuildTarget target, string stagingAreaData, string stagingArea,
										  string playerPackage, string installPath, string companyName,
										  string productName,
										  BuildOptions options)
	{
		try
		{
			new PostProcessAndroidPlayer().PostProcessInternal(target, stagingAreaData, stagingArea, playerPackage, installPath, companyName, productName, options);
		}
		catch (CommandInvokationFailure ex)
		{
			foreach (var error in ex.Errors)
				Debug.LogError(error + "\n");

			ShowErrDlgAndThrow("Build failure", ex.HighLevelMessage, ex);
		}
		catch (ProcessAbortedException ex)
		{
			Debug.LogWarning("Build process aborted (" + ex.Message + ")\n");
		}
	}

	private void PostProcessInternal(BuildTarget target, string stagingAreaData, string stagingArea,
								  	string playerPackage, string installPath, string companyName,
								  	string productName,
								  	BuildOptions options)
	{
		if (!ArePasswordsProvided())
			ShowErrDlgAndThrow ("Can not sign application",
			                    "Unable to sign application; please provide passwords!");

		bool devBuild = (options & BuildOptions.Development) != 0;
		bool autoRunPlayer = (options & BuildOptions.AutoRunPlayer) != 0;
		float numSteps = autoRunPlayer ? 21f : 14f;
		progress.Reset (numSteps);

		progress.Step("Android build", "Copying unity resources");
		FileUtil.CopyFileOrDirectory(playerPackage + "/Data/unity default resources",
									  stagingAreaData + "/unity default resources");

		string assetsData = stagingArea + "/assets/bin";
		Directory.CreateDirectory(assetsData);
		FileUtil.MoveFileOrDirectory(stagingAreaData, assetsData + "/Data");

		progress.Step("Android build", "Scanning for android libraries");

		// copy android libraries to ensure .meta fiels are stripped
		string targetLibrariesFolder = Path.Combine(stagingArea, "android-libraries");
		FileUtil.CreateOrCleanDirectory(targetLibrariesFolder);
		{
			AndroidLibraries tempAndroidLibraries = new AndroidLibraries();
			tempAndroidLibraries.FindAndAddLibraryProjects(Path.Combine(AndroidPlugins, "*"));
			foreach (string libraryPath in tempAndroidLibraries)
				FileUtil.CopyDirectoryRecursiveForPostprocess(libraryPath, Path.Combine(targetLibrariesFolder, Path.GetFileName(libraryPath)), true);
		}
		AndroidLibraries androidLibraries = new AndroidLibraries();
		androidLibraries.FindAndAddLibraryProjects(Path.Combine(targetLibrariesFolder, "*"));


		progress.Step("Android build", "Generating android manifest");
		var mainManifest = CopyMainManifest(Path.Combine(stagingArea, "AndroidManifest-main.xml"), playerPackage);
		InjectBundleAndSDKVersion(mainManifest);

		string manifest = MergeManifests(Path.Combine(stagingArea, "AndroidManifest.xml"), mainManifest, androidLibraries);
		string packageName = PatchManifest(manifest, assetsData, devBuild, progress.Advance());

		ThrowIfInvalid(packageName);

		progress.Step ("Creating staging area", "Setting up target contents directory...");
		Directory.CreateDirectory(stagingArea + "/bin");

		// Build player directly in builds folder for debugging purposes
		if ((options & BuildOptions.InstallInBuildFolder) != 0)
		{
			FileUtil.CopyDirectoryRecursive(stagingArea + "/assets", playerPackage + "/assets", true);
			return;
		}

		SplitLargeFiles(stagingArea + "/assets/bin/Data", "*.assets", 1 * 1024 * 1024);

		bool useObb = PlayerSettings.Android.useAPKExpansionFiles;
		if (useObb)
		{
			// Keep everything needed to run the first level in the .apk; the rest goes to the .obb
			// Also make sure to exclude any streaming assets, since they will be handled later
			const string regex = 	@"^(?"+
								 	@":" + @"Data/mainData" +
									@"|" + @"Data/sharedassets0\.assets(?:\.res[GS]?)?(?:\.split\w+)?" +
									@"|" + @"Data/unity default resources" +
									@"|" + @"Data/Resources/unity_builtin_extra" +
									@"|" + @"Data/PlayerConnectionConfigFile" +
									@"|" + @"Data/Managed/.+\.dll(?:\.mdb)?" +
									@"|" + @"Data/.+\.resS" +
									@")$";

			const string invalidFiles = @"^(?"+
										@":" + @"Data/sharedassets0\.assets\.resS" +
										@")$";

			// Remove files which are kept in the .apk
			FileUtil.CopyDirectoryRecursive(stagingArea + "/assets", stagingArea + "/assets.apk");
			foreach (string file in Directory.GetFiles(assetsData, "*", SearchOption.AllDirectories))
			{
				if (Regex.IsMatch(file.Substring(assetsData.Length+1).Replace(@"\",@"/"), invalidFiles))
					ShowErrDlgAndThrow("Failed creating apk expansion package",
						"Streaming resources are not allowed in the first scene when using the 'Split Application Binary' option.");

				if (!Regex.IsMatch(file.Substring(assetsData.Length+1).Replace(@"\",@"/"), regex))
					continue;
				FileUtil.DeleteFileOrDirectory(file);
			}

			string errorMsg = "Android Asset Packaging Tool failed." + consoleMsg;
			string[] progress_msg = { "Creating APK Expansion package", "Compiling all assets into one archive..." };
			string output = Exec(sdk_tools.AAPT, "package -v -f -F obb.ap_ -A assets", stagingArea, progress_msg, progress.Advance(), errorMsg);
			if (!output.Contains ("Found 0 custom asset files") &&
			   (!output.Contains("Done!") || !File.Exists(Path.Combine(stagingArea, "obb.ap_"))))
			{
				Debug.LogError ("Android Asset Packaging Tool failed: " + output);
				ShowErrDlgAndThrow ("AAPT Failed!", errorMsg);
			}

			// Restore /assets, and remove the files kept in the .obb
			Directory.Delete(stagingArea + "/assets", true);
			FileUtil.MoveFileOrDirectory(stagingArea + "/assets.apk", stagingArea + "/assets");
			foreach (string file in Directory.GetFiles(assetsData, "*", SearchOption.AllDirectories))
			{
				if (Regex.IsMatch(file.Substring(assetsData.Length+1).Replace(@"\",@"/"), regex))
					continue;
				FileUtil.DeleteFileOrDirectory(file);
			}
		}

		AndroidDevice device = null;
		if (autoRunPlayer)
			device = FindDevice(PlayerSettings.Android.targetDevice);

		// Copy resources to staging area
		FileUtil.CopyDirectoryRecursive(playerPackage + "/assets", stagingArea + "/assets", true);
		FileUtil.CopyDirectoryRecursive(playerPackage + "/res", stagingArea + "/res", true);

		// If available, also copy over plugin resources
		if (Directory.Exists(AndroidPlugins + "/res"))
			FileUtil.CopyDirectoryRecursiveForPostprocess(AndroidPlugins + "/res", stagingArea + "/res", true);

		// And assets
		if (Directory.Exists(AndroidPlugins + "/assets"))
			FileUtil.CopyDirectoryRecursiveForPostprocess(AndroidPlugins + "/assets", stagingArea + "/assets", true);

		var stringsXml = new AndroidXmlDocument(Path.Combine(stagingArea, "res/values/strings.xml"));
		productName = productName.Replace("'", "\\'");
		productName = productName.Replace("\"", "\\\"");
		PatchStringRes(stringsXml, "string", "app_name", productName);
		stringsXml.Save();

		int splash_mode = (int) (PlayerSettings.advancedLicense ? PlayerSettings.Android.splashScreenScale : AndroidSplashScreenScale.Center);
		bool hideStatusBar = PlayerSettings.statusBarHidden;
		bool devPlayer = (options & BuildOptions.Development) != 0;
		var settingsXml = new AndroidXmlDocument(Path.Combine(stagingArea, "assets/bin/Data/settings.xml"));
		PatchStringRes(settingsXml, "integer", "splash_mode", splash_mode.ToString());
		PatchStringRes(settingsXml, "bool", "hide_status_bar", hideStatusBar.ToString());
		PatchStringRes(settingsXml, "bool", "useObb", useObb.ToString());
		if (devPlayer)
			PatchStringRes(settingsXml, "bool", "development_player", devPlayer.ToString());
		settingsXml.Save();

		progress.Step("Rebuilding resources", "Updating resources with player settings...");
		ReplaceIconResources(stagingArea);
		CompileResources(stagingArea, packageName, androidLibraries);

		// Copy from build/AndroidPlayer folder to Temp staging area (Temp/StagingArea)
		bool jarPluginInRoot = PostprocessBuildPlayer.InstallPluginsByExtension(AndroidPlugins, ".jar", string.Empty, stagingArea + "/plugins", false);
		bool jarPluginInBin = PostprocessBuildPlayer.InstallPluginsByExtension(AndroidPlugins + "/bin", ".jar", string.Empty, stagingArea + "/plugins", false);
		bool jarPluginInLibs = PostprocessBuildPlayer.InstallPluginsByExtension(AndroidPlugins + "/libs", ".jar", string.Empty, stagingArea + "/plugins", false);

		bool jarPlugin = jarPluginInRoot || jarPluginInBin || jarPluginInLibs;
		if (!jarPlugin && androidLibraries.Count == 0) // TODO: check for jar files
			FileUtil.CopyFileOrDirectory(playerPackage + "/bin/classes.dex", stagingArea + "/bin/classes.dex");
		else
			BuildDex (playerPackage, stagingArea, androidLibraries);

		// copy native libs
		switch (PlayerSettings.Android.targetDevice)
		{
			case AndroidTargetDevice.ARMv7:
				CopyNativeLibs (target, playerPackage, stagingArea, "armeabi-v7a", "armeabi");
				break;
		/*	case AndroidTargetDevice.x86:
				CopyNativeLibs (target, playerPackage, stagingArea, "x86");
				break;*/
		}

		// move streaming assets to raw/ folder to disable zip deflate method
		{
			var src = assetsData + "/Data";
			var dst = stagingArea + "/raw/bin/Data";
            var ext = ".resS";
			var contents = Directory.GetFileSystemEntries(src);
			foreach (var path in contents)
			{
				if (string.Compare(Path.GetExtension(path), ext, true) != 0) continue;
				if (!Directory.Exists(dst))
					Directory.CreateDirectory(dst);

				var targetPath = Path.Combine(dst, Path.GetFileName(path));
				File.Move(path, targetPath);
			}
		}

		// Copy streaming/raw assets
		if (Directory.Exists(PostprocessBuildPlayer.StreamingAssets))
		{
			FileUtil.CopyDirectoryRecursiveForPostprocess(PostprocessBuildPlayer.StreamingAssets, stagingArea + "/raw", true);
		}

		// Zip /raw assets with 'store' method
		if (Directory.Exists(stagingArea + "/raw"))
		{
			string errorMsg = "Android Asset Packaging Tool failed." + consoleMsg;
			string[] progress_msg = { "Creating streaming package", "Compiling all streaming assets into one archive..." };
			string output = Exec(sdk_tools.AAPT, "package -v -f -F raw.ap_ -A raw -0 \"\"", stagingArea, progress_msg, progress.Advance(), errorMsg);
			if (!output.Contains ("Found 0 custom asset files"))
				if (!output.Contains ("Done!") || !File.Exists (Path.Combine (stagingArea, "raw.ap_")))
				{
					Debug.LogError ("Android Asset Packaging Tool failed: " + output);
					ShowErrDlgAndThrow ("AAPT Failed!", errorMsg);
				}
		}

		if (useObb)
		{
			BuildObb (stagingArea);
			string obb = Path.Combine(stagingArea, "main.obb");
			if (File.Exists(obb))
				PatchStringRes(settingsXml, "bool", GetMD5HashOfEOCD(obb), true.ToString());

			settingsXml.Save();
		}

		{
			string errorMsg = "Android Asset Packaging Tool failed." + consoleMsg;
			string[] progress_msg = { "Creating asset package", "Compiling all assets into one archive..." };
			string output = Exec(sdk_tools.AAPT, "package -v -f -F assets.ap_ -A assets", stagingArea, progress_msg, progress.Advance(), errorMsg);
			if (!output.Contains("Done!") || !File.Exists(Path.Combine(stagingArea, "assets.ap_")))
			{
				Debug.LogError ("Android Asset Packaging Tool failed: " + output);
				ShowErrDlgAndThrow ("AAPT Failed!", errorMsg);
			}
		}

		if ((options & BuildOptions.AcceptExternalModificationsToPlayer) != 0)
		{
			AndroidProjectExport.Export(playerPackage,
				stagingArea,
				androidLibraries,
				installPath,
				packageName,
				productName,
				platform_api_level,
				useObb);
			return;
		}

		bool sign_using_keystore = PlayerSettings.Android.keyaliasName.Length != 0;
		BuildApk (stagingArea, sign_using_keystore, devPlayer, androidLibraries);

		AlignPackage (stagingArea);

		if (autoRunPlayer)
			UploadAndStartPlayer(manifest,
								 stagingArea, device, packageName,
								 devPlayer || Unsupported.IsDeveloperBuild(),
								 false);

		//and copy the new files in there.
		progress.Step ("Copying to output folder", "Moving final Android package...");

		File.Delete(installPath);
		if (File.Exists(installPath))
			ShowErrDlgAndThrow("Unable to delete old apk!", String.Format("Target apk could not be overwritten: {0}", installPath));

		FileUtil.MoveFileOrDirectory(Path.Combine(stagingArea, "Package.apk"), installPath);
		if (!File.Exists(installPath))
			ShowErrDlgAndThrow("Unable to create new apk!", String.Format("Unable to move file '{0}' -> '{1}", Path.Combine(stagingArea, "Package.apk"), installPath));

		if (useObb && File.Exists (Path.Combine (stagingArea, "main.obb")))
		{
			string apkName = Path.GetFileNameWithoutExtension(installPath);
			string obbName = Path.Combine(Path.GetDirectoryName(installPath), String.Format ("{0}.main.obb", apkName));
			FileUtil.DeleteFileOrDirectory(obbName);
			FileUtil.MoveFileOrDirectory(Path.Combine(stagingArea, "main.obb"), obbName);
		}
	}

	private void SplitLargeFiles(string path, string extension, int threshold)
	{
		const int bufferSize = 16 * 1024;
		Byte[] buffer = new Byte[bufferSize];
		foreach (string file in Directory.GetFiles(path, extension, SearchOption.AllDirectories))
		{
			FileInfo fi = new FileInfo(file);
			if (fi.Length < threshold)
				continue;
			System.IO.FileStream instream = File.Open(fi.FullName, FileMode.Open);
			for (long fileLeft = fi.Length, splitNr = 0; fileLeft > 0; fileLeft -= threshold, ++splitNr)
			{
				long splitSize = Math.Min(fileLeft, threshold);
				System.IO.FileStream outstream = File.Open(Path.Combine(fi.DirectoryName, fi.Name + ".split" + splitNr), FileMode.OpenOrCreate);
				for (long splitLeft = splitSize; splitLeft > 0; splitLeft -= bufferSize)
				{
					int readSize = (int)Math.Min(splitLeft, bufferSize);
					instream.Read(buffer, 0, readSize);
					outstream.Write(buffer, 0, readSize);
				}
				outstream.Close();
			}
			instream.Close();
			File.Delete(file);
		}
	}

	private bool ArePasswordsProvided()
	{
		if (PlayerSettings.Android.keyaliasName.Length == 0)
			return true;

		return PlayerSettings.Android.keystorePass.Length != 0
			   && PlayerSettings.Android.keyaliasPass.Length != 0;
	}

	private void CompileResources(string stagingArea, string packageName, AndroidLibraries androidLibraries)
	{
		string generatedSource = "gen";
		string generatedSourceDir = Directory.CreateDirectory(Path.Combine(stagingArea, generatedSource)).FullName;
		{
			// Rebuild resources (to fetch the changes in the manifest)
			string args = String.Format("package --auto-add-overlay -v -f -m -J {0} -M {1} -S \"{2}\" -I \"{3}\" -F {4}",
										generatedSource,
										AndroidManifest.AndroidManifestFile,
										"res",
										android_jar,
										"bin/resources.ap_");
			if (androidLibraries.Count > 0)
			{
				args += string.Format(" --extra-packages {0}", string.Join(":", androidLibraries.GetPackageNames()));
				foreach (var dir in androidLibraries.GetResourceDirectories())
					args += string.Format(" -S \"{0}\"", dir);
			}

			string errorMsg = "Failed to re-package resources." + consoleMsg;
			string[] progress_msg = {"Compiling resources", "Repackages resources."};
			string output = Exec(sdk_tools.AAPT, args, stagingArea, progress_msg, progress.LastValue(), errorMsg);
			if (!output.Contains("Done!") || !File.Exists(Path.Combine(stagingArea, "bin/resources.ap_")))
			{
				Debug.LogError ("Failed to re-package resources with the following parameters:\n" + args + "\n" + output);
				ShowErrDlgAndThrow ("Resource re-package failed!", errorMsg);
			}
		}
		if (androidLibraries.Count > 0)
		{
			if (!IsValidJavaPackageName(packageName))
				ShowErrDlgAndThrow("Resource compilation failed!", "Package name '" + packageName + "' is not a valid java package name. " + consoleMsg);

			List<string> javaFiles = new List<string>();
			foreach (var file in Directory.GetFiles(generatedSourceDir, "*.java", SearchOption.AllDirectories))
				javaFiles.Add(file.Substring(generatedSourceDir.Length + 1));

			string classesDir = Directory.CreateDirectory(Path.Combine(stagingArea, "bin/classes")).FullName;
			string args = string.Format("-bootclasspath \"{0}\" -d \"{1}\" -source 1.6 -target 1.6 -encoding ascii \"{2}\"",
			                            android_jar,
			                            classesDir,
			                            string.Join("\" \"", javaFiles.ToArray()));
			string errorMsg = "Failed to recompile android resource files." + consoleMsg;
			string[] progress_msg = {"Compiling resources", "Compiling android resource files."};
			string output = Exec(javac_exe, args, generatedSourceDir, progress_msg, progress.LastValue(), errorMsg);
			if (output.Trim().Length > 0)
			{
				Debug.LogError ("Failed to compile resources with the following parameters:\n" + args + "\n" + output);
				ShowErrDlgAndThrow ("Resource compilation failed!", errorMsg);
			}
		}
	}

	private void BuildDex (string playerPackage, string stagingArea, AndroidLibraries androidLibraries)
	{
		FileUtil.CopyFileOrDirectory(playerPackage + "/bin/classes.jar",
									  stagingArea + "/bin/classes.jar");

		// Building DEX files from classes.jar
		string[] progress_msg = {"Building DEX",
		                         "Converting java.class to dex-format..."};
		var command = new string[] {
			"dx", "--dex", "--verbose", "--output=bin/classes.dex", "bin/classes.jar"
		};
		if (Directory.Exists(Path.Combine(stagingArea, "bin/classes")))
			ArrayUtility.Add(ref command, "bin/classes");
		if (Directory.Exists(Path.Combine(stagingArea, "plugins")))
			ArrayUtility.Add(ref command, "plugins");
		foreach (var libPath in androidLibraries.GetLibraryDirectories())
			ArrayUtility.Add(ref command, libPath);
		foreach (var libPath in androidLibraries.GetCompiledJarFiles())
			ArrayUtility.Add(ref command, libPath);

		string output = SDKTool(command, stagingArea, progress_msg, progress.Advance (), "Unable to convert classes into dex format." + consoleMsg);
		if (!File.Exists(Path.Combine(stagingArea, "bin/classes.dex")))
		{
			string cd = Path.Combine(Environment.CurrentDirectory, stagingArea);
			string msg = "Failed to compile Java code to DEX:\n" + cd + "> " + string.Join(" ", command) + "\n" + output;
			Debug.LogError (msg);
			msg = "Failed to compile Java code to DEX." + consoleMsg;
			ShowErrDlgAndThrow ("Building DEX Failed!", msg);
		}
	}

	static void ShowErrDlgAndThrow (string title, string message)
	{
		ShowErrDlgAndThrow(title, message, new UnityException (title + "\n" + message));
	}

	static void ShowErrDlgAndThrow (string title, string message, Exception ex)
	{
		EditorUtility.DisplayDialog (title, message, "Ok");
		throw ex;
	}

	static void ThrowIfInvalid(string packageName)
	{
		if (!IsValidAndroidBundleIdentifier(packageName))
		{
			string message = "Please set the Bundle Identifier in the Player Settings.";
			message += " The value must follow the convention 'com.YourCompanyName.YourProductName'";
			message += " and can contain alphanumeric characters and underscore.";
			message += "\nEach segment must not start with a numeric character or underscore.";

			Selection.activeObject = Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings");
			ShowErrDlgAndThrow ("Bundle Identifier has not been set up correctly", message);
		}

		if(!IsValidJavaPackageName(packageName))
		{
			string message = "As of Unity 4.2 the restrictions on the Bundle Identifier has been updated ";
			message += " to include those for java package names. Specifically the";
			message += " restrictions have been updated regarding reserved java keywords.";
			message += "\n";
			message += "\nhttp://docs.oracle.com/javase/tutorial/java/package/namingpkgs.html";
			message += "\nhttp://docs.oracle.com/javase/specs/jls/se7/html/jls-3.html#jls-3.9";
			message += "\n";
			Debug.LogWarning(message);
		}
	}

	private AndroidDevice FindDevice(AndroidTargetDevice androidTargetDevice)
	{
		List<string> deviceIds = null;
		do
		{
			deviceIds = ADB.Devices(new Progress("Getting list of attached devices", "Trying locate a suitable Android device...", progress.Advance()).Show);
		}
		while (deviceIds.Count == 0 && EditorUtility.DisplayDialog("No Android device found!",
			" * Make sure USB debugging has been enabled\n" +
			" * Check your device, in most cases there should be a small icon in the status bar telling you if the USB connection is up.\n" +
			" * If you are sure that device is attached then it might be USB driver problem, for details please check Android SDK Setup section in Unity manual.",
			"Retry", "Cancel"));

		if (deviceIds.Count < 1)
		{
			string msg = string.Format ("No Android devices found.{0}\n",
										Application.platform == RuntimePlatform.WindowsEditor ?
										" If you are sure that device is attached then it might be USB driver problem," +
										" for details please check Android SDK Setup section in Unity devial." : "");
			ShowErrDlgAndThrow ("Couldn't find Android device", msg);
		}
		AndroidDevice device = new AndroidDevice(deviceIds[0]);
		//AndroidDevice device = AndroidDevice.First(new Progress("Getting list of attached devices", "Trying locate a suitable Android device...", progress.Advance()).Show);
		int sdk_version = Convert.ToInt32(device.Properties["ro.build.version.sdk"]);
		if (sdk_version < 7)
		{
			string message = "Device: " + device.Describe() + "\n";
			message += "The connected device is not running Android OS 2.0 or later.";
			message += " Unity Android does not support earlier versions of the Android OS;";
			message += " please upgrade your device to a later OS version.";

			Selection.activeObject = Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings");
			ShowErrDlgAndThrow ("Device software is not supported", message);
		}

		string apk_armeabi = androidTargetDevice == 0 ? "armeabi-v7a" : "armeabi";
		string cpu_armeabi = device.Properties["ro.product.cpu.abi"];
		if (androidTargetDevice == 0 && !apk_armeabi.Equals(cpu_armeabi))
		{
			string message = "Device: " + device.Describe() + "\n";
			message += "The connected device is not compatible with ARMv7a binaries.";
			Selection.activeObject = Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings");
			ShowErrDlgAndThrow ("Device hardware is not supported", message);
		}

		int opengles_version = 0;
		try {
			opengles_version = Convert.ToInt32(device.Properties["ro.opengles.version"]);
		} catch (KeyNotFoundException) {
			opengles_version = -1;
		}
		int glesMode = 0x00020000;
        switch(PlayerSettings.targetGlesGraphics)
        {
            case TargetGlesGraphics.OpenGLES_3_0:
                glesMode = 0x00030000;
                break;
            default:
                break;
        }

		int minSdkVersion = (int)PlayerSettings.Android.minSdkVersion;
	
		// Skip version check for GLES3 on pre-Android 4.3 devices
		if (!(PlayerSettings.targetGlesGraphics == TargetGlesGraphics.OpenGLES_3_0 && minSdkVersion < 18))
		{
			if (opengles_version >= 0 && opengles_version < glesMode)
			{
				string message = "The connected device is not compatible with the selcted OpenGLES version.";
				message += " Please select a lower OpenGLES version under Player Settings instead.";

				Selection.activeObject = Unsupported.GetSerializedAssetInterfaceSingleton("PlayerSettings");
				ShowErrDlgAndThrow("Device hardware is not supported", message);
			}
		}

		{
			string manufacturer = device.Properties["ro.product.manufacturer"];
			string model        = device.Properties["ro.product.model"];
			bool secure         = device.Properties["ro.secure"].Equals("1");

			string action = String.Format("{0}{1} {2}", Char.ToUpper(manufacturer[0]), manufacturer.Substring(1), model);
			string label  = String.Format("Android API-{0}", sdk_version);
			Analytics.Event("Android Device", action, label, secure ? 1 : 0);

			string gles = String.Format("gles {0}.{1}", opengles_version >> 16, opengles_version & 0xffff);
			if (opengles_version < 0)
				gles = "gles 2.0";
			Analytics.Event("Android Architecture", cpu_armeabi, gles, 1);

			string chipset		= device.Properties["ro.board.platform"];
			ulong  memtotal		= device.MemInfo["MemTotal"];
			memtotal = UpperboundPowerOf2(memtotal) / (1024 * 1024); // translate to MB
			Analytics.Event("Android Chipset", chipset, String.Format("{0}MB", memtotal), 1);
		}

		return device;
	}

	private static ulong UpperboundPowerOf2(ulong i)
	{
		i--;
		i |= i >> 1;
		i |= i >> 2;
		i |= i >> 4;
		i |= i >> 8;
		i |= i >> 16;
		i |= i >> 32;
		return i+1;
	}

	private void UploadAndStartPlayer(string manifestName,
									  string stagingArea, AndroidDevice device,
									  string packageName,
									  bool devPlayer, bool retryUpload)
	{
		string result = device.Install(Path.Combine(stagingArea, "Package.apk"), new Progress("Installing new player", "Pushing new content to device " + device.Describe(), progress.Advance()).Show);
		if (!retryUpload &&
			(result.Contains("[INSTALL_FAILED_UPDATE_INCOMPATIBLE]") ||
			 result.Contains("[INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES]")) )
		{
			Debug.LogWarning("Application update incompatible (signed with different keys?); removing previous installation (PlayerPrefs will be lost)...\n");
			device.Uninstall(packageName, new Progress("Uninstalling old player", "Removing " + packageName + " from device " + device.Describe(), progress.LastValue()).Show);

			UploadAndStartPlayer(manifestName, stagingArea, device, packageName, devPlayer, true);
			return;
		}

		if (result.Contains("protocol failure")
			|| result.Contains("No space left on device")
			|| result.Contains("[INSTALL_FAILED_INSUFFICIENT_STORAGE]")
			|| result.Contains("[INSTALL_FAILED_UPDATE_INCOMPATIBLE]")
			|| result.Contains("[INSTALL_FAILED_MEDIA_UNAVAILABLE]")
			|| result.Contains("[INSTALL_PARSE_FAILED_INCONSISTENT_CERTIFICATES]")
			|| result.Contains("Failure ["))
		{
			Debug.LogError ("Installation failed with the following output:\n" + result);
			ShowErrDlgAndThrow ("Unable to install APK!",
			                    "Installation failed." + consoleMsg);
		}

		device.SetProperty("debug.checkjni", devPlayer ? "1" : "0", new Progress("Setting up environment", "Setting device property CheckJNI to " + devPlayer, progress.LastValue()).Show);

		if (PlayerSettings.Android.useAPKExpansionFiles && File.Exists (Path.Combine (stagingArea, "main.obb")))
		{
			int versionCode = PlayerSettings.Android.bundleVersionCode;

			int sdk_version = Convert.ToInt32(device.Properties["ro.build.version.sdk"]);

			// 4.1 = /sdcard/Android/obb/<packageName>/main.<version>.<packageName>.obb
			// 4.2 = /mnt/shell/emulated/obb/<packageName>/main.<version>.<packageName>.obb

			string externalRootPre4_2  = "/sdcard/Android";
			string externalRootPost4_2 = "/mnt/shell/emulated";
			string externalRoot = sdk_version < 17 ? externalRootPre4_2 : externalRootPost4_2;
			string expPath = "obb";
			string obbName = String.Format("main.{0}.{1}.obb", versionCode.ToString(), packageName);
			string obbPath = String.Format("{0}/{1}/{2}/{3}", externalRoot, expPath, packageName, obbName);
			device.Push(Path.Combine(stagingArea, "main.obb"), obbPath, new Progress("Pushing expansion (.obb) file", "Copying APK Expansion file to device " + device.Describe(), progress.LastValue()).Show);
		}

		if (devPlayer)
			device.Forward("tcp:54999", "localabstract:Unity-" + packageName, new Progress("Setting up profiler tunnel", "Forwarding localhost:54999 to localabstract:Unity-" + packageName, progress.LastValue()).Show);

		string activityWithIntent = GetActivityWithLaunchIntent(manifestName);
		if (activityWithIntent.Length == 0)
			ShowErrDlgAndThrow ("Unable to start activity!",
			                    "No activity in the manifest with action MAIN and category LAUNCHER. Try launching the application manually on the device.");

		device.Launch(packageName, activityWithIntent, new Progress("Launching new player", "Attempting to start Unity Player on device " + device.Describe(), progress.Advance()).Show);
	}

	private void AlignPackage (string stagingArea)
	{
		var errorMsg = "Failed to align APK package." + consoleMsg;
		var progress_msg = new[] {"Aligning APK package","Optimizing target package alignment..." };
		var cd = Path.Combine (Environment.CurrentDirectory, stagingArea);
		var args = String.Format ("4 \"{0}/Package_unaligned.apk\" \"{0}/Package.apk\"", cd);
		var output = Exec(sdk_tools.ZIPALIGN, args, stagingArea, progress_msg, progress.Advance(), errorMsg);
		if (output.Contains ("zipalign") || output.Contains ("Warning") || !File.Exists (Path.Combine (cd, "Package.apk")))
		{
			Debug.LogError (output);
			ShowErrDlgAndThrow ("APK Aligning Failed!", errorMsg);
		}
	}

	private void BuildApk (string stagingArea, bool sign_using_keystore, bool debuggable, AndroidLibraries androidLibraries)
	{
		var progress_msg = new[]
		{
			"Creating APK package",
			"Building target package from assets archive and pre-built binaries..."
		};
		var cd = Path.Combine (System.Environment.CurrentDirectory, stagingArea);
		var command = new string[] {
			"apk",	String.Format("{0}/Package_unaligned.apk", cd),
			"-z",	String.Format("{0}/assets.ap_", cd),
			"-z",	String.Format("{0}/bin/resources.ap_", cd),
			"-nf",	String.Format("{0}/libs", cd),
			"-f",	String.Format("{0}/bin/classes.dex", cd),
			"-v"
		};
		foreach (var libs in androidLibraries.GetLibraryDirectories())
			command = command.Concat(new string[] { "-nf", libs }).ToArray();

		if (sign_using_keystore)
		{
			string keystore = Path.IsPathRooted(PlayerSettings.Android.keystoreName)
				? PlayerSettings.Android.keystoreName
				: Path.Combine(Directory.GetCurrentDirectory(), PlayerSettings.Android.keystoreName);
			command = command.Concat(new string[] {
				"-k",	keystore,
				"-kp",	PlayerSettings.Android.keystorePass,
				"-kk",	PlayerSettings.Android.keyaliasName,
				"-kkp",	PlayerSettings.Android.keyaliasPass
			}).ToArray();
		}

		if (debuggable)
			command = command.Concat(new string[] { "-d" }).ToArray();

		if (File.Exists (Path.Combine (stagingArea, "raw.ap_")) && !PlayerSettings.Android.useAPKExpansionFiles)
			command = command.Concat(new string[] {
				"-z",	String.Format("{0}/raw.ap_", cd)
			}).ToArray();

		var output = SDKTool (command, stagingArea, progress_msg, progress.Advance(), "Failed to build apk." + consoleMsg);

		var package_file = Path.Combine (stagingArea, "Package_unaligned.apk");
		var fi = new FileInfo (package_file);

		if (!File.Exists (package_file) || fi.Length == 0)
		{
			Debug.LogError (output);
			ShowErrDlgAndThrow ("APK Builder Failed!",
			                    "Failed to build APK package." + consoleMsg);
		}
	}

	private void BuildObb (string stagingArea)
	{
		bool includeObb = File.Exists(Path.Combine (stagingArea, "obb.ap_"));
		bool includeRaw = File.Exists(Path.Combine (stagingArea, "raw.ap_"));
		if (!includeObb && !includeRaw)
			return;
		var progress_msg = new[]
		{
			"Creating OBB package",
			"Building APK Expansion package from assets archive..."
		};
		var cd = Path.Combine (System.Environment.CurrentDirectory, stagingArea);
		string[] command = new string[] {
			"apk", String.Format("{0}/main.obb", cd), "-u"
		};

		if (includeObb)
			command = command.Concat(new string[] {"-z", String.Format("{0}/obb.ap_", cd)}).ToArray();

		if (includeRaw)
			command = command.Concat(new string[] {"-z", String.Format("{0}/raw.ap_", cd)}).ToArray();

		var output = SDKTool(command, stagingArea, progress_msg, progress.Advance(), "Failed to build OBB." + consoleMsg);

		var package_file = Path.Combine (stagingArea, "main.obb");
		var fi = new FileInfo (package_file);

		if (!File.Exists (package_file) || fi.Length == 0)
		{
			Debug.LogError (output);
			ShowErrDlgAndThrow ("OBB Builder Failed!",
			                    "Failed to build OBB package." + consoleMsg);
		}
		if (fi.Length >= (long)2 * 1024 * 1024 * 1024)
		{
			Debug.LogError (output);
			ShowErrDlgAndThrow ("OBB Builder Failed!",
			                    "OBB file too big for Android Market (max 2GB).\n" + consoleMsg);
		}
	}
	static string GetMD5HashOfEOCD(string fileName)
	{
		long maxEOCDsize = 22 + 64*1024;
		FileStream file = new FileStream(fileName, FileMode.Open);
		file.Seek(file.Length - Math.Min(file.Length, maxEOCDsize), SeekOrigin.Begin);
		MD5 md5 = new MD5CryptoServiceProvider();
		byte[] hash = md5.ComputeHash(file);
		file.Close();

		System.Text.StringBuilder sb = new System.Text.StringBuilder();
		for (int i = 0; i < hash.Length; i++)
			sb.Append(hash[i].ToString("x2"));

		return sb.ToString();
	}

	static void ReplaceIconResources (string stagingArea)
	{
		var replaceResource = new[]
		{
			"res.drawable-ldpi.app_icon.png",		"res/drawable-ldpi/app_icon.png",
			"res.drawable-mdpi.app_icon.png",		"res/drawable/app_icon.png",
			"res.drawable-hdpi.app_icon.png",		"res/drawable-hdpi/app_icon.png",
			"res.drawable-xhdpi.app_icon.png",		"res/drawable-xhdpi/app_icon.png",
			"res.drawable-xxhdpi.app_icon.png",		"res/drawable-xxhdpi/app_icon.png",
			"app_splash.png",						"assets/bin/Data/splash.png",
		};

		if (!PlayerSettings.advancedLicense)
			Array.Resize(ref replaceResource, replaceResource.Length-2);

		for (int i = 0; i < replaceResource.Length; i += 2)
		{
			var replaced_file = Path.Combine (stagingArea, replaceResource[i + 0]);
			var original_file = Path.Combine (stagingArea, replaceResource[i + 1]);

			if (!File.Exists (replaced_file))
				continue;

			FileUtil.DeleteFileOrDirectory (original_file);
			Directory.CreateDirectory (Path.GetDirectoryName (original_file));
			FileUtil.MoveFileOrDirectory (replaced_file, original_file);
		}
	}

	private int ExtractApiLevel (string platform)
	{
		var parts = platform.Split ('-');
		if (parts.Count () > 1)
		{
			try {
				return Convert.ToInt32(parts[1]);
			} catch (FormatException) { }
		}

		return -1;
	}

	private int GetAndroidPlatform ()
	{
		int platformApiLevel = -1;
		string[] target_ids = sdk_tools.ListTargetPlatforms(new Progress("Android SDK Detection", "Detecting installed platforms", progress.Get()).Show);
		foreach (string target_id in target_ids)
		{
			int nextPlatformApiLevel = ExtractApiLevel(target_id);
			if (nextPlatformApiLevel > platformApiLevel)
				if (GetAndroidPlatformPath(nextPlatformApiLevel) != null)
					platformApiLevel = nextPlatformApiLevel;
		}
		return platformApiLevel;
	}

	private string GetAndroidPlatformPath(int apiLevel)
	{
		string platform_path_format = "{0}/platforms/android-{1}";
		string path = String.Format(platform_path_format, sdk_tools.SDKRootDir, apiLevel);
		if (File.Exists(Path.Combine(path, "android.jar")))
			return path;
		return null;
	}

	static void CreateSupportsTextureElem (AndroidManifest manifestXML, AndroidBuildSubtarget subTarget)
	{
		// TODO: change it when smth apart tegra do support compressed srgb textures
		if( PlayerSettings.colorSpace == ColorSpace.Linear && subTarget != AndroidBuildSubtarget.DXT )
			Debug.LogWarning("Linear rendering works only on new Tegra devices");

		switch(subTarget)
		{
		case AndroidBuildSubtarget.Generic:
		case AndroidBuildSubtarget.ETC:
			manifestXML.AddSupportsGLTexture("GL_OES_compressed_ETC1_RGB8_texture");
			break;
		case AndroidBuildSubtarget.DXT:
			manifestXML.AddSupportsGLTexture("GL_EXT_texture_compression_dxt1");
			manifestXML.AddSupportsGLTexture("GL_EXT_texture_compression_dxt5");
			manifestXML.AddSupportsGLTexture("GL_EXT_texture_compression_s3tc");

			//we should ask nvidia/google about proper way of handling this
			/*
			if(PlayerSettings.colorSpace == ColorSpace.Linear)
				manifestXML.AddSupportsGLTexture("GL_NV_sRGB_formats"));
			*/

			break;
		case AndroidBuildSubtarget.PVRTC:
			manifestXML.AddSupportsGLTexture("GL_IMG_texture_compression_pvrtc");
			break;
		case AndroidBuildSubtarget.ATC:
			manifestXML.AddSupportsGLTexture("GL_AMD_compressed_ATC_texture");
			manifestXML.AddSupportsGLTexture("GL_ATI_texture_compression_atitc");
			break;
        case AndroidBuildSubtarget.ASTC:
            manifestXML.AddSupportsGLTexture("GL_KHR_texture_compression_astc_ldr");
            break;
		case AndroidBuildSubtarget.ETC2:
			// Nothing to do here, Khronos does not specify an extension for ETC2, GLES3.0 mandates support.
			break; 

		default:
			Debug.LogWarning("SubTarget not recognized : " + subTarget);
			break;
		}
	}

	static string PreferredInstallLocationAsString ()
	{
		switch (PlayerSettings.Android.preferredInstallLocation)
		{
			case AndroidPreferredInstallLocation.Auto: return "auto";
			case AndroidPreferredInstallLocation.PreferExternal: return "preferExternal";
			case AndroidPreferredInstallLocation.ForceInternal: return "internalOnly";
		}

		return "preferExternal";
	}

	static string GetOrientationAttr()
	{
		var orientation = PlayerSettings.defaultInterfaceOrientation;
		string orientationAttr = null;

		var autoPortrait = PlayerSettings.allowedAutorotateToPortrait || PlayerSettings.allowedAutorotateToPortraitUpsideDown;
		var autoLandscape = PlayerSettings.allowedAutorotateToLandscapeLeft || PlayerSettings.allowedAutorotateToLandscapeRight;

		if (orientation == UIOrientation.Portrait)
			orientationAttr = "portrait";
		else if (orientation == UIOrientation.PortraitUpsideDown)
			orientationAttr = "reversePortrait";
		else if (orientation == UIOrientation.LandscapeRight)
			orientationAttr = "reverseLandscape";
		else if (orientation == UIOrientation.LandscapeLeft)
			orientationAttr = "landscape";
		else if (autoPortrait && autoLandscape)
			orientationAttr = "fullSensor";
		else if (autoPortrait)
			orientationAttr = "sensorPortrait";
		else if (autoLandscape)
			orientationAttr = "sensorLandscape";
		else
			orientationAttr = "unspecified";

		return orientationAttr;
	}

	static void SetPermissionAttributes (AndroidManifest manifestXML,
	                                     bool development,
	                                     AssemblyReferenceChecker checker)
	{
		bool needPhoneState = false;
		bool needInternetPermission = true;

		bool forceInternetPermission =
			PlayerSettings.Android.forceInternetPermission ? true : false;
		bool forceSDCardPermission =
			PlayerSettings.Android.forceSDCardPermission ? true : false;

		// If user asked to force the permission, don't bother checking whether
		// we use network classes
		if (!forceInternetPermission)
			needInternetPermission = doesReferenceNetworkClasses (checker);

		if (checker.HasReferenceToMethod ("UnityEngine.SystemInfo::get_deviceUniqueIdentifier") ||
			checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::get_uniqueIdentifier"))
			needPhoneState = needInternetPermission = true;

		bool needOBB = PlayerSettings.Android.useAPKExpansionFiles;
		bool needNetworkState = checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::get_internetReachability") ||
								checker.HasReferenceToMethod ("UnityEngine.Application::get_internetReachability");

		needNetworkState |= needOBB;
		needInternetPermission |= needOBB;
		forceSDCardPermission |= needOBB;

		// Add internet permission if it's necessary
		if (needInternetPermission || development)
			manifestXML.AddUsesPermission("android.permission.INTERNET");

		if (checker.HasReferenceToMethod ("UnityEngine.Handheld::Vibrate"))
			manifestXML.AddUsesPermission("android.permission.VIBRATE");

		if (needPhoneState)
			manifestXML.AddUsesPermission("android.permission.READ_PHONE_STATE");

		if (needNetworkState)
			manifestXML.AddUsesPermission("android.permission.ACCESS_NETWORK_STATE");

		if (needOBB)
			manifestXML.AddUsesPermission("android.permission.ACCESS_WIFI_STATE");

		if (needOBB)
			manifestXML.AddUsesPermission("com.android.vending.CHECK_LICENSE");

		if (checker.HasReferenceToMethod ("UnityEngine.Input::get_location")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_lastLocation")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::get_locationServiceStatus")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::get_locationServiceEnabledByUser")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::StartLocationServiceUpdates")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::StopLocationServiceUpdates"))
		{
			manifestXML.AddUsesPermission("android.permission.ACCESS_FINE_LOCATION");
			manifestXML.AddUsesFeature("android.hardware.location.gps", false /*encourage gps, but don't require it*/);
		}

		if (checker.HasReferenceToType ("UnityEngine.WebCamTexture"))
		{
			manifestXML.AddUsesPermission("android.permission.CAMERA");
			// By default we don't require any camera since a WebCamTexture may not be a crucial part of the app.
			// We need to explicitly say so, since CAMERA otherwise implicitly marks camera and autofocus as required.
			manifestXML.AddUsesFeature("android.hardware.camera", false);
			manifestXML.AddUsesFeature("android.hardware.camera.autofocus", false);
			manifestXML.AddUsesFeature("android.hardware.camera.front", false);
		}

		if (checker.HasReferenceToType ("UnityEngine.Microphone"))
		{
			manifestXML.AddUsesPermission("android.permission.RECORD_AUDIO");
			manifestXML.AddUsesFeature("android.hardware.microphone", true);
		}

		// In dev build add WRITE_EXTERNAL_STORAGE permission, so that
		// automated tests could write stuff to sdcard.
		if (forceSDCardPermission || development)
			manifestXML.AddUsesPermission("android.permission.WRITE_EXTERNAL_STORAGE");

		if (checker.HasReferenceToMethod ("UnityEngine.Input::get_acceleration")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::GetAccelerationEvent")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::get_accelerationEvents")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::get_accelerationEventCount"))
			manifestXML.AddUsesFeature("android.hardware.sensor.accelerometer", true);

		if (checker.HasReferenceToMethod ("UnityEngine.Input::get_touches")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::GetTouch")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::get_touchCount")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::get_multiTouchEnabled")
		    || checker.HasReferenceToMethod ("UnityEngine.Input::set_multiTouchEnabled")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_touches")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::GetTouch")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_touchCount")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_multiTouchEnabled")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::set_multiTouchEnabled"))
		{
			manifestXML.AddUsesFeature("android.hardware.touchscreen", true);
			manifestXML.AddUsesFeature("android.hardware.touchscreen.multitouch", false);
			manifestXML.AddUsesFeature("android.hardware.touchscreen.multitouch.distinct", false);
		}

		// All these calls below are deprecated, but we still need to check and
		// add the permissions for them to function properly:
		if (checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_acceleration")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::GetAccelerationEvent")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_accelerationEvents")
		    || checker.HasReferenceToMethod ("UnityEngine.iPhoneInput::get_accelerationEventCount"))
			manifestXML.AddUsesFeature("android.hardware.sensor.accelerometer", true);

		if (checker.HasReferenceToMethod ("UnityEngine.iPhoneUtils::Vibrate"))
			manifestXML.AddUsesPermission("android.permission.VIBRATE");

		if (checker.HasReferenceToMethod ("UnityEngine.Screen::set_sleepTimeout")
			|| checker.HasReferenceToMethod ("UnityEngine.Handheld::PlayFullScreenMovie")
			|| checker.HasReferenceToMethod ("UnityEngine.iPhoneSettings::set_screenCanDarken"))
			manifestXML.AddUsesPermission("android.permission.WAKE_LOCK");
	}

	private string CopyMainManifest(string target, string playerPackage)
	{
		string mainManifest	= Path.Combine(AndroidPlugins, AndroidManifest.AndroidManifestFile);
		if (!File.Exists(mainManifest))
			mainManifest = Path.Combine(playerPackage, AndroidManifest.AndroidManifestFile);
		return new AndroidManifest(mainManifest).SaveAs(target);
	}

	private string MergeManifests(string targetManifest, string mainManifest, AndroidLibraries libraries)
	{
		string[] libraryManifests = libraries.GetManifestFiles();
		if (libraryManifests.Length > 0)
			sdk_tools.MergeManifests(targetManifest, mainManifest, libraryManifests,
				new Progress("Android Library Support", "Merging AndroidManifest.xml files", progress.Get()).Show);
		else
			FileUtil.CopyFileOrDirectory(mainManifest, targetManifest);
		return targetManifest;
	}

	private void InjectBundleAndSDKVersion(string manifest)
	{
		AndroidManifest manifestXML = new AndroidManifest(manifest);

		string version = PlayerSettings.bundleVersion;
		int versionCode = PlayerSettings.Android.bundleVersionCode;
		manifestXML.SetVersion(version, versionCode);

		// Update sdk version if not already set
		int minSdkVersion = (int) PlayerSettings.Android.minSdkVersion;
		int targetSdkVersion = minSdkVersion > platform_api_level ? minSdkVersion : platform_api_level;
		manifestXML.AddUsesSDK(minSdkVersion, targetSdkVersion);

		string packageName = PlayerSettings.bundleIdentifier;

		// If there's something valid in identifier supplied in editor, just go
		// ahead and use it. Otherwise, use whatever's in the manifest.
		if (IsValidAndroidBundleIdentifier(packageName))
			manifestXML.SetPackageName(packageName);
		manifestXML.Save();
	}

	private string PatchManifest(string manifest, 
								 string assetsData,
								 bool development,
								 float progress_value)
	{
		AndroidManifest manifestXML = new AndroidManifest(manifest);

		var loc = PreferredInstallLocationAsString ();
		manifestXML.SetInstallLocation(loc);

		manifestXML.SetDebuggable(development);

		var glesVersion = "0x00020000";
		// Add GLES 3.0 version requirement only if targeting API level 18 (Android 4.3) or later).
		int minSdkVersion = (int)PlayerSettings.Android.minSdkVersion;
		if (PlayerSettings.targetGlesGraphics == TargetGlesGraphics.OpenGLES_3_0 && minSdkVersion >= 18)
            glesVersion = "0x00030000";

		manifestXML.AddGLESVersion(glesVersion);

		if (EditorUserBuildSettings.androidBuildSubtarget != AndroidBuildSubtarget.Generic)
			CreateSupportsTextureElem (manifestXML, EditorUserBuildSettings.androidBuildSubtarget);

		// patch unity activities
		string[] activities = new string[]
		{
			"com.unity3d.player.UnityPlayerNativeActivity"
		};
		string orientationAttribute = GetOrientationAttr();
		bool activityPatched = false;
		foreach (string activity in activities)
			activityPatched = manifestXML.SetOrientation(activity, orientationAttribute) || activityPatched;
		if (!activityPatched)
			Debug.LogWarning(string.Format("Unable to find unity activity in manifest. You need to make sure orientation attribut is set to {0} manually.", orientationAttribute));

		AssemblyReferenceChecker checker = new AssemblyReferenceChecker ();
		bool collectMethods = true;
		bool ignoreSystemDlls = true;
		var stagingAreaDataManaged = assetsData + "/Data/Managed";
		checker.CollectReferences (stagingAreaDataManaged, collectMethods, progress_value, ignoreSystemDlls);
		SetPermissionAttributes (manifestXML, development, checker);
		StripUnityLibEntryForNativeActitivy(manifestXML);

		if (!PlayerSettings.advancedLicense)
		{
			var assemblyName = checker.WhoReferencesClass ("System.Net.Sockets", true);
			if (assemblyName != null)
			{
				var ex = "'System.Net.Sockets' are supported only with Unity Android Pro."
				         + " Referenced from assembly '" + assemblyName + "'.";
				throw new SystemException (ex);
			}
		}

		manifestXML.SaveAs(manifest);

		return manifestXML.GetPackageName();
	}

	static void PatchStringRes(AndroidXmlDocument doc, string tag, string attrib, string value)
	{
		XmlElement element = doc.GetElementWithAttribute(doc.DocumentElement, tag, "name", attrib);
		if (element == null)
		{
			element = (XmlElement)doc.DocumentElement.AppendChild(doc.CreateElement(tag));
			element.Attributes.Append(doc.CreateAttribute("", "name", "", attrib));
		}
		element.InnerText = value;
	}

	static bool IsValidBundleIdentifier(string domainish, string extraValidChars) {
		if (string.IsNullOrEmpty(domainish))
			return false;

		if (domainish == "com.Company.ProductName" || domainish == "com.unity3d.player")
			return false;

		string validCharacters = "a-zA-Z0-9" + extraValidChars;

		string regexStr = string.Format(@"^([{0}]+[.])*[{0}]+[.][{0}]+$", validCharacters);
		Regex re = new Regex(regexStr);
		return re.IsMatch(domainish);
	}

	static bool IsValidAndroidBundleIdentifier(string bundleIdentifier)
	{
		// should use ^[a-z]\w*(\.[a-z]\w*){2,}$ here instead..
		string extraValidChars = "_";
		bool illegalBundleIdent = false;
		foreach (string segment in bundleIdentifier.Split(new Char[] { '.' }))
			if (segment.Length < 1 || Char.IsDigit(segment[0]) || extraValidChars.IndexOf(segment[0]) != -1)
				illegalBundleIdent = true;

		return !illegalBundleIdent && IsValidBundleIdentifier(bundleIdentifier, extraValidChars);
	}

	// http://docs.oracle.com/javase/tutorial/java/package/namingpkgs.html
	// http://docs.oracle.com/javase/specs/jls/se7/html/jls-3.html#jls-3.9
	static bool IsValidJavaPackageName(string packageName)
	{
		if (!IsValidAndroidBundleIdentifier(packageName))
			return false;

		foreach (string segment in packageName.Split(new Char[] { '.' }))
			if (ReservedJavaKeywords.Contains(segment))
				return false;
		return true;
	}

	static void StripUnityLibEntryForNativeActitivy(XmlDocument manifestXML)
	{
		foreach (XmlElement element in manifestXML.GetElementsByTagName("meta-data"))
			if (element.GetAttribute("android:name") == "android.app.lib_name" && element.GetAttribute("android:value") == "unity")
				element.ParentNode.RemoveChild(element);
	}

	static string GetActivityWithLaunchIntent(string manifest)
	{
		var action = "android.intent.action.MAIN";
		var category = "android.intent.category.LAUNCHER";
		var doc = new AndroidManifest(manifest);
		foreach (XmlElement actionElement in doc.GetElementsByTagName ("action"))
		{
			if (actionElement.GetAttribute("android:name") != action)
				continue;
			XmlElement intentFilter = (XmlElement) actionElement.ParentNode;
			if ("intent-filter" != intentFilter.LocalName)
				continue;

			foreach (XmlElement categoryElement in intentFilter.GetElementsByTagName("category"))
			{
				if (categoryElement.GetAttribute("android:name") != category)
					continue;
				XmlElement activityElement = (XmlElement) intentFilter.ParentNode;
				if ("activity" != activityElement.LocalName)
					continue;

				return activityElement.GetAttribute("android:name");
			}
		}
		return "";
	}

	static bool doesReferenceNetworkClasses (AssemblyReferenceChecker checker)
	{
		return checker.HasReferenceToType ("System.Net.Sockets")
			|| checker.HasReferenceToType ("UnityEngine.Network")
			|| checker.HasReferenceToType ("UnityEngine.RPC")
			|| checker.HasReferenceToType ("UnityEngine.WWW");
	}

	private string SDKTool(string[] command, string workingdir, string[] progress_strings, float progress_value, string errorMsg)
	{
		return sdk_tools.RunCommand(command, workingdir, new Progress(progress_strings[0], progress_strings[1], progress_value).Show, errorMsg);
	}

	static string Exec(string command, string args, string workingdir, string[] progress_strings, float progress_value, string errorMsg)
	{
		var psi = new System.Diagnostics.ProcessStartInfo();
		psi.FileName = command;
		psi.Arguments = args;
		psi.WorkingDirectory = workingdir;
		psi.CreateNoWindow = true;
		return Command.Run(psi, new Progress(progress_strings[0], progress_strings[1], progress_value).Show, errorMsg);
	}

	private class Progress
	{
		private string m_Title;
		private string m_Message;
		private float  m_Value;

		public Progress(string title, string message, float val)
		{
			m_Title   = title;
			m_Message = message;
			m_Value   = val;
		}

		public void Show(Program program)
		{
			if (EditorUtility.DisplayCancelableProgressBar(m_Title, m_Message, m_Value))
				throw new ProcessAbortedException(m_Title);
		}
	}
}

} // namespace UnityEditor.Android
