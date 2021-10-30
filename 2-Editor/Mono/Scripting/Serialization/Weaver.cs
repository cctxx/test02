using System;
using System.IO;
using UnityEditor.Utils;

namespace UnityEditor.Scripting.Serialization
{
	internal static class Weaver
	{
		public static void WeaveInto(string assemblyPath, string destPath)
		{
			using (var p = SerializationWeaverProgramWith(CommandLineArgsFrom(assemblyPath, destPath)))
			{
				try
				{
					p.Start();
				}
				catch
				{
					p.LogProcessStartInfo ();
					throw new Exception("Could not start SerializationWeaver.exe");
				}

				p.WaitForExit();

				if (p.ExitCode == 0)
					return;

				throw new Exception("Failed running SerializationWeaver. output was: "+p.GetAllOutput ());
			}
		}

		private static string CommandLineArgsFrom (string assemblyPath, string destPath)
		{
			return assemblyPath + " " + DestPathFrom (destPath);
		}

		private static string DestPathFrom (string destPath)
		{
			return (File.GetAttributes(destPath) & FileAttributes.Directory) == FileAttributes.Directory
				? destPath
				: Path.GetDirectoryName(destPath);
		}

		private static ManagedProgram SerializationWeaverProgramWith (string arguments)
		{
			return ManagedProgramFor(EditorApplication.applicationContentsPath + "/SerializationWeaver/SerializationWeaver.exe", arguments);
		}

		private static ManagedProgram ManagedProgramFor(string exe, string arguments)
		{
			return new ManagedProgram(MonoInstallationFinder.GetMonoInstallation("MonoBleedingEdge"), "4.0", exe, arguments);
		}
	}
}