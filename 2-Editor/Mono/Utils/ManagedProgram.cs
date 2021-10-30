using System.Diagnostics;
using System.IO;
using UnityEditor.Scripting.Compilers;
using UnityEditor.Utils;
using UnityEngine;

namespace UnityEditor.Scripting
{
	internal class ManagedProgram : Program
	{
		public ManagedProgram(string monodistribution, string profile, string executable, string arguments) :
			this (monodistribution, profile, executable, arguments, true)
		{
		}

		public ManagedProgram(string monodistribution, string profile, string executable, string arguments, bool setMonoEnvironmentVariables)
		{
			var monoexe = PathCombine(monodistribution, "bin", "mono");
			var profileAbspath = PathCombine(monodistribution, "lib", "mono", profile);
			if (Application.platform == RuntimePlatform.WindowsEditor)
				monoexe = CommandLineFormatter.PrepareFileName(monoexe + ".exe");

			var startInfo = new ProcessStartInfo
			{
				Arguments = CommandLineFormatter.PrepareFileName(executable) + " " + arguments,
				CreateNoWindow = true,
				FileName = monoexe,
				RedirectStandardError = true,
				RedirectStandardOutput = true,
				WorkingDirectory = Application.dataPath + "/..",
				UseShellExecute = false
			};

			if (setMonoEnvironmentVariables)
			{
				startInfo.EnvironmentVariables["MONO_PATH"] = profileAbspath;
				startInfo.EnvironmentVariables["MONO_CFG_DIR"] = PathCombine(monodistribution, "etc");
			}
			
			// if you ever need to debug assembly loading, uncomment the following two lines
			//startInfo.EnvironmentVariables["MONO_LOG_LEVEL"] = "info";
			//startInfo.EnvironmentVariables["MONO_LOG_MASK"] = "asm";
			
			_process.StartInfo = startInfo;
		}

		static string PathCombine(params string[] parts)
		{
			var path = parts[0];
			for (var i = 1; i < parts.Length; ++i)
				path = Path.Combine(path, parts[i]);
			return path;
		}
	}
}

