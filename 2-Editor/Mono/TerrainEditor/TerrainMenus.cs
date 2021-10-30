using UnityEngine;
using UnityEditor;
using System.Collections;
using System.IO;

namespace UnityEditor
{

internal class TerrainMenus
{
	[MenuItem("GameObject/Create Other/Terrain")]
	static void CreateTerrain ()
	{
        if (EditorUserBuildSettings.selectedBuildTargetGroup == BuildTargetGroup.Wii)
        {
            Debug.LogError("Terrain is not supported by selected platform.");
            return;
        }
		// Create the storage for the terrain in the project
		// (So we can reuse it in multiple scenes)
		TerrainData terrainData = new TerrainData ();
		const int size = 513;
		terrainData.heightmapResolution = size;
		terrainData.size = new Vector3 (2000, 600, 2000);
		
		terrainData.heightmapResolution = 512;
		terrainData.baseMapResolution = 1024;
		terrainData.SetDetailResolution(1024, terrainData.detailResolutionPerPatch);

		AssetDatabase.CreateAsset(terrainData, AssetDatabase.GenerateUniqueAssetPath("Assets/New Terrain.asset"));
		
		Selection.activeObject = Terrain.CreateTerrainGameObject(terrainData);
	}

	internal static void ImportRaw ()
	{
		string saveLocation = EditorUtility.OpenFilePanel("Import Raw Heightmap", "", "raw");
		if (saveLocation != "")
		{
			ImportRawHeightmap wizard = ScriptableWizard.DisplayWizard<ImportRawHeightmap>("Import Heightmap", "Import");
			wizard.InitializeImportRaw(GetActiveTerrain(), saveLocation);
		}
	}
	
	internal static void ExportHeightmapRaw ()
	{
		ExportRawHeightmap wizard = ScriptableWizard.DisplayWizard<ExportRawHeightmap>("Export Heightmap", "Export");
		wizard.InitializeDefaults(GetActiveTerrain());
	}
	
	internal static void SetHeightmapResolution ()
	{
		SetResolutionWizard wizard = ScriptableWizard.DisplayWizard<SetResolutionWizard>("Set Heightmap resolution", "Set Resolution");
		wizard.InitializeDefaults(GetActiveTerrain());
	}

	internal static void MassPlaceTrees ()
	{
		PlaceTreeWizard wizard = ScriptableWizard.DisplayWizard<PlaceTreeWizard>("Place Trees", "Place");
		wizard.InitializeDefaults(GetActiveTerrain());
	}

	internal static void Flatten ()
	{
		FlattenHeightmap wizard = ScriptableWizard.DisplayWizard<FlattenHeightmap>("Flatten Heightmap", "Flatten");
		wizard.InitializeDefaults(GetActiveTerrain());
	}

	internal static void RefreshPrototypes ()
	{
		GetActiveTerrainData().RefreshPrototypes();
		GetActiveTerrain().Flush();
	}

	static void FlushHeightmapModification ()
	{
//@TODO        GetActiveTerrain().treeDatabase.RecalculateTreePosition();
        GetActiveTerrain().Flush();
	}	
	
	static Terrain GetActiveTerrain ()
	{
		Object[] selection = Selection.GetFiltered(typeof(Terrain), SelectionMode.Editable);
		
		if (selection.Length != 0)
			return selection[0] as Terrain;
		else
			return Terrain.activeTerrain;
	}
	static TerrainData GetActiveTerrainData ()
	{
		if (GetActiveTerrain ())
			return GetActiveTerrain().terrainData;
		else
			return null;
	}
}

class TerrainDetailContextMenus
{

	[MenuItem ("CONTEXT/TerrainEngineDetails/Add Grass Texture")]
	static internal void AddDetailTexture (MenuCommand item)
	{
		DetailTextureWizard wizard = ScriptableWizard.DisplayWizard<DetailTextureWizard>("Add Grass Texture", "Add");
		wizard.m_DetailTexture = null;
		wizard.InitializeDefaults((Terrain)item.context, -1);
	}

	[MenuItem ("CONTEXT/TerrainEngineDetails/Add Detail Mesh")]
	static internal void AddDetailMesh (MenuCommand item)
	{
		DetailMeshWizard wizard = ScriptableWizard.DisplayWizard<DetailMeshWizard>("Add Detail Mesh", "Add");
		wizard.m_Detail = null;
        wizard.InitializeDefaults((Terrain)item.context, -1);
	}
	
	[MenuItem ("CONTEXT/TerrainEngineDetails/Edit")]
	static internal void EditDetail (MenuCommand item)
	{ 
		Terrain terrain = (Terrain)item.context;
		DetailPrototype prototype = terrain.terrainData.detailPrototypes[item.userData];
		
		if (prototype.usePrototypeMesh)
		{
			DetailMeshWizard wizard = ScriptableWizard.DisplayWizard<DetailMeshWizard>("Edit Detail Mesh", "Apply");
			wizard.InitializeDefaults((Terrain)item.context, item.userData);
		}
		else
		{
			DetailTextureWizard wizard = ScriptableWizard.DisplayWizard<DetailTextureWizard>("Edit Grass Texture", "Apply");
			wizard.InitializeDefaults((Terrain)item.context, item.userData);
		}
	}
	
	[MenuItem ("CONTEXT/TerrainEngineDetails/Edit", true)]
	static internal bool EditDetailCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.detailPrototypes.Length;
	}
	
	[MenuItem ("CONTEXT/TerrainEngineDetails/Remove")]
	static internal void RemoveDetail (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		TerrainEditorUtility.RemoveDetail( terrain, item.userData );
	}
	
	[MenuItem ("CONTEXT/TerrainEngineDetails/Remove", true)]
	static internal bool RemoveDetailCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.detailPrototypes.Length;
	}
}

class TerrainSplatContextMenus
{
	[MenuItem ("CONTEXT/TerrainEngineSplats/Add Texture...")]
	static internal void AddSplat (MenuCommand item)
	{
		TerrainSplatEditor.ShowTerrainSplatEditor("Add Terrain Texture", "Add", (Terrain)item.context, -1);
	}
	
	[MenuItem ("CONTEXT/TerrainEngineSplats/Edit Texture...")]
	static internal void EditSplat (MenuCommand item)
	{
		TerrainSplatEditor.ShowTerrainSplatEditor ("Edit Terrain Texture", "Apply", (Terrain)item.context, item.userData);
	}
	
	[MenuItem ("CONTEXT/TerrainEngineSplats/Edit Texture...", true)]
	static internal bool EditSplatCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.splatPrototypes.Length;
	}
	
	[MenuItem ("CONTEXT/TerrainEngineSplats/Remove Texture")]
	static internal void RemoveSplat (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		TerrainEditorUtility.RemoveSplatTexture( terrain.terrainData, item.userData );
	}
	
	[MenuItem ("CONTEXT/TerrainEngineSplats/Remove Texture", true)]
	static internal bool RemoveSplatCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.splatPrototypes.Length;
	}
}

class TerrainTreeContextMenus
{
	[MenuItem ("CONTEXT/TerrainEngineTrees/Add Tree")]
	static internal void AddTree (MenuCommand item)
	{
		TreeWizard wizard = ScriptableWizard.DisplayWizard<TreeWizard>("Add Tree", "Add");
        wizard.InitializeDefaults((Terrain)item.context, -1);
	}
	
	[MenuItem ("CONTEXT/TerrainEngineTrees/Edit Tree")]
	static internal void EditTree (MenuCommand item)
	{
        TreeWizard wizard = ScriptableWizard.DisplayWizard<TreeWizard>("Edit Tree", "Apply");
		wizard.InitializeDefaults((Terrain)item.context, item.userData);
	}

	[MenuItem ("CONTEXT/TerrainEngineTrees/Edit Tree", true)]
	static internal bool EditTreeCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.treePrototypes.Length;
	}
	
	[MenuItem ("CONTEXT/TerrainEngineTrees/Remove Tree")]
	static internal void RemoveTree (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		TerrainEditorUtility.RemoveTree (terrain, item.userData);
	}

	[MenuItem ("CONTEXT/TerrainEngineTrees/Remove Tree", true)]
	static internal bool RemoveTreeCheck (MenuCommand item)
	{
		Terrain terrain = (Terrain)item.context;
		return item.userData >= 0 && item.userData < terrain.terrainData.treePrototypes.Length;
	}
}

/*
    [MenuItem ("Terrain/Import Heightmap - Texture...")]
    static void ImportHeightmap () {
        ImportTextureHeightmap wizard = ScriptableWizard.DisplayWizard<ImportTextureHeightmap>("Import Heightmap", "Import");
        wizard.InitializeDefaults(GetActiveTerrain());
    }
*/


} //namespace
