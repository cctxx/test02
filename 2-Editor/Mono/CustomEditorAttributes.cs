using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using System;
using System.Reflection;

namespace UnityEditor
{

/// Remap Viewed type to inspector type
internal class CustomEditorAttributes
{
	private static readonly List<MonoEditorType> kSCustomEditors = new List<MonoEditorType> ();
	private static readonly List<MonoEditorType> kSCustomMultiEditors = new List<MonoEditorType>();
	private static bool s_Initialized;

	class MonoEditorType
	{
		public Type       m_InspectedType;
		public Type       m_InspectorType;
		public bool       m_EditorForChildClasses;
	}

	internal static Type TryLoadLogicGraphInspector(UnityEngine.Object o)
	{
		#if UNITY_LOGIC_GRAPH
		if (!typeof(MonoBehaviour).IsAssignableFrom(o.GetType()))
			return null;

		if (UnityEditorInternal.InternalGraphUtility.GetEditorGraphData(o.GetInstanceID()) == null)
			return null;

		var asm = AppDomain.CurrentDomain.GetAssemblies().FirstOrDefault(a => a.GetName().Name == "UnityEditor.Graphs");
		if (asm == null)
			return null;

		return asm.GetType("UnityEditor.Graphs.LogicGraph.GraphBehaviourInspector");
		#else
			return null;
		#endif	
	}
	
	internal static Type FindCustomEditorType (UnityEngine.Object o, bool multiEdit)
	{
		if (!s_Initialized)
		{
			var editorAssemblies = EditorAssemblies.loadedAssemblies;
			for (int i=editorAssemblies.Length-1;i>=0;i--)
				Rebuild(editorAssemblies[i]);

			s_Initialized = true;
		}

		Type inspected = o.GetType();
		
		var editors = multiEdit ? kSCustomMultiEditors : kSCustomEditors;

		// pass 1: do we have a class for this custom inspector
		MonoEditorType inspector = editors.FirstOrDefault(x => inspected == x.m_InspectedType);
			
		if (inspector != null)
			return inspector.m_InspectorType;
		
		// pass 2: do we have a class for any of the base classes?
		while (inspected != typeof (UnityEngine.Object))
		{
			inspector = editors.FirstOrDefault (x => inspected == x.m_InspectedType && x.m_EditorForChildClasses);
			if (inspector != null)
				return inspector.m_InspectorType;
			inspected = inspected.BaseType;
		}

		return TryLoadLogicGraphInspector(o);
	}

	internal static void Rebuild (Assembly assembly)
	{
		Type[] types = AssemblyHelper.GetTypesFromAssembly(assembly);			
		foreach (var type in types)
		{
			object[] attrs = type.GetCustomAttributes(typeof(CustomEditor), false);
			foreach (CustomEditor inspectAttr in  attrs)
			{
				var t = new MonoEditorType();
				if (inspectAttr.m_InspectedType == null)
					Debug.Log("Can't load custom inspector " + type.Name + " because the inspected type is null.");
				else if (!type.IsSubclassOf (typeof(Editor)))
				{
					// Suppress a warning on TweakMode, we fucked this up in the default project folder
					// and it's going to be too hard for customers to figure out how to fix it and also quite pointless.
					if (type.FullName == "TweakMode" && type.IsEnum && inspectAttr.m_InspectedType.FullName == "BloomAndFlares")
						continue;
					
					Debug.LogWarning(type.Name + " uses the CustomEditor attribute but does not inherit from Editor.\nYou must inherit from Editor. See the Editor class script documentation.");
				}
				else
				{
					t.m_InspectedType = inspectAttr.m_InspectedType;
					t.m_InspectorType = type;
					t.m_EditorForChildClasses = inspectAttr.m_EditorForChildClasses;
					kSCustomEditors.Add (t);

					if (type.GetCustomAttributes(typeof(CanEditMultipleObjects), false).Length > 0)
						kSCustomMultiEditors.Add (t);
				}
			}
		}
	}
}
}
