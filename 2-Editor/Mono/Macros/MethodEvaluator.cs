using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;

namespace UnityEditor.Macros
{
	public static class MethodEvaluator
	{
		public static object Eval(string assemblyFile, string typeName, string methodName, Type[] paramTypes, object[] args)
		{	
			var assemblyDirectory = Path.GetDirectoryName(assemblyFile);
			var resolver = new AssemblyResolver(assemblyDirectory);
			AppDomain.CurrentDomain.AssemblyResolve += resolver.AssemblyResolve;
			try
			{
				var assembly = Assembly.LoadFrom(assemblyFile);
				var method = assembly.GetType(typeName, true).GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static, null, paramTypes, null);
				if (method == null)
					throw new ArgumentException(string.Format("Method {0}.{1}({2}) not found in assembly {3}!", typeName, methodName, ToCommaSeparatedString(paramTypes), assembly.FullName));
				return method.Invoke(null, args);
			}
			finally
			{
				AppDomain.CurrentDomain.AssemblyResolve -= resolver.AssemblyResolve;
			}
		}

		private static string ToCommaSeparatedString<T>(IEnumerable<T> items)
		{
			return string.Join(", ", items.Select(o => o.ToString()).ToArray());
		}

		public class AssemblyResolver
		{
			private readonly string _assemblyDirectory;

			public AssemblyResolver(string assemblyDirectory)
			{
				_assemblyDirectory = assemblyDirectory;
			}

			public Assembly AssemblyResolve(object sender, ResolveEventArgs args)
			{
				var simpleName = args.Name.Split(',')[0];
				var assemblyFile = Path.Combine(_assemblyDirectory, simpleName + ".dll");
				if (File.Exists(assemblyFile))				
					return Assembly.LoadFrom(assemblyFile);
				return null;
			}
		}
	}
}
