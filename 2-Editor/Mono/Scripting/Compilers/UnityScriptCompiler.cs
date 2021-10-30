using System;
using System.Collections.Generic;
using System.IO;
using System.Text.RegularExpressions;
using UnityEditor.Utils;

namespace UnityEditor.Scripting.Compilers
{
	class UnityScriptCompiler : MonoScriptCompilerBase
	{
		private static readonly Regex UnityEditorPattern = new Regex(@"UnityEditor\.dll$", RegexOptions.ExplicitCapture);

		public UnityScriptCompiler(MonoIsland island)
			: base(island)
		{
		}

		protected override CompilerOutputParserBase CreateOutputParser()
		{
			return new UnityScriptCompilerOutputParser();
		}

		override protected Program StartCompiler()
		{
			var arguments = new List<string>
        	{
        		"-target:library",
        		"-i:UnityEngine",
        		"-i:System.Collections",
        		"-base:UnityEngine.MonoBehaviour",
        		"-nowarn:BCW0016",
        		"-nowarn:BCW0003",
        		"-method:Main",
        		"-out:" + _island._output,
                "-x-type-inference-rule-attribute:" + typeof(UnityEngineInternal.TypeInferenceRuleAttribute)
        	};

            // If we would compile with debug information for Metro, we would fail WACK
            if (CompilingForMetro () == false)
            {
                arguments.Add("-debug");
            }
		   
			if (StrictBuildTarget())
				arguments.Add("-pragmas:strict,downcast");
			
			foreach (var define in _island._defines)
				arguments.Add("-define:" + define);
			
			foreach (var dll in _island._references)
				arguments.Add("-r:" + PrepareFileName(dll));

			var compilingEditorScripts = Array.Exists(_island._references, UnityEditorPattern.IsMatch);
			if (compilingEditorScripts)
				arguments.Add("-i:UnityEditor");
			else if (!BuildPipeline.IsUnityScriptEvalSupported(_island._target))
				arguments.Add(string.Format("-disable-eval:eval is not supported on the current build target ({0}).", _island._target));

			foreach (string source in _island._files)
				arguments.Add(PrepareFileName(source));

			var compilerPath = Path.Combine(GetProfileDirectory(), "us.exe");
			return StartCompiler(_island._target, compilerPath, arguments);
		}

		private bool StrictBuildTarget()
		{
			return Array.IndexOf(_island._defines, "ENABLE_DUCK_TYPING") == -1;
		}

		protected override string[] GetStreamContainingCompilerMessages()
		{
			return GetStandardOutput();
		}
		
	}

}


	
