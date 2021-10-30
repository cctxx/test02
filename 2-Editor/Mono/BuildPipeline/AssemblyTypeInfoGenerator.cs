// #define DOLOG

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using Mono.Cecil;

namespace UnityEditor
{

public class AssemblyTypeInfoGenerator
{
	[StructLayout (LayoutKind.Sequential)]
	public struct FieldInfo
	{
		public string name;
		public string type;
	};
	
	[StructLayout (LayoutKind.Sequential)]
	public struct ClassInfo
	{
		public string name;
		public FieldInfo[] fields;
	};
	
	AssemblyDefinition assembly_;
	List<ClassInfo> classes_ = new List<ClassInfo>();

	public ClassInfo[] ClassInfoArray { get { return classes_.ToArray(); } }
	
	public AssemblyTypeInfoGenerator(string assembly)
	{
		assembly_ = AssemblyDefinition.ReadAssembly (assembly);
	}

	// In embedded API inner names are separated by dot instead of fwdslash, eg. MyClass.MyInner, not MyClass/MyInner
	// so we convert '/' to '.' here.
	string GetFullTypeName (TypeReference type)
	{
		string typeName = type.FullName;
		// Mono, when getting typename doesn't include ` in the name, so remove it here for now too
		while (true)
		{
			int greve = typeName.IndexOf ('`');
			if (greve == -1)
				break;

			int notNumber = greve+1;
			while (notNumber < typeName.Length)
			{
				if (typeName[notNumber] < '0' || typeName[notNumber] > '9')
					break;
				else
					notNumber++;
			}

			typeName = typeName.Remove (greve, notNumber - greve);
		}

		return typeName.Replace ('/', '.');
	}

#if DOLOG
	int indentLevel = 0;
	void DoIndent () { for (int i=0; i<indentLevel; ++i) Console.Write ("  "); }
	void ChangeIndent (int diff) { indentLevel += diff; }
#endif
		
	private FieldInfo[] getFields (TypeDefinition type)
	{
#if DOLOG
		ChangeIndent (1);
#endif

		List<FieldInfo> fields = new List<FieldInfo>();
			
		// Fetch info for inner types first
		foreach (TypeDefinition innerType in type.NestedTypes)
		{
			ClassInfo ci = new ClassInfo ();
			ci.name = GetFullTypeName (innerType);
			ci.fields = getFields (innerType);

#if DOLOG
			DoIndent (); Console.WriteLine ("InnerType '{0}' {1} fields", ci.name, ci.fields.Length);
#endif

			classes_.Add (ci);
		}

		// Get fields for the class itself
		foreach (FieldDefinition field in type.Fields)
		{
#if DOLOG
			foreach (CustomAttribute ca in field.CustomAttributes)
			{
				DoIndent (); Console.WriteLine ("[" + ca.AttributeType.Name + "]");
			}
#endif

			// Do not record static fields
			if (field.IsStatic)
				continue;

			if (field.IsInitOnly)
				continue;

			if ((field.Attributes & FieldAttributes.NotSerialized) != 0)
				continue;

			bool hasSerializeField = false;
			foreach (CustomAttribute ca in field.CustomAttributes)
			{
				if (ca.AttributeType.Name.CompareTo("SerializeField") == 0)
				{
					hasSerializeField = true;
					break;
				}
			}

			// Do not record private and non-serializable fields
			if (!field.IsPublic && !hasSerializeField)
				continue;

			FieldInfo ti = new FieldInfo ();
			ti.name = field.Name;
			ti.type = GetFullTypeName (field.FieldType);

			fields.Add (ti);
			
#if DOLOG
			DoIndent (); Console.WriteLine ("{0} {1}", ti.type, ti.name);
#endif				
		}
			
#if DOLOG
		ChangeIndent (-1);
#endif

		return fields.ToArray ();
	}

	public void gatherClassInfo ()
	{
		foreach (ModuleDefinition module in assembly_.Modules)
		{
			foreach (TypeDefinition type in module.Types)
			{
#if DOLOG
				DoIndent (); Console.WriteLine ("Type '{0}' {1} fields", type.FullName, type.Fields.Count);
#endif
					
				// Skip compiler-generated <Module> class
				if (type.Name == "<Module>")
					continue;
				
/*				bool serialize = (type.Attributes & TypeAttributes.Serializable) != 0;
				if (!serialize)
					continue;
*/
				
				ClassInfo ci = new ClassInfo ();
				ci.name = GetFullTypeName (type);
				ci.fields = getFields (type);
				classes_.Add (ci);
			}
		}
	}
};

}
