#include "UnityPrefix.h"
#include "ImportMesh.h"
#include "CImportMesh.h"
#include "Runtime/Allocator/MemoryMacros.h"

typedef unsigned int uint;

template <class T>
void CImportVectorToImportVector(const T* const in, const int inCount, std::vector<T>& out)
{
	out.assign(in, in + inCount);
}

template <class T>
void ImportVectorToCImportVector(const std::vector<T>& in, const T*& out, int& outCount)
{
	outCount = in.size ();
	out = outCount > 0 ? &in.front() : NULL;
}

static void AnimationCurveToCAnimationCurve(const AnimationCurve& in, CImportAnimationCurve& out)
{
	// use internal mode to support old fbx importer which we can't upgrade
	out.pre = in.GetPreInfinityInternal ();
	out.post = in.GetPostInfinityInternal ();
	out.size = in.GetKeyCount ();
	if ( out.size > 0 )
		out.data = &in.GetKey (0);
	
	#if UNITY_EDITOR
	AssertIf(sizeof(KeyframeTpl<float>) != sizeof(float) * 4);
	#endif
}

static void CAnimationCurveToAnimationCurve (const CImportAnimationCurve& in, AnimationCurve& out)
{
	out.SetPreInfinityInternal (in.pre);
	out.SetPostInfinityInternal (in.post);
	const float* data = reinterpret_cast<const float*> (in.data);
	
	out.ResizeUninitialized(in.size);
	for (int i=0;i<in.size;i++)
	{
		KeyframeTpl<float>& key = out.GetKey(i);
		key.time = data[i*4 + 0];
		key.value = data[i*4 + 1];
		key.inSlope = data[i*4 + 2];
		key.outSlope = data[i*4 + 3];
	}
	
	out.InvalidateCache ();
}

static void CImportNodeToImportNodeRecurse (const CImportNode& in, ImportNode& out)
{
	out.name = in.name;
	memcpy (&out.position, in.position, sizeof (in.position));
	memcpy (&out.rotation, in.rotation, sizeof (in.rotation));
	memcpy (&out.scale, &in.scale, sizeof (in.scale));
	out.meshIndex = in.meshIndex;
	memcpy (&out.meshTransform, in.meshTransform, sizeof (Matrix4x4f));
	out.cameraIndex = in.cameraIndex;
	out.lightIndex = in.lightIndex;

	in.importnode = &out;

	out.children.resize (in.childCount);	
	
	for (int i=0;i!=in.userDataCount;i++)
	{
		const CImportNodeUserData& cud = in.userData[i];
		ImportNodeUserData ud;
		ud.data_type_indicator = cud.data_type_indicator;
		ud.name.assign(cud.name);
		ud.boolData = cud.boolData;
		ud.intData = cud.intData;
		ud.floatData = cud.floatData;
		ud.stringData = cud.stringData;

		ud.colorData = ColorRGBAf(cud.colorData[0], cud.colorData[1], cud.colorData[2], cud.colorData[3]);

		for (int i2=0;i2!=4;i2++)
			ud.vectorData[i2] = cud.vectorData[i2];
		
		out.userData.push_back(ud);
	}

	for (int i=0;i<in.childCount;i++)
		CImportNodeToImportNodeRecurse (in.children[i], out.children[i]);
}

static void ImportNodeToCImportNodeRecurse (const ImportNode& in, CImportNode& out, CImportNode*& allocatedNode)
{
	out.name = in.name.c_str ();
	memcpy (out.position, &in.position, sizeof (in.position));
	memcpy (out.rotation, &in.rotation, sizeof (in.rotation));
	memcpy (out.scale, &in.scale, sizeof (in.scale));
	out.meshIndex = in.meshIndex;
	memcpy (out.meshTransform, &in.meshTransform, sizeof (Matrix4x4f));
	out.cameraIndex = in.cameraIndex;
	out.lightIndex = in.lightIndex;

	out.childCount = (int) in.children.size ();
	out.children = allocatedNode;
	in.cimportnode = &out;
	
	out.userDataCount = (int) in.userData.size();
	out.userData = (CImportNodeUserData*) UNITY_MALLOC( kMemSerialization, sizeof(CImportNodeUserData) * out.userDataCount );
	for (int i=0; i!= in.userData.size(); i++)
	{
		CImportNodeUserData& cud = out.userData[i];
		const ImportNodeUserData& ud = in.userData[i];

		cud.name = in.userData[i].name.c_str();
		cud.data_type_indicator = ud.data_type_indicator;
		cud.boolData = ud.boolData;
		cud.floatData = ud.floatData;
		cud.intData = ud.intData;
		cud.stringData = ud.stringData.c_str();
		
		ColorRGBAf colorDataf = ud.colorData;
		for(int i2=0; i2!=4; i2++)  //Lucas: can this be done smarter?
		{
			cud.colorData[i2] = colorDataf.GetPtr()[i2];
			cud.vectorData[i2] = ud.vectorData[i2];
		}
	}

	
	allocatedNode += in.children.size ();
	for (uint i=0;i<in.children.size ();i++)
		ImportNodeToCImportNodeRecurse (in.children[i], out.children[i], allocatedNode);
}

void ReleaseCImportScene (CImportScene& scene)
{
	for (int i=0;i<scene.meshCount;i++)
	{
		UNITY_FREE (kMemSerialization, scene.meshes[i].bones);
		UNITY_FREE (kMemSerialization, scene.meshes[i].shapes);
		UNITY_FREE (kMemSerialization, scene.meshes[i].shapeChannels);
	}
	UNITY_FREE (kMemSerialization, scene.meshes); scene.meshes = NULL;
	UNITY_FREE (kMemSerialization, scene.materials); scene.materials = NULL;
	UNITY_FREE (kMemSerialization, scene.nodes); scene.nodes = NULL;
	
	for (int i = 0; i < scene.animationClipCount; ++i)
	{
		UNITY_FREE (kMemSerialization, scene.animationClips[i].nodeAnimations);
		UNITY_FREE (kMemSerialization, scene.animationClips[i].floatAnimations);
	}
	UNITY_FREE (kMemSerialization, scene.animationClips); scene.animationClips = NULL;
}

inline int CountNodesRecurse (const ImportNode& node) 
{
	int count = 1;
	for (uint i=0;i<node.children.size ();i++)
		count += CountNodesRecurse (node.children[i]);
	return count;
}

void ImportSceneInfoToCImportSceneInfo (const ImportSceneInfo& inSceneInfo, CImportSceneInfo& outSceneInfo)
{
	outSceneInfo.applicationName = inSceneInfo.applicationName.c_str();
	outSceneInfo.applicationDetailedName = inSceneInfo.applicationDetailedName.c_str();
	outSceneInfo.exporterInfo = inSceneInfo.exporterInfo.c_str();
	outSceneInfo.hasApplicationName = inSceneInfo.hasApplicationName;
	outSceneInfo.hasSkeleton = inSceneInfo.hasSkeleton;
}

void CImportSceneInfoToImportSceneInfo (const CImportSceneInfo& inSceneInfo, ImportSceneInfo& outSceneInfo)
{
	outSceneInfo.applicationName = inSceneInfo.applicationName;
	outSceneInfo.applicationDetailedName = inSceneInfo.applicationDetailedName;
	outSceneInfo.exporterInfo = inSceneInfo.exporterInfo;
	outSceneInfo.hasApplicationName = inSceneInfo.hasApplicationName;
	outSceneInfo.hasSkeleton = inSceneInfo.hasSkeleton;
}

void ImportTextureToCImportTexture(const ImportTexture& in, CImportTexture& out)
{
	out.path = in.path.c_str();
	out.relativePath = in.relativePath.c_str();
	Assert(sizeof(in.offset) == sizeof(out.offset) && sizeof(in.scale) == sizeof(out.scale));
	memcpy(out.offset, &in.offset, sizeof(in.offset));
	memcpy(out.scale, &in.scale, sizeof(in.scale));
}

void CImportTextureToImportTexture(const CImportTexture& in, ImportTexture& out)
{
	out.path = in.path;
	out.relativePath = in.relativePath;
	Assert(sizeof(in.offset) == sizeof(out.offset) && sizeof(in.scale) == sizeof(out.scale));
	memcpy(&out.offset, in.offset, sizeof(in.offset));
	memcpy(&out.scale, in.scale, sizeof(in.scale));
}

void CImportBlendShapeToImportBlendShape(const CImportBlendShape& in, ImportBlendShape& out)
{
	out.targetWeight = in.targetWeight;
	CImportVectorToImportVector(in.vertices, in.vertexCount, out.vertices);
	CImportVectorToImportVector(in.normals, in.normalCount, out.normals);	
}

void ImportBlendShapeToCImportBlendShape(const ImportBlendShape& in, CImportBlendShape& out)
{
	out.targetWeight = in.targetWeight;
	ImportVectorToCImportVector(in.vertices, out.vertices, out.vertexCount);
	ImportVectorToCImportVector(in.normals, out.normals, out.normalCount);
}

void CImportBlendShapeChannelToImportBlendShapeChannel(const CImportBlendShapeChannel& in, ImportBlendShapeChannel& out)
{
	out.name = in.name;
	out.frameIndex = in.frameIndex;
	out.frameCount = in.frameCount;
}

void ImportBlendShapeChannelToCImportBlendShapeChannel(const ImportBlendShapeChannel& in, CImportBlendShapeChannel& out)
{
	out.name = in.name.c_str();
	out.frameIndex = in.frameIndex;
	out.frameCount = in.frameCount;
}

void ImportNodeAnimationToCImportNodeAnimation(const ImportNodeAnimation& in, CImportNodeAnimation& out)
{
	out.node = in.node->cimportnode;

	for (int q=0;q<4;q++)
		AnimationCurveToCAnimationCurve (in.rotation[q], out.rotation[q]);
	for (int q=0;q<3;q++)
		AnimationCurveToCAnimationCurve (in.translation[q], out.translation[q]);
	for (int q=0;q<3;q++)
		AnimationCurveToCAnimationCurve (in.scale[q], out.scale[q]);
}

void ImportFloatAnimationToCImportFloatAnimation(const ImportFloatAnimation& in, CImportFloatAnimation& out)
{
	out.node = in.node->cimportnode;
	out.className = in.className.c_str();
	out.propertyName = in.propertyName.c_str();
	AnimationCurveToCAnimationCurve (in.curve, out.curve);
}

template <class A, class CA, class Converter>
void AnimationsToCAnimations(const std::list<A>& in, CA*& out, int& outCount, Converter converter)
{
	outCount = in.size();
	out = (CA*)UNITY_CALLOC(kMemSerialization, outCount, sizeof(CA));
	Assert(out);	
	typename std::list<A>::const_iterator it = in.begin ();
	for (int i = 0; i < outCount; ++i, ++it)
		converter(*it, out[i]);
}

void ImportSceneToCImportScene (ImportScene& inScene, CImportScene& outScene)
{
	outScene.defaultAnimationClipName = inScene.defaultAnimationClipName.c_str ();
	outScene.sampleRate = inScene.sampleRate;

	// Convert nodes
	int nodeCount = 0;
	for (uint i=0;i<inScene.nodes.size ();i++)
		nodeCount += CountNodesRecurse (inScene.nodes[i]);
	CImportNode* allocator = (CImportNode*)UNITY_MALLOC (kMemSerialization, sizeof (CImportNode) * nodeCount);

	outScene.nodes = allocator;
	outScene.nodeCount = (int) inScene.nodes.size ();
	allocator += inScene.nodes.size ();
	for (uint i=0;i<inScene.nodes.size ();i++)
		ImportNodeToCImportNodeRecurse (inScene.nodes[i], outScene.nodes[i], allocator);
	
	// Convert meshes
	outScene.meshes = (CImportMesh*)UNITY_CALLOC ( kMemSerialization, inScene.meshes.size(), sizeof (CImportMesh) );
	outScene.meshCount = (int) inScene.meshes.size ();
	for (uint i=0;i<inScene.meshes.size ();i++)
	{
		ImportMesh& in = inScene.meshes[i];
		CImportMesh& out = outScene.meshes[i];
		
		out.vertexCount = (int) in.vertices.size ();
		if ( out.vertexCount > 0 )
			out.vertices = in.vertices[0].GetPtr ();
		
		out.skinCount = (int) in.skin.size ();
		if ( out.skinCount > 0 )
			out.skin = &in.skin[0];
		
		out.normalCount = (int) in.normals.size ();
		if ( out.normalCount > 0 )
			out.normals = in.normals[0].GetPtr ();

		out.tangentCount = (int) in.tangents.size ();
		if ( out.tangentCount > 0 )
			out.tangents = in.tangents[0].GetPtr ();

		out.colorCount = (int) in.colors.size ();
		if ( out.colorCount > 0 )
			out.colors = (UInt32*)&in.colors[0];

		out.uvCount[0] = (int) in.uvs[0].size ();
		if ( out.uvCount[ 0 ] > 0 )
			out.uvs[0] = in.uvs[0][0].GetPtr ();
		
		out.uvCount[1] = (int) in.uvs[1].size ();
		if ( out.uvCount[ 1 ] > 0 )
			out.uvs[1] = in.uvs[1][0].GetPtr ();
		
		out.polygonCount = (int) in.polygons.size ();
		if ( out.polygonCount > 0 )
			out.polygons = &in.polygons[0];
		
		out.polygonSizesCount = (int) in.polygonSizes.size ();
		if ( out.polygonSizesCount > 0 )
			out.polygonSizes = &in.polygonSizes[0];
		
		out.materialCount = (int) in.materials.size ();
		if ( out.materialCount > 0 )
			out.materials = &in.materials[0];
		
		out.name = in.name.c_str ();

		if (!in.bones.empty ())
		{
			out.bones = (CImportBone*)UNITY_MALLOC (kMemSerialization, sizeof (CImportBone) * in.bones.size ());
			AssertIf(!out.bones);
			out.boneCount = (int) in.bones.size ();
			for (uint b=0;b<in.bones.size ();b++)
			{
				memcpy (out.bones[b].bindpose, &in.bones[b].bindpose, sizeof (Matrix4x4f));
				ImportBone& bone = in.bones[b];
				out.bones[b].node = (CImportNode*)bone.node->cimportnode;
			}
		}	
		else
		{
			out.bones = NULL;
			out.boneCount = 0;
		}


		if (!in.shapes.empty() && !in.shapeChannels.empty())
		{
			out.shapes = (CImportBlendShape*)UNITY_MALLOC (kMemSerialization, sizeof(CImportBlendShape) * in.shapes.size());
			AssertIf(!out.shapes);
			out.shapeCount = (int) in.shapes.size();
			for (unsigned int j = 0; j < in.shapes.size(); ++j)
				ImportBlendShapeToCImportBlendShape(in.shapes[j], out.shapes[j]);

			out.shapeChannels = (CImportBlendShapeChannel*)UNITY_MALLOC (kMemSerialization, sizeof(CImportBlendShapeChannel) * in.shapes.size());
			AssertIf(!out.shapeChannels);
			out.shapeChannelCount = (int) in.shapeChannels.size();
			for (unsigned int j = 0; j < in.shapeChannels.size(); ++j)
				ImportBlendShapeChannelToCImportBlendShapeChannel(in.shapeChannels[j], out.shapeChannels[j]);
		}	
		else
		{
			out.shapeChannels = NULL;
			out.shapeChannelCount = 0;
			
			out.shapes = NULL;
			out.shapeCount = 0;
		}
	}	


	// Convert Materials
	outScene.materials = (CImportMaterial*)UNITY_MALLOC (kMemSerialization, inScene.materials.size () * sizeof (CImportMaterial));
	AssertIf(!outScene.materials);
	outScene.materialCount = (int) inScene.materials.size ();
	for (int i=0;i<outScene.materialCount;i++)
	{
		outScene.materials[i].name = inScene.materials[i].name.c_str ();
		memcpy (outScene.materials[i].diffuse, &inScene.materials[i].diffuse, sizeof (inScene.materials[i].diffuse));
		memcpy (outScene.materials[i].ambient, &inScene.materials[i].ambient, sizeof (inScene.materials[i].ambient));
		ImportTextureToCImportTexture(inScene.materials[i].texture, outScene.materials[i].texture);
		ImportTextureToCImportTexture(inScene.materials[i].normalMap, outScene.materials[i].normalMap);
		outScene.materials[i].hasTransparencyTexture = inScene.materials[i].hasTransparencyTexture;
	}

	ImportVectorToCImportVector(inScene.cameras, outScene.cameras, outScene.cameraCount);
	ImportVectorToCImportVector(inScene.lights, outScene.lights, outScene.lightCount);


	// Convert Animation clips
	outScene.animationClips = (CImportAnimationClip*)UNITY_MALLOC (kMemSerialization, inScene.animationClips.size() * sizeof(CImportAnimationClip));
	AssertIf(!outScene.animationClips);
	outScene.animationClipCount = (int) inScene.animationClips.size ();
	for (int i=0;i<outScene.animationClipCount;i++)
	{
		const ImportAnimationClip& inClip = inScene.animationClips[i];
		CImportAnimationClip& outClip = outScene.animationClips[i];

		outClip.name = inClip.name.c_str ();
		outClip.bakeStart = inClip.bakeStart;
		outClip.bakeStop = inClip.bakeStop;

		AnimationsToCAnimations(inClip.nodeAnimations, outClip.nodeAnimations, outClip.nodeAnimationCount, ImportNodeAnimationToCImportNodeAnimation);
		AnimationsToCAnimations(inClip.floatAnimations, outClip.floatAnimations, outClip.floatAnimationCount, ImportFloatAnimationToCImportFloatAnimation);
	}

	ImportSceneInfoToCImportSceneInfo(inScene.sceneInfo, outScene.sceneInfo);
}

void CImportNodeAnimationToImportNodeAnimation(const CImportNodeAnimation& in, ImportNodeAnimation& out)
{
	out.node = in.node->importnode;

	for (int q=0;q<4;q++)
		CAnimationCurveToAnimationCurve(in.rotation[q], out.rotation[q]);
	for (int q=0;q<3;q++)
		CAnimationCurveToAnimationCurve(in.translation[q], out.translation[q]);
	for (int q=0;q<3;q++)
		CAnimationCurveToAnimationCurve(in.scale[q], out.scale[q]);
}

void CImportFloatAnimationToImportFloatAnimation(const CImportFloatAnimation& in, ImportFloatAnimation& out)
{
	out.node = in.node->importnode;
	out.className = in.className;
	out.propertyName = in.propertyName;
	CAnimationCurveToAnimationCurve(in.curve, out.curve);
}

template <class CA, class A, class Converter>
void CAnimationsToAnimations(const CA* const in, const int inCount, std::list<A>& out, Converter converter)
{
	for (int i = 0; i < inCount; ++i)
	{
		out.push_back(A());
		converter(in[i], out.back());
	}
}

void CImportSceneToImportScene (CImportScene& inScene, ImportScene& outScene)
{
	outScene.defaultAnimationClipName = inScene.defaultAnimationClipName;
	outScene.sampleRate = inScene.sampleRate;

	// Convert nodes
	outScene.nodes.resize (inScene.nodeCount);
	for (int i=0;i<inScene.nodeCount;i++)
		CImportNodeToImportNodeRecurse (inScene.nodes[i], outScene.nodes[i]);

	// Convert meshes
	outScene.meshes.resize (inScene.meshCount);
	for (int i=0;i<inScene.meshCount;i++)
	{
		CImportMesh& in = inScene.meshes[i];
		ImportMesh& out = outScene.meshes[i];
		
		out.vertices.assign ((Vector3f*)in.vertices, (Vector3f*)in.vertices + in.vertexCount);
		out.skin.assign ((BoneInfluence*)in.skin, (BoneInfluence*)in.skin + in.skinCount);
		out.normals.assign ((Vector3f*)in.normals, (Vector3f*)in.normals + in.normalCount);
		out.tangents.assign ((Vector4f*)in.tangents, (Vector4f*)in.tangents + in.tangentCount);
		out.colors.assign ((ColorRGBA32*)in.colors, (ColorRGBA32*)in.colors + in.colorCount);
		out.uvs[0].assign ((Vector2f*)in.uvs[0], (Vector2f*)in.uvs[0] + in.uvCount[0]);
		out.uvs[1].assign ((Vector2f*)in.uvs[1], (Vector2f*)in.uvs[1] + in.uvCount[1]);
		out.polygons.assign ((int*)in.polygons, (int*)in.polygons + in.polygonCount);
		out.polygonSizes.assign ((int*)in.polygonSizes, (int*)in.polygonSizes + in.polygonSizesCount);
		out.materials.assign (in.materials, in.materials + in.materialCount);
				
		out.name = in.name;
		
		out.bones.resize (in.boneCount);
		for (int b=0;b<in.boneCount;b++)
		{
			memcpy (&out.bones[b].bindpose, in.bones[b].bindpose, sizeof (Matrix4x4f));
			out.bones[b].node = in.bones[b].node->importnode;
		}

		out.shapes.resize (in.shapeCount);
		for (int j = 0; j < in.shapeCount; ++j)
			CImportBlendShapeToImportBlendShape(in.shapes[j], out.shapes[j]);

		out.shapeChannels.resize (in.shapeChannelCount);
		for (int j = 0; j < in.shapeChannelCount; ++j)
			CImportBlendShapeChannelToImportBlendShapeChannel(in.shapeChannels[j], out.shapeChannels[j]);
	}

	// Convert Materials
	outScene.materials.resize (inScene.materialCount);
	for (int i=0;i<inScene.materialCount;i++)
	{
		outScene.materials[i].name = inScene.materials[i].name;
		memcpy (&outScene.materials[i].diffuse, inScene.materials[i].diffuse, sizeof (inScene.materials[i].diffuse));
		memcpy (&outScene.materials[i].ambient, inScene.materials[i].ambient, sizeof (inScene.materials[i].ambient));
		CImportTextureToImportTexture(inScene.materials[i].texture, outScene.materials[i].texture);
		CImportTextureToImportTexture(inScene.materials[i].normalMap, outScene.materials[i].normalMap);
		outScene.materials[i].hasTransparencyTexture = inScene.materials[i].hasTransparencyTexture;
	}

	CImportVectorToImportVector(inScene.cameras, inScene.cameraCount, outScene.cameras);
	CImportVectorToImportVector(inScene.lights, inScene.lightCount, outScene.lights);

	// Convert Animation clips
	outScene.animationClips.resize(inScene.animationClipCount);
	for (int i=0;i<inScene.animationClipCount;i++)
	{
		const CImportAnimationClip& inClip = inScene.animationClips[i];
		ImportAnimationClip& outClip = outScene.animationClips[i];
		
		outClip.name = inClip.name;
		outClip.bakeStart = inClip.bakeStart;
		outClip.bakeStop = inClip.bakeStop;

		CAnimationsToAnimations(inClip.nodeAnimations, inClip.nodeAnimationCount, outClip.nodeAnimations, CImportNodeAnimationToImportNodeAnimation);
		CAnimationsToAnimations(inClip.floatAnimations, inClip.floatAnimationCount, outClip.floatAnimations, CImportFloatAnimationToImportFloatAnimation);		
	}

	CImportSceneInfoToImportSceneInfo(inScene.sceneInfo, outScene.sceneInfo);
}
