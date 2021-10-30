using System.Text.RegularExpressions;

namespace UnityEditor.Scripting.Compilers
{
	internal class MonoCSharpCompilerOutputParser : CompilerOutputParserBase
	{
		private static Regex sCompilerOutput = new Regex(@"\s*(?<filename>.*)\((?<line>\d+),(?<column>\d+)\):\s*(?<type>warning|error)\s*(?<id>[^:]*):\s*(?<message>.*)", RegexOptions.ExplicitCapture | RegexOptions.Compiled);

		protected override Regex GetOutputRegex()
		{
			return sCompilerOutput;
		}

		protected override string GetErrorIdentifier()
		{
			return "error";
		}
	}
}