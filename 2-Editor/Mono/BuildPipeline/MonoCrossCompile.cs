using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditorInternal;
using System;
using System.Threading;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;
using Mono.Cecil;

namespace UnityEditor
{
[Flags]
internal enum CrossCompileOptions
{
	Dynamic = 0,
	FastICall = 1 << 0,
	Static = 1 << 1,
	Debugging = 1 << 2,
    ExplicitNullChecks = 1 << 3,
    LoadSymbols = 1 << 4
}

internal class MonoCrossCompile
{
	private class JobCompileAOT
	{
		public JobCompileAOT(BuildTarget target, string crossCompilerAbsolutePath, 
							string assembliesAbsoluteDirectory, CrossCompileOptions crossCompileOptions, 
							string input, string output, string additionalOptions)
		{
			m_target = target;
			m_crossCompilerAbsolutePath = crossCompilerAbsolutePath;
			m_assembliesAbsoluteDirectory = assembliesAbsoluteDirectory;
			m_crossCompileOptions = crossCompileOptions;
			m_input = input;
			m_output = output;
			m_additionalOptions = additionalOptions;
		}
		
		public void ThreadPoolCallback(System.Object threadContext)
		{
			try
			{
				MonoCrossCompile.CrossCompileAOT(m_target, m_crossCompilerAbsolutePath, 
												m_assembliesAbsoluteDirectory, m_crossCompileOptions, 
												m_input, m_output, m_additionalOptions);
			}
			catch (Exception ex)
			{
				m_Exception = ex;
			}
			m_doneEvent.Set();
		}
		
		private BuildTarget 		m_target;
		private string 				m_crossCompilerAbsolutePath;
		private string 				m_assembliesAbsoluteDirectory;
		private CrossCompileOptions m_crossCompileOptions;
		public  string 				m_input;
		public  string 				m_output;
		public  string 				m_additionalOptions;
			
		public ManualResetEvent 	m_doneEvent = new ManualResetEvent(false);
		public Exception 			m_Exception = null;
	}

	static public void CrossCompileAOTDirectory (BuildTarget buildTarget, CrossCompileOptions crossCompileOptions, 
												string sourceAssembliesFolder, string targetCrossCompiledASMFolder,
												string additionalOptions)
	{
		CrossCompileAOTDirectory (buildTarget, crossCompileOptions, sourceAssembliesFolder, targetCrossCompiledASMFolder, "", additionalOptions);
	}

	static public void CrossCompileAOTDirectory (BuildTarget buildTarget, CrossCompileOptions crossCompileOptions, 
												string sourceAssembliesFolder, string targetCrossCompiledASMFolder, 
												string pathExtension, string additionalOptions)
	{
		string crossCompilerPath = BuildPipeline.GetBuildToolsDirectory(buildTarget);
		if (Application.platform == RuntimePlatform.OSXEditor)
			crossCompilerPath = Path.Combine(Path.Combine(crossCompilerPath, pathExtension), "mono-xcompiler");
		else
			crossCompilerPath = Path.Combine(Path.Combine(crossCompilerPath, pathExtension), "mono-xcompiler.exe");
		
		sourceAssembliesFolder = Path.Combine(Directory.GetCurrentDirectory(), sourceAssembliesFolder);
		targetCrossCompiledASMFolder = Path.Combine(Directory.GetCurrentDirectory(), targetCrossCompiledASMFolder);


		// Generate AOT Files (using OSX cross-compiler)
		foreach (string fileName in Directory.GetFiles(sourceAssembliesFolder))
		{
			if (Path.GetExtension(fileName) != ".dll")
				continue;

            // Cross AOT compile	
			string inputPath = Path.GetFileName(fileName);
			string outputPath = Path.Combine(targetCrossCompiledASMFolder, inputPath + ".s");
            
            if (EditorUtility.DisplayCancelableProgressBar("Building Player", "AOT cross compile " + inputPath, 0.95F))
                return;
			CrossCompileAOT(buildTarget, crossCompilerPath, sourceAssembliesFolder, 
							crossCompileOptions, inputPath, outputPath, additionalOptions);
		}
	}
	
	static public bool CrossCompileAOTDirectoryParallel(BuildTarget buildTarget, CrossCompileOptions crossCompileOptions, 
														string sourceAssembliesFolder, string targetCrossCompiledASMFolder,
														string additionalOptions)
	{
		return CrossCompileAOTDirectoryParallel(buildTarget, crossCompileOptions, sourceAssembliesFolder, 
												targetCrossCompiledASMFolder, "", additionalOptions);
	}
	
	static public bool CrossCompileAOTDirectoryParallel(BuildTarget buildTarget, CrossCompileOptions crossCompileOptions, 
														string sourceAssembliesFolder, string targetCrossCompiledASMFolder, 
														string pathExtension, string additionalOptions)
	{
		string crossCompilerPath = BuildPipeline.GetBuildToolsDirectory(buildTarget);
		if (Application.platform == RuntimePlatform.OSXEditor)
			crossCompilerPath = Path.Combine(Path.Combine(crossCompilerPath, pathExtension), "mono-xcompiler");
		else
			crossCompilerPath = Path.Combine(Path.Combine(crossCompilerPath, pathExtension), "mono-xcompiler.exe");
		
		sourceAssembliesFolder = Path.Combine(Directory.GetCurrentDirectory(), sourceAssembliesFolder);
		targetCrossCompiledASMFolder = Path.Combine(Directory.GetCurrentDirectory(), targetCrossCompiledASMFolder);


		// Generate AOT Files (using OSX cross-compiler)
		int workerThreads = 1;
		int completionPortThreads = 1;
		ThreadPool.GetMaxThreads(out workerThreads, out completionPortThreads);
		
		List<JobCompileAOT> jobList = new List<JobCompileAOT>();
		List<ManualResetEvent> eventList = new List<ManualResetEvent>();

		foreach (string fileName in Directory.GetFiles(sourceAssembliesFolder))
		{
			if (Path.GetExtension(fileName) != ".dll")
				continue;

            // Cross AOT compile	
			string inputPath = Path.GetFileName(fileName);
			string outputPath = Path.Combine(targetCrossCompiledASMFolder, inputPath + ".s");
			
			JobCompileAOT job = new JobCompileAOT(buildTarget, crossCompilerPath, sourceAssembliesFolder, 
												crossCompileOptions, inputPath, outputPath,
												additionalOptions);
			jobList.Add(job);
			eventList.Add(job.m_doneEvent);
			
			ThreadPool.QueueUserWorkItem(job.ThreadPoolCallback);
		}
		
		ManualResetEvent[] events = eventList.ToArray();
		EditorUtility.DisplayProgressBar("Building Player", "AOT cross compile.", 0.95F);
		bool success = WaitHandle.WaitAll(events, 1000 * 60 * 5); // 5 minute limit in case of FOOBARs
		
		foreach (var job in jobList)
		{
			if (job.m_Exception != null)
			{
				Debug.LogError(string.Format("Cross compilation job {0} failed.\n{1}", job.m_input, job.m_Exception.ToString()));
				success = false;
			}
		}
		
		return success;
	}
		
	
	static bool IsDebugableAssembly(string fname)
	{
		fname = Path.GetFileName(fname);
		return fname.StartsWith("Assembly", StringComparison.OrdinalIgnoreCase);
	}

	static void CrossCompileAOT (BuildTarget target, string crossCompilerAbsolutePath, string assembliesAbsoluteDirectory, CrossCompileOptions crossCompileOptions, string input, string output, string additionalOptions)
	{
        string arguments = "";
        
        // We don't want debugging for non-script assemblies (anyway source code is not available for the end users)
        if (!IsDebugableAssembly(input))
        {
        	crossCompileOptions &= ~CrossCompileOptions.Debugging;
            crossCompileOptions &= ~CrossCompileOptions.LoadSymbols;
        }

		bool debugging = ((crossCompileOptions & CrossCompileOptions.Debugging) != 0);
		bool loadSymbols = ((crossCompileOptions & CrossCompileOptions.LoadSymbols) != 0);
		bool initDebugging = (debugging || loadSymbols);
        if (initDebugging)
				arguments += "--debug ";

		if (debugging)
		{
			// Do not put locals into registers when debugging
			arguments += "--optimize=-linears ";
		}
			
		arguments += "--aot=full,asmonly,";
			
        if (initDebugging)
            arguments += "write-symbols,";

		if ((crossCompileOptions & CrossCompileOptions.Debugging) != 0)
            arguments += "soft-debug,";
        else if (!initDebugging)
			arguments += "nodebug,";

        if (target != BuildTarget.iPhone)
        {
            //arguments += "fail-if-methods-are-skipped,";
            arguments += "print-skipped,";
        }
			
		if (additionalOptions != null & additionalOptions.Trim().Length > 0)
				arguments += additionalOptions.Trim() + ",";
            
	    string outputFileName = Path.GetFileName(output);
        // Mono outfile parameter doesnt take absolute paths,
        // So we temporarily write into the assembliesAbsoluteDirectory and move it away afterwards
        string outputTempPath = Path.Combine(assembliesAbsoluteDirectory, outputFileName);
		if ((crossCompileOptions & CrossCompileOptions.FastICall) != 0)
			arguments += "ficall,";
		if ((crossCompileOptions & CrossCompileOptions.Static) != 0)
			arguments += "static,";
        arguments += "outfile=\"" + outputFileName + "\" \"" + input + "\" ";
				

        System.Diagnostics.Process process = new System.Diagnostics.Process ();
		process.StartInfo.FileName = crossCompilerAbsolutePath;
		process.StartInfo.Arguments = arguments;
		process.StartInfo.EnvironmentVariables["MONO_PATH"] = assembliesAbsoluteDirectory;
		process.StartInfo.EnvironmentVariables["GAC_PATH"] = assembliesAbsoluteDirectory;
        process.StartInfo.EnvironmentVariables["GC_DONT_GC"] = "yes please";
        if ((crossCompileOptions & CrossCompileOptions.ExplicitNullChecks) != 0)
            process.StartInfo.EnvironmentVariables["MONO_DEBUG"] = "explicit-null-checks";
		process.StartInfo.UseShellExecute = false;
        process.StartInfo.CreateNoWindow = true;
		process.StartInfo.RedirectStandardOutput = true;
	
		process.StartInfo.WorkingDirectory = assembliesAbsoluteDirectory;
		MonoProcessUtility.RunMonoProcess(process, "AOT cross compiler", outputTempPath);
		// For some reason we can't pass a full path to outfile, so we move the .s file after compilation instead	
        File.Move(outputTempPath, output);
	}	



}
}
