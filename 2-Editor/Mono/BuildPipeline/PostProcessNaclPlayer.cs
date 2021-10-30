using System.Collections.Generic;
using System.IO;
using Mono.Cecil;
using UnityEditor;
using UnityEditorInternal;
using System.Security.Cryptography;
using System.Text;
using System.Diagnostics;
using System;

namespace UnityEditorInternal {
internal class PostProcessNaclPlayer
{
	static string ProgramFilesx86
	{
		get {
			if (8 == IntPtr.Size
				|| (!String.IsNullOrEmpty(Environment.GetEnvironmentVariable("PROCESSOR_ARCHITEW6432"))))
			{
				return Environment.GetEnvironmentVariable("ProgramFiles(x86)");
			}

			return Environment.GetEnvironmentVariable("ProgramFiles");
		}
	}
	
	static bool IsWindows
	{
		get { 
			return Environment.OSVersion.Platform != PlatformID.Unix; 
		}
	}
	
	static string BrowserPath
	{
		get
		{
			if (IsWindows)
			{
				string try1 = Path.Combine(ProgramFilesx86, Path.Combine("Google", Path.Combine("Chrome", Path.Combine("Application", "chrome.exe"))));
				if (File.Exists(try1)) return try1;
				string try2 = "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe";
				if (File.Exists(try2)) return try2;
				string try3 = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), Path.Combine("Google", Path.Combine("Chrome", Path.Combine("Application", "chrome.exe"))));
				if (File.Exists(try3)) return try3;
				throw new Exception("Cannot find Chrome");
			}
			else
			{
				string chrome = "/Applications/Google Chrome.app/Contents/MacOS/Google Chrome";
				if (File.Exists(chrome))
					return chrome;
				
				throw new Exception("Cannot find Chrome");
			}
		}
	}

	static bool s_ChromeFailedToLaunch = false;
	
	internal static void Launch (BuildTarget target, string targetfile)
	{
		// Strip "../" from path, so we get the same hash as chrome uses.
		targetfile = Path.GetFullPath (targetfile);
		
		// We need to get the app id to launch the chrome app. Chrome uses the first 
		// 16 bytes of the SHA256 hash of the path, mapped to lower case characters for this.
		byte[] hashData = IsWindows ? Encoding.Unicode.GetBytes(targetfile):Encoding.UTF8.GetBytes(targetfile);
		byte[] hash = new SHA256Managed().ComputeHash(hashData);	
		string appID = "";
		for (int i=0;i<16;i++)
		{
			appID += (char)('a'+(hash[i]>>4));
			appID += (char)('a'+(hash[i]&0xf));
		}
		string url =  "chrome-extension://"+appID+"/" + Path.GetFileName(targetfile) + "_nacl.html";
		
		do
		{
			s_ChromeFailedToLaunch = false;
			
			Process chrome = new Process();
			chrome.StartInfo.FileName = BrowserPath;
			chrome.StartInfo.Arguments = " --load-extension=\""+targetfile+"\" \""+url+"\"";
			chrome.StartInfo.UseShellExecute = false;
			chrome.Start();   
			
			// Wait for five seconds to see if we get an error.
			System.Threading.Thread.Sleep (5000);

			s_ChromeFailedToLaunch = chrome.HasExited;
			if (s_ChromeFailedToLaunch)
			{
				if (IsWindows)
					s_ChromeFailedToLaunch = EditorUtility.DisplayDialog("Is Google Chrome already running?",
							"If Chrome is already running, you will only be able to test content which has been installed into Chrome before. If your content is not showing in Chrome, please quit Chrome and try again.", 
							"Try again", "Cancel");
				else
					s_ChromeFailedToLaunch = EditorUtility.DisplayDialog("Could not start Google Chrome. Is it already running?",
							"Please quit any running instances of Chrome and try again.", 
							"Try again", "Cancel");
			}
		}
		while (s_ChromeFailedToLaunch);
	}
	
	static void StripPhysicsFiles(string path, bool stripPhysics)
	{
		string[] files;
		string[] directories;

		files = Directory.GetFiles(path);
		foreach(string file in files)
		{
			if (file.Contains("_nophysx"))
			{
				if (stripPhysics)
				{
					string replacePath = file.Replace("_nophysx", "");
					File.Delete (replacePath);
					File.Move (file, replacePath);
				}
				else
					File.Delete (file);
			}
		}

		directories = Directory.GetDirectories(path);
		foreach(string directory in directories)
		{
			// Process each directory recursively
			StripPhysicsFiles(directory, stripPhysics);
		}
	}	
	
	internal static void PostProcess(BuildOptions options, string installPath, string downloadWebplayerUrl, int width, int height)
	{
		PostProcessWebPlayer.PostProcess(options, installPath, downloadWebplayerUrl, width, height);
		
		string installName = Path.GetFileName(installPath);
		string installDir = installPath;
		string playbackEngine = BuildPipeline.GetPlaybackEngineDirectory(BuildTarget.NaCl, BuildOptions.None);

		string naclEngineName = "unity_nacl_files_3.x.x";
		string naclFilesPath = Path.Combine(installPath, naclEngineName);
		string src = Path.Combine(playbackEngine, "unity_nacl_files");
		if (Directory.Exists(naclFilesPath))
			FileUtil.DeleteFileOrDirectory(naclFilesPath);
		FileUtil.CopyFileOrDirectory(src, naclFilesPath);

		StripPhysicsFiles (naclFilesPath, PlayerSettings.stripPhysics);
		
		string templatePath = Path.Combine(playbackEngine, "template.html");
		string indexFilePath = Path.Combine(installDir, installName + "_nacl.html");
		if (File.Exists(indexFilePath))
			FileUtil.DeleteFileOrDirectory(indexFilePath);
		FileUtil.CopyFileOrDirectory(templatePath, indexFilePath);

		string templateManifestPath = Path.Combine(playbackEngine, "template.json");
		string manifestPath = Path.Combine(installDir, "manifest.json");
		if (File.Exists(manifestPath))
			FileUtil.DeleteFileOrDirectory(manifestPath);
		FileUtil.CopyFileOrDirectory(templateManifestPath, manifestPath);

		// Construct built-in replacements list
		List<string> replacements = new List<string>();
		replacements.Add("%UNITY_WIDTH%"); replacements.Add(width.ToString());
		replacements.Add("%UNITY_HEIGHT%"); replacements.Add(height.ToString());
		replacements.Add("%UNITY_WEB_NAME%"); replacements.Add(PlayerSettings.productName);
		replacements.Add("%UNITY_WEB_PATH%"); replacements.Add(installName + ".unity3d");
		replacements.Add("%UNITY_WEB_HTML_PATH%"); replacements.Add(installName + "_nacl.html");
		replacements.Add("%UNITY_NACL_PARAMETERS%"); replacements.Add((options & BuildOptions.Development) != 0?"softexceptions=\"1\"":"");
		replacements.Add("%UNITY_NACL_ENGINE_PATH%"); replacements.Add(naclEngineName);
		replacements.Add("%UNITY_BETA_WARNING%");
		replacements.Add(InternalEditorUtility.IsUnityBeta()
							? "\r\n\t\t<p style=\"color: #c00; font-size: small; font-style: italic;\">Built with beta version of Unity.</p>"
							: string.Empty);

		// Add custom replacements
		foreach (string key in PlayerSettings.templateCustomKeys)
		{
			replacements.Add("%UNITY_CUSTOM_" + key.ToUpper() + "%");
			replacements.Add(PlayerSettings.GetTemplateCustomValue(key));
		}

		// Exchange template markers for data
		FileUtil.ReplaceText(indexFilePath, replacements.ToArray());
		FileUtil.ReplaceText(manifestPath, replacements.ToArray());
	}
}
}