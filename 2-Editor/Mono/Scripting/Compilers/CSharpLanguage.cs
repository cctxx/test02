using ICSharpCode.NRefactory;
using ICSharpCode.NRefactory.Visitors;
using System.IO;
using System.Linq;
using System.Collections.Generic;

namespace UnityEditor.Scripting.Compilers
{
	internal class CSharpLanguage : SupportedLanguage
	{
		public override string GetExtensionICanCompile()
		{
			return "cs";
		}
		public override string GetLanguageName()
		{
			return "CSharp";
		}

		public override ScriptCompilerBase CreateCompiler(MonoIsland island, bool buildingForEditor, BuildTarget targetPlatform)
		{
			// Note: To access Assembly-CSharp-firstpass classes from JS/Boo, Assembly-CSharp-firstpass must be compiled with Mono compiler
			if (!buildingForEditor && targetPlatform.ToString().Contains("MetroPlayer") &&
				 // If UseNetCore is selected, that means user __doesn't need__ Assembly-CSharp-firstpass classes to be accessed from JS/Boo
				 (PlayerSettings.Metro.compilationOverrides == PlayerSettings.MetroCompilationOverrides.UseNetCore ||

				 // If UseNetCorePartially is selected, that means user __needs__ Assembly-CSharp-firstpass classes to be accessed from JS/Boo
				 (PlayerSettings.Metro.compilationOverrides == PlayerSettings.MetroCompilationOverrides.UseNetCorePartially && 
				island._output.Contains("Assembly-CSharp-firstpass.dll") == false)))
                return new MicrosoftCSharpCompiler(island);

			return new MonoCSharpCompiler(island);
		}

		public override string GetNamespace(string fileName)
		{
			using (var parser = ParserFactory.CreateParser(fileName))
			{
				parser.Parse();
				if (parser.Errors.Count == 0)
				{
					var visitor = new NamespaceVisitor();
					VisitorData data = new VisitorData() {TargetClassName = Path.GetFileNameWithoutExtension(fileName)};
					parser.CompilationUnit.AcceptVisitor(visitor, data);
					return string.IsNullOrEmpty(data.DiscoveredNamespace) ? string.Empty : data.DiscoveredNamespace;

				}
			}
			return string.Empty;
		}
		
		class VisitorData
		{
			public VisitorData()
			{
				CurrentNamespaces = new Stack<string>();
			}
			
			public string TargetClassName;
			public Stack<string> CurrentNamespaces;
			public string DiscoveredNamespace;
		}

		class NamespaceVisitor : AbstractAstVisitor
		{
			public override object VisitNamespaceDeclaration(ICSharpCode.NRefactory.Ast.NamespaceDeclaration namespaceDeclaration, object data)
			{
				VisitorData visitorData = (VisitorData)data;
				visitorData.CurrentNamespaces.Push(namespaceDeclaration.Name);
				// Visit children (E.g. TypeDcelarion objects)
				namespaceDeclaration.AcceptChildren(this, visitorData);
				visitorData.CurrentNamespaces.Pop();

				return null;
			}

			public override object VisitTypeDeclaration(ICSharpCode.NRefactory.Ast.TypeDeclaration typeDeclaration, object data)
			{
				VisitorData visitorData = (VisitorData)data;
				if (typeDeclaration.Name == visitorData.TargetClassName)
				{
					string fullNamespace = string.Empty;
					foreach (string ns in visitorData.CurrentNamespaces)
					{
						if (fullNamespace == string.Empty)
							fullNamespace = ns;
						else
							fullNamespace = ns + "." + fullNamespace;
					}
					visitorData.DiscoveredNamespace = fullNamespace;
				}
				return null;
			}
		}
	}
}