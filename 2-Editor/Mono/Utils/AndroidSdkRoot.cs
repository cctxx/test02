using System.IO;
using UnityEngine;

namespace UnityEditor
{
	internal class AndroidSdkRoot
	{
		private static string GuessPerPlatform ()
		{
			switch (Application.platform)
			{
				case RuntimePlatform.WindowsEditor:
					var winx86 = Microsoft.Win32.Registry.GetValue (@"HKEY_LOCAL_MACHINE\SOFTWARE\Android SDK Tools", "Path", "").ToString ();
					var winx64 = Microsoft.Win32.Registry.GetValue (@"HKEY_LOCAL_MACHINE\SOFTWARE\Wow6432Node\Android SDK Tools", "Path", "").ToString ();
					var programFiles = System.Environment.GetEnvironmentVariable ("ProgramFiles");
					var programFilesx86 = System.Environment.GetEnvironmentVariable ("ProgramFiles(x86)");
					if (!string.IsNullOrEmpty (winx86))
						return winx86;
					else if (!string.IsNullOrEmpty (winx64))
						return winx64;
					else if (!string.IsNullOrEmpty (programFilesx86))
						return programFilesx86;
					else if (!string.IsNullOrEmpty (programFiles))
						return programFiles;
				break;

				case RuntimePlatform.OSXEditor:
					var home = System.Environment.GetEnvironmentVariable ("HOME");
					if (!string.IsNullOrEmpty (home))
						return home;
				break;

				default:
					// Nothing found, or Linux?
					return "";
			}

			return "";
		}

		internal static string Browse (string sdkPath)
		{
			var platformSdk = Application.platform == RuntimePlatform.OSXEditor
			                  ? "android-sdk-mac_x86" : "android-sdk-windows";
			var msg = "Select Android SDK root folder";
			var defaultFolder = string.IsNullOrEmpty (sdkPath)
			                    ? GuessPerPlatform () : sdkPath;

			do
			{
				sdkPath = EditorUtility.OpenFolderPanel (msg, defaultFolder, platformSdk);

				// user pressed cancel?
				if (sdkPath.Length == 0)
					return "";
			} while (!IsSdkDir (sdkPath));

			return sdkPath;
		}

		internal static bool IsSdkDir (string path)
		{
			return Directory.Exists (Path.Combine (path, "tools"));
		}
	}
}
