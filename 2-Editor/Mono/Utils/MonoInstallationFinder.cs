using System.IO;
using UnityEngine;

namespace UnityEditor.Utils
{
	class MonoInstallationFinder
	{
		public static string GetFrameWorksFolder()
		{
			if (Application.platform == RuntimePlatform.WindowsEditor)
				return Path.GetDirectoryName(EditorApplication.applicationPath) + "/Data/";
			else if (Application.platform == RuntimePlatform.OSXEditor)
				return EditorApplication.applicationPath + "/Contents/Frameworks/";
			else // Linux...?
				return Path.GetDirectoryName(EditorApplication.applicationPath) + "/Data/";
		}

		public static string GetProfileDirectory (BuildTarget target, string profile)
		{
			var monoprefix = GetMonoInstallation();
			return Path.Combine(monoprefix, "lib/mono/" + profile);
		}
		
		public static string GetMonoInstallation()
		{
#if INCLUDE_MONO_2_12
			return GetMonoInstallation("MonoBleedingEdge");
#else
			return GetMonoInstallation("Mono");
#endif
		}
		
		public static string GetMonoInstallation(string monoName)
		{
			return Path.Combine(GetFrameWorksFolder(), monoName);
		}
	}
}