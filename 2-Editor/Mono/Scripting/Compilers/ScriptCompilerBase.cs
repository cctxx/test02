using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEditor.Utils;

namespace UnityEditor.Scripting.Compilers
{	
	internal abstract class ScriptCompilerBase : IDisposable
	{
		private Program process;
		private string _responseFile = null;
		// ToDo: would be nice to move MonoIsland to MonoScriptCompilerBase
		protected MonoIsland _island;
		
		protected abstract Program StartCompiler();
		
		protected abstract CompilerOutputParserBase CreateOutputParser();

		protected ScriptCompilerBase(MonoIsland island)
		{
			_island = island;
		}
		
		protected string[] GetErrorOutput()
		{
			return process.GetErrorOutput();
		}
		
		protected string[] GetStandardOutput()
		{
			return process.GetStandardOutput();
		}

        protected bool CompilingForMetro ()
        {
            if (_island._target == BuildTarget.MetroPlayer)
                return true;
            return false;
        }
		
		public void BeginCompiling()
		{
			if (process != null)
				throw new InvalidOperationException("Compilation has already begun!");
			process = StartCompiler();
		}

		public virtual void Dispose()
		{
			if (process != null)
			{
				process.Dispose();
				process = null;
			}
			if (_responseFile != null)
			{
				File.Delete(_responseFile);
				_responseFile = null;
			}
		}

		public virtual bool Poll()
		{
			if (process == null)
				return true;

			return process.HasExited;
		}

		protected void AddCustomResponseFileIfPresent(List<string> arguments, string responseFileName)
		{
			var relativeCustomResponseFilePath = Path.Combine("Assets", responseFileName);

			if (!File.Exists(relativeCustomResponseFilePath))
				return;

			arguments.Add("@" + relativeCustomResponseFilePath);
		}
		protected string GenerateResponseFile(List<string> arguments)
		{
			_responseFile = CommandLineFormatter.GenerateResponseFile(arguments);
			return _responseFile;
		}

		protected static string PrepareFileName(string fileName)
		{
			return CommandLineFormatter.PrepareFileName(fileName);
		}

		//do not change the returntype, native unity depends on this one.
		public virtual CompilerMessage[] GetCompilerMessages()
		{
			if (!Poll())
				Debug.LogWarning("Compile process is not finished yet. This should not happen.");

			DumpStreamOutputToLog();

			return CreateOutputParser().Parse(GetStreamContainingCompilerMessages(), CompilationHadFailure ()).ToArray();
		}
		
		protected bool CompilationHadFailure ()
		{
			return (process.ExitCode!=0);
		}


		protected virtual string[] GetStreamContainingCompilerMessages()
		{
			return GetErrorOutput();
		}

		private void DumpStreamOutputToLog()
		{
			bool hadCompilationFailure = CompilationHadFailure();

			string[] errorOutput = GetErrorOutput();

			if (hadCompilationFailure || errorOutput.Length != 0)
			{
				Console.WriteLine("");
				Console.WriteLine("-----Compiler Commandline Arguments:");
				process.LogProcessStartInfo();
			
				string[] stdOutput = GetStandardOutput();
				
				Console.WriteLine("-----CompilerOutput:-stdout--exitcode: "+process.ExitCode+"--compilationhadfailure: "+hadCompilationFailure+"--outfile: "+_island._output);
				foreach (string line in stdOutput)
					Console.WriteLine(line);

				Console.WriteLine("-----CompilerOutput:-stderr----------");
				foreach (string line in errorOutput)
					Console.WriteLine(line);
				Console.WriteLine("-----EndCompilerOutput---------------");
			}
		}
	}

	/// Marks the type of a [[CompilerMessage]]
	internal enum CompilerMessageType
	{
		/// The message is an error. The compilation has failed.
		Error = 0,
		/// The message is an warning only. If there are no error messages, the compilation has completed successfully.
		Warning = 1
	}

	/// This struct should be returned from GetCompilerMessages() on ScriptCompilerBase implementations
	internal struct CompilerMessage
	{
		/// The text of the error or warning message
		public string message;
		/// The path name of the file the message refers to
		public string file;
		/// The line in the source file the message refers to
		public int line;
		/// The column of the line the message refers to
		public int column;
		/// The type of the message. Either Error or Warning
		public CompilerMessageType type;
	}
}
