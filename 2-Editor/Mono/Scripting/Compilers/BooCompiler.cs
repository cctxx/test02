using System.Collections.Generic;
using System.IO;
using UnityEditor.Utils;

namespace UnityEditor.Scripting.Compilers {

	class BooCompiler : MonoScriptCompilerBase
	{
		
		public BooCompiler(MonoIsland island) : base(island)
		{
		}
		
		override protected Program StartCompiler()
		{
			var arguments = new List<string>
			{
				"-debug",
				"-target:library",
				"-out:" + _island._output,
				"-x-type-inference-rule-attribute:" + typeof(UnityEngineInternal.TypeInferenceRuleAttribute)
			};

			foreach (string dll in _island._references)
				arguments.Add("-r:" + PrepareFileName(dll));
			foreach(string define in _island._defines) 
				arguments.Add("-define:" + define);
			foreach(string source in _island._files)
				arguments.Add(PrepareFileName(source));

			string compilerPath = Path.Combine(GetProfileDirectory(), "booc.exe");
			return StartCompiler(_island._target, compilerPath, arguments);

		}

		protected override CompilerOutputParserBase CreateOutputParser()
		{
			return new BooCompilerOutputParser();
		}
	}

}
