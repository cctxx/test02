using UnityEngine;
using System.Collections;

namespace UnityEditor {

internal class TreePlacementUtility
{
	public static void PlaceRandomTrees (TerrainData terrainData, int treeCount)
	{
		int nbPrototypes = terrainData.treePrototypes.Length;
		if (nbPrototypes == 0)
		{
			Debug.Log("Can't place trees because no prototypes are defined");
			return;
		}

		Undo.RegisterCompleteObjectUndo (terrainData, "Mass Place Trees");

		TreeInstance[] instances = new TreeInstance[treeCount];
		int i = 0;
		while (i < instances.Length)
		{
			TreeInstance instance = new TreeInstance();
			instance.position = new Vector3(Random.value, 0, Random.value);
			if (terrainData.GetSteepness (instance.position.x, instance.position.z) < 30)
			{
				Color color = Color.Lerp(Color.white, Color.gray * 0.7F, Random.value);
				color.a = 1;
				instance.color = color;
				instance.lightmapColor = Color.white;
				instance.prototypeIndex = Random.Range(0, nbPrototypes);
	
				instance.widthScale = 1.0F;
				instance.heightScale = 1.0F;

				instances[i] = instance;
				i++;
			}
		}
		terrainData.treeInstances = instances;
		terrainData.RecalculateTreePositions();
	}

}


} //namespace
