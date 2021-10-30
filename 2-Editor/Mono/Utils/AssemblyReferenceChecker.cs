using UnityEngine;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using Mono.Cecil;

namespace UnityEditor
{
	internal class AssemblyReferenceChecker
	{
		private HashSet<string> referencedMethods = new HashSet<string> ();
		private HashSet<string> referencedTypes = new HashSet<string> ();
		private HashSet<string> definedMethods = new HashSet<string> ();
		private List<AssemblyDefinition> assemblyDefinitions = new List<AssemblyDefinition> ();
		private List<string> assemblyFileNames = new List<string> ();
		private DateTime startTime = DateTime.MinValue;

		public AssemblyReferenceChecker ()
		{}

		public void CollectReferences (string path, bool withMethods,
		                               float progressValue, bool ignoreSystemDlls)
		{
			assemblyDefinitions = new List<AssemblyDefinition> ();
			
			string[] fileNames = Directory.Exists(path) ? Directory.GetFiles (path) : new string[0];

			foreach (var fileName in fileNames)
			{
				if (Path.GetExtension (fileName) != ".dll")
					continue;

				AssemblyDefinition assembly = AssemblyDefinition.ReadAssembly(fileName);

				if (ignoreSystemDlls && IsiPhoneIgnoredSystemDll (assembly.Name.Name))
					continue;

				assemblyFileNames.Add (Path.GetFileName (fileName));
				assemblyDefinitions.Add (assembly);
			}

			var arr = assemblyDefinitions.ToArray ();
			referencedTypes = MonoAOTRegistration.BuildReferencedTypeList (arr);

			if (withMethods)
				CollectReferencedMethods (arr, referencedMethods, definedMethods, progressValue);
		}
		
		private void CollectReferencedMethods (AssemblyDefinition[] definitions, 
											HashSet<string> referencedMethods, HashSet<string> definedMethods, 
		                                    float progressValue)
		{
			foreach (var ass in definitions)
				foreach (TypeDefinition typ in ass.MainModule.Types)
					CollectReferencedMethods(typ, referencedMethods, definedMethods, progressValue);
		}

		private void CollectReferencedMethods (TypeDefinition typ, HashSet<string> referencedMethods, HashSet<string> definedMethods, float progressValue)
		{
			DisplayProgress (progressValue);
			foreach (TypeDefinition nestedTyp in typ.NestedTypes)
				CollectReferencedMethods(nestedTyp, referencedMethods, definedMethods, progressValue);

			foreach (MethodDefinition method in typ.Methods)
			{
				if (!method.HasBody)
					continue;

				foreach (Mono.Cecil.Cil.Instruction instr in method.Body.Instructions)
					if (Mono.Cecil.Cil.OpCodes.Call == instr.OpCode)
						referencedMethods.Add (instr.Operand.ToString ());

				definedMethods.Add(method.ToString());
			}
		}

		private void DisplayProgress (float progressValue)
		{
			var elapsedTime = DateTime.Now - startTime;
			var progress_strings = new[] {"Fetching assembly references",
			                              "Building list of referenced assemblies..."};

			if (elapsedTime.TotalMilliseconds >= 100)
			{
				if (EditorUtility.DisplayCancelableProgressBar (progress_strings[0],
				                                                progress_strings[1],
				                                                progressValue))
					throw new Exception (progress_strings[0] + " aborted");

				startTime = DateTime.Now;
			}
		}

		public bool HasReferenceToMethod (string methodName)
		{
			return referencedMethods.Any (item => item.Contains (methodName));
		}
		
		public bool HasDefinedMethod (string methodName)
		{
			return definedMethods.Any (item => item.Contains (methodName));
		}
		
		public bool HasReferenceToType (string typeName)
		{
			return referencedTypes.Any (item => item.StartsWith (typeName));
		}

		public AssemblyDefinition[] GetAssemblyDefinitions ()
		{
			return assemblyDefinitions.ToArray ();
		}

		public string[] GetAssemblyFileNames ()
		{
			return assemblyFileNames.ToArray ();
		}

		public string WhoReferencesClass (string klass, bool ignoreSystemDlls)
		{
			foreach (var assembly in assemblyDefinitions)
			{
				if (ignoreSystemDlls && IsiPhoneIgnoredSystemDll (assembly.Name.Name))
					continue;

				var arr = new AssemblyDefinition[] {assembly};
				var types = MonoAOTRegistration.BuildReferencedTypeList (arr);

				if (types.Any (item => item.StartsWith (klass)))
					return assembly.Name.Name;
			}

			return null;
		}

		private bool IsiPhoneIgnoredSystemDll (string name)
		{
			return name.StartsWith ("System")
			       || name.Equals ("UnityEngine")
			       || name.Equals ("Mono.Posix");
		}
		
		public static bool GetScriptsHaveMouseEvents(string path)
		{
			AssemblyReferenceChecker checker = new AssemblyReferenceChecker();
			checker.CollectReferences(path, true, 0.0f, true);

			return checker.HasDefinedMethod("OnMouse");
		}
	}
}
