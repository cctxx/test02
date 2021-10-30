#ifndef CIMPORTMESH_H
#define CIMPORTMESH_H

struct ImportScene;
struct BoneInfluence;
struct ImportNode;
struct CImportNodeUserData;

struct CImportTexture
{
	const char* path;
	const char* relativePath;
	float offset[2];
	float scale[2];
};

struct CImportMaterial
{
	const char* name;
	float diffuse[4];
	float ambient[4];
	CImportTexture texture;
	CImportTexture normalMap;
	bool hasTransparencyTexture;
};

struct CImportNode
{
	const char* name;
	float position[3];
	float rotation[4];
	float scale[3];
	int   meshIndex;
	float meshTransform[16];
//	float localPivot[3];

	int cameraIndex;
	int lightIndex;
	
	
	int userDataCount;
	CImportNodeUserData* userData;

	CImportNode* children;
	int          childCount;
	mutable ImportNode*   importnode;
};

struct CImportBone
{
	CImportNode* node;
	float bindpose[16];
};

struct CImportAnimationCurve
{
	int pre;
	int post;
	int size;
	const void* data;
};

struct CImportNodeAnimation
{
	CImportNode* node;

	CImportAnimationCurve rotation[4];
	CImportAnimationCurve translation[3];
	CImportAnimationCurve scale[3];
};
struct CImportFloatAnimation
{
	CImportNode* node;

	CImportAnimationCurve curve;
	const char* className;
	const char* propertyName;
};

struct CImportBlendShapeChannel
{
	const char* name;
	
	int frameIndex;
	int frameCount;
};

struct CImportBlendShape
{
	float targetWeight;
	
	const Vector3f* vertices;
	int vertexCount;

	const Vector3f* normals;
	int normalCount;
	
	///@TODO: What about tangents???
};

struct CImportMesh
{
	float*  vertices;
	int     vertexCount;

	BoneInfluence*  skin;
	int     skinCount;
	
	float*  normals;
	int     normalCount;

	float*  tangents;
	int     tangentCount;
	
	UInt32* colors;
	int     colorCount;
	
	float*  uvs[2];
	int     uvCount[2];
	
	UInt32* polygons;
	int     polygonCount;

	UInt32* polygonSizes;
	int     polygonSizesCount;
	
	CImportBone* bones;
	int          boneCount;
	
	int*    materials;
	int     materialCount;

	CImportBlendShapeChannel* shapeChannels;
	int                       shapeChannelCount;
	
	CImportBlendShape*   shapes;
	int                  shapeCount;

	const char* name;  
};

struct CImportAnimationClip
{
	const char* name;

	double bakeStart;
	double bakeStop;

	CImportNodeAnimation* nodeAnimations;
	int nodeAnimationCount;

	CImportFloatAnimation* floatAnimations;
	int floatAnimationCount;
};

struct CImportSceneInfo
{
	const char* applicationName;
	const char* applicationDetailedName;
	const char* exporterInfo;
	bool hasApplicationName;
	bool hasSkeleton;
};

struct CImportScene
{
	CImportMesh*     meshes;
	int              meshCount;

	CImportNode*     nodes;
	int              nodeCount;
	
	CImportMaterial* materials;
	int              materialCount;

	const ImportCameraComponent* cameras;
	int              cameraCount;

	const ImportLightComponent* lights;
	int              lightCount;

	const char*      defaultAnimationClipName;
	
	CImportAnimationClip* animationClips;
	int              animationClipCount;

	float            sampleRate;

	CImportSceneInfo sceneInfo;
};

struct CImportSettings
{
	CImportSettings() : importBlendShapes(true), importNormals(false), importTangents(false), importSkinMesh(false) {}

	int importAnimations;
	int adjustClipsByTimeRange;
	const char* absolutePath;
	const char* originalExtension;

	bool importBlendShapes;
	bool importSkinMesh;
	bool importNormals;
	bool importTangents;
};

struct CImportNodeUserData
{
	const char* name;
	
	enum UserDataType data_type_indicator;
	

	//only one of the items below is actually used. Joe does not like unions being used in cross-app-dll communication.
	int intData;
	float floatData;
	float colorData[4];
	float vectorData[4];
	const char* stringData;
	bool boolData;
};


void ReleaseCImportScene (CImportScene& scene);
void ImportSceneToCImportScene (ImportScene& inScene, CImportScene& outScene);
void CImportSceneToImportScene (CImportScene& inScene, ImportScene& outScene);

#endif
