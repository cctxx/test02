using System;
using System.Collections;
using System.IO;
using System.Text.RegularExpressions;
using UnityEngine;

namespace UnityEditor
{
	public partial class FileUtil
	{
		internal static ArrayList ReadAllText(string path)
		{
			ArrayList strings = new ArrayList();
			using (StreamReader sr = File.OpenText(NiceWinPath(path)))
			{
				string s = "";
				while((s = sr.ReadLine()) != null)
					strings.Add(s);
			}
			return strings;
		}

		internal static void WriteAllText(string path, ArrayList strings)
		{
			StreamWriter sw = new StreamWriter(NiceWinPath(path));
		
			foreach ( string line in strings)
				sw.WriteLine(line);

			sw.Close();
		}

		internal static void ReplaceText (string path, params string[] input)
		{
			ArrayList data = ReadAllText(path);
		
			for (int i=0;i<input.Length;i+=2)
			{
				for (int q = 0; q < data.Count; ++q)
				{
					string s = (string)data[q];
					data[q] = s.Replace(input[i], input[i+1]);
				}
			}
	
			WriteAllText(path, data);
		}

		internal static bool ReplaceTextRegex (string path, params string[] input)
		{
			bool res = false;
			ArrayList data = ReadAllText(path);
		
			for (int i=0;i<input.Length;i+=2)
			{
				for (int q = 0; q < data.Count; ++q)
				{
					string s = (string)data[q];
					data[q] = Regex.Replace(s, input[i], input[i+1]);
				
					if (s != (string)data[q])
						res = true;
				}
			}
		
			WriteAllText(path, data);
			return res;
		}

		internal static bool AppendTextAfter (string path, string find, string append)
		{
			bool res = false;
			ArrayList data = ReadAllText(path);
		
			for (int q = 0; q < data.Count; ++q)
			{
				if (((string)data[q]).IndexOf(find) > -1)
				{
					data.Insert(q + 1, append);
					res = true;
					break;
				}
			}
		
			WriteAllText(path, data);
			return res;
		}

		internal static void CopyDirectoryRecursive (string source, string target)
		{
			CopyDirectoryRecursive(source, target, false, false);
		}

		internal static void CopyDirectoryRecursiveIgnoreMeta(string source, string target)
		{
			CopyDirectoryRecursive(source, target, false, true);
		}

		internal static void CopyDirectoryRecursive(string source, string target, bool overwrite)
		{
			CopyDirectoryRecursive(source, target, overwrite, false);
		}

		internal static void CopyDirectory(string source, string target, bool overwrite)
		{
			CopyDirectoryFiltered(source, target, overwrite, null, false);
		}
		
		internal static void CopyDirectoryRecursive (string source, string target, bool overwrite, bool ignoreMeta)
		{
			CopyDirectoryRecursiveFiltered(source, target, overwrite, ignoreMeta? @"\.meta$": null);
		}
		
		internal static void CopyDirectoryRecursiveForPostprocess (string source, string target, bool overwrite)
		{
			CopyDirectoryRecursiveFiltered(source, target, overwrite, @".*/\.+|\.meta$");
		}
		
		internal static void CopyDirectoryRecursiveFiltered (string source, string target, bool overwrite, string regExExcludeFilter)
		{
			CopyDirectoryFiltered (source, target, overwrite, regExExcludeFilter, true);
		}

		internal static void CopyDirectoryFiltered(string source, string target, bool overwrite, string regExExcludeFilter, bool recursive)
		{
			Regex exclude = null;
			try
			{
				if (regExExcludeFilter != null)
					exclude = new Regex(regExExcludeFilter);
			}
			catch (ArgumentException)
			{
				Debug.Log("CopyDirectoryRecursive: Pattern '" + regExExcludeFilter + "' is not a correct Regular Expression. Not excluding any files.");
				return;
			}
			
			// Check if the target directory exists, if not, create it.
			if (Directory.Exists(target) == false)
			{
				Directory.CreateDirectory(target);
				overwrite = false; // no reason to perform this on subdirs
			}

			// Copy each file into the new directory.
			foreach (string fi in Directory.GetFiles(source))
			{	
				if (exclude != null && exclude.IsMatch(fi))
					continue;
				
				string fname = Path.GetFileName(fi);
				string targetfname = Path.Combine(target, fname);
			
				UnityFileCopy(fi, targetfname, overwrite);
			}

			if (recursive)
			{
				// Copy each subdirectory recursively.
				foreach (string di in Directory.GetDirectories (source))
				{
					if (exclude != null && exclude.IsMatch (di))
						continue;

					string fname = Path.GetFileName (di);

					CopyDirectoryFiltered(Path.Combine(source, fname), Path.Combine(target, fname), overwrite, regExExcludeFilter, recursive);
				}
			}
		}

        internal static void UnityDirectoryDelete(string path)
        {
            UnityDirectoryDelete(path, false);
        }

        internal static void UnityDirectoryDelete(string path, bool recursive)
        {
            Directory.Delete(NiceWinPath(path), recursive);
        }


		internal static void MoveFileIfExists(string src, string dst)
		{
			if 	(File.Exists(src))
			{
				DeleteFileOrDirectory (dst);
				MoveFileOrDirectory (src, dst);
				File.SetLastWriteTime(dst, DateTime.Now);
			}
		}

		internal static void CopyFileIfExists(string src, string dst, bool overwrite)
		{
			if 	(File.Exists(src))
			{
				UnityFileCopy(src, dst, overwrite);
			}
		}

		internal static void UnityFileCopy(string from, string to, bool overwrite)
		{
			File.Copy(NiceWinPath(from), NiceWinPath(to), overwrite);
		}

		internal static string NiceWinPath(string unityPath)
		{
			// IO functions do not like mixing of \ and / slashes, esp. for windows network paths (\\path)
			return Application.platform == RuntimePlatform.WindowsEditor ? unityPath.Replace("/", @"\") : unityPath;
		}

		internal static string UnityGetFileNameWithoutExtension(string path)
		{
			// this is because on Windows \\ means network path, in unity all \ are converted to /
			// network paths become // and Path class functions think it's the same as / 
			return Path.GetFileNameWithoutExtension(path.Replace("//", @"\\")).Replace(@"\\", "//");
		}

		internal static string UnityGetFileName(string path)
		{
			// this is because on Windows \\ means network path, in unity all \ are converted to /
			// network paths become // and Path class functions think it's the same as / 
			return Path.GetFileName(path.Replace("//", @"\\")).Replace(@"\\", "//");
		}

		internal static string UnityGetDirectoryName(string path)
		{
			// this is because on Windows \\ means network path, in unity all \ are converted to /
			// network paths become // and Path class functions think it's the same as / 
			return Path.GetDirectoryName(path.Replace("//", @"\\")).Replace(@"\\", "//");
		}

		internal static void UnityFileCopy(string from, string to)
		{
			UnityFileCopy(from, to, false);
		}

		internal static void CreateOrCleanDirectory(string dir)
		{
			if (Directory.Exists(dir))
				Directory.Delete(dir, true);
			Directory.CreateDirectory(dir);
		}

		internal static string RemovePathPrefix (string fullPath, string prefix)
		{
			var partsOfFull = fullPath.Split (Path.DirectorySeparatorChar);
			var partsOfPrefix = prefix.Split (Path.DirectorySeparatorChar);
			int index = 0;

			if (partsOfFull[0] == string.Empty)
				index = 1;

			while (index < partsOfFull.Length
			       && index < partsOfPrefix.Length
			       && partsOfFull[index] == partsOfPrefix[index])
				++index;

			if (index == partsOfFull.Length)
				return "";

			return string.Join (Path.DirectorySeparatorChar.ToString (),
			                    partsOfFull, index, partsOfFull.Length - index);
		}
	}
}
