using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using UnityEngine;

namespace UnityEditor.Scripting.Compilers
{
	internal abstract class CompilerOutputParserBase
	{
		static protected CompilerMessage CreateInternalCompilerErrorMessage(string[] compileroutput)
		{
			CompilerMessage message;
			message.file = "";
			message.message = String.Join("\n", compileroutput);
			message.type = CompilerMessageType.Error;
			message.line = 0;
			message.column = 0;

			message.message = "Internal compiler error. See the console log for more information. output was:" + message.message;
			return message;
		}

		internal static protected CompilerMessage CreateCompilerMessageFromMatchedRegex(string line, Match m, string erroridentifier)
		{
			CompilerMessage message;
			message.file = m.Groups["filename"].Value;
			message.message = line;
			message.line = Int32.Parse(m.Groups["line"].Value);
			message.column = Int32.Parse(m.Groups["column"].Value);
			message.type = (m.Groups["type"].Value == erroridentifier) ? CompilerMessageType.Error : CompilerMessageType.Warning;
			return message;
		}

		public virtual IEnumerable<CompilerMessage> Parse(string[] errorOutput, bool compilationHadFailure)
		{
			return Parse(errorOutput, new string[0], compilationHadFailure);
		}

		public virtual IEnumerable<CompilerMessage> Parse(string[] errorOutput, string[] standardOutput, bool compilationHadFailure)
		{
			var hasErrors = false;
			var msgs = new List<CompilerMessage>();
			var regex = GetOutputRegex();
			
			foreach (var line in errorOutput)
			{
				//Jamplus can fail with enormous lines in the stdout, parsing of which can take 30! seconds.
				var line2 = line.Length > 1000 ? line.Substring(0, 100) : line;
				
				Match m = regex.Match(line2);
				if (!m.Success) continue;
				CompilerMessage message = CreateCompilerMessageFromMatchedRegex(line, m, GetErrorIdentifier());

				if (message.type == CompilerMessageType.Error)
					hasErrors = true;

				msgs.Add(message);
			}
			if (compilationHadFailure && !hasErrors)
			{
				msgs.Add(CreateInternalCompilerErrorMessage(errorOutput));
			}
			return msgs;
		}

		protected abstract string GetErrorIdentifier();

		protected abstract Regex GetOutputRegex();
	}
}
