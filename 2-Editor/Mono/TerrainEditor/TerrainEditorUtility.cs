using UnityEngine;
using UnityEditor;
using System.Collections;
using System.Collections.Generic;

namespace UnityEditor {

internal class TerrainEditorUtility
{
	private static void Extract (uint staticFlag, ref TerrainData[] datas, ref Vector3[] positions, ref int[] castShadows, ref Material[] materials, ref int[] lightmapSizes, ref int[] lightmapIndices, ref bool[] selection)
	{
		List<TerrainData> terrainDatas = new List<TerrainData> ();
		List<Vector3> terrainPositions = new List<Vector3> ();
		List<int> terrainCastShadows = new List<int> ();
		List<Material> terrainMaterials = new List<Material> ();
		List<int> terrainLightmapSizes = new List<int>();
		List<int> terrainLightmapIndices = new List<int>();
		List<bool> terrainSelection = new List<bool>();

		int size = Terrain.activeTerrains.Length;

		for (int i = 0 ; i < size; i++)
		{
			Terrain terrain = Terrain.activeTerrains[i];
			if (GameObjectUtility.AreStaticEditorFlagsSet (terrain.gameObject, (StaticEditorFlags)staticFlag) && terrain.terrainData)
			{
				terrainDatas.Add(terrain.terrainData);
				terrainPositions.Add(terrain.GetPosition());
				terrainCastShadows.Add(terrain.castShadows ? 1 : 0);
				terrainMaterials.Add(terrain.materialTemplate);
				terrainLightmapSizes.Add(terrain.lightmapSize);
				terrainLightmapIndices.Add(terrain.lightmapIndex);
				terrainSelection.Add(Selection.Contains(terrain.gameObject));
			}
		}

		datas = terrainDatas.ToArray();
		positions = terrainPositions.ToArray();
		castShadows = terrainCastShadows.ToArray();
		materials = terrainMaterials.ToArray();
		lightmapSizes = terrainLightmapSizes.ToArray();
		lightmapIndices = terrainLightmapIndices.ToArray();
		selection = terrainSelection.ToArray();
	}
	
	internal static void RemoveSplatTexture (TerrainData terrainData, int index)
	{
		Undo.RegisterCompleteObjectUndo (terrainData, "Remove texture");
		
		int width = terrainData.alphamapWidth;
		int height = terrainData.alphamapHeight;
		float[,,] alphamap = terrainData.GetAlphamaps (0, 0, width, height);
		int alphaCount = alphamap.GetLength(2);
		
		int newAlphaCount = alphaCount-1;
		float[,,] newalphamap = new float[height,width,newAlphaCount];
		
		// move further alphamaps one index below
		for (int y=0;y<height;++y) {
			for (int x=0;x<width;++x) {
				for (int a=0;a<index;++a)
					newalphamap[y,x,a] = alphamap[y,x,a];
				for (int a=index+1;a<alphaCount;++a)
					newalphamap[y,x,a-1] = alphamap[y,x,a];
			}
		}
		
		// normalize weights in new alpha map
		for (int y=0;y<height;++y) {
			for (int x=0;x<width;++x) {
				float sum = 0.0F;
				for (int a=0;a<newAlphaCount;++a)
					sum += newalphamap[y,x,a];
				if( sum >= 0.01 ) {
					float multiplier = 1.0F / sum;
					for (int a=0;a<newAlphaCount;++a)
						newalphamap[y,x,a] *= multiplier;
				} else {
					// in case all weights sum to pretty much zero (e.g.
					// removing splat that had 100% weight), assign
					// everything to 1st splat texture (just like
					// initial terrain).
					for (int a=0;a<newAlphaCount;++a)
						newalphamap[y,x,a] = (a==0) ? 1.0f : 0.0f;
				}
			}
		}
		
		// remove splat from terrain prototypes
		SplatPrototype[] splats = terrainData.splatPrototypes;
		SplatPrototype[] newSplats = new SplatPrototype[splats.Length-1];
		for (int a=0;a<index;++a)
			newSplats[a] = splats[a];
		for (int a=index+1;a<alphaCount;++a)
			newSplats[a-1] = splats[a];
		terrainData.splatPrototypes = newSplats;
		
		// set new alphamaps
		terrainData.SetAlphamaps (0,0,newalphamap);
	}
	
	
	internal static void RemoveTree (Terrain terrain, int index)
	{
		TerrainData terrainData = terrain.terrainData;
		if( terrainData == null )
			return;
		Undo.RegisterCompleteObjectUndo (terrainData, "Remove tree");
		terrainData.RemoveTreePrototype (index);
	}
	
	internal static void RemoveDetail (Terrain terrain, int index)
	{
		TerrainData terrainData = terrain.terrainData;
		if( terrainData == null )
			return;
		Undo.RegisterCompleteObjectUndo (terrainData, "Remove detail object");
		terrainData.RemoveDetailPrototype (index);
	}
}

} //namespace
