using UnityEngine;

using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Text.RegularExpressions;

namespace UnityEditor.Android
{

// This class heavily relies on Tools/Android/SDKTools which is a java library
internal class AndroidSDKTools
{
	private static readonly Regex Warnings = new Regex("^Warning:(.*)$", RegexOptions.Multiline);

	// Error messages
	private const string ConsoleMessage			= "See the Console for more details. ";
	private const string KeystoreMessage		= "Please make sure the location and password of the keystore is correct. ";
	private const string SDKManagerMessage		= "Please run the SDK Manager manually to make sure you have the latest set of tools and the required platforms installed. ";

	private const string CreateKeystoreError	= "Unable to create keystore. Please make sure the location of the keystore is valid. " + ConsoleMessage;
	private const string ListKeystoreKeysError	= "Unable to list keys in the keystore. " + KeystoreMessage + ConsoleMessage;
	private const string CreateKeystoreKeyError	= "Unable to create key in keystore. " + KeystoreMessage + ConsoleMessage;
	private const string MergeManifestsError	= "Unable to merge android manifests. " + ConsoleMessage;
	private const string ListPlatformsError		= "Unable to list target platforms. Please make sure the android sdk path is correct. " + ConsoleMessage;
	private const string ToolsVersionError		= "Unable to determine the tools version of the Android SDK. " + SDKManagerMessage + ConsoleMessage;
	private const string UpdateSDKError			= "Unable to update the SDK. " + SDKManagerMessage + ConsoleMessage;
	private const string InstallPlatformError	= "Unable to install additional SDK platform. " + SDKManagerMessage + ConsoleMessage;
	private const string ResolvingBuildToolsDir = "Unable to resolve build tools directory. " + ConsoleMessage;

	private const string COMPATIBLE_KEY_IDENTIFIER = "[compatible]";
	private const int DEFAULT_JVM_MEMORY = 1024;

	public readonly string SDKRootDir;
	public readonly string SDKToolsDir;
	public readonly string SDKBuildToolsDir;
	public readonly string SDKPlatformToolsDir;

	public string ADB      { get { return PlatformToolsExe("adb"); } }
	public string AAPT     { get { return BuildToolsExe("aapt"); } }
	public string ZIPALIGN { get { return ToolsExe("zipalign"); } }

	AndroidSDKTools(string sdkRoot)
	{
		SDKRootDir          = sdkRoot;
		SDKToolsDir         = Path.Combine(SDKRootDir, "tools");
		SDKPlatformToolsDir = Path.Combine(SDKRootDir, "platform-tools");
		SDKBuildToolsDir    = RunCommand(new string[]{"build-tool-dir"}, null, ResolvingBuildToolsDir).Trim();
		if (SDKBuildToolsDir.Length == 0)
			SDKBuildToolsDir = SDKPlatformToolsDir;
	}

	public void DumpDiagnostics()
	{
		Console.WriteLine("AndroidSDKTools:");
		Console.WriteLine("");
		Console.WriteLine("\troot          : {0}", SDKRootDir);
		Console.WriteLine("\ttools         : {0}", SDKToolsDir);
		Console.WriteLine("\tplatform-tools: {0}", SDKPlatformToolsDir);
		Console.WriteLine("\tbuild-tools   : {0}", SDKBuildToolsDir);
		Console.WriteLine("");
		Console.WriteLine("\tadb      : {0}", ADB);
		Console.WriteLine("\taapt     : {0}", AAPT);
		Console.WriteLine("\tzipalign : {0}", ZIPALIGN);
		Console.WriteLine("\tjava     : {0}", Path.Combine(AndroidFileLocator.LocateJDKbin(), Exe("java")));
		Console.WriteLine("");
	}

	public string[] ReadAvailableKeys(string keystore, string storepass)
	{
		// validate params
		if (keystore.Length == 0 || storepass.Length == 0)
			return null;

		string[] output = RunCommand(new string[]{"keytool-list", keystore, storepass}, null, ListKeystoreKeysError).
			Split(new char[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);

		List<string> availableKeyalias = new List<string>();
		for (int i = 0; i < output.Length; ++i)
			if (output[i].StartsWith(COMPATIBLE_KEY_IDENTIFIER))
				availableKeyalias.Add(output[i].Substring(COMPATIBLE_KEY_IDENTIFIER.Length + 1));

		return availableKeyalias.ToArray();
	}

	public void CreateKey(string keystore, string storepass, string alias, string password, string dname, int validityInDays)
	{
		if (!File.Exists(keystore)) // auto create key store if it doen't exist
			RunCommand(new string[]{"keytool-createkeystore", keystore, storepass}, null, CreateKeystoreError);

		RunCommand(
			new string[]{"keytool-genkey", keystore, storepass, "-keyalias", alias, "-keypass", password, "-validity", validityInDays.ToString(), "-dname", dname},
			null,
			CreateKeystoreKeyError);
	}

	public void MergeManifests(string target, string mainManifest, string[] libraryManifests, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		List<string> command = new List<string>();
		command.AddRange(new string[] {"manifmerger", "merge", "--out", target,	"--main", mainManifest});
		foreach (string libraryManifest in libraryManifests)
		{
			command.Add("--libs");
			command.Add(libraryManifest);
		}
		RunCommand(command.ToArray(), waitingForProcessToExit, MergeManifestsError);
	}

	public String ToolsVersion(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return RunCommand(new string[] { "version", "tool" }, waitingForProcessToExit, ToolsVersionError).Trim();
	}

	public String PlatformToolsVersion(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return RunCommand(new string[] { "version", "platform-tool" }, waitingForProcessToExit, ToolsVersionError).Trim();
	}

	public string[] ListTargetPlatforms(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		string output = RunCommand(new string[] {"android", "list",  "target",  "-c"}, waitingForProcessToExit, ListPlatformsError);
		List<string> targetPlatforms = new List<string>();
		foreach (string line in output.Split(new char[]{ '\n', '\r' }))
			if (line.StartsWith("android-"))
				targetPlatforms.Add(line);

		return targetPlatforms.ToArray();
	}

	public string UpdateSDK(Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return RunCommand(new string[] {"android", "update", "sdk", "-u", "-t", "tool,platform-tool"}, waitingForProcessToExit, UpdateSDKError);
	}

	public string InstallPlatform(int apiLevel, Command.WaitingForProcessToExit waitingForProcessToExit)
	{
		return RunCommand(new string[] {"android", "update", "sdk", "-u", "-t", string.Format("android-{0}", apiLevel)}, waitingForProcessToExit, InstallPlatformError);
	}

	public string RunCommand(string[] sdkToolCommand, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		return RunCommand(sdkToolCommand, null, waitingForProcessToExit, errorMsg);
	}

	public string RunCommand(string[] sdkToolCommand, string workingdir, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		return RunCommand(sdkToolCommand, DEFAULT_JVM_MEMORY, workingdir, waitingForProcessToExit, errorMsg);
	}

	private string RunCommand(string[] sdkToolCommand, int memoryMB, string workingdir, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		string javaExe = Path.Combine(AndroidFileLocator.LocateJDKbin(), Exe("java"));
		return RunCommand(javaExe, SDKToolsDir, sdkToolCommand, memoryMB, workingdir, waitingForProcessToExit, errorMsg);
	}

	public static bool VerifyJava(string javaExe)
	{
		try
		{
			string versionStr = RunCommand(javaExe, null, new string[] { "javaversion" }, DEFAULT_JVM_MEMORY, null, null, "Incompatible java version '" + javaExe + "'");
			string[] version = versionStr.Split('.');
			if (version.Length > 1)
				if (Convert.ToUInt32(version[0]) == 1)
					if (Convert.ToUInt32(version[1]) >= 6)
						return true; // all good
			if (version.Length > 0)
				if (Convert.ToUInt32(version[0]) >= 6)
					return true; // all good

			throw new Exception(string.Format("Incompatible java version: {0} ('{1}')", versionStr, javaExe));
		}
		catch (Exception error)
		{
			Console.WriteLine(string.Format("Failed java version detection for '{0}'", javaExe));
			Console.WriteLine(error);
		}
		return false;
	}

	private static string RunCommand(string javaExe, string sdkToolsDir, string[] sdkToolCommand, int memoryMB, string workingdir, Command.WaitingForProcessToExit waitingForProcessToExit, string errorMsg)
	{
		if (workingdir == null)
			workingdir = Directory.GetCurrentDirectory();

		string javaArgs = "";
		javaArgs += "-Xmx" + memoryMB + "M ";
		if (sdkToolsDir != null)
			javaArgs += "-Dcom.android.sdkmanager.toolsdir=\"" + sdkToolsDir + "\" ";
		javaArgs +="-Dfile.encoding=UTF8 ";
		javaArgs += "-jar \"" + Path.Combine (BuildPipeline.GetBuildToolsDirectory(BuildTarget.Android), "sdktools.jar") + "\" ";
		javaArgs += "-"; // use stdin for arguments

		var psi = new System.Diagnostics.ProcessStartInfo();
		psi.FileName = javaExe;
		psi.Arguments = javaArgs;
		psi.WorkingDirectory = workingdir;
		psi.CreateNoWindow = true;
		psi.StandardOutputEncoding = Encoding.UTF8;
		try
		{
			bool inputSent = false;
			string result = Command.Run(psi, (program) =>
			{
				if (!inputSent && program != null)
				{
					StreamWriter input = new StreamWriter(program.GetStandardInput()); // UTF8 StreamWriter
					foreach (var arg in sdkToolCommand)
						input.WriteLine(arg);
					input.Close();
					inputSent = true;
				}
				if (waitingForProcessToExit != null)
					waitingForProcessToExit(program);
			}, errorMsg);

			foreach (Match warning in Warnings.Matches(result))
				Debug.LogWarning(warning.Groups[1].Value);
			return Warnings.Replace(result, "");
		}
		catch (CommandInvokationFailure cmdFailure)
		{
			string lastError = null;
			List<string> errors = new List<string>();
			{
				// JVM crap (on windows the jvm requires continuous memory so we re-invoke with smaller heap on failure)
				string cmdOut = cmdFailure.StdOutString() + cmdFailure.StdErrString();
				if (cmdOut.Contains("Could not reserve enough space for object heap") ||
					cmdOut.Contains("The specified size exceeds the maximum representable size"))
				{
					if (memoryMB > 64)
						return RunCommand(javaExe, sdkToolsDir, sdkToolCommand, memoryMB/2, workingdir, waitingForProcessToExit, errorMsg);
					errors.Add("Error: Out of memory (details: unable to create jvm process due to unsufficient continuous memory)");
				}
			}
			foreach (var stderr in cmdFailure.StdErr)
			{
				if (stderr.StartsWith("Error:"))
					errors.Add(lastError = stderr);
				else if (lastError != null) // multi-line error
					lastError += "\n" + stderr;
			}
			cmdFailure.Errors = errors.ToArray();
			throw cmdFailure;
		}
	}

	private string BuildToolsExe(string command)
	{
		string newStylePath = Path.Combine(SDKBuildToolsDir, Exe(command));
		if (File.Exists(newStylePath))
			return newStylePath;
		return PlatformToolsExe(command);
	}

	private string PlatformToolsExe(string command)
	{
		string newStylePath = Path.Combine(SDKPlatformToolsDir, Exe(command));
		if (File.Exists(newStylePath))
			return newStylePath;
		return ToolsExe(command);
	}

	private string ToolsExe(string command)
	{
		return Path.Combine(SDKToolsDir, Exe(command));
	}

	private static string Exe(string command)
	{
		return command += (Application.platform == RuntimePlatform.WindowsEditor ? ".exe" : "");
	}

	private static AndroidSDKTools s_Instance;
	public static AndroidSDKTools GetInstance()
	{
		string sdk_root = EditorPrefs.GetString ("AndroidSdkRoot");
		if (!AndroidSdkRoot.IsSdkDir(sdk_root))
		{
			sdk_root = AndroidSdkRoot.Browse(sdk_root);
			if (!AndroidSdkRoot.IsSdkDir(sdk_root))
				return null; // operation cancelled

			EditorPrefs.SetString("AndroidSdkRoot", sdk_root);
		}

		if (s_Instance == null || sdk_root != s_Instance.SDKRootDir)
			s_Instance = new AndroidSDKTools(sdk_root);

		return s_Instance;
	}

	public static AndroidSDKTools GetInstanceOrThrowException()
	{
		AndroidSDKTools tools = GetInstance();
		if (tools != null)
			return tools;

		throw new UnityException("Unable to locate Android SDK!");
	}
}
}
