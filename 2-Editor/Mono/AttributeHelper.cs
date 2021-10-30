using System.Collections.Generic;
using System.Linq;
using UnityEngine;
using System;
using System.Collections;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace UnityEditor
{

internal class AttributeHelper
{
	[StructLayout (LayoutKind.Sequential)]
	struct MonoGizmoMethod
	{
		public MethodInfo drawGizmo;
		public Type       drawnType;
		public int        options;
	}

	[StructLayout (LayoutKind.Sequential)]
	struct MonoMenuItem
	{
		public string menuItem;
		public string execute;
		public string validate;
		public int    priority;
		public int    index;
		public Type   type;
	}

	static MonoGizmoMethod[] ExtractGizmos (Assembly assembly)
	{
		ArrayList commands = new ArrayList ();

		Type[] types = AssemblyHelper.GetTypesFromAssembly(assembly);
		foreach (Type klass in types)
		{
			MethodInfo[] methods = klass.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			
			// Iterate all methods
			// - Find static gizmo commands with attributes
			// - Add them to the commands list
			for  (int i =  0; i < methods.GetLength(0); i++)
			{
				MethodInfo mi = methods[i];
					
				object[] attrs = mi.GetCustomAttributes(typeof(DrawGizmo), false);
				foreach (DrawGizmo gizmoAttr in  attrs)
				{
					ParameterInfo[] parameters = mi.GetParameters();
					if (parameters.Length != 2)
						continue;

					//@TODO: add type checking error messages.
					MonoGizmoMethod item = new MonoGizmoMethod ();
					
					if (gizmoAttr.drawnType == null)
						item.drawnType = parameters[0].ParameterType;
					else if (gizmoAttr.drawnType.IsSubclassOf (parameters[0].ParameterType))
						item.drawnType = gizmoAttr.drawnType;
					else
						continue;

					if (parameters[1].ParameterType != typeof(GizmoType) && parameters[1].ParameterType != typeof(int))
						continue;
					
					item.drawGizmo = mi;
					item.options = (int)gizmoAttr.drawOptions;
					
					commands.Add (item);
				}
			}
		}

		// Copy to doesn't seem to work - arg
		MonoGizmoMethod[] output = new MonoGizmoMethod[commands.Count];
		int a=0;
		foreach (MonoGizmoMethod item in commands)
			output[a++] = item;
		return output;
	}

	static MonoMenuItem[] ExtractMenuCommands (Assembly assembly)
	{
		bool includeInternalMenus = EditorPrefs.GetBool ("InternalMode", false);
		
		Hashtable commands = new Hashtable ();
		
		Type[] types = AssemblyHelper.GetTypesFromAssembly(assembly);
		foreach (Type klass in types)
		{
			MethodInfo[] methods = klass.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			
			// Iterate all methods
			// - Find static menu commands with attributes
			// - Add them into the commands hash
			for  (int i =  0; i < methods.GetLength(0); i++)
			{
				MethodInfo mi = methods[i];
					
				object[] attrs = mi.GetCustomAttributes(typeof(MenuItem), false);
				foreach (MenuItem menuAttr in  attrs)
				{
					MonoMenuItem item = new MonoMenuItem ();
					if (commands[menuAttr.menuItem] != null)
						item = (MonoMenuItem)commands[menuAttr.menuItem];
					
					if (menuAttr.menuItem.StartsWith ("internal:", StringComparison.Ordinal))
					{
						if (!includeInternalMenus)
							continue;
						item.menuItem = menuAttr.menuItem.Substring (9);
					}
					else
						item.menuItem = menuAttr.menuItem;
					item.type = klass;
					if (menuAttr.validate) {
						item.validate = mi.Name;
					} else {
						item.execute = mi.Name;
						item.index = i;
						item.priority = menuAttr.priority;
					}
					commands[menuAttr.menuItem] = item;
				}
			}
		}

		// Copy to doesn't seem to work - arg
		MonoMenuItem[] output = new MonoMenuItem[commands.Count];
		int a=0;
		foreach (MonoMenuItem item in commands.Values)
		{
			output[a++] = item;
		}
		
		Array.Sort(output, new CompareMenuIndex());
		
		return output;
	}
	
	internal class CompareMenuIndex : IComparer  {
		int IComparer.Compare( System.Object xo, System.Object yo )  {
			MonoMenuItem mix = (MonoMenuItem)xo;
			MonoMenuItem miy = (MonoMenuItem)yo;
			if (mix.priority != miy.priority)
				return mix.priority.CompareTo (miy.priority);
			return mix.index.CompareTo (miy.index);
		}
	}
	static MonoMenuItem[] ExtractContextMenu (Type klass)
	{
		Hashtable commands = new Hashtable ();

		MethodInfo[] methods = klass.GetMethods(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
		
		for  (int i =  0; i < methods.GetLength(0); i++)
		{
			MethodInfo mi = methods[i];
				
			object[] attrs = mi.GetCustomAttributes(typeof(ContextMenu), false);
			foreach (ContextMenu menuAttr in  attrs)
			{
				MonoMenuItem item = new MonoMenuItem ();
				if (commands[menuAttr.menuItem] != null)
					item = (MonoMenuItem)commands[menuAttr.menuItem];
				
				item.menuItem = menuAttr.menuItem;
				item.type = klass;
				item.execute = mi.Name;
				commands[menuAttr.menuItem] = item;
			}
		}
		
		// Copy to doesn't seem to work - arg
		MonoMenuItem[] output = new MonoMenuItem[commands.Count];
		int a=0;
		foreach (MonoMenuItem item in commands.Values)
		{
			output[a++] = item;
		}
		return output;
	}

	static string GetComponentMenuName (Type klass)
	{
		object[] attrs = klass.GetCustomAttributes(typeof(AddComponentMenu), false);
		if (attrs.Length > 0)
		{
			AddComponentMenu menu = (AddComponentMenu)attrs[0];
			return menu.componentMenu;
		}
		else
			return null;
	}
	
	static internal ArrayList FindEditorClassesWithAttribute (Type attrib)
	{
		var attributedKlasses = new ArrayList();
		foreach (var klass in EditorAssemblies.loadedTypes)
		{
			if (klass.GetCustomAttributes(attrib, false).Length != 0)
				attributedKlasses.Add(klass);
		}
		return attributedKlasses;
	}

	static internal void InvokeStaticMethod (Type type, string methodName, object[] arguments)
	{
		MethodInfo method = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
		
		if (method != null)
		{
			method.Invoke(null, arguments);
		}
	}

	static internal void InvokeMethod (Type type, string methodName, object[] arguments)
	{
		MethodInfo method = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
		
		if (method != null)
		{
			method.Invoke(null, arguments);
		}
	}
	
	static internal object InvokeMemberIfAvailable (object target, string methodName, object[] args)
	{
		MethodInfo method = target.GetType ().GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance);
		if (method != null)
		{
			return method.Invoke(target, args);
		}
		else
		{
			return null;
		}
	}

/*
	static internal void InvokeStaticEditorMethodsWithMethodName (string methodName, object[] arguments)
	{
		ArrayList attributedKlasses = new ArrayList();
		foreach (Assembly ass in m_EditorAssemblies)
		{	
			Type[] types = ass.GetTypes ();
			foreach (Type klass in types)
			{
				MethodInfo method = klass.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
				
				if (method != null)
				{
					method.Invoke(null, arguments);
				}
			}
		}
	}
*/
	
	
}
}