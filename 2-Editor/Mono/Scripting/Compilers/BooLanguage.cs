using Boo.Lang.Parser;
using Boo.Lang.Compiler;
using Boo.Lang.Compiler.Ast;

using System.Linq;

namespace UnityEditor.Scripting.Compilers
{
	internal class BooLanguage : SupportedLanguage
	{
		public override string GetExtensionICanCompile()
		{
			return "boo";
		}
		public override string GetLanguageName()
		{
			return "Boo";
		}

		public override ScriptCompilerBase CreateCompiler(MonoIsland island, bool buildingForEditor, BuildTarget targetPlatform)
		{
			return new BooCompiler(island);
		}

		public override string GetNamespace (string fileName)
		{
			try {
				return BooParser.ParseFile (fileName).Modules.First ().Namespace.Name;
			} catch { }
			
			return base.GetNamespace (fileName);
		}
	}
}