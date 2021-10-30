using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using System.Text;
using UnityEditor.Utils;
using UnityEditor;

namespace UnityEditor.Scripting.Compilers
{
	internal class MicrosoftCSharpCompiler : ScriptCompilerBase
	{
        public MicrosoftCSharpCompiler(MonoIsland island)
            : base(island)
		{
		}
		internal static string WindowsDirectory
		{
			get
			{
				return System.Environment.GetEnvironmentVariable("windir");
			}
		}

		internal static string ProgramFilesDirectory
		{
			get
			{
				string mainLetter = Path.GetFullPath(WindowsDirectory + "\\..\\..");

				string programFilesOS64 = mainLetter + "Program Files (x86)";
				string programFilesOS32 = mainLetter + "Program Files";
				string programFiles;
				// First check Program Files (x86) if such directory doesn't exist, it means we're on 32 bit system.
				if (Directory.Exists(programFilesOS64)) programFiles = programFilesOS64;
				else if (Directory.Exists(programFilesOS32)) programFiles = programFilesOS32;
				else
				{
					throw new System.Exception("Path '" + programFilesOS64 + "' or '" + programFilesOS32 + "' doesn't exist.");
				}
				return programFiles;
			}
		}
		internal static string NETCoreFrameworkReferencesDirectory
		{
			get 
			{
				switch (EditorUserBuildSettings.metroSDK)
				{
					case MetroSDK.SDK80: return ProgramFilesDirectory + @"\Reference Assemblies\Microsoft\Framework\.NETCore\v4.5";
					case MetroSDK.SDK81: return ProgramFilesDirectory + @"\Reference Assemblies\Microsoft\Framework\.NETCore\v4.5.1";
					default: throw new Exception("Unknown Windows SDK: " + EditorUserBuildSettings.metroSDK.ToString());
				}
				
			}
		}

		private string[] GetNETMetroAssemblies ()
        {
            return new[]
                {
                    "Microsoft.CSharp.dll",
                    "Microsoft.VisualBasic.dll",
                    "mscorlib.dll",
                    "System.Collections.Concurrent.dll",
                    "System.Collections.dll",
                    "System.ComponentModel.Annotations.dll",
                    "System.ComponentModel.DataAnnotations.dll",
                    "System.ComponentModel.dll",
                    "System.ComponentModel.EventBasedAsync.dll",
                    "System.Core.dll",
                    "System.Diagnostics.Contracts.dll",
                    "System.Diagnostics.Debug.dll",
                    "System.Diagnostics.Tools.dll",
                    "System.Diagnostics.Tracing.dll",
                    "System.dll",
                    "System.Dynamic.Runtime.dll",
                    "System.Globalization.dll",
                    "System.IO.Compression.dll",
                    "System.IO.dll",
                    "System.Linq.dll",
                    "System.Linq.Expressions.dll",
                    "System.Linq.Parallel.dll",
                    "System.Linq.Queryable.dll",
                    "System.Net.dll",
                    "System.Net.Http.dll",
                    "System.Net.Http.Rtc.dll",
                    "System.Net.NetworkInformation.dll",
                    "System.Net.Primitives.dll",
                    "System.Net.Requests.dll",
                    "System.Numerics.dll",
                    "System.ObjectModel.dll",
                    "System.Reflection.Context.dll",
                    "System.Reflection.dll",
                    "System.Reflection.Extensions.dll",
                    "System.Reflection.Primitives.dll",
                    "System.Resources.ResourceManager.dll",
                    "System.Runtime.dll",
                    "System.Runtime.Extensions.dll",
                    "System.Runtime.InteropServices.dll",
                    "System.Runtime.InteropServices.WindowsRuntime.dll",
                    "System.Runtime.Numerics.dll",
                    "System.Runtime.Serialization.dll",
                    "System.Runtime.Serialization.Json.dll",
                    "System.Runtime.Serialization.Primitives.dll",
                    "System.Runtime.Serialization.Xml.dll",
                    "System.Runtime.WindowsRuntime.dll",
                    "System.Runtime.WindowsRuntime.UI.Xaml.dll",
                    "System.Security.Principal.dll",
                    "System.ServiceModel.dll",
                    "System.ServiceModel.Duplex.dll",
                    "System.ServiceModel.Http.dll",
                    "System.ServiceModel.NetTcp.dll",
                    "System.ServiceModel.Primitives.dll",
                    "System.ServiceModel.Security.dll",
                    "System.ServiceModel.Web.dll",
                    "System.Text.Encoding.dll",
                    "System.Text.Encoding.Extensions.dll",
                    "System.Text.RegularExpressions.dll",
                    "System.Threading.dll",
                    "System.Threading.Tasks.dll",
                    "System.Threading.Tasks.Parallel.dll",
                    "System.Windows.dll",
                    "System.Xml.dll",
                    "System.Xml.Linq.dll",
                    "System.Xml.ReaderWriter.dll",
                    "System.Xml.Serialization.dll",
                    "System.Xml.XDocument.dll",
                    "System.Xml.XmlSerializer.dll",
                };
        }
        private string GetNetMetroPredefines ()
        {
            return string.Join ("\r\n", new[]
                {
                    @"using System.Runtime.Serialization;",
                    @"using System.Runtime.InteropServices;",
                    @"#if NETFX_CORE",
                    @"namespace System",
                    @"{",
                    @"    [AttributeUsageAttribute(AttributeTargets.Class | AttributeTargets.Struct | AttributeTargets.Enum | AttributeTargets.Delegate,Inherited = false)]"
                    ,
                    @"    [ComVisible(true)]",
                    @"    public sealed class SerializableAttribute : Attribute",
                    @"    {}",
                    @"",
                    @"    public sealed class NonSerializedAttribute : Attribute",
                    @"    {}",
                    @"}",
                    @"#endif"
                });
        }
        private string GetNetMetroAssemblyInfoWindows80()
        {
            return string.Join("\r\n", new[]
                {
                    @"using System;",
                    @" using System.Reflection;",
                    @"[assembly: global::System.Runtime.Versioning.TargetFrameworkAttribute("".NETCore,Version=v4.5"", FrameworkDisplayName = "".NET for Windows Store apps"")]"
                });
        }
		private string GetNetMetroAssemblyInfoWindows81()
		{
			return string.Join("\r\n", new[]
                {
                    @"using System;",
                    @" using System.Reflection;",
                    @"[assembly: global::System.Runtime.Versioning.TargetFrameworkAttribute("".NETCore,Version=v4.5.1"", FrameworkDisplayName = "".NET for Windows Store apps (Windows 8.1)"")]"
                });
		}
		override protected Program StartCompiler()
		{
			var arguments = new List<string>
			                	{
                                    "/target:library",
			                		"/nowarn:0169",
			                		"/out:" + PrepareFileName(_island._output)
			                	};

			// Always build with "/debug:pdbonly", "/optimize+", because even if the assembly is optimized
			// it seems you can still succesfully debug C# scripts in VS2012 on Metro
			// Don't delete this code yet, it might be still needed
			//if (_island._defines.Contains("DEVELOPMENT_BUILD"))
			//	arguments.InsertRange (0, new string[] {"/debug:full", "/optimize-", "/debug+"});
			//else
			string windir = WindowsDirectory;

			arguments.InsertRange(0, new string[]{"/debug:pdbonly", "/optimize+" });
			
			string argsPrefix = "";
			if (CompilingForMetro() &&
				(PlayerSettings.Metro.compilationOverrides == PlayerSettings.MetroCompilationOverrides.UseNetCore ||
				 PlayerSettings.Metro.compilationOverrides == PlayerSettings.MetroCompilationOverrides.UseNetCorePartially))
			{
                argsPrefix = "/noconfig "; // This will ensure that csc.exe won't include C:\Windows\Microsoft.NET\Framework\v4.0.30319\csc.rsp
                                           // csc.rsp references .NET 4.5 assemblies which cause conflicts for us
                arguments.Add("/nostdlib+");
                arguments.Add("/define:NETFX_CORE");

				string windowsWinmdPath;
				string windowsSDKTag;
				switch (EditorUserBuildSettings.metroSDK)
				{
					case MetroSDK.SDK80:
						windowsSDKTag = "8.0";
						windowsWinmdPath = ProgramFilesDirectory + "\\Windows Kits\\8.0\\References\\CommonConfiguration\\Neutral\\Windows.winmd";
						break;
					case MetroSDK.SDK81:
						windowsSDKTag = "8.1";
						windowsWinmdPath = ProgramFilesDirectory + "\\Windows Kits\\8.1\\References\\CommonConfiguration\\Neutral\\Windows.winmd";
						break;
					default:
						throw new Exception("Unknown Windows SDK: " + EditorUserBuildSettings.metroSDK.ToString());
				}

				if (!File.Exists(windowsWinmdPath))
				{
					throw new Exception(string.Format("'{0}' not found, do you have Windows {1} SDK installed?",
													windowsWinmdPath, windowsSDKTag));
				}

                arguments.Add("/reference:\"" + windowsWinmdPath + "\"");
			    string prefix = NETCoreFrameworkReferencesDirectory + "\\";
                foreach (string dll in GetNETMetroAssemblies())
                    arguments.Add("/reference:\"" + prefix + dll +"\"");

			    string predefines = Path.Combine(Path.GetTempPath (), "Predefines.cs");
                if (File.Exists(predefines)) File.Delete(predefines);
                File.WriteAllText(predefines, GetNetMetroPredefines());
                arguments.Add(predefines);

				string assemblyInfo;
				string assemblyInfoSource;
				switch (EditorUserBuildSettings.metroSDK)
				{
					case MetroSDK.SDK80:
						assemblyInfo = Path.Combine(Path.GetTempPath(), ".NETCore,Version=v4.5.AssemblyAttributes.cs");
						assemblyInfoSource = GetNetMetroAssemblyInfoWindows80();
						break;
					case MetroSDK.SDK81:
						assemblyInfo = Path.Combine(Path.GetTempPath(), ".NETCore,Version=v4.5.1.AssemblyAttributes.cs");
						assemblyInfoSource = GetNetMetroAssemblyInfoWindows81();
						break;
					default:
						throw new Exception("Unknown Windows SDK: " + EditorUserBuildSettings.metroSDK.ToString());
				}

                if (File.Exists(assemblyInfo)) File.Delete(assemblyInfo);
				File.WriteAllText(assemblyInfo, assemblyInfoSource);
                arguments.Add(assemblyInfo);
			}

            foreach (string dll in _island._references)
                arguments.Add("/reference:" + PrepareFileName(dll));

            foreach (string define in _island._defines)
                arguments.Add("/define:" + define);

            foreach (string source in _island._files)
            {
                arguments.Add(PrepareFileName(source).Replace('/', '\\'));
            }
            string csc = Path.Combine(windir, @"Microsoft.NET\Framework\v4.0.30319\Csc.exe");

            if (!File.Exists(csc))
            {
                throw new System.Exception("'" + csc + "' not found, either .NET 4.5 is not insalled or your OS is not Windows 8/8.1.");
            }

			AddCustomResponseFileIfPresent(arguments, "csc.rsp");

			var responseFile = CommandLineFormatter.GenerateResponseFile(arguments);
			var psi = new ProcessStartInfo() { Arguments = argsPrefix + "@" + responseFile, FileName = csc };
		    var program = new Program(psi);
		    program.Start();
		    return program;
		}
		protected override string[] GetStreamContainingCompilerMessages()
		{
			return GetStandardOutput();
		}
	    protected override CompilerOutputParserBase CreateOutputParser()
		{
			return new MicrosoftCSharpCompilerOutputParser();
		}
	}
}