using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.CompilerServices;
using UnityEngine;

namespace UnityEditor
{
	/// <summary>
	/// Marks a class as requiring eager initialization.
	/// 
	/// Classes marked with this attribute will have their static constructors
	/// executed whenever assemblies are loaded or reloaded.
	/// 
	/// Very useful for event (re)wiring.
	/// </summary>
	[AttributeUsage(AttributeTargets.Class)]
	public class InitializeOnLoadAttribute : Attribute
	{
	}

	/// <summary>
	/// Holds information about the current set of editor assemblies.
	/// </summary>
	static class EditorAssemblies
	{
		/// <summary>
		/// The currently loaded editor assemblies
		/// (This is kept up to date from <see cref="SetLoadedEditorAssemblies"/>)
		/// </summary>
		static internal Assembly[] loadedAssemblies
		{
			get; private set;
		}

		static internal IEnumerable<Type> loadedTypes
		{
			get { return loadedAssemblies.SelectMany(assembly => AssemblyHelper.GetTypesFromAssembly(assembly)); }
		}
		
		static internal IEnumerable<Type> SubclassesOf(Type parent)
		{
			return loadedTypes.Where(klass => klass.IsSubclassOf (parent));
		}

		/// <summary>
		/// This method is called from unmanaged code.
		/// </summary>
		// ReSharper disable UnusedMember.Local
		private static void SetLoadedEditorAssemblies(Assembly[] assemblies)
		// ReSharper restore UnusedMember.Local
		{
			loadedAssemblies = assemblies;
			RunClassConstructors();
		}

		private static void RunClassConstructors()
		{
			foreach (var type in loadedTypes)
			{
				if (!type.IsDefined(typeof(InitializeOnLoadAttribute), false))
					continue;

				try
				{
					RuntimeHelpers.RunClassConstructor(type.TypeHandle);
				}
				catch (TypeInitializationException x)
				{
					Debug.LogError(x.InnerException);
				}
			}
		}
	}
}