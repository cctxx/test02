#ifndef IMPORTMESH_H
#define IMPORTMESH_H

#include "Runtime/Math/Color.h"
#include "Runtime/Math/Quaternion.h"
#include "Runtime/Math/Vector2.h"
#include "Runtime/Math/Vector4.h"
#include "Runtime/Filters/Mesh/Mesh.h"
#include <string>
#include <vector>
#include <list>
#include "Runtime/Math/AnimationCurve.h"
#include "Runtime/Camera/Lighting.h"

struct ImportNode;
struct CImportNode;
struct ImportNodeUserData;
namespace Unity { class GameObject; }
using namespace Unity;
class Mesh;


struct ImportTexture
{
	ImportTexture();

	std::string path;
	std::string relativePath;
	Vector2f offset;
	Vector2f scale;
};


struct ImportMaterial
{
	std::string name;
	ColorRGBAf  diffuse; // white (default)
	ColorRGBAf  ambient; // white (default)
	ImportTexture texture;
	ImportTexture normalMap;
	bool hasTransparencyTexture;
	
	ImportMaterial ();
};


struct ImportBone
{
	// The node this bone uses to deform the mesh!
	ImportNode* node;
	Matrix4x4f bindpose;

	ImportBone () { node = NULL; bindpose.SetIdentity (); }
};

struct ImportBlendShape
{
	float                 targetWeight;
	
	// vertices, normals and tangents are stored as non-delta data, i.e. plain object-space just as mesh data
	std::vector<Vector3f> vertices;
	std::vector<Vector3f> normals;
	
	
	////@TODO: Review why this doesn't get copied in CImportMesh...
	std::vector<Vector3f> tangents;
};

struct ImportBlendShapeChannel
{
	std::string name;
	
	// index into shapes
	int frameIndex;
	int frameCount;
};

struct ImportMesh
{
	std::vector<Vector3f>        vertices;
	dynamic_array<BoneInfluence> skin; // per vertex skin info

	// Before mesh is split across seams, these arrays are per polygon vertex!
	// That is their size == polygons.size
	std::vector<Vector3f>      normals;
	std::vector<Vector4f>      tangents;
	std::vector<ColorRGBA32>   colors;
	std::vector<Vector2f>      uvs[2];
	std::vector<int>		   smoothingGroups;

	// Index buffer, potentially arbitrary number of indices per polygon
	std::vector<UInt32>        polygons;

	// Per-polygon arrays.
	std::vector<UInt32>        polygonSizes; // Size of each polygon
	std::vector<int>           materials; // Material index of each polygon


	std::string                name;
	std::vector<ImportBone>    bones;
	std::vector<ImportBlendShape>   shapes;
	std::vector<ImportBlendShapeChannel> shapeChannels;
	bool	                            hasAnyQuads;


	ImportMesh() : hasAnyQuads(false) {  }

	// src argument is optional
	void Reserve (int vertexCount, int faceCount, const ImportMesh* src);
	unsigned AdviseVertexFormat () const;
};

struct ImportBaseAnimation
{
	ImportNode*    node;
};

struct ImportNodeAnimation : public ImportBaseAnimation
{
	AnimationCurve rotation[4];
	AnimationCurve translation[3];
	AnimationCurve scale[3];
};
typedef std::list<ImportNodeAnimation> ImportNodeAnimations;

struct ImportFloatAnimation : public ImportBaseAnimation
{
	AnimationCurve curve;
	std::string className;
	std::string propertyName;
};
typedef std::list<ImportFloatAnimation> ImportFloatAnimations;


struct ImportCameraComponent
{
	bool orthographic;
	float orthographicSize;
	float fieldOfView;
	float nearPlane;
	float farPlane;
};

struct ImportLightComponent
{
	LightType type;
	float rangeStart;
	float rangeEnd;
	float spotAngle;
	float intensity;
	ColorRGBAf color;
	bool castShadows;
};

struct ImportNode
{
	std::string name;
	Vector3f    position; // zero (default)
	Quaternionf rotation; // identity (default)
	Vector3f    scale; // identity (default)
	int         meshIndex;// -1 default
	Matrix4x4f  meshTransform;// identity (default)
	int			cameraIndex;
	int			lightIndex;
	
	std::vector<ImportNodeUserData> userData;
	

	std::vector<ImportNode> children;

	struct InstantiatedMesh
	{
		InstantiatedMesh(GameObject* gameObject_, Mesh* mesh_) : gameObject(gameObject_), mesh(mesh_) {}

		GameObject* gameObject;
		Mesh* mesh;
	};

	
	mutable GameObject* instantiatedGameObject;
	// instantiatedLodMeshes.size() is:
	// 0 - when ImportNode doesn't have meshes
	// 1 - when ImportNode has a mesh which doesn't have to be split (then instantiatedLodMeshes[0].gameObject is the same as instantiatedGameObject)
	// N - when ImportNode has a mesh which has to be split (then instantiatedLodMeshes[i].gameObject is a child of instantiatedGameObject)
	mutable std::vector<InstantiatedMesh> instantiatedLodMeshes;
	mutable CImportNode* cimportnode;
	
	ImportNode();
};
typedef std::vector<ImportNode> ImportNodes;


enum UserDataType
{
	kUserDataBool,
	kUserDataFloat,
	kUserDataColor,
	kUserDataInt,
	kUserDataVector,
	kUserDataString
};


struct ImportNodeUserData
{
	std::string name;
	
	UserDataType data_type_indicator;

	//data
	bool boolData;
	Vector4f vectorData;
	std::string stringData;
	ColorRGBA32 colorData;
	int intData;				
	float floatData;
};


namespace ObsoleteTangentSpaceOptions
{
	// this should be used only for backwards compatibility in streaming
	enum 
	{
		kTangentSpaceAll = 0,
		kTangentSpaceOnlyNormals = 1,
		kTangentSpaceNone = 2
	};
}


enum TangentSpaceOptions
{
	kTangentSpaceOptionsImport = 0,
	kTangentSpaceOptionsCalculate = 1,
	kTangentSpaceOptionsNone = 2
};


enum MeshOptimizationOptions
{
	kMeshOptimizationNone = 0,
	kMeshOptimizationPreTransformCache = 1,				// vertex / index reorder so they come in increasing order
	kMeshOptimizationPostTransformCache = 2,			// just reorder the index buffer to maximize post-transform cache
	kMeshOptimizationFull = 3,
	//
	kMeshOptimizationTriStrips = 4,
	kMeshOptimizationPreTransformCacheTriStrips = 5,
	kMeshOptimizationPostTransformCacheTriStrips = 6,
	kMeshOptimizationFullTriStrips = 7,
};


struct ImportMeshSettings
{
	bool	optimizeMesh; // default true;
	bool    weldVertices;// true (default)
	bool    invertWinding;// false (default)
	bool    swapUVChannels; // false
	bool	generateSecondaryUV; //false
	
	// padding goes here due to align fail
	float	secondaryUVAngleDistortion;
	float	secondaryUVAreaDistortion;
	float	secondaryUVHardAngle;
	float	secondaryUVPackMargin;

	// Tangent space settings
	TangentSpaceOptions normalImportMode;
	TangentSpaceOptions tangentImportMode;
	float	normalSmoothAngle;	/// 60 (default)
	bool	splitTangentsAcrossUV; // default false

	ImportMeshSettings ();
};


struct ImportAnimationClip
{
	std::string name;
	double bakeStart;
	double bakeStop;

	ImportNodeAnimations nodeAnimations;
	ImportFloatAnimations floatAnimations;

	bool HasAnimations() { return !nodeAnimations.empty() || !floatAnimations.empty(); }
};


struct ImportSceneInfo
{
	std::string applicationName;
	std::string applicationDetailedName;
	std::string exporterInfo;
	bool hasApplicationName;
	bool hasSkeleton;
	
	ImportSceneInfo ()
	{
		hasSkeleton = false;
	}
};



struct ImportScene
{
	ImportNodes                         nodes;
	std::string                         defaultAnimationClipName;
	std::vector<ImportMesh>             meshes;
	std::vector<ImportMaterial>         materials;
	std::vector<ImportCameraComponent>  cameras;
	std::vector<ImportLightComponent>	lights;
	
	std::vector<ImportAnimationClip>	animationClips;

	float                               sampleRate;

	ImportSceneInfo sceneInfo;
};

#endif
