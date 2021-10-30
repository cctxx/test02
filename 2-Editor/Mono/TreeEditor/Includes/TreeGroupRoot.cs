using System.Collections.Generic;
using UnityEngine;
using UnityEditor;

namespace TreeEditor
{

[System.Serializable]
public class TreeGroupRoot : TreeGroup
{
    // These should be propagated to every child..
    public float adaptiveLODQuality = 0.8f;
    public int shadowTextureQuality = 3; // texture resolution / 2^shadowTextureQuality
    public bool enableWelding = true;
    public bool enableAmbientOcclusion = true;
    public bool enableMaterialOptimize = true;

    public float aoDensity = 1.0f;
    public float rootSpread = 5.0f;
	public float groundOffset;

    public Matrix4x4 rootMatrix = Matrix4x4.identity;

    public void SetRootMatrix(Matrix4x4 m)
    {
        rootMatrix = m;

        // Root node needs to remove scale and position...
        rootMatrix.m03 = 0.0f;
        rootMatrix.m13 = 0.0f;
        rootMatrix.m23 = 0.0f;
        rootMatrix = MathUtils.OrthogonalizeMatrix(rootMatrix);
        nodes[0].matrix = rootMatrix;
    }

    override public bool CanHaveSubGroups()
    {
        return true;
    }

    override public void UpdateParameters()
    {
        Profiler.BeginSample("UpdateParameters");

        // Set properties
        nodes[0].size = rootSpread;
        nodes[0].matrix = rootMatrix;

        // Update sub-groups
        base.UpdateParameters();

        Profiler.EndSample(); // UpdateParameters
    }
    
}

}