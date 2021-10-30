using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Security;
using System.Security.Cryptography;
using System.Xml;
using System.Text;
using System.Text.RegularExpressions;

using UnityEditor.Scripting;

namespace UnityEditor.VisualStudioIntegration
{
	enum ScriptingLanguage
	{
		None,
		Boo,
		CSharp,
		UnityScript,
	}
	
	class SolutionSynchronizer
	{
		public static readonly ISolutionSynchronizationSettings DefaultSynchronizationSettings =
			new DefaultSolutionSynchronizationSettings();
			
		static readonly string WindowsNewline = "\r\n";

		/// <summary>
		/// Map source extensions to ScriptingLanguages
		/// </summary>
		static readonly Dictionary<string, ScriptingLanguage> SupportedExtensions = new Dictionary<string, ScriptingLanguage>
		{
			{".cs", ScriptingLanguage.CSharp},
			{".js", ScriptingLanguage.UnityScript},
			{".boo", ScriptingLanguage.Boo},
			{".xml", ScriptingLanguage.None},
			{".shader", ScriptingLanguage.None},
			{".compute", ScriptingLanguage.None},
			{".cginc", ScriptingLanguage.None},
			{".glslinc", ScriptingLanguage.None},
			{".txt", ScriptingLanguage.None},
			{".fnt", ScriptingLanguage.None},
			{".cd", ScriptingLanguage.None},
			
			// Also without dots, for your convenience
			{"cs", ScriptingLanguage.CSharp},
			{"js", ScriptingLanguage.UnityScript},
			{"boo", ScriptingLanguage.Boo},
			{"xml", ScriptingLanguage.None},
			{"shader", ScriptingLanguage.None},
			{"compute", ScriptingLanguage.None},
			{"cginc", ScriptingLanguage.None},
			{"glslinc", ScriptingLanguage.None},
			{"txt", ScriptingLanguage.None},
			{"fnt", ScriptingLanguage.None},
			{"cd", ScriptingLanguage.None},
		};
		
		/// <summary>
		/// Map ScriptingLanguages to project extensions
		/// </summary>
		static readonly Dictionary<ScriptingLanguage, string> ProjectExtensions = new Dictionary<ScriptingLanguage, string>
		{
			{ ScriptingLanguage.Boo, ".booproj" },
			{ ScriptingLanguage.CSharp, ".csproj" },
			{ ScriptingLanguage.UnityScript, ".unityproj" },
			{ ScriptingLanguage.None, ".csproj" },
		};
		
		static readonly Regex _MonoDevelopPropertyHeader = new Regex(@"^\s*GlobalSection\(MonoDevelopProperties.*\)");
		public static readonly string MSBuildNamespaceUri = "http://schemas.microsoft.com/developer/msbuild/2003";

		private readonly string _projectDirectory;
		private readonly ISolutionSynchronizationSettings _settings;
		private readonly string _projectName;
		private readonly string vsstub = "-vs";
		private Dictionary<string,string> _pathCache;
		private static readonly string DefaultMonoDevelopSolutionProperties = string.Join ("\r\n", new[]{
"GlobalSection(MonoDevelopProperties) = preSolution",
"		StartupItem = Assembly-CSharp.csproj",
"		Policies = $0",
"		$0.TextStylePolicy = $1",
"		$1.inheritsSet = null",
"		$1.scope = text/x-csharp",
"		$0.CSharpFormattingPolicy = $2",
"		$2.inheritsSet = Mono",
"		$2.inheritsScope = text/x-csharp",
"		$2.scope = text/x-csharp",
"		$0.TextStylePolicy = $3",
"		$3.FileWidth = 120",
"		$3.TabWidth = 4",
"		$3.EolMarker = Unix",
"		$3.inheritsSet = Mono",
"		$3.inheritsScope = text/plain",
"		$3.scope = text/plain",
"	EndGlobalSection",
});
		
		public SolutionSynchronizer(string projectDirectory, ISolutionSynchronizationSettings settings)
		{
			_projectDirectory = projectDirectory;
			_settings = settings;
			_projectName = Path.GetFileName(_projectDirectory);
			_pathCache = new Dictionary<string, string> ();
		}

		public SolutionSynchronizer(string projectDirectory) : this(projectDirectory, DefaultSynchronizationSettings)
		{	
		}

		public bool ShouldFileBePartOfSolution(string file)
		{
			string extension = Path.GetExtension (file);
			
			// Dll's are not scripts but still need to be included..
			if (extension == ".dll")
				return true;
				
			return SupportedExtensions.ContainsKey (extension);
		}

		public bool ProjectExists(MonoIsland island)
		{
			return File.Exists (ProjectFile (island, false));
		}

		public bool SolutionExists()
		{
			return File.Exists (SolutionFile(false));
		}

		private static void DumpIsland (MonoIsland island)
		{
			Console.WriteLine ("{0} ({1})", island._output, island._classlib_profile);
			Console.WriteLine ("Files: ");
			Console.WriteLine (string.Join ("\n", island._files));
			Console.WriteLine ("References: ");
			Console.WriteLine (string.Join ("\n", island._references));
			Console.WriteLine ("");
		}

		/// <summary>
		/// Syncs the scripting solution if any affected files are relevant.
		/// </summary>
		/// <returns>
		/// Whether the solution was synced.
		/// </returns>
		/// <param name='affectedFiles'>
		/// A set of files whose status has changed
		/// </param>
		public bool SyncIfNeeded(IEnumerable<string> affectedFiles)
		{
			// Don't sync if we haven't synced before
			if (SolutionExists () && affectedFiles.Any(ShouldFileBePartOfSolution)) {
				Sync();
				return true;
			}
			
			return false;
		}

		public void Sync()
		{
			// Only synchronize islands that have associated source files
			IEnumerable<MonoIsland> islands = UnityEditorInternal.InternalEditorUtility.GetMonoIslands ().
				Where (i => 0 < i._files.Length);
			
			string otherAssetsProjectPart = GenerateAllAssetProjectPart();
			
			SyncSolution (islands);
			foreach (MonoIsland island in islands)
				SyncProject (island, otherAssetsProjectPart);
		}
		
		string GenerateAllAssetProjectPart ()
		{
			StringBuilder projectBuilder = new StringBuilder ();
			foreach (string asset in AssetDatabase.GetAllAssetPaths ())
			{
				string extension = Path.GetExtension (asset);
				if (SupportedExtensions.ContainsKey (extension) && ScriptingLanguage.None == SupportedExtensions[extension])
				{
					projectBuilder.AppendFormat ("     <None Include=\"{0}\" />{1}", EscapedRelativePathFor (asset), WindowsNewline);
				}
			}
			
			return projectBuilder.ToString();
		}

		void SyncProject(MonoIsland island, string otherAssetsProjectPart)
		{
			SyncFileIfNotChanged (ProjectFile (island, false), ProjectText (island, false, otherAssetsProjectPart));
			SyncFileIfNotChanged (ProjectFile (island, true), ProjectText (island, true, otherAssetsProjectPart));
		}
		
		private static void SyncFileIfNotChanged (string filename, string newContents)
		{
			if (File.Exists (filename) && 
			    newContents == File.ReadAllText (filename)) {
				return;
			}
			File.WriteAllText (filename, newContents);
		}
		
		public static readonly Regex scriptReferenceExpression = new Regex (
			@"^Library.ScriptAssemblies.(?<project>Assembly-(?<language>[^-]+)(?<editor>-Editor)?(?<firstpass>-firstpass)?).dll$",
			RegexOptions.Compiled | RegexOptions.IgnoreCase);
		
		string ProjectText (MonoIsland island, bool forVisualStudio, string allAssetsProject)
		{
			var projectBuilder = new StringBuilder (ProjectHeader (island));
			var references = new List<string> ();
			var projectReferences = new List<Match> ();
			Match match;
			string extension;
			string fullFile;
			
			foreach (string file in island._files) {
				extension = Path.GetExtension (file).ToLower ();
				fullFile = Path.IsPathRooted (file)?  file:  Path.Combine (_projectDirectory, file);
				
				if (".dll" != extension) {
					var tagName = "Compile";
					projectBuilder.AppendFormat ("     <{0} Include=\"{1}\" />{2}", tagName, EscapedRelativePathFor (fullFile), WindowsNewline);
				} else {
					references.Add (fullFile);
				}
			}
			
			projectBuilder.Append(allAssetsProject);


			foreach (string reference in references.Union (island._references))
			{
				if (reference.EndsWith ("/UnityEditor.dll") || reference.EndsWith ("/UnityEngine.dll") || reference.EndsWith ("\\UnityEditor.dll") || reference.EndsWith ("\\UnityEngine.dll")) {
					continue;
				} else if (null != (match = scriptReferenceExpression.Match (reference)) && match.Success) {
					if (!forVisualStudio ||
				    ScriptingLanguage.CSharp == (ScriptingLanguage)Enum.Parse (typeof(ScriptingLanguage), match.Groups["language"].Value, true)) {
						projectReferences.Add (match);
						continue;
					}
				}
				
				string fullReference = Path.IsPathRooted (reference)? reference: Path.Combine (_projectDirectory, reference);
				if (!AssemblyHelper.IsManagedAssembly (fullReference))
					continue;
				if (AssemblyHelper.IsInternalAssembly (fullReference))
					continue;

				string relative = EscapedRelativePathFor (fullReference);
				projectBuilder.AppendFormat (" <Reference Include=\"{0}\">{1}", Path.GetFileNameWithoutExtension (relative), WindowsNewline);
				projectBuilder.AppendFormat (" <HintPath>{0}</HintPath>{1}", relative, WindowsNewline);
				projectBuilder.AppendLine (" </Reference>");
			}
			
			if (0 < projectReferences.Count) {
				string referencedProject;
				projectBuilder.AppendLine ("  </ItemGroup>");
				projectBuilder.AppendLine ("  <ItemGroup>");
				foreach (Match reference in projectReferences) {
					referencedProject = reference.Groups["project"].Value;
					if (forVisualStudio)
						referencedProject += vsstub;
					projectBuilder.AppendFormat ("    <ProjectReference Include=\"{0}{1}\">{2}", referencedProject,
						GetProjectExtension ((ScriptingLanguage)Enum.Parse (typeof(ScriptingLanguage), reference.Groups["language"].Value, true)), WindowsNewline);
					projectBuilder.AppendFormat ("      <Project>{{{0}}}</Project>", ProjectGuid (Path.Combine ("Temp", reference.Groups["project"].Value + ".dll")), WindowsNewline);
					projectBuilder.AppendFormat ("      <Name>{0}</Name>", referencedProject, WindowsNewline);
					projectBuilder.AppendLine ("    </ProjectReference>");
				}
			}

			projectBuilder.Append (ProjectFooter (island));
			return projectBuilder.ToString ();
		}
		
		string GetProperDirectoryCapitalization(DirectoryInfo dirInfo)
		{
			if (dirInfo.FullName == _projectDirectory)
				return dirInfo.FullName;
			if (_pathCache.ContainsKey (dirInfo.FullName))
				return _pathCache[dirInfo.FullName];
			
			DirectoryInfo parentDirInfo = dirInfo.Parent;
			if (null == parentDirInfo)
				return dirInfo.FullName.ToUpperInvariant ();
			return (_pathCache[dirInfo.FullName] = Path.Combine(GetProperDirectoryCapitalization(parentDirInfo), parentDirInfo.GetDirectories ().First (
				dir => dir.Name.Equals (dirInfo.Name, StringComparison.OrdinalIgnoreCase)
			).Name));
		}
		
		public string ProjectFile(MonoIsland island, bool forVisualStudio)
		{
			ScriptingLanguage language = SupportedExtensions[island.GetExtensionOfSourceFiles ()];
			string stub = forVisualStudio? vsstub: string.Empty;
			return Path.Combine(_projectDirectory, string.Format ("{0}{1}{2}", Path.GetFileNameWithoutExtension (island._output), stub, ProjectExtensions[language]));
		}

		internal string SolutionFile(bool onlyCSharp)
		{
			string addendum = onlyCSharp? "-csharp": string.Empty;
			return Path.Combine(_projectDirectory, string.Format ("{0}{1}.sln", _projectName, addendum));
		}

		private string AssetsFolder()
		{
			return Path.Combine(_projectDirectory, "Assets");
		}

		private string ProjectHeader(MonoIsland island)
		{
			string toolsversion = "4.0";
			string productversion = "10.0.20506";
			ScriptingLanguage language = SupportedExtensions[island.GetExtensionOfSourceFiles ()];
			
			if (_settings.VisualStudioVersion == 9)
			{
				toolsversion = "3.5";
				productversion = "9.0.21022";
			}

			return String.Format(_settings.GetProjectHeaderTemplate (language), toolsversion, productversion, ProjectGuid(island._output),
			                     _settings.EngineAssemblyPath,
			                     _settings.EditorAssemblyPath,
			                     string.Join(";", _settings.Defines.Union (island._defines).ToArray ()),
			                     MSBuildNamespaceUri,
			                     Path.GetFileNameWithoutExtension (island._output));
		}

		private void SyncSolution(IEnumerable<MonoIsland> islands)
		{
			SyncFileIfNotChanged (SolutionFile (false), SolutionText (islands, false));
			SyncFileIfNotChanged (SolutionFile (true), SolutionText (islands, true));
		}

		private string SolutionText(IEnumerable<MonoIsland> islands, bool onlyCSharp)
		{
			var fileversion = "11.00";
			if (_settings.VisualStudioVersion == 9) fileversion = "10.00";
			IEnumerable<MonoIsland> relevantIslands = islands.Where (i => (!onlyCSharp || ScriptingLanguage.CSharp == SupportedExtensions[i.GetExtensionOfSourceFiles ()]));
			string projectEntries = GetProjectEntries (relevantIslands, onlyCSharp);
			string projectConfigurations = string.Join (WindowsNewline, relevantIslands.Select (i => GetProjectActiveConfigurations (ProjectGuid(i._output))).ToArray ());
			return String.Format(_settings.SolutionTemplate, fileversion, projectEntries, projectConfigurations, ReadExistingMonoDevelopSolutionProperties ());
		}
		
		/// <summary>
		/// Get a Project("{guid}") = "MyProject", "MyProject.unityproj", "{projectguid}"
		/// entry for each relevant language
		/// </summary>
		private string GetProjectEntries (IEnumerable<MonoIsland> islands, bool forVisualStudio)
		{
			var projectEntries = islands.Select (i => string.Format (
				DefaultSynchronizationSettings.SolutionProjectEntryTemplate,
				SolutionGuid (), _projectName, Path.GetFileName (ProjectFile (i, forVisualStudio)), ProjectGuid (i._output)
			));
			
			return string.Join (WindowsNewline, projectEntries.ToArray ());
		}
		
		/// <summary>
		/// Generate the active configuration string for a given project guid
		/// </summary>
		private string GetProjectActiveConfigurations (string projectGuid)
		{
			return string.Format (
				DefaultSynchronizationSettings.SolutionProjectConfigurationTemplate,
				projectGuid);
		}

		private string EscapedRelativePathFor(FileSystemInfo file)
		{
			return EscapedRelativePathFor (file.FullName);
		}

		private string EscapedRelativePathFor(string file)
		{
			var projectDir = _projectDirectory.Replace("/", "\\");
			file = file.Replace("/", "\\");
			return SecurityElement.Escape(file.StartsWith(projectDir) ? file.Substring(_projectDirectory.Length + 1) : file);
		}

		string ProjectGuid(string assembly)
		{
			return SolutionGuidGenerator.GuidForProject(_projectName + Path.GetFileNameWithoutExtension (assembly));
		}
		
		string SolutionGuid()
		{
			return SolutionGuidGenerator.GuidForSolution(_projectName);
		}

		string ProjectFooter(MonoIsland island)
		{
			ScriptingLanguage language = SupportedExtensions[island.GetExtensionOfSourceFiles ()];
			return string.Format (_settings.GetProjectFooterTemplate (language), ReadExistingMonoDevelopProjectProperties (island));
		}
		
		string ReadExistingMonoDevelopSolutionProperties ()
		{
			if (!SolutionExists ()) return DefaultMonoDevelopSolutionProperties;
			string[] lines;
			try {
				lines = File.ReadAllLines (SolutionFile (false));
			} catch (IOException) {
				return DefaultMonoDevelopSolutionProperties;
			}
			
			StringBuilder existingOptions = new StringBuilder ();
			bool collecting = false;
			
			foreach (string line in lines) {
				if (_MonoDevelopPropertyHeader.IsMatch (line)) {
					collecting = true;
				}
				if (collecting) {
					existingOptions.AppendFormat ("{0}{1}", line, WindowsNewline);
					if (line.Contains ("EndGlobalSection")) {
						collecting = false;
					}
				}
			}
			
			if (0 < existingOptions.Length) {
				return existingOptions.ToString ();
			}
			
			return DefaultMonoDevelopSolutionProperties;
		}
		
		string ReadExistingMonoDevelopProjectProperties (MonoIsland island)
		{
			if (!ProjectExists (island)) return string.Empty;
			XmlDocument doc = new XmlDocument();
			XmlNamespaceManager manager;
			try {
				doc.Load (ProjectFile (island, false));
				manager = new XmlNamespaceManager (doc.NameTable);
				manager.AddNamespace ("msb", MSBuildNamespaceUri);
			} catch (Exception ex) {
				if (ex is IOException ||
				    ex is XmlException)
					return string.Empty;
				throw;
			}
			
			XmlNodeList nodes = doc.SelectNodes ("/msb:Project/msb:ProjectExtensions", manager);
			if (0 == nodes.Count) return string.Empty;
			
			StringBuilder sb = new StringBuilder ();
			foreach (XmlNode node in nodes) {
				sb.AppendLine (node.OuterXml);
			}
			return sb.ToString ();
		}

		[Obsolete ("Use AssemblyHelper.IsManagedAssembly")]
		public static bool IsManagedAssembly (string file)
		{
			return AssemblyHelper.IsManagedAssembly (file);
		}
		
		public static string GetProjectExtension (ScriptingLanguage language)
		{
			if (!ProjectExtensions.ContainsKey (language))
				throw new ArgumentException ("Unsupported language", "language");
			
			return ProjectExtensions[language];
		}
	}

	public static class SolutionGuidGenerator
	{
		public static string GuidForProject(string projectName)
		{
			return ComputeGuidHashFor(projectName + "salt");
		}

		public static string GuidForSolution(string projectName)
		{
			return ComputeGuidHashFor(projectName);
		}

		private static string ComputeGuidHashFor(string input)
		{
			var hash = MD5.Create().ComputeHash(Encoding.Default.GetBytes(input));
			return HashAsGuid(HashToString(hash));
		}

		private static string HashAsGuid(string hash)
		{
			var guid = hash.Substring(0, 8) + "-" + hash.Substring(8, 4) + "-" + hash.Substring(12, 4) + "-" + hash.Substring(16, 4) + "-" + hash.Substring(20, 12);
			return guid.ToUpper();
		}

		private static string HashToString(byte[] bs)
		{
			var sb = new StringBuilder();
			foreach (byte b in bs)
				sb.Append(b.ToString("x2"));
			return sb.ToString();
		}
	}
}
