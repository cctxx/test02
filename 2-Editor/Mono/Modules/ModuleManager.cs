using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Collections.Generic;

using UnityEditor;
using UnityEditorInternal;
using UnityEngine;
using Unity.DataContract;
using UnityEditor.Utils;

namespace UnityEditor.Modules
{
	internal static class ModuleManager
	{
		[NonSerialized]
		static List<IPlatformSupportModule> mPlatformModules;

		[NonSerialized]
		static List<IEditorModule> mEditorModules;

		[NonSerialized]
		static IPackageManagerModule mPackageManager;

		static IPackageManagerModule PackageManager {
			get {
				Initialize ();
				return mPackageManager;
			}
		}
		
		static List<IPlatformSupportModule> PlatformSupportModules
		{ 
			get
			{
				Initialize ();
				if (mPlatformModules == null)
					RegisterPlatformSupportModules ();
				return mPlatformModules;
			}
		}

		static List<IEditorModule> EditorModules
		{
			get {
				if (mEditorModules == null)
					return new List<IEditorModule> ();
				return mEditorModules;
			}
		}

		internal static bool IsRegisteredModule (string file)
		{
			file = file.NormalizePath ();
			return (mPackageManager != null && mPackageManager.GetType ().Assembly.Location == file);
		}

		// entry point from native
		internal static void Initialize ()
		{
			if (mPackageManager == null)
				RegisterPackageManager ();
		}

		// entry point from native
		internal static void InitializePlatformSupportModules ()
		{
			Initialize ();
			RegisterPlatformSupportModules ();
		}

		// entry point from native
		internal static void Shutdown ()
		{
			if (mPackageManager != null)
				mPackageManager.Shutdown (true);

			mPackageManager = null;
			mPlatformModules = null;
			mEditorModules = null;
		}

		static void RegisterPackageManager ()
		{
			mEditorModules = new List<IEditorModule> (); // TODO: no editor modules support for now, so just cache this

			try 
			{
				Assembly assembly = AppDomain.CurrentDomain.GetAssemblies ().FirstOrDefault (a => null != a.GetType ("Unity.PackageManager.PackageManager"));
				if (assembly != null) {
					if (InitializePackageManager (assembly, null))
						return;
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine ("Error enumerating assemblies looking for package manager. {0}", ex);
			}

			// check if locator assembly is in the domain (it should be loaded along with UnityEditor/UnityEngine at this point)
			Type locatorType = AppDomain.CurrentDomain.GetAssemblies ().Where (a => a.GetName ().Name ==  "Unity.Locator").Select (a => a.GetType ("Unity.PackageManager.Locator")).FirstOrDefault ();
			try {
				locatorType.InvokeMember ("Scan", BindingFlags.Public | BindingFlags.Static | BindingFlags.InvokeMethod, null, null, new[] { Path.Combine (EditorApplication.applicationContentsPath, "PackageManager"), Application.unityVersion });
			} catch (Exception ex) {
				Console.WriteLine ("Error scanning for packages. {0}", ex);
				return;
			}

			Unity.DataContract.PackageInfo package;
			// get the package manager package
			try {
				package = locatorType.InvokeMember ("GetPackageManager", BindingFlags.Public | BindingFlags.Static | BindingFlags.InvokeMethod, null, null, new[] { Application.unityVersion }) as Unity.DataContract.PackageInfo;
				if (package == null) {
					Console.WriteLine ("No package manager found!");
					return;
				}
			}
			catch (Exception ex)
			{
				Console.WriteLine ("Error scanning for packages. {0}", ex);
				return;
			}

			try {
				InitializePackageManager (package);
			} catch (Exception ex) {
				Console.WriteLine ("Error initializing package manager. {0}", ex);
			}


			// this will only happen when unity first starts up
			if (mPackageManager != null)
				mPackageManager.CheckForUpdates ();
		}

		// instantiate package manager and add it to the native assembly list for automatic loading at domain reloads (play mode start, etc)
		static bool InitializePackageManager (Unity.DataContract.PackageInfo package)
		{
			string dll = package.Files.Where (x => x.Value == PackageFileType.Dll).Select (x => x.Key).FirstOrDefault ();
			if (dll == null || !File.Exists (Path.Combine (package.BasePath, dll)))
				return false;
			InternalEditorUtility.SetPlatformPath (package.BasePath);
			Assembly assembly = Assembly.LoadFile (Path.Combine (package.BasePath, dll));
			return InitializePackageManager (assembly, package);
		}

		static bool InitializePackageManager (Assembly assembly, Unity.DataContract.PackageInfo package)
		{
			mPackageManager = AssemblyHelper.FindImplementors<IPackageManagerModule> (assembly).FirstOrDefault ();

			if (mPackageManager == null)
				return false;

			string dllpath = assembly.Location;

			// if we have a package, it's because it came from the locator, which means we need to setup the dll
			// for loading on the next domain reloads
			if (package != null)
				InternalEditorUtility.SetupCustomDll (Path.GetFileName (dllpath), dllpath);

			else // just set the package with the path to the loaded assembly so package manager can get its information from there
				package = new Unity.DataContract.PackageInfo () { BasePath = Path.GetDirectoryName (dllpath) } ;

			mPackageManager.ModuleInfo = package;
			mPackageManager.EditorInstallPath = EditorApplication.applicationContentsPath;
			mPackageManager.UnityVersion = new PackageVersion (Application.unityVersion);

			mPackageManager.Initialize ();
			return true;
		}

		static void RegisterPlatformSupportModules ()
		{
			Console.WriteLine("Registering platform support modules:");
			var stopwatch = System.Diagnostics.Stopwatch.StartNew();
			
			mPlatformModules = RegisterModulesFromLoadedAssemblies<IPlatformSupportModule> (RegisterPlatformSupportModulesFromAssembly).ToList ();

			stopwatch.Stop();
			Console.WriteLine("Registered platform support modules in: " +  stopwatch.Elapsed.TotalSeconds + "s.");
		}

		static IEnumerable<T> RegisterModulesFromLoadedAssemblies<T> (Func<Assembly,IEnumerable<T>> processAssembly)
		{
			if (processAssembly == null)
				throw new ArgumentNullException ("processAssembly");

			return AppDomain.CurrentDomain.GetAssemblies().Aggregate (new List<T> (), 
																	  delegate (List<T> list, Assembly assembly) {
				try {
					var modules = processAssembly (assembly);
					if (modules != null)
						list.AddRange (modules);
				} catch (Exception ex) {
					throw new Exception("Error while registering modules from " + assembly.FullName, ex);
				}
				return list;
			});
		}

		internal static IEnumerable<IPlatformSupportModule> RegisterPlatformSupportModulesFromAssembly(Assembly assembly)
		{
			return AssemblyHelper.FindImplementors<IPlatformSupportModule> (assembly);
		}

		static IEnumerable<IEditorModule> RegisterEditorModulesFromAssembly (Assembly assembly)
		{
			return AssemblyHelper.FindImplementors<IEditorModule> (assembly);
		}

		internal static List<string> GetJamTargets ()
		{
			List<string> jamTargets = new List<string>();

			foreach (var module in PlatformSupportModules)
			{
				jamTargets.Add(module.JamTarget);
			}

			return jamTargets;
		}
		
		internal static IBuildPostprocessor GetBuildPostProcessor(string target)
		{
			if (target == null) return null;
			foreach (var module in PlatformSupportModules)
			{
				if (module.TargetName == target)
				{
					return module.CreateBuildPostprocessor();
				}
			}
			
			return null;
		}
		internal static IBuildPostprocessor GetBuildPostProcessor(BuildTarget target)
		{
			return GetBuildPostProcessor(GetTargetStringFromBuildTarget(target));
		}
		internal static ISettingEditorExtension GetEditorSettingsExtension(string target)
		{
			if (string.IsNullOrEmpty(target)) return null;
			foreach (var module in PlatformSupportModules)
			{
				if (module.TargetName == target)
				{
					return module.CreateSettingsEditorExtension();
				}
			}
			
			return null;
		}
		
		internal static List<IPreferenceWindowExtension> GetPreferenceWindowExtensions()
		{
			List<IPreferenceWindowExtension> prefWindExtensions = new List<IPreferenceWindowExtension>();

			foreach (var module in PlatformSupportModules)
			{
				IPreferenceWindowExtension prefWindowExtension = module.CreatePreferenceWindowExtension();

				if (prefWindowExtension != null)
				{
					prefWindExtensions.Add(prefWindowExtension);
				}
			}

			return prefWindExtensions;
		}

		// This is for the smooth transition to future generic target names without subtargets
		internal static string GetTargetStringFromBuildTarget(BuildTarget target) {
			switch (target) {
				case BuildTarget.iPhone: return "iOS";
				case BuildTarget.XBOX360: return "Xbox360";
				case BuildTarget.MetroPlayer: return "Metro";
				case BuildTarget.BB10: return "BB10";
				case BuildTarget.Tizen: return "Tizen";
				case BuildTarget.PS3: return "PS3";
	//			case BuildTarget.Android: return "Android";		// disabled while waiting for Martin to finish his PostProcess refactoring
				default: return null;
			}
		}
		
		// This is for the smooth transition to future generic target names without subtargets
		internal static string GetTargetStringFromBuildTargetGroup(BuildTargetGroup target) {
			switch (target) {
				case BuildTargetGroup.iPhone: return "iOS";
				case BuildTargetGroup.XBOX360: return "Xbox360";
				case BuildTargetGroup.Metro: return "Metro";
				case BuildTargetGroup.BB10: return "BB10";
				case BuildTargetGroup.Tizen: return "Tizen";
				case BuildTargetGroup.PS3: return "PS3";
	//			case BuildTarget.Android: return "Android";		// disabled while waiting for Martin to finish his PostProcess refactoring
				default: return null;
			}
		}
	}
}
