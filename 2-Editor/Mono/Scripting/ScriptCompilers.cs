using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using UnityEditor.Scripting.Compilers;
using UnityEngine;
using Object = System.Object;

namespace UnityEditor.Scripting
{
	internal struct SupportedLanguageStruct
	{
		public string extension;
		public string languageName;
	}

	[StructLayout(LayoutKind.Sequential)]
	internal struct MonoIsland
	{
		public readonly BuildTarget _target;
		public readonly string _classlib_profile; //not used yet.
		public readonly string[] _files;
		public readonly string[] _references;
		public readonly string[] _defines;
		public readonly string _output;

		public MonoIsland(BuildTarget target, string classlib_profile, string[] files, string[] references, string[] defines, string output)
		{
			_target = target;
			_classlib_profile = classlib_profile;
			_files = files;
			_references = references;
			_defines = defines;
			_output = output;
		}

		public string GetExtensionOfSourceFiles()
		{
			return ScriptCompilers.GetExtensionOfSourceFile(_files[0]);
		}
	}
	
	internal static class ScriptCompilers
	{
		private static List<SupportedLanguage> _supportedLanguages;

		static ScriptCompilers()
		{
			_supportedLanguages = new List<SupportedLanguage>();
			
			var types = new List<Type>();
			types.Add(typeof(CSharpLanguage));
			types.Add(typeof(BooLanguage));
			types.Add(typeof(UnityScriptLanguage));
			
			foreach(var t in types)
			{
				_supportedLanguages.Add((SupportedLanguage)Activator.CreateInstance(t));
			}
		}

		internal static SupportedLanguageStruct[] GetSupportedLanguageStructs()
		{
			//we communicate with the runtime by xforming our SupportedLaanguage class to a struct, because that's
			//just a lot easier to marshall between native and managed code.
			return _supportedLanguages.Select(lang => new SupportedLanguageStruct
			                                          	{
			                                          		extension = lang.GetExtensionICanCompile(), 
															languageName = lang.GetLanguageName()
			                                          	}).ToArray();
		}

		internal static string GetNamespace(string file)
		{
			if (string.IsNullOrEmpty(file)) throw new ArgumentException("Invalid file");
			
			string extension = GetExtensionOfSourceFile(file);
			foreach (var lang in _supportedLanguages)
			{
				if (lang.GetExtensionICanCompile() == extension)
					return lang.GetNamespace(file);
			}

			throw new ApplicationException("Unable to find a suitable compiler");
		}

		internal static ScriptCompilerBase CreateCompilerInstance(MonoIsland island, bool buildingForEditor, BuildTarget targetPlatform)
		{
			if (island._files.Length==0) throw new ArgumentException("Cannot compile MonoIsland with no files");
			
			foreach(var lang in _supportedLanguages)
			{
				if (lang.GetExtensionICanCompile() == island.GetExtensionOfSourceFiles())
					return lang.CreateCompiler(island,buildingForEditor, targetPlatform);
			}
			
			throw new ApplicationException("Unable to find a suitable compiler");
		}
		
		public static string GetExtensionOfSourceFile(string file)
		{
			var ext = Path.GetExtension(file).ToLower();
			ext = ext.Substring(1); //strip dot
			return ext;
		}
	}
}