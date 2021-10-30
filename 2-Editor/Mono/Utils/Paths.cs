using System;
using System.IO;
using System.Collections.Generic;

namespace UnityEditor.Utils
{
	internal static class Paths
	{
		public static string Combine(params string[] components)
		{
			if (components.Length < 1)
				throw new ArgumentException("At least one component must be provided!");

			string path = components[0];
			for (int i = 1; i < components.Length; i++)
				path = Path.Combine(path, components[i]);
			return path;
		}
		
		public static string[] Split (string path)
		{
			List<string> result = new List<string> (path.Split (Path.DirectorySeparatorChar));
			
			for (int i = 0; i < result.Count;)
			{
				result[i] = result[i].Trim ();
				if (result[i].Equals (""))
				{
					result.RemoveAt (i);
				}
				else
				{
					i++;
				}
			}
			
			return result.ToArray ();
		}
		
		public static string GetFileOrFolderName (string path)
		{
			string name;
			
			if (File.Exists (path))
			{
				name = Path.GetFileName (path);
			}
			else if (Directory.Exists (path))
			{
				string[] pathSplit = Split (path);
				name = pathSplit[pathSplit.Length - 1];
			}
			else
			{
				throw new ArgumentException ("Target '" + path + "' does not exist.");
			}
			
			return name;
		}

		public static string CreateTempDirectory()
		{
			string projectPath = Path.GetTempFileName();
			File.Delete(projectPath);
			Directory.CreateDirectory(projectPath);
			return projectPath;
		}

		public static string NormalizePath (this string path)
		{
			if (Path.DirectorySeparatorChar == '\\')
				return path.Replace ('/', Path.DirectorySeparatorChar);
			return path.Replace ('\\', Path.DirectorySeparatorChar);
		}
	}
}
