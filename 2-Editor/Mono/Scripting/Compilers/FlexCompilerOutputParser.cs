using System.Text.RegularExpressions;
using UnityEditor.Scripting.Compilers;

namespace UnityEditor.Scripting.Compilers
{
    internal class FlexCompilerOutputParser : CompilerOutputParserBase
    {
        private static Regex sCompilerOutput = new Regex(@"(?<filename>.*)\((?<line>\d+)\)\: col\: (?<column>\d+) (?<type>Warning|Error): (?<message>.*)", RegexOptions.ExplicitCapture | RegexOptions.Compiled);

        protected override Regex GetOutputRegex()
        {
            return sCompilerOutput;
        }

        protected override string GetErrorIdentifier()
        {
            return "Error";
        }
    }
}