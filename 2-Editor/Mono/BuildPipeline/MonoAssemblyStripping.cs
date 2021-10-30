using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using System.IO;
using System;
using Mono.Cecil;
using System.Diagnostics;
using System.Text;
using UnityEditor.Utils;
using Debug = System.Diagnostics.Debug;

namespace UnityEditor
{

internal class MonoProcessUtility
{	
	public static string ProcessToString(Process process)
	{
		return process.StartInfo.FileName + " " +
							process.StartInfo.Arguments + " current dir : " +
							process.StartInfo.WorkingDirectory + "\n";
	}
	
	public static void RunMonoProcess(Process process, string name, string resultingFile)
	{
        StringBuilder output = new StringBuilder("");
        StringBuilder error = new StringBuilder("");
		
		process.StartInfo.RedirectStandardOutput = true;
		process.StartInfo.RedirectStandardError = true;
		
		process.OutputDataReceived += new DataReceivedEventHandler((sender, dataLine) => 
						{ if (!String.IsNullOrEmpty(dataLine.Data)) output.Append(dataLine.Data); });
		process.ErrorDataReceived += new DataReceivedEventHandler((sender, dataLine) => 
						{ if (!String.IsNullOrEmpty(dataLine.Data)) error.Append(dataLine.Data); });
		
		process.Start();
		process.BeginOutputReadLine();
		process.BeginErrorReadLine();
		process.WaitForExit(1000 * 60 * 5); // 5 minutes

		if (process.ExitCode != 0 || !File.Exists(resultingFile))
		{
			process.CancelOutputRead();
			process.CancelErrorRead();
			
			string detailedMessage = "Failed " + name + ": " + ProcessToString(process) + " result file exists: " + File.Exists(resultingFile);
            detailedMessage += "\n";
            detailedMessage += "stdout: " + output + "\n";
            detailedMessage += "stderr: " + error + "\n";
			System.Console.WriteLine(detailedMessage);
			throw new UnityException(detailedMessage);	
			
			/// ;; TODO add micromscorlib warning
		}

	}
	
	public static string GetMonoExec(BuildTarget buildTarget)
	{
		string monoRuntime = BuildPipeline.GetMonoBinDirectory(buildTarget);
		
		if (Application.platform == RuntimePlatform.OSXEditor)
			return Path.Combine(monoRuntime, "mono");
		else
			return Path.Combine(monoRuntime, "mono.exe");
	}
	
	public static string GetMonoPath(BuildTarget buildTarget)
	{
		string monoRuntime = BuildPipeline.GetMonoLibDirectory(buildTarget);
		return monoRuntime + Path.PathSeparator + ".";
	}
	
	public static Process PrepareMonoProcess(BuildTarget target, string workDir)
	{
		Process process = new System.Diagnostics.Process ();
		process.StartInfo.FileName = GetMonoExec(target);
		
		// ;; TODO fix this hack for strange process handle duplication problem inside mono
		process.StartInfo.EnvironmentVariables["_WAPI_PROCESS_HANDLE_OFFSET"] = "5";
		
		process.StartInfo.EnvironmentVariables["MONO_PATH"] = GetMonoPath(target);
		process.StartInfo.UseShellExecute = false;
		process.StartInfo.RedirectStandardOutput = true;
		process.StartInfo.RedirectStandardError = true;
		process.StartInfo.CreateNoWindow = true;
	
		process.StartInfo.WorkingDirectory= workDir;
		
		return process;
	}
	
}

internal class MonoAssemblyStripping
{
	
	
	static void ReplaceFile(string src, string dst)
	{
		if (File.Exists(dst))
			FileUtil.DeleteFileOrDirectory(dst);
			
		FileUtil.CopyFileOrDirectory(src, dst);
	}
	
		
	static public void MonoCilStrip (BuildTarget buildTarget, string managedLibrariesDirectory, string[] fileNames)
	{
		string basePath = BuildPipeline.GetBuildToolsDirectory(buildTarget);
		string cilStripper = Path.Combine(basePath, "mono-cil-strip.exe");
		
		foreach(string fileName in fileNames)
		{
			Process process = MonoProcessUtility.PrepareMonoProcess (buildTarget, managedLibrariesDirectory);
			string outFile = fileName + ".out";
			
			process.StartInfo.Arguments = "\"" + cilStripper + "\"";
			process.StartInfo.Arguments += " \"" + fileName + "\" \"" + fileName + ".out\"";
			
			MonoProcessUtility.RunMonoProcess(process, "byte code stripper", Path.Combine(managedLibrariesDirectory, outFile));
			
			ReplaceFile(managedLibrariesDirectory + "/" + outFile, managedLibrariesDirectory + "/" + fileName);
			File.Delete(managedLibrariesDirectory + "/" + outFile);
		}
	}	
	
	public static string GenerateBlackList(string librariesFolder, RuntimeClassRegistry usedClasses, string[] allAssemblies)
	{
		string path = "tmplink.xml";
		usedClasses.SynchronizeClasses();
		using (TextWriter w = new StreamWriter(Path.Combine(librariesFolder, path)))
        {
			w.WriteLine("<linker>");
        	w.WriteLine("<assembly fullname=\"UnityEngine\">");
			
			foreach (string className in usedClasses.GetAllManagedClassesAsString())
				w.WriteLine(string.Format("<type fullname=\"UnityEngine.{0}\" preserve=\"{1}\"/>", 
					                          className, usedClasses.GetRetentionLevel(className)));
			w.WriteLine("</assembly>");

            var resolver = new DefaultAssemblyResolver();
            resolver.AddSearchDirectory(librariesFolder);
			
			// Generate blacklist for custom assemblies that have Monobehaviour inside
			foreach (var file in allAssemblies)
			{
				AssemblyDefinition assembly = resolver.Resolve(Path.GetFileNameWithoutExtension(file), new ReaderParameters { AssemblyResolver = resolver });
				
				w.WriteLine("<assembly fullname=\"{0}\">", assembly.Name.Name);
				foreach (TypeDefinition typ in assembly.MainModule.Types)
				{
					if (DoesTypeEnheritFrom(typ, "UnityEngine.MonoBehaviour") || DoesTypeEnheritFrom(typ, "UnityEngine.ScriptableObject"))
                        w.WriteLine("<type fullname=\"{0}\" preserve=\"all\"/>", typ.FullName);
				}
				w.WriteLine("</assembly>");	
			}
				
			w.WriteLine("</linker>");
		}
			
		return path;
	}

    private static bool DoesTypeEnheritFrom(TypeReference type, string typeName)
    {
        while (type != null)
        {
            if (type.FullName == typeName)
            {
                return true;
            }
            type = type.Resolve().BaseType;
        }
        return false;
    }

    static private string StripperExe ()
	{
		switch (Application.platform)
		{
		case RuntimePlatform.WindowsEditor:
			return "Tools/UnusedBytecodeStripper.exe";
		default:
			return "Tools/UnusedByteCodeStripper/UnusedBytecodeStripper.exe";
		}
	}

	static public void MonoLink(BuildTarget buildTarget, string managedLibrariesDirectory, string[] input, string[] allAssemblies, RuntimeClassRegistry usedClasses)
	{
		Process process = MonoProcessUtility.PrepareMonoProcess(buildTarget, managedLibrariesDirectory);
		string basePath = BuildPipeline.GetBuildToolsDirectory(buildTarget);

		string generatedBlackList = null;

		string frameworks = MonoInstallationFinder.GetFrameWorksFolder();
		string monolinker = Path.Combine(frameworks, StripperExe());
		string linkDescriptor = Path.Combine(Path.GetDirectoryName(monolinker), "link.xml");
		string output = Path.Combine(managedLibrariesDirectory, "output");

		Directory.CreateDirectory(output);

		process.StartInfo.Arguments = "\"" + monolinker + "\" -l none -c link";
		foreach (string fileName in input)
			process.StartInfo.Arguments += " -a \"" + fileName + "\"";
		process.StartInfo.Arguments += " -out output -x \"" + linkDescriptor + "\"" + " -d \"" + managedLibrariesDirectory + "\"";

		string platformDescriptor = Path.Combine(basePath, "link.xml");
		if (File.Exists(platformDescriptor))
			process.StartInfo.Arguments += " -x \"" + platformDescriptor + "\"";

		string localDescriptor = Path.Combine(Path.Combine(Directory.GetCurrentDirectory(), "Assets"), "link.xml");
		if (File.Exists(localDescriptor))
			process.StartInfo.Arguments += " -x \"" + localDescriptor + "\"";

		if (usedClasses != null)
		{
			generatedBlackList = GenerateBlackList(managedLibrariesDirectory, usedClasses, allAssemblies);
			process.StartInfo.Arguments += " -x \"" + generatedBlackList + "\"";
		}

		MonoProcessUtility.RunMonoProcess(process, "assemblies stripper", Path.Combine(output, "mscorlib.dll"));

		// Move files to final destination & cleanup

		if (buildTarget == BuildTarget.FlashPlayer)
		{
			// We only copy user assemblies back so cil2as won't fail to detect overloads later
			// in the process
			var userAssemblyNames = input.Select(_ => Path.GetFileName(_));
			CopyFiles(userAssemblyNames, output, managedLibrariesDirectory);
		}
		else
		{
			// Remove original dll files
			DeleteAllDllsFrom(managedLibrariesDirectory);

			// Copy monolinked dll files
			CopyAllDlls(managedLibrariesDirectory, output);
		}

		if (generatedBlackList != null)
			FileUtil.DeleteFileOrDirectory(Path.Combine(managedLibrariesDirectory, generatedBlackList));

		FileUtil.DeleteFileOrDirectory(output);

	}

	private static void CopyFiles(IEnumerable<string> files, string fromDir, string toDir)
	{
		foreach (var f in files)
			FileUtil.ReplaceFile(Path.Combine(fromDir, f), Path.Combine(toDir, f));
	}

	private static void CopyAllDlls(string fromDir, string toDir)
	{
		var di = new DirectoryInfo(toDir);
		var files = di.GetFiles("*.dll");
		foreach (FileInfo fi in files)
		{
			FileUtil.ReplaceFile(Path.Combine(toDir, fi.Name), Path.Combine(fromDir, fi.Name));
		}
	}

	private static void DeleteAllDllsFrom(string managedLibrariesDirectory)
	{
		var di = new DirectoryInfo(managedLibrariesDirectory);
		var files = di.GetFiles("*.dll");
		foreach (FileInfo fi in files)
		{
			FileUtil.DeleteFileOrDirectory(fi.FullName);
		}
	}

    /*
	static public void CleanupReferences(string managedLibrariesDirectory, ArrayList scriptNames, ArrayList assemblyNames, ArrayList fileNames)
	{
		Hashtable asmfile = new Hashtable();
		Hashtable deps = new Hashtable();
		Hashtable processed = new Hashtable();
		Queue processQ = new Queue();
		
		if (scriptNames == null || scriptNames.Count < 1)
			return;
		
		// Build assembly name -> file name mapping
		for (int i = 0; i < assemblyNames.Count; i++)
			asmfile.Add(assemblyNames[i] + ".dll", fileNames[i]);
		
		// Add scripts to queue
		foreach (string script in scriptNames)
			processQ.Enqueue(asmfile[script]);
		
		// Main processing loop
		while (processQ.Count > 0)
		{
			string assembly = (string)processQ.Dequeue();	
			if (!processed.Contains(assembly))
			{	
				ArrayList refs = GetReferences(managedLibrariesDirectory, assembly);
			
				foreach (string rf in refs)
				{
					string dep = rf + ".dll"; 
					
					if (!deps.ContainsKey(dep))
					{
						//Debug.Log(dep);
						deps.Add(dep, null);
						processQ.Enqueue(dep);
					}
				}
			
				processed.Add(assembly, null);
				
				if (!deps.ContainsKey(assembly))
					deps.Add(assembly, null);
			}
		}	
		
		// Remove assemblies that were not referenced
		for (int i = fileNames.Count - 1; i >= 0; i--)
		{
			if (!deps.ContainsKey(fileNames[i]))
			{
				Debug.Log("Removing unnecessary dependency: " + fileNames[i]);
				FileUtil.DeleteFileOrDirectory(managedLibrariesDirectory + "/" + fileNames[i]);
				assemblyNames.RemoveAt(i);
				fileNames.RemoveAt(i);
			}	
		}
	}
	
	static ArrayList GetReferences (string buildingDataFolder, string assembly)
	{
		System.Diagnostics.Process process = new System.Diagnostics.Process ();
		process.StartInfo.FileName=EditorApplication.applicationContentsPath + "/Tools/monodis";
		process.StartInfo.Arguments = "--assemblyref \"" + assembly + "\"";
		process.StartInfo.EnvironmentVariables.Add("MONO_PATH", EditorApplication.applicationContentsPath + "/Frameworks/MonoiPhone.framework" + ":.");
		process.StartInfo.UseShellExecute=false;
		process.StartInfo.RedirectStandardOutput = true;
		process.StartInfo.RedirectStandardError = true;

	
		process.StartInfo.WorkingDirectory= Path.Combine(Directory.GetCurrentDirectory(), buildingDataFolder);
		process.Start();
		process.WaitForExit();

		if (process.ExitCode != 0 )
		{
			Debug.Log("Monodis failed: " + process.StartInfo.FileName + " " + 
				process.StartInfo.Arguments + " current dir : " + process.StartInfo.WorkingDirectory + 
				" std= " + process.StandardOutput.ReadToEnd() + " err= " +  process.StandardError.ReadToEnd());	
			throw new System.Exception("Failed getting reference from " + process.StartInfo.Arguments);	
		}
		else
		{
			return ParseMonodisOutput(process.StandardOutput);
		}
	}
	
	static ArrayList ParseMonodisOutput(StreamReader reader)
	{
		ArrayList res = new ArrayList();
		
		 while (reader.Peek() >= 0) 
         {
             string line = reader.ReadLine();
             string[] tokens = line.Trim().Split('=');
             if (tokens.Length > 1 && tokens[0].Equals("Name"))
             	res.Add(tokens[1]);
         }
         
         return res;
	}
	*/
}
}
