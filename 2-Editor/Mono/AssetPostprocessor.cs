using UnityEngine;
using System;
using System.Collections;
using System.Collections.Generic;
using System.Reflection;

namespace UnityEditor
{

internal class AssetPostprocessingInternal
{
	static void LogPostProcessorMissingDefaultConstructor(Type type)
	{
		Debug.LogError(string.Format("{0} requires a default constructor to be used as an asset post processor", type.ToString()));
	}
	
	
	// Postprocess on all assets once an automatic import has completed
	static void PostprocessAllAssets (string[] importedAssets, string[] addedAssets, string[] deletedAssets, string[] movedAssets, string[] movedFromPathAssets)
	{
		object[] args = { importedAssets, deletedAssets, movedAssets, movedFromPathAssets };
		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf (typeof(AssetPostprocessor)) )
		{
			MethodInfo method = assetPostprocessorClass.GetMethod("OnPostprocessAllAssets", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
				method.Invoke(null, args);
		}
		
		///@TODO: we need addedAssets for SyncVS. Make this into a proper API and write tests
		SyncVS.PostprocessSyncProject(importedAssets, addedAssets, deletedAssets, movedAssets, movedFromPathAssets);
	}


	//This is undocumented, and a "safeguard" for when visualstudio gets a new release that is incompatible with ours, so that users can postprocess our csproj to fix it.
	//(or just completely replace them). Hopefully we'll never need this.
	static internal void CallOnGeneratedCSProjectFiles()
	{
		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf(typeof(AssetPostprocessor)))
		{
			MethodInfo method = assetPostprocessorClass.GetMethod("OnGeneratedCSProjectFiles", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
			if (method != null)
			{
				object[] args = { };
				method.Invoke(null, args);
			}
		}
	}

	internal class CompareAssetImportPriority : IComparer
	{
		int IComparer.Compare( System.Object xo, System.Object yo )  {
			int x = ((AssetPostprocessor)xo).GetPostprocessOrder();
			int y = ((AssetPostprocessor)yo).GetPostprocessOrder();
			return x.CompareTo( y );
		}
	}

	internal class PostprocessStack
	{
		internal ArrayList m_ImportProcessors = null;
	}

	static ArrayList m_PostprocessStack = null;
	static ArrayList m_ImportProcessors = null;

	static void InitPostprocessors (string pathName)
	{
		m_ImportProcessors = new ArrayList();
		
		// @TODO: This is just a temporary hack for the import settings.
		// We should add importers to the asset, persist them and show an inspector for them.
		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf(typeof(AssetPostprocessor)))
		{
			try
			{
				var assetPostprocessor = Activator.CreateInstance(assetPostprocessorClass) as AssetPostprocessor;
				assetPostprocessor.assetPath = pathName;
				m_ImportProcessors.Add(assetPostprocessor);
			}
			catch (MissingMethodException)
			{
				LogPostProcessorMissingDefaultConstructor(assetPostprocessorClass);
			}
			catch (Exception e)
			{
				Debug.LogException(e);
			}
		}
		
		m_ImportProcessors.Sort(new CompareAssetImportPriority());
		
		// Setup postprocessing stack to support rentrancy (Import asset immediate)
		PostprocessStack postStack = new PostprocessStack();
		postStack.m_ImportProcessors = m_ImportProcessors;
		if (m_PostprocessStack == null)
			m_PostprocessStack = new ArrayList();
		m_PostprocessStack.Add(postStack);
	}

	static void CleanupPostprocessors ()
	{
		if (m_PostprocessStack != null)
		{
			m_PostprocessStack.RemoveAt(m_PostprocessStack.Count - 1);
			if (m_PostprocessStack.Count != 0)
			{
				PostprocessStack postStack = (PostprocessStack)m_PostprocessStack[m_PostprocessStack.Count - 1];
				m_ImportProcessors = postStack.m_ImportProcessors;
			}
		}
	}

	static uint[] GetMeshProcessorVersions()
	{
		List<uint> versions = new List<uint> ();

		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf(typeof(AssetPostprocessor)))
		{
			try
			{
				var inst = Activator.CreateInstance(assetPostprocessorClass) as AssetPostprocessor;
				var type = inst.GetType();
				bool hasPreProcessMethod = type.GetMethod("OnPreprocessModel", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				bool hasProcessMeshAssignMethod = type.GetMethod("OnProcessMeshAssingModel", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				bool hasPostProcessMethod = type.GetMethod("OnPostprocessModel", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				uint version = inst.GetVersion();
				if (version != 0 && (hasPreProcessMethod || hasProcessMeshAssignMethod || hasPostProcessMethod))
				{
					versions.Add(version);
				}
			}
			catch (MissingMethodException)
			{
				LogPostProcessorMissingDefaultConstructor(assetPostprocessorClass);
			}
			catch (Exception e)
			{
				Debug.LogException(e);
			}
		}

		return versions.ToArray();
	}
	
	static void PreprocessMesh (string pathName)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPreprocessModel", null);
		}
	}

	static Material ProcessMeshAssignMaterial (Renderer renderer, Material material)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			object[] args = { material, renderer };
			object assignedMaterial = AttributeHelper.InvokeMemberIfAvailable(inst, "OnAssignMaterialModel", args);
			if (assignedMaterial as Material)
				return assignedMaterial as Material;
		}
		
		return null;
	}

	static bool ProcessMeshHasAssignMaterial ()
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			if (inst.GetType ().GetMethod("OnAssignMaterialModel", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null)
				return true;
		}
		
		return false;
	}

	static void PostprocessMesh (GameObject gameObject)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			object[] args = { gameObject };
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPostprocessModel", args);
		}
	}

	static void PostprocessGameObjectWithUserProperties(GameObject go, string[] prop_names, object[] prop_values)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors)
		{
			//inst.preview = null;
			object[] args = { go, prop_names, prop_values };
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPostprocessGameObjectWithUserProperties", args);
		}
	}

	static uint[] GetTextureProcessorVersions ()
	{
		List<uint> versions = new List<uint> ();

		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf (typeof (AssetPostprocessor)))
		{
			try
			{
				var inst = Activator.CreateInstance(assetPostprocessorClass) as AssetPostprocessor;
				var type = inst.GetType();
				bool hasPreProcessMethod = type.GetMethod("OnPreprocessTexture", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				bool hasPostProcessMethod = type.GetMethod("OnPostprocessTexture", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				uint version = inst.GetVersion();
				if (version != 0 && (hasPreProcessMethod || hasPostProcessMethod))
				{
					versions.Add(version);
				}
			}
			catch (MissingMethodException)
			{
				LogPostProcessorMissingDefaultConstructor(assetPostprocessorClass);
			}
			catch (Exception e)
			{
				Debug.LogException(e);
			}
		}

		return versions.ToArray ();
	}

	static void PreprocessTexture (string pathName)
	{
		
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPreprocessTexture", null);
		}
	}

	static void PostprocessTexture (Texture2D tex, string pathName)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			object[] args = { tex };
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPostprocessTexture", args);
		}
	}

	static uint[] GetAudioProcessorVersions ()
	{
		List<uint> versions = new List<uint> ();

		foreach (var assetPostprocessorClass in EditorAssemblies.SubclassesOf (typeof (AssetPostprocessor)))
		{
			try
			{
				var inst = Activator.CreateInstance(assetPostprocessorClass) as AssetPostprocessor;
				var type = inst.GetType();
				bool hasPreProcessMethod = type.GetMethod("OnPreprocessAudio", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				bool hasPostProcessMethod = type.GetMethod("OnPostprocessAudio", BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance) != null;
				uint version = inst.GetVersion();
				if (version != 0 && (hasPreProcessMethod || hasPostProcessMethod))
				{
					versions.Add(version);
				}
			}
			catch (MissingMethodException)
			{
				LogPostProcessorMissingDefaultConstructor(assetPostprocessorClass);
			}
			catch (Exception e)
			{
				Debug.LogException(e);
			}
		}

		return versions.ToArray ();
	}
	
	static void PreprocessAudio (string pathName)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPreprocessAudio", null);
		}
	}
	
	static void PostprocessAudio (AudioClip tex, string pathName)
	{
		foreach (AssetPostprocessor inst in m_ImportProcessors )
		{
			object[] args = { tex };
			AttributeHelper.InvokeMemberIfAvailable(inst, "OnPostprocessAudio", args);
		}
	}
}
}
