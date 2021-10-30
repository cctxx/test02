#pragma once
#include "Configuration/UnityConfigure.h"

#if ENABLE_TERRAIN

#include "Runtime/Math/Vector3.h"
#include "Runtime/Geometry/AABB.h"
#include "Runtime/Utilities/Utility.h"
#include "Runtime/Dynamics/Collider.h"
#include "Runtime/Utilities/LinkedList.h"
#include "Runtime/Dynamics/PhysicMaterial.h"

class Mesh;
class NxHeightField;
class NxHeightFieldDesc;
struct SubMesh;
class TerrainData;
class TerrainCollider;
class TerrainRenderer;
class VBO;

enum 
{
	kDirectionLeft = 0, kDirectionRight = 1, kDirectionUp = 2, kDirectionDown = 3, 
	kDirectionLeftUp = 0, kDirectionRightUp = 1, kDirectionLeftDown = 2, kDirectionRightDown = 3,
};

enum
{
	kDirectionLeftFlag = (1<<kDirectionLeft),
	kDirectionRightFlag = (1<<kDirectionRight),
	kDirectionUpFlag = (1<<kDirectionUp),
	kDirectionDownFlag = (1<<kDirectionDown),	
	kDirectionDirectNeighbourMask = (kDirectionLeftFlag|kDirectionRightFlag|kDirectionUpFlag|kDirectionDownFlag),
};

//made this value constant. It is not currently being modified anyways, and this way,
//cached triangles strips can be used across several terrains.
//Must be an odd number.
#define kPatchSize 17

class Heightmap
{
  public:
	enum { kMaxHeight =  32766 };
	
	DECLARE_SERIALIZE(Heightmap)
	
	Heightmap (TerrainData* owner);
	virtual ~Heightmap  ();

	int GetWidth () const { return m_Width; }
	int GetHeight () const { return m_Height; }
	int GetMipLevels () const { return m_Levels; }
	
	const Vector3f& GetScale () const { return m_Scale; }
	
	Vector3f GetSize () const;
	
	void SetSize (const Vector3f& size);
		
	virtual void AwakeFromLoad(AwakeFromLoadMode mode){}
	
	/// After editing is complete we need to recompute the error for the modified patches
	/// We also update the min&max height of each patch
	void RecomputeInvalidPatches (UNITY_TEMP_VECTOR(int)& recomputedPatches);
	
	float GetMaximumHeightError (int x, int y, int level) const;

	/// The scaled interpolated height at the normalized coordinate x, y [0...1]
	/// Out of bounds x and y will be clamped
	float GetInterpolatedHeight (float x, float y) const;
	
	/// Interpolates 4 height values just like GetInterpolated height
	/// Used for optimized heightmap lookups when you know the corners.
	// Corners are laid out like this
	/// 0 1
	/// 2 3
	static float Bilerp (const float* corners, float u, float v);

	/// The scaled height at height map pixel x, y.
	/// Out of bounds x and y will be clamped
	float GetHeight (int x, int y) const;

	SInt16 GetRawHeight(int sampleIndex) const { return m_Heights[sampleIndex]; }

	float GetHeightRespectingNeighbors (int x, int y, const TerrainRenderer *renderer) const;
	
	float GetSteepness (float x, float y) const;
	
	Vector3f GetInterpolatedNormal (float x, float y) const;

	void GetHeights (int xBase, int yBase, int width, int height, float* heights) const;
	void SetHeights (int xBase, int yBase, int width, int height, const float* heights, bool delay);
	
	int GetAdjustedSize (int size) const;
	
	void SetResolution (int resolution);
	int GetResolution () const { return m_Width; }
	
	int GetTotalPatchCount () const { return GetPatchIndex(0, 0, m_Levels) + 1; }

	int GetMaterialIndex () const;
	
	void GetPatchData (int xPatch, int yPatch, int mipLevel, float* heights) const;

	void UpdatePatchIndices (Mesh& mesh, int xPatch, int yPatch, int level, int edgeMask);
	void UpdatePatchMesh (Mesh& mesh, int xPatch, int yPatch, int mipLevel, int edgeMask, TerrainRenderer *renderer);
	void UpdatePatchIndices (VBO& vbo, SubMesh& subMesh, int xPatch, int yPatch, int level, int edgeMask);
	void UpdatePatchMesh (VBO& vbo, SubMesh& subMesh, int xPatch, int yPatch, int mipLevel, int edgeMask, TerrainRenderer *renderer);
	AABB GetBounds (int xPatch, int yPatch, int mipLevel) const;
	
	NxHeightField* GetNxHeightField ();

	typedef List< ListNode<TerrainCollider> > TerrainColliderList;
	TerrainColliderList& GetTerrainColliders () { return m_TerrainColliders; }

	///@TODO: THIS  IS STILL INCORRECT
	Vector3f CalculateNormalSobel (int x, int y) const;
	Vector3f CalculateNormalSobelRespectingNeighbors (int x, int y, const TerrainRenderer *renderer) const;

	AABB GetBounds () const;
	
	void AwakeFromLoad ();

#if ENABLE_PHYSICS
	PPtr<PhysicMaterial> GetPhysicMaterial() const { return m_DefaultPhysicMaterial; }
	void SetPhysicMaterial(PPtr<PhysicMaterial> mat);
#endif
	
private:  
  
  	TerrainData* m_TerrainData;
	/// Raw heightmap
	std::vector<SInt16>    m_Heights;
	
	// Precomputed error of every patch
	std::vector<float>		m_PrecomputedError; 
	 // Precomputed min&max value of each terrain patch
	std::vector<float>		m_MinMaxPatchHeights;
	
	TerrainColliderList		m_TerrainColliders;
			
	int                         m_Width;
	int                         m_Height;
	int                         m_Levels;
	Vector3f                m_Scale;
	
	int GetPatchCountX (int level) const { return 1 << (m_Levels - level); }
	int GetPatchCountY (int level) const { return 1 << (m_Levels - level); }

	inline float GetPatchHeight (float* data, int x, int y) const
	{
		return data[y + x * kPatchSize];
	}

	/// Calculates the index of the patch given it's level and x, y index
	int GetPatchIndex (int x, int y, int level) const;

	float InterpolatePatchHeight (float* data, float fx, float fy) const;
	
	float ComputeMaximumHeightError (int xPatch, int yPatch, int level) const;

	void RecalculateMinMaxHeight (int xPatch, int yPatch, int mipLevel);
	// Precompute error only on a part of the heightmap
	// if forceHighestLod is enabled we simply set the error to infinity
	// This casues the heightmap to be rendered at full res (Used while editing)
	void PrecomputeError (int minX, int minY, int width, int height, bool forceHighestLod);

#if ENABLE_PHYSICS
	PPtr<PhysicMaterial> m_DefaultPhysicMaterial;
	NxHeightField*       m_NxHeightField;	

	void CleanupNx ();
	void CreateNx ();
	void UpdateNx ();
	void BuildDesc (NxHeightFieldDesc& desc);
	
	void RecreateShapes ();
#endif
};

template<class TransferFunc>
void Heightmap::Transfer (TransferFunc& transfer)
{
	TRANSFER(m_Heights);
	transfer.Align();
	TRANSFER(m_PrecomputedError);
	TRANSFER(m_MinMaxPatchHeights);
#if ENABLE_PHYSICS
	TRANSFER(m_DefaultPhysicMaterial);
#endif
	TRANSFER(m_Width);
	TRANSFER(m_Height);
	TRANSFER(m_Levels);
	TRANSFER(m_Scale);
}

#endif // ENABLE_TERRAIN
