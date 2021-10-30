using System;
using System.Linq;
using System.Collections.Generic;
using System.IO;
using UnityEditor.Utils;
using UnityEngine;

namespace UnityEditor.Scripting.Compilers
{

	class MonoCSharpCompiler : MonoScriptCompilerBase
	{
		public MonoCSharpCompiler(MonoIsland island) : base(island)
		{
		}

		override protected Program StartCompiler()
		{
			var arguments = new List<string>
			                	{
			                		"-debug",
			                		"-target:library",
			                		"-nowarn:0169",
			                		"-out:" + PrepareFileName(_island._output)
			                	};
			foreach (string dll in _island._references)
				arguments.Add("-r:" + PrepareFileName(dll));
			foreach (string define in _island._defines)
				arguments.Add("-define:" + define);
			foreach (string source in _island._files)
				arguments.Add(PrepareFileName(source));

            // If additional references are not used in C# files, they won't be added to final package
		    foreach (string reference in GetAdditionalReferences ())
		    {
		        string path = Path.Combine (GetProfileDirectory (), reference);
				if (File.Exists(path)) arguments.Add("-r:" + PrepareFileName(path));
		    }

#if INCLUDE_MONO_2_12
			return StartCompiler(_island._target, GetCompilerPath(arguments), arguments, false);
#else
			return StartCompiler(_island._target, GetCompilerPath(arguments), arguments);
#endif
		}
        private string[] GetAdditionalReferences()
        {
            return new []
                {
                    "System.Runtime.Serialization.dll",
                    "System.Xml.Linq.dll",
                };
        }
		private string GetCompilerPath(List<string> arguments)
		{
#if INCLUDE_MONO_2_12
			string dir = MonoInstallationFinder.GetProfileDirectory(_island._target, "4.5");
			var compilerPath = Path.Combine(dir,"mcs.exe");
			if (File.Exists(compilerPath))
			{
				arguments.Add("-sdk:"+_island._classlib_profile);
				return compilerPath;
			}
#else
			string dir = GetProfileDirectory();
			foreach(var option in new[] {"smcs","gmcs","mcs"})
			{
				var compilerPath = Path.Combine(dir,option+".exe");
				if (File.Exists(compilerPath)) return compilerPath;
			}
#endif
			throw new ApplicationException("Unable to find csharp compiler in "+dir);
		}
		
		protected override CompilerOutputParserBase CreateOutputParser()
		{
			return new MonoCSharpCompilerOutputParser();
		}

		public static string[] Compile(string[] sources, string[] references, string[] defines, string outputFile)
		{
			var island = new MonoIsland(BuildTarget.StandaloneWindows,"unity", sources, references, defines, outputFile);
			using (var c = new MonoCSharpCompiler(island))
			{
				c.BeginCompiling();
				while (!c.Poll())
					System.Threading.Thread.Sleep(50);
				return c.GetCompilerMessages().Select(cm => cm.message).ToArray();
			}
		}
	}
}
