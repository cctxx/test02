using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using UnityEngine;
using UnityEditor.Utils;

namespace UnityEditor.Scripting.Compilers
{
	internal abstract class MonoScriptCompilerBase : ScriptCompilerBase
	{
		
		protected MonoScriptCompilerBase (MonoIsland island)
			: base (island)
		{
		}

		protected ManagedProgram StartCompiler (BuildTarget target, string compiler, List<string> arguments)
		{
			return StartCompiler (target, compiler, arguments, true);
		}

		protected ManagedProgram StartCompiler (BuildTarget target, string compiler, List<string> arguments,
		                                        bool setMonoEnvironmentVariables)
		{
			AddCustomResponseFileIfPresent(arguments, Path.GetFileNameWithoutExtension(compiler) + ".rsp");

			var responseFile = CommandLineFormatter.GenerateResponseFile (arguments);
			var monodistro = MonoInstallationFinder.GetMonoInstallation ();
			var program = new ManagedProgram(monodistro, _island._classlib_profile, compiler, " @" + responseFile,
			                                  setMonoEnvironmentVariables) {};
			program.Start ();
			return program;
		}

		protected string GetProfileDirectory ()
		{
			return MonoInstallationFinder.GetProfileDirectory (_island._target, _island._classlib_profile);
		}
	}
}
