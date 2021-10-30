using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Reflection;
using UnityEditorInternal;
using System;
using System.Runtime.InteropServices;
using System.Runtime.CompilerServices;
using System.Text.RegularExpressions;
using Mono.Cecil;

namespace UnityEditor
{
internal class MonoAOTRegistration
{
	static void ExtractNativeMethodsFromTypes(ICollection<TypeDefinition> types, ArrayList res)
	{
		foreach (TypeDefinition typ in types)
		{
			foreach (MethodDefinition method in typ.Methods)
			{
				if (method.IsStatic && method.IsPInvokeImpl && method.PInvokeInfo.Module.Name.Equals("__Internal"))
				{
					if (res.Contains(method.Name))
						throw new SystemException("Duplicate native method found : " + method.Name + ". Please check your source carefully.");

					res.Add(method.Name);
				}
			}

			if (typ.HasNestedTypes)
				ExtractNativeMethodsFromTypes(typ.NestedTypes, res);
		}
	}

	// Builds list of extern methods that are marked with DllImport("__Internal") attribute
	// @TODO: This needs to be rewritten using cecil
	static ArrayList BuildNativeMethodList(AssemblyDefinition[] assemblies)
	{
		ArrayList res = new ArrayList();

		foreach (AssemblyDefinition ass in assemblies)
		{
			//string assname = ass.Name.Name;
			if (! "System".Equals(ass.Name.Name))
			{
				ExtractNativeMethodsFromTypes(ass.MainModule.Types, res);
			}
		}

		return res;
	}

	static public HashSet<string> BuildReferencedTypeList(AssemblyDefinition[] assemblies)
	{
		HashSet<string> res = new HashSet<string>();

		foreach (AssemblyDefinition ass in assemblies)
		{
			//string assname = ass.Name.Name;
			if (!ass.Name.Name.StartsWith("System") && !ass.Name.Name.Equals("UnityEngine"))
			{
				foreach (TypeReference typ in ass.MainModule.GetTypeReferences())
				{
					res.Add(typ.FullName);
				}
			}
		}

		return res;
	}

	static public void WriteCPlusPlusFileForStaticAOTModuleRegistration(BuildTarget buildTarget, string librariesFolder,
			CrossCompileOptions crossCompileOptions,
			bool advancedLic, string targetDevice, bool stripping, RuntimeClassRegistry usedClassRegistry,
			AssemblyReferenceChecker checker)
	{
        using (TextWriter w = new StreamWriter(Path.Combine(librariesFolder, "RegisterMonoModules.cpp")))
        {
			string[] fileNames = checker.GetAssemblyFileNames ();
			AssemblyDefinition[] assemblies = checker.GetAssemblyDefinitions ();

            bool fastICall = (crossCompileOptions & CrossCompileOptions.FastICall) != 0;

            ArrayList nativeMethods = BuildNativeMethodList(assemblies);

            if (buildTarget == BuildTarget.iPhone)
            {
                w.WriteLine("#include \"RegisterMonoModules.h\"");
            }

            w.WriteLine("");

            if (buildTarget != BuildTarget.PS3 && buildTarget != BuildTarget.XBOX360)
            {
                w.WriteLine("extern bool gEnableGyroscope;");
                w.WriteLine("");
            }


		    w.WriteLine("extern \"C\"\n{");

		    w.WriteLine("	typedef void* gpointer;");
		    w.WriteLine("	typedef int gboolean;");

			w.WriteLine("#if !(TARGET_IPHONE_SIMULATOR)");
            if (buildTarget == BuildTarget.iPhone)
            {
                w.WriteLine("	const char*			UnityIPhoneRuntimeVersion = \"{0}\";", Application.unityVersion);
                w.WriteLine("	void				mono_dl_register_symbol (const char* name, void *addr);");

                w.WriteLine("	extern int 			mono_ficall_flag;");
            }

            w.WriteLine("	void				mono_aot_register_module(gpointer *aot_info);");
            w.WriteLine("	extern gboolean		mono_aot_only;");

		    for (int q=0; q<fileNames.Length; ++q)
		    {
			    string fileName = fileNames[q];
			    string assemblyName = assemblies[q].Name.Name;
			    assemblyName = assemblyName.Replace (".", "_");
			    assemblyName = assemblyName.Replace ("-", "_");
				assemblyName = assemblyName.Replace (" ", "_");

			    w.WriteLine("	extern gpointer*	mono_aot_module_{0}_info; // {1}", assemblyName, fileName);
		    }
			w.WriteLine("#endif // !(TARGET_IPHONE_SIMULATOR)");
			foreach (string nmethod in nativeMethods)
			{
				w.WriteLine("	void	{0}();", nmethod);
			}
		    w.WriteLine("}");

		    w.WriteLine("void RegisterMonoModules()");
		    w.WriteLine("{");

            if (buildTarget != BuildTarget.PS3 && buildTarget != BuildTarget.XBOX360)
            {
                var gyroUser = checker.WhoReferencesClass("UnityEngine.Gyroscope", true);

                if (gyroUser == null)
                {
                    w.WriteLine("    gEnableGyroscope = false;");
                }
                else
                {
                    w.WriteLine("    gEnableGyroscope = true;");
                }
            }

		    w.WriteLine("#if !(TARGET_IPHONE_SIMULATOR)");
	        w.WriteLine(	"	mono_aot_only = true;");

            if (buildTarget == BuildTarget.iPhone)
            {
                w.WriteLine("	mono_ficall_flag = {0};", fastICall ? "true" : "false");
            }

		    foreach (AssemblyDefinition definition in assemblies)
		    {
			    string assemblyName = definition.Name.Name;
			    assemblyName = assemblyName.Replace (".", "_");
			    assemblyName = assemblyName.Replace ("-", "_");
				assemblyName = assemblyName.Replace (" ", "_");
			    w.WriteLine("	mono_aot_register_module(mono_aot_module_{0}_info);", assemblyName);
		    }

		    w.WriteLine(	"");

			if (buildTarget == BuildTarget.iPhone)
			{
				foreach (string nmethod in nativeMethods)
				{
					w.WriteLine("	mono_dl_register_symbol(\"{0}\", (void*)&{0});", nmethod);
				}
			}
			w.WriteLine("#endif // !(TARGET_IPHONE_SIMULATOR)");
		    w.WriteLine("}");
		    w.WriteLine("");

		    AssemblyDefinition unityEngineAssemblyDefinition = null;
		    for (int i=0;i<fileNames.Length;i++)
		    {
			    if (fileNames[i] == "UnityEngine.dll")
				    unityEngineAssemblyDefinition = assemblies[i];
		    }

		    if (buildTarget == BuildTarget.iPhone)
            {
                AssemblyDefinition[] inputAssemblies = { unityEngineAssemblyDefinition };
                GenerateRegisterInternalCalls(inputAssemblies, w);

				// Enrich list of used classes with the .NET ones
				ResolveDefinedNativeClassesFromMono(inputAssemblies, usedClassRegistry);
				ResolveReferencedUnityEngineClassesFromMono(assemblies, unityEngineAssemblyDefinition, usedClassRegistry);

				if (stripping && usedClassRegistry != null)
					GenerateRegisterClassesForStripping(usedClassRegistry, w);
				else
					GenerateRegisterClasses(usedClassRegistry, w);
            }

            w.Close();
        }
	}

	public static void ResolveReferencedUnityEngineClassesFromMono(AssemblyDefinition[] assemblies, AssemblyDefinition unityEngine, RuntimeClassRegistry res)
	{
		if (res == null)
			return;

		foreach (AssemblyDefinition definition in assemblies)
		if (definition != unityEngine)
			{
				foreach (TypeReference typeReference in definition.MainModule.GetTypeReferences())
					if (typeReference.Namespace.StartsWith("UnityEngine"))
					{
						string className = typeReference.Name;
						res.AddMonoClass(className);
					}
			}
	}

	public static void ResolveDefinedNativeClassesFromMono(AssemblyDefinition[] assemblies, RuntimeClassRegistry res)
	{
		if (res == null)
			return;

		foreach (AssemblyDefinition definition in assemblies)
		{
			foreach (TypeDefinition typeDefinition in definition.MainModule.Types)
			{
				// Skip blank types
				if (typeDefinition.Fields.Count > 0 || typeDefinition.Methods.Count > 0 ||
					typeDefinition.Properties.Count > 0)
					{
						string className = typeDefinition.Name;
						res.AddMonoClass(className);
					}
			}
		}
	}

	public static void GenerateRegisterClassesForStripping(RuntimeClassRegistry allClasses, TextWriter output)
	{
		output.Write("void RegisterAllClasses() \n{\n");

		allClasses.SynchronizeClasses();

		foreach (string className in allClasses.GetAllNativeClassesAsString())
		{
			output.WriteLine(string.Format("extern int RegisterClass_{0}();\nRegisterClass_{0}();", className));
		}

		output.Write("\n}\n");
	}

	public static void GenerateRegisterClasses(RuntimeClassRegistry allClasses, TextWriter output)
	{
		output.Write("void RegisterAllClasses() \n{\n");
		output.Write("void RegisterAllClassesIPhone();\nRegisterAllClassesIPhone();\n");
		output.Write("\n}\n");
	}

	// Generate a cpp file that registers all exposed internal calls
	// for example: Register_UnityEngine_AnimationClip_get_length
	//
	public static void GenerateRegisterInternalCalls(AssemblyDefinition[] assemblies, TextWriter output)
	{
		output.Write("void RegisterAllStrippedInternalCalls ()\n{\n");

		foreach (AssemblyDefinition definition in assemblies)
		{
			foreach (TypeDefinition typeDefinition in definition.MainModule.Types)
			{
				foreach (MethodDefinition method in typeDefinition.Methods)
					GenerateInternalCallMethod(typeDefinition, method, output);
			}
		}
		output.Write("\n}\n");
	}

	static void GenerateInternalCallMethod (TypeDefinition typeDefinition, MethodDefinition method, TextWriter output)
	{
		if (!method.IsInternalCall)
			return;

		string registerNameDeclare = string.Format("\tvoid Register_{0}_{1}_{2} ();", typeDefinition.Namespace, typeDefinition.Name, method.Name);
		string registerName = string.Format("\tRegister_{0}_{1}_{2} ();", typeDefinition.Namespace, typeDefinition.Name, method.Name);

		registerName = registerName.Replace('.', '_');
		registerNameDeclare = registerNameDeclare.Replace('.', '_');

		output.WriteLine(registerNameDeclare);
		output.WriteLine(registerName);
	}

	/*
	[MenuItem("Test/GenerateRegisterInternalCalls")]
	public static void GenerateRegisterInternalCallsTest()
	{
		AssemblyDefinition[] definition = { AssemblyFactory.GetAssembly("/Users/joe/Sources/unity-trunk/build/iPhonePlayer/Managed/UnityEngine.dll")};
		GenerateRegisterInternalCalls(definition, new StreamWriter("/test.txt"));
	}
	*/
}
}
