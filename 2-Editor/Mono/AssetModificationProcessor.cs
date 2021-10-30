using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEditor.VersionControl;
using UnityEditorInternal;
using UnityEditorInternal.VersionControl;
using System.Reflection;

namespace UnityEditor
{
internal class AssetModificationProcessorInternal
{

	enum FileMode
	{
		Binary,
		Text
	}

	static bool CheckArgumentTypes(Type[] types, MethodInfo method)
	{
		ParameterInfo[] parameters = method.GetParameters();
	
		if (types.Length != parameters.Length)
		{
			Debug.LogWarning("Parameter count did not match. Expected: " + types.Length.ToString () + " Got: " + parameters.Length.ToString () + " in " + method.DeclaringType.ToString () + "." + method.Name);
			return false;
		}

		int i = 0;
		foreach (Type type in types)
		{
			ParameterInfo pInfo = parameters[i];
			if (type != pInfo.ParameterType)
			{
				Debug.LogWarning("Parameter type mismatch at parameter " + i + ". Expected: " + type.ToString() + " Got: " + pInfo.ParameterType.ToString() + " in " + method.DeclaringType.ToString() + "." + method.Name);
				return false;
			}
			++i;
		}

		return true;
	}

	static bool CheckArgumentTypesAndReturnType(Type[] types, MethodInfo method, System.Type returnType)
	{
		if (returnType != method.ReturnType)
		{
			Debug.LogWarning("Return type mismatch. Expected: " + returnType.ToString() + " Got: " + method.ReturnType.ToString() + " in " + method.DeclaringType.ToString() + "." + method.Name);
			return false;
		}

		return CheckArgumentTypes(types, method);
	}

	static bool CheckArguments (object[] args, MethodInfo method)
	{
		Type[] types = new Type[args.Length];

		for (int i = 0; i < args.Length; i++)
			types[i] = args[i].GetType ();

		return CheckArgumentTypes (types, method);
	}

	static bool CheckArgumentsAndReturnType(object[] args, MethodInfo method, System.Type returnType)
	{
		Type[] types = new Type[args.Length];
		
		for (int i = 0; i < args.Length; i++)
			types[i] = args[i].GetType();

		return CheckArgumentTypesAndReturnType (types, method, returnType);
	}

#pragma warning disable 0618
	static System.Collections.Generic.IEnumerable<System.Type> assetModificationProcessors = null;
	static System.Collections.Generic.IEnumerable<System.Type> AssetModificationProcessors
	{
		get
		{
			if (assetModificationProcessors == null)
			{
				List<Type> processors = new List<Type> ();
				processors.AddRange (EditorAssemblies.SubclassesOf (typeof (UnityEditor.AssetModificationProcessor)));
				processors.AddRange (EditorAssemblies.SubclassesOf (typeof (global::AssetModificationProcessor)));
				assetModificationProcessors = processors.ToArray ();
			}
			return assetModificationProcessors;
		}
	}
#pragma warning restore 0618

	static void OnWillCreateAsset (string path)
	{
		foreach (var assetModificationProcessorClass in AssetModificationProcessors )
		{
			MethodInfo method = assetModificationProcessorClass.GetMethod("OnWillCreateAsset", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				object[] args = { path };
				if (!CheckArguments (args, method))
					continue;

				method.Invoke (null, args);
			}
		}
	}
	

	static void FileModeChanged(string[] assets, UnityEditor.VersionControl.FileMode mode)
	{
		// Make sure that all assets are checked out in version control and
		// that we have the most recent status
		if (Provider.enabled)
		{
			if (Provider.PromptAndCheckoutIfNeeded(assets, ""))
				Provider.SetFileMode(assets, mode);
		}
	}
	
	// Postprocess on all assets once an automatic import has completed
	static void OnWillSaveAssets (string[] assets, out string[] assetsThatShouldBeSaved, out string[] assetsThatShouldBeReverted, int explicitlySaveScene)
	{
		assetsThatShouldBeReverted = new string[0];
		assetsThatShouldBeSaved = assets;

		bool showSaveDialog = assets.Length > 0 && EditorPrefs.GetBool("VerifySavingAssets", false) && InternalEditorUtility.isHumanControllingUs;

		// If we are only saving a scene and the user explicitly said we should, skip the dialog. We don't need
		// to verify this twice.
		if (explicitlySaveScene != 0 && assets.Length == 1 && assets[0].EndsWith(".unity"))
			showSaveDialog = false;

		var tempAssets = new List<string> ();
		foreach (string asset in assetsThatShouldBeSaved)
		{
			if (AssetDatabase.IsOpenForEdit (asset))
			{
				tempAssets.Add (asset);
			} 
			else
			{
				Debug.LogWarning (String.Format ("{0} is locked and cannot be saved", asset));
			}
		}
		assets = tempAssets.ToArray ();
		
		if (showSaveDialog)
		{
			AssetSaveDialog.ShowWindow(assets, out assetsThatShouldBeSaved);
		}

		// Try to checkout if needed. The may fail but is cached below.
		if (!Provider.PromptAndCheckoutIfNeeded(assetsThatShouldBeSaved, ""))
		{
			Debug.LogError("Could not checkout the following files in version control before saving: " + 
						string.Join(", ", assetsThatShouldBeSaved));
			assetsThatShouldBeSaved = new string[0];
			return;
		}

		foreach (var assetModificationProcessorClass in AssetModificationProcessors )
		{
			MethodInfo method = assetModificationProcessorClass.GetMethod("OnWillSaveAssets", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				object[] args = { assetsThatShouldBeSaved };
				if (!CheckArguments (args, method))
					continue;

				string[] result = (string[])method.Invoke (null, args);
				if (result != null)
					assetsThatShouldBeSaved = result;
			}
		}
	}

	static void RequireTeamLicense()
	{
		if (!InternalEditorUtility.HasMaint ())
			throw new MethodAccessException ("Requires team license");
	}
	
	static AssetMoveResult OnWillMoveAsset (string fromPath, string toPath, string[] newPaths, string[] NewMetaPaths)
	{
		AssetMoveResult finalResult = AssetMoveResult.DidNotMove;
		if (!InternalEditorUtility.HasMaint())
			return finalResult;

		finalResult = AssetModificationHook.OnWillMoveAsset (fromPath, toPath);

		foreach (var assetModificationProcessorClass in AssetModificationProcessors)
		{
			MethodInfo method = assetModificationProcessorClass.GetMethod("OnWillMoveAsset", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				RequireTeamLicense ();

				object[] args = { fromPath, toPath };
				if (!CheckArgumentsAndReturnType (args, method, finalResult.GetType()))
					continue;

				finalResult |= (AssetMoveResult)method.Invoke(null, args);
			}
		}

		return finalResult;
	}

	static AssetDeleteResult OnWillDeleteAsset (string assetPath, RemoveAssetOptions options)
	{
		AssetDeleteResult finalResult = AssetDeleteResult.DidNotDelete;
		if (!InternalEditorUtility.HasMaint())
			return finalResult;

		finalResult = AssetModificationHook.OnWillDeleteAsset(assetPath, options);

		foreach (var assetModificationProcessorClass in AssetModificationProcessors)
		{
			MethodInfo method = assetModificationProcessorClass.GetMethod("OnWillDeleteAsset", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				RequireTeamLicense ();

				object[] args = { assetPath, options };
				if (!CheckArgumentsAndReturnType (args, method, finalResult.GetType ()))
					continue;

				finalResult |= (AssetDeleteResult)method.Invoke(null, args);
			}
		}

		return finalResult;
	}

	internal static MethodInfo[] isOpenForEditMethods = null;
	internal static MethodInfo[] GetIsOpenForEditMethods()
	{
		if (isOpenForEditMethods == null)
		{
			List<MethodInfo> mArray = new List<MethodInfo>();
			foreach (var assetModificationProcessorClass in AssetModificationProcessors)
			{
				MethodInfo method = assetModificationProcessorClass.GetMethod ("IsOpenForEdit", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
				if (method != null)
				{
					RequireTeamLicense ();

					string dummy = "";
					bool bool_dummy = false;
					Type[] types = { dummy.GetType (), dummy.GetType ().MakeByRefType () };
					if (!CheckArgumentTypesAndReturnType (types, method, bool_dummy.GetType ()))
						continue;
						
					mArray.Add(method);
				}
			}
			
			isOpenForEditMethods = mArray.ToArray(); 
		}
		
		return isOpenForEditMethods;
	}
	
		
	internal static bool IsOpenForEdit (string assetPath, out string message)
	{
		bool finalResult = true;
		message = "";

		finalResult &= AssetModificationHook.IsOpenForEdit(assetPath, out message);

		foreach (var method in GetIsOpenForEditMethods ())
		{
			object[] args = {assetPath, message};
			if (!((bool)method.Invoke (null, args)))
			{
				message = args[1] as string;
				return false;
			}
		}

		return finalResult;
	}

	internal static void OnStatusUpdated()
	{

		WindowPending.OnStatusUpdated ();

		foreach (var assetModificationProcessorClass in AssetModificationProcessors)
		{
			MethodInfo method = assetModificationProcessorClass.GetMethod("OnStatusUpdated", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				RequireTeamLicense();

				object[] args = { };
				if (!CheckArgumentsAndReturnType(args, method, typeof(void)))
					continue;

				method.Invoke(null, args);
			}
		}
	}
}
}
