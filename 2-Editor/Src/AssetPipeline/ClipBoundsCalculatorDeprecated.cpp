#include "UnityPrefix.h"
#include "ClipBoundsCalculatorDeprecated.h"
#include "AssetPathUtilities.h"
#include "Runtime/Animation/Animation.h"
#include "Runtime/Animation/AnimationState.h"
#include "Runtime/Animation/AnimationClip.h"
#include "Runtime/Math/Matrix4x4.h"
#include "Runtime/Filters/Deformation/SkinnedMeshFilter.h"
#include "Runtime/Filters/Mesh/MeshRenderer.h"
#include "Runtime/Filters/Mesh/LodMesh.h"
#include <map>
#include <vector>

//#define PRINT_TIME

#ifdef PRINT_TIME
#include "Runtime/Input/TimeManager.h"
#endif

namespace
{
	typedef std::vector<int> NodeParents;

	struct NodeInfo
	{
	public:
		NodeInfo() : index(-2), position(0), rotation(0), scale(0), rigidMesh(0), skinnedMesh(0), blendShape(0) {}

		bool HasCurves() const { return position || rotation || scale; }

		bool HasMesh() const { return rigidMesh || skinnedMesh; }

	public:
		int index;
		std::string path;

		// Rigid mesh or Skinned mesh
		Mesh* rigidMesh;
		SkinnedMeshRenderer* skinnedMesh;
		SkinnedMeshRenderer* blendShape;

		std::vector<int> boneIndexToMatrixIndex;
		int              rootBoneMatrixIndex; 

		// Animation
		const AnimationCurveVec3* position;
		const AnimationCurveQuat* rotation;
		const AnimationCurveVec3* scale;
		
		MinMaxAABB skinnedMeshBounds;
	};

	typedef std::map<const GameObject*, NodeInfo> NodeToInfoMap;

	void BuildNodeArray(const int parentIndex, const std::string& parentPath, GameObject& gameObject, NodeToInfoMap& map, NodeParents& nodeParents)
	{
		Assert(&gameObject);

		//const ImportNode& node = nodes[i];

		nodeParents.push_back(parentIndex);

		NodeInfo ni;
		ni.index = map.size();
		ni.path = (parentIndex < 0 ? "" : parentPath + "/") + gameObject.GetName();

		{
			MeshRenderer* r = gameObject.QueryComponent(MeshRenderer);
			ni.rigidMesh = !r ? NULL : r->GetSharedMesh();
		}
		ni.skinnedMesh = gameObject.QueryComponent(SkinnedMeshRenderer);
		if (ni.skinnedMesh)
		{
			Mesh* m = ni.skinnedMesh->GetMesh();
			if (!m->GetBoneWeights())
			{
				ni.rigidMesh = m;

				// Mesh is actually rigid mesh, but we use SkinnedMeshRenderer because this mesh has BlendShapes
				Assert(m->GetBlendShapeChannelCount() != 0 );
				ni.blendShape = ni.skinnedMesh;
				ni.skinnedMesh = NULL;
			}
		}
		Assert(!ni.rigidMesh || !ni.skinnedMesh);

		map.insert(std::make_pair(&gameObject, ni));

		Transform& transform = gameObject.GetComponent(Transform);
		for (Transform::iterator it = transform.begin(), end = transform.end(); it != end; ++it)
			BuildNodeArray(ni.index, ni.path, (*it)->GetGameObject(), map, nodeParents);
	}

	void BuildBoneMaps(NodeToInfoMap& nodeMap)
	{
		for (NodeToInfoMap::iterator it = nodeMap.begin(), end = nodeMap.end(); it != end; ++it)
		{
			NodeInfo& ni = it->second;

			if (ni.skinnedMesh)
			{
				const dynamic_array<PPtr<Transform> >& bones = ni.skinnedMesh->GetBones();
				ni.boneIndexToMatrixIndex.resize(bones.size(), -1);
				NodeToInfoMap::const_iterator it;

				for (size_t i = 0; i < bones.size(); ++i)
				{	
					// Sometimes bones can be null if they are destroyed in the AssetPostprocessor
					if (bones[i])
					{
						const Transform& bone = *bones[i];
						it = nodeMap.find(&bone.GetGameObject());
						Assert(it != nodeMap.end());

						ni.boneIndexToMatrixIndex[i] = it->second.index;
					}
				}

				it = nodeMap.find(&ni.skinnedMesh->GetActualRootBone().GetGameObject());
				Assert(it != nodeMap.end());
				ni.rootBoneMatrixIndex = it->second.index;
			}
		}
	}
/*
	std::string GetGameObjectPath(const NodeToInfoMap& map, const GameObject& node, const GameObject& findGO)
	{
		if (&node == &findGO)
		{
			map.find(&node
		}

		for (NodeToInfoMap::const_iterator it = map.begin(), end = map.end(); it != end; ++it)
			if (it->first->instantiatedGameObject == &gameObject)
				return it->second.path;

		AssertMsg(false, "Couldn't find path for gameObject %s", gameObject.GetName());
		return "";
	}*/

	typedef std::vector<char> AnimatedNodes;

	void SetPositionCurve(NodeInfo& nodeInfo, const AnimationCurveVec3* curve) { nodeInfo.position = curve; }
	void SetRotationCurve(NodeInfo& nodeInfo, const AnimationCurveQuat* curve) { nodeInfo.rotation = curve; }
	void SetScaleCurve(NodeInfo& nodeInfo, const AnimationCurveVec3* curve) { nodeInfo.scale = curve; }

	typedef std::map<std::string, NodeInfo*> NodePathMap;
	NodePathMap animatedNodePaths;

	template <class T, class U>
	void GatherAnimationCurves(NodePathMap& animatedNodePaths, AnimatedNodes& animatedNodes, T& curves, U SetCurveFuntion) 
	{
		for (size_t i = 0; i < curves.size(); ++i)
		{
			NodePathMap::iterator it = animatedNodePaths.find(curves[i].path);
			if (it != animatedNodePaths.end())
			{
				SetCurveFuntion(*it->second, &curves[i].curve);
				animatedNodes[it->second->index] = true;
			}				
		}
	}

	void GatherAnimationCurves(const std::string& clipTransformPath, const AnimationClip& clip, NodeToInfoMap& nodeMap, AnimatedNodes& animatedNodes)
	{
		NodePathMap animatedNodePaths;

		for (NodeToInfoMap::iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			const std::string& path = it->second.path;
			if (clipTransformPath.empty())
				animatedNodePaths.insert(std::make_pair(path, &it->second));
			else if (BeginsWith(path, clipTransformPath))
			{
				std::string subpath = (path.size() == clipTransformPath.size() ? "" : path.substr(clipTransformPath.size() + 1));
				animatedNodePaths.insert(std::make_pair(subpath, &it->second));
			}
		}

		Assert(animatedNodes.size() == nodeMap.size());
		GatherAnimationCurves(animatedNodePaths, animatedNodes, clip.GetPositionCurves(), SetPositionCurve);
		GatherAnimationCurves(animatedNodePaths, animatedNodes, clip.GetRotationCurves(), SetRotationCurve);
		GatherAnimationCurves(animatedNodePaths, animatedNodes, clip.GetScaleCurves(), SetScaleCurve);
	}

	void FindInderectlyAnimatedNodes(const NodeParents& nodeParents, AnimatedNodes& animatedNodes)
	{
		Assert(nodeParents.size() == animatedNodes.size());
		for (size_t i = 0; i < animatedNodes.size(); ++i)
		{
			int parent = nodeParents[i];
			if (parent >= 0 && animatedNodes[parent])
				animatedNodes[i] = true;
		}
	}

	// Animation
	template <class T>
	T SampleCurve(const AnimationCurveTpl<T>* const curve, float time)
	{
		Assert(curve);
		Assert(curve->IsValid());
		return curve->EvaluateClamp(time);
	}

	void SampleRestPose(const NodeToInfoMap& nodeMap, std::vector<Matrix4x4f>& matrices)
	{
		// setting initial poses
		for (NodeToInfoMap::const_iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			const GameObject& go = *it->first;
			const Transform& transform = go.GetComponent(Transform);
			const int index = it->second.index;
			matrices[index].SetTRS(transform.GetLocalPosition(), transform.GetLocalRotation(), transform.GetLocalScale());
		}
	}

	bool SampleFrame(float time, const NodeToInfoMap& nodeMap, const std::vector<Matrix4x4f>& restPoseMatrices, std::vector<Matrix4x4f>& matrices)
	{
		bool hasCurves = false;

		// sampling curves
		for (NodeToInfoMap::const_iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			const GameObject& go = *it->first;
			Assert(&go);
			const Transform& transform = go.GetComponent(Transform);

			const NodeInfo& nodeInfo = it->second;
			const int index = it->second.index;

			if (!nodeInfo.HasCurves())
				matrices[index] = restPoseMatrices[index];
			else
			{
				Quaternionf rot;
				if (nodeInfo.rotation)
				{
					rot = SampleCurve(nodeInfo.rotation, time);
					const float kEpsilon = 0.03f;
#if UNITY_EDITOR
					if (!CompareApproximately(SqrMagnitude(rot), 1, kEpsilon))
					{
						WarningStringObject(Format("Quaternion is invalid (%f, %f, %f, %f) length=%f\n%s", rot.x, rot.y, rot.z, rot.w, SqrMagnitude(rot), GetAssetPathFromInstanceID(go.GetInstanceID()).c_str()), &go);
					}
#endif
					// We need to normalize here, because MatrixToQuaternion conversion has the same assert as above, just with much lower epsilon
					rot = Normalize(rot);
				}
				else
					rot = transform.GetLocalRotation();

				// Position curves have globalScale applied, so we transform it back to "import space"
				Vector3f pos = !nodeInfo.position ? transform.GetLocalPosition() : SampleCurve(nodeInfo.position, time);
				Vector3f scale = !nodeInfo.scale ? transform.GetLocalScale() : SampleCurve(nodeInfo.scale, time);
				
				matrices[index].SetTRS(pos, rot, scale);						

				hasCurves = true;
			}
		}

		return hasCurves;
	}

	// LocalSpace->WorldSpace
	void CalculateWorldSpaceMatrices(const NodeParents& parents, std::vector<Matrix4x4f>& matrices)
	{
		Matrix4x4f temp;

		for (int i = 0; i < parents.size(); ++i)
		{
			int p = parents[i];
			Assert(p < i);
			if (p >= 0)
			{
				MultiplyMatrices4x4(&matrices[p], &matrices[i], &temp);
				matrices[i] = temp;
			}
		}
	}

	// Calculate Bounds for meshes
	MinMaxAABB CalculateRigidMeshBounds(const Mesh& mesh, const Matrix4x4f& meshMatrix)
	{
		MinMaxAABB bounds;
		for (StrideIterator<Vector3f> vertices = mesh.GetVertexBegin(), end = mesh.GetVertexEnd (); vertices != end; ++vertices)
			bounds.Encapsulate(meshMatrix.MultiplyPoint3(*vertices));

		return bounds;
	}

	//bool IsMeshAnimated(const Mesh& mesh, const NodeToInfoMap& nodeMap, const AnimatedNodes& animatedNodes)
	bool IsSkinnedMeshAnimated(SkinnedMeshRenderer& renderer, const NodeToInfoMap& nodeMap, const AnimatedNodes& animatedNodes)
	{
		dynamic_array<PPtr<Transform> > bones = renderer.GetBones();

		for (size_t i = 0; i < bones.size(); ++i)
		{
			const GameObject& go = bones[i]->GetGameObject();
			NodeToInfoMap::const_iterator it = nodeMap.find(&go);
			Assert(it != nodeMap.end());

			const int index = it->second.index;

			if (animatedNodes[index])
				return true;
		}

		return false;
	}

	// TODO : this shouldn't be called twice with worldToLocalMatrix and without it (if posible)
	MinMaxAABB CalculateSkinnedMeshBounds(SkinnedMeshRenderer& renderer, const std::vector<int>& boneIndexToMatrixIndex, const std::vector<Matrix4x4f>& matrices, const Matrix4x4f* worldToLocalMatrix)
	{
		dynamic_array<PPtr<Transform> > bones = renderer.GetBones();

		Assert(bones.size() == boneIndexToMatrixIndex.size());
		std::vector<Matrix4x4f> boneMatrices(bones.size());

		if (worldToLocalMatrix)
		{
			for (size_t i = 0; i < boneMatrices.size(); ++i)
			{
				const int index = boneIndexToMatrixIndex[i];
				const Matrix4x4f& m = index >= 0 ? matrices[index] : Matrix4x4f::identity;
				MultiplyMatrices4x4(worldToLocalMatrix, &m, &boneMatrices[i]);
			}
		}
		else
		{
			for (size_t i = 0; i < boneMatrices.size(); ++i)
			{
				const int index = boneIndexToMatrixIndex[i];
				boneMatrices[i] = index >= 0 ? matrices[index] : Matrix4x4f::identity;
			}
		}

		MinMaxAABB bounds;
		renderer.CalculateVertexBasedBounds (&boneMatrices[0], bounds);

		return bounds;
	}

	typedef std::vector<NodeInfo*> AnimatedMeshes;

	struct ClipBoundsInfo
	{
		int nodeIndex;
		MinMaxAABB worldSpaceBounds;
		MinMaxAABB localSpaceBounds;
		AnimatedMeshes animatedMeshes;
	};

	typedef std::vector<ClipBoundsInfo> ClipBoundsInfos;

	bool HasVertices(const NodeInfo& meshNodeInfo)
	{
		Assert(meshNodeInfo.rigidMesh || meshNodeInfo.skinnedMesh);
		const Mesh* mesh = meshNodeInfo.rigidMesh ? meshNodeInfo.rigidMesh : meshNodeInfo.skinnedMesh->GetMesh();
		return mesh->GetVertexCount() > 0;
	}

	void CalculateMeshBoundsForClip(const NodeInfo& meshNodeInfo, const int meshNodeIndex, const NodeToInfoMap& nodeMap, const std::vector<Matrix4x4f>& matrices, ClipBoundsInfo& clipBounds)
	{
		Assert(meshNodeInfo.HasMesh());

		if (HasVertices(meshNodeInfo))
		{
			MinMaxAABB bounds;
			if (meshNodeInfo.rigidMesh)
			{
				bounds = CalculateRigidMeshBounds(*meshNodeInfo.rigidMesh, matrices[meshNodeIndex]);
			}
			else
			{
				Assert(meshNodeInfo.skinnedMesh);
				bounds = CalculateSkinnedMeshBounds(*meshNodeInfo.skinnedMesh, meshNodeInfo.boneIndexToMatrixIndex, matrices, NULL);					
			}

			Assert(bounds.IsValid());
			clipBounds.worldSpaceBounds.Encapsulate(bounds);
		}
	}

	void CalculateBoundsForSkinnedMeshes(NodeToInfoMap& nodeMap, const AnimatedMeshes& animatedSkinnedMeshes, std::vector<Matrix4x4f>& matrices)
	{
		for (int i = 0; i < animatedSkinnedMeshes.size(); ++i)
		{
			NodeInfo& node = *animatedSkinnedMeshes[i];
			Assert(node.skinnedMesh);

			Matrix4x4f nodeMatrix;
 			if (Matrix4x4f::Invert_General3D(matrices[node.rootBoneMatrixIndex], nodeMatrix))
			{
				MinMaxAABB bounds = CalculateSkinnedMeshBounds(*node.skinnedMesh, node.boneIndexToMatrixIndex, matrices, &nodeMatrix);
				node.skinnedMeshBounds.Encapsulate(bounds);
			}
			else
			{
				// The chosen rootbone has scale that is close to zero
				// There is no accurate way to calculate the localSpace bounding volume when the rootbone scale approached 0.
				// Actually using any scale on the root bone seems like a bad idea, but i guess that if the rootbone is a real hierarchy & doesnt have degenerate scale it is ok.
				node.skinnedMesh->SetUpdateWhenOffscreen(true);
			}
		}
	}

	// Calculate Bounds for frame at time time
	void CalculateFrameBounds(float time, NodeToInfoMap& nodeMap, const NodeParents& parents, const AnimatedMeshes& animatedSkinnedMeshes, const std::vector<Matrix4x4f>& restPoseMatrices, std::vector<Matrix4x4f>& matrices, ClipBoundsInfos& clipBounds)
	{
		SampleFrame(time, nodeMap, restPoseMatrices, matrices);
		CalculateWorldSpaceMatrices(parents, matrices);

		for (ClipBoundsInfos::iterator it = clipBounds.begin(), end = clipBounds.end(); it != end; ++it)
		{
			ClipBoundsInfo& cbi = *it;
			for (int i = 0; i < cbi.animatedMeshes.size(); ++i)
			{
				NodeInfo& node = *cbi.animatedMeshes[i];
				Assert(node.HasMesh());
				CalculateMeshBoundsForClip(node, node.index, nodeMap, matrices, *it);
			}
		}
		
		CalculateBoundsForSkinnedMeshes(nodeMap, animatedSkinnedMeshes, matrices);
	}

	// Transforms clip bounds from world space to local space
	bool TransformClipBounds(float time, NodeToInfoMap& nodeMap, const NodeParents& parents, const std::vector<Matrix4x4f>& restPoseMatrices, std::vector<Matrix4x4f>& matrices, ClipBoundsInfos& clipBounds)
	{
		bool hasCurves = SampleFrame(time, nodeMap, restPoseMatrices, matrices);
		CalculateWorldSpaceMatrices(parents, matrices);
		
		for (ClipBoundsInfos::iterator it = clipBounds.begin(), end = clipBounds.end(); it != end; ++it)
		{
			ClipBoundsInfo& cbi = *it;
			if (cbi.worldSpaceBounds.IsValid())
			{
				Matrix4x4f worldToLocal = matrices[cbi.nodeIndex];				
				worldToLocal.Invert_Full();

				AABB b;
				TransformAABBSlow (cbi.worldSpaceBounds, worldToLocal, b);
				cbi.localSpaceBounds.Encapsulate(b);
			}
		}

		return hasCurves;
	}

	void CalculateBoundsAtRestPoseForSkinnedMeshes(NodeToInfoMap& nodeMap, const NodeParents& parents, std::vector<Matrix4x4f>& restPoseMatrices, std::vector<Matrix4x4f>& matrices)
	{
		matrices = restPoseMatrices;
		CalculateWorldSpaceMatrices(parents, matrices);

		AnimatedMeshes skinnedMeshes;
		for (NodeToInfoMap::iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{			
			if (it->second.skinnedMesh)
				skinnedMeshes.push_back(&it->second);
		}

		CalculateBoundsForSkinnedMeshes(nodeMap, skinnedMeshes, matrices);
	}

	void ApplySkinnedMeshBounds(const NodeToInfoMap& nodeMap)
	{
		for (NodeToInfoMap::const_iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			const NodeInfo& nodeInfo = it->second;

			SkinnedMeshRenderer* skinnedMesh = NULL;
			MinMaxAABB bounds;

			if (nodeInfo.skinnedMesh)
			{
				skinnedMesh = nodeInfo.skinnedMesh;

				Assert(nodeInfo.skinnedMeshBounds.IsValid());
				bounds = nodeInfo.skinnedMeshBounds;
			}
			else if (nodeInfo.blendShape) 
			{
				skinnedMesh = nodeInfo.blendShape;
				Assert(nodeInfo.rigidMesh);
				bounds = nodeInfo.rigidMesh->GetBounds();
			}

			if (skinnedMesh)
			{				
				Assert(bounds.IsValid());

				#define USE_BUGGY_SKINNEDMESH_AABB_EXTRACTION 1
				
				#if USE_BUGGY_SKINNEDMESH_AABB_EXTRACTION
				AABB b;
				skinnedMesh->GetLocalAABB(b);
				bounds.Encapsulate(b);
				skinnedMesh->SetLocalAABB(bounds);
				#else

				MinMaxAABB bounds = nodeInfo.skinnedMeshBounds;
				nodeInfo.skinnedMesh->SetLocalAABB(bounds);
				
				#endif
			}
		}
	}

	typedef std::pair<float, float> ClipRange;

	struct ClipInfo
	{
		Animation* animationComponent;
		AnimationClip* clip;
		ClipRange clipRange;
		const GameObject* clipGameObject;
	};

	bool IsValidRange(const ClipRange& range)
	{
		return range.first != std::numeric_limits<float>::infinity();
	}

	std::pair<float, float> CalculateRange(const std::vector<ClipInfo>& clips)
	{
		ClipRange range;
		bool first = true;

		for (std::vector<ClipInfo>::const_iterator it = clips.begin(), end = clips.end(); it != end; ++it)
		{
			const ClipRange& cr = it->clipRange;
			if (IsValidRange(cr))
			{
				if (first)
				{
					range = cr;
					first = false;
				}
				else
				{
					range.first = std::min(range.first, cr.first);
					range.second = std::max(range.second, cr.second);
				}
			}
		}

		return !first ? range : std::make_pair(std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());
	}

	bool IsIdenticalSampleRate(const std::vector<ClipInfo>& clips, const float sampleRate)
	{
		for (std::vector<ClipInfo>::const_iterator it = clips.begin(), end = clips.end(); it != end; ++it)
			if (it->clip->GetSampleRate() != sampleRate)
				return false;

		return true;
	}

	float GetSampleRate(const std::vector<ClipInfo>& clips)
	{
		Assert(!clips.empty());
		const float sampleRate = clips[0].clip->GetSampleRate();
		Assert(IsIdenticalSampleRate(clips, sampleRate));

		return sampleRate;
	}

	struct ClipGroup
	{
		// We use these just for checking that we don't have any duplicates 
		// You could get duplicates if we would allow two animations with the same name
		std::set<AnimationClip*> animationsClips;
		std::set<Animation*> animationComponents;

		// acltualClips
		std::vector<ClipInfo> clips;
	};

	typedef std::map<std::string, ClipGroup> NamedClipMap;

	void ApplyAnimationBounds(const std::vector<ClipInfo>& clips, ClipBoundsInfos& clipBounds, bool newClips)
	{
		Assert(clips.size() == clipBounds.size());

		for (size_t i = 0, size = clips.size(); i < size; ++i)
		{
			const ClipInfo& ci = clips[i];
			const ClipBoundsInfo& cbi = clipBounds[i];

			MinMaxAABB b = cbi.localSpaceBounds;
			if (!b.IsValid())
				b.Encapsulate(Vector3f::zero);

			// It should calculate bounds only for new clips
			//Assert(!ci.clip->GetBounds().IsValid());

			if (newClips)
			{
				// We set bounds for new clips
				ci.clip->SetBounds(b);
			}
			// We would like to expand bound of clips based on all meshes that it is attached to,
			// but that leads to various complications in import-order-dependencies. At the moment
			// there is one dependency already: because of m_HasExtraRoot we have to make sure that 
			// we import models first and then animations. From bounds perspective we would like to
			// have the opposite order, but even with that we would have problems if you would 
			// reimport just one of assets - Unity doesn't reimport dependencies.
			/*else
			{
				// We expand bounds for shared clips
				b.Encapsulate(ci.clip->GetBounds());
				ci.clip->SetBounds(b);
			}*/
		}
	}

	void GroupClips(const NodeToInfoMap& nodeMap, const std::set<std::string>* animationClipNames, NamedClipMap& groupedClips)
	{
		for (NodeToInfoMap::const_iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			Animation* animationComp = it->first->QueryComponent(Animation);
			if (!animationComp)
				continue;

			for (Animation::Animations::const_iterator itA = animationComp->GetClips().begin(), endA = animationComp->GetClips().end(); itA != endA; ++itA)
			{
				Assert(itA->IsValid());
				AnimationClip* clip = *itA;
				Assert(clip);

				const std::string& clipName = clip->GetName();
				
				ClipGroup& clipGroup = groupedClips[clipName];
				// Make sure we don't have duplicates
				{
					bool res = clipGroup.animationsClips.insert(clip).second;
					Assert(res);
				}
				{
					bool res = clipGroup.animationComponents.insert(animationComp).second;
					Assert(res);
				}

				ClipInfo ci;
				ci.animationComponent = animationComp;
				ci.clip = clip;
				ci.clipRange = clip->GetRange();
				ci.clipGameObject = &animationComp->GetGameObject();

				clipGroup.clips.push_back(ci);
			}
		}

		// we at first fill the array completely (and perform validation of duplication)
		// then we filter if necessary
		if (animationClipNames)
			for (NamedClipMap::iterator it = groupedClips.begin(); it != groupedClips.end(); )
			{
				if (animationClipNames->find(it->first) == animationClipNames->end())
					groupedClips.erase(it++);
				else
					++it;
			}
	}

	std::vector<NodeInfo*> GatherAnimatedMeshes(NodeToInfoMap& nodeMap, const AnimatedNodes& animatedNodes, bool skinnedMeshesOnly)
	{
		// Gather meshes animated by this clip
		std::vector<NodeInfo*> result;

		for (NodeToInfoMap::iterator it = nodeMap.begin(); it != nodeMap.end(); ++it)
		{
			NodeInfo& node = it->second;
			if (node.HasMesh())	
			{
				bool animated = false;
				if (node.rigidMesh)
				{
					if (!skinnedMeshesOnly)
						animated = animatedNodes[node.index];
				}
				else
				{
					Assert(node.skinnedMesh);
					animated = IsSkinnedMeshAnimated(*node.skinnedMesh, nodeMap, animatedNodes);
				}

				if (animated)
					result.push_back(&node);
			}

		}

		return result;
	}

	void CalculateClipBoundsInternal(GameObject& gameObject, const std::set<std::string>* animationClipNames)
	{
		#ifdef PRINT_TIME
		float time = GetTimeSinceStartup();
		#endif

		NodeToInfoMap nodeMap;
		NodeParents nodeParents;
		BuildNodeArray(-1, std::string(), gameObject, nodeMap, nodeParents);

		// create mapping from bone indices into nodeMap index for SkinnedMeshes
		BuildBoneMaps(nodeMap);

		std::vector<Matrix4x4f> restPoseMatrices(nodeMap.size()), matrices(nodeMap.size());

		SampleRestPose(nodeMap, restPoseMatrices);

		NamedClipMap namedClips;
		GroupClips(nodeMap, animationClipNames, namedClips);

		for (NamedClipMap::iterator it = namedClips.begin(), end = namedClips.end(); it != end; ++it)
		{
			const std::vector<ClipInfo>& clips = it->second.clips;

			const ClipRange allRange = CalculateRange(clips);

			if (IsValidRange(allRange))
			{
				ClipBoundsInfos clipBounds(clips.size());
				AnimatedNodes animatedNodes(nodeMap.size(), 0);

				// Gathering nodes which are animated from each clip
				for (size_t i = 0, size = clips.size(); i < size; ++i)
				{
					const ClipInfo& ci = clips[i];
					ClipBoundsInfo& cbi = clipBounds[i];

					NodeToInfoMap::iterator itN = nodeMap.find(ci.clipGameObject);
					AssertMsg(itN != nodeMap.end(), "Couldn't find path for gameObject %s", ci.clipGameObject->GetName());

					cbi.nodeIndex = itN->second.index;					
					const std::string& clipTransformPath = itN->second.path;

					AnimatedNodes clipAnimatedNodes(nodeMap.size(), 0);

					GatherAnimationCurves(clipTransformPath, *ci.clip, nodeMap, clipAnimatedNodes);
					// Gathering nodes which have animated parents
					FindInderectlyAnimatedNodes(nodeParents, clipAnimatedNodes);

					cbi.animatedMeshes = GatherAnimatedMeshes(nodeMap, clipAnimatedNodes, false);

					// merge animatedNodes
					for (size_t j = 0, sizej = animatedNodes.size(); j < sizej; ++j)
						animatedNodes[j] = animatedNodes[j] || clipAnimatedNodes[j];
				}

				// Gathering nodes which have animated parents
				FindInderectlyAnimatedNodes(nodeParents, animatedNodes);

				// Gather skinned meshed animated by this set of clips clip
				AnimatedMeshes animatedSkinnedMeshes = GatherAnimatedMeshes(nodeMap, animatedNodes, true);

				// Calculate mesh bounds and animation bounds in world space
				const float deltaTime = 1 / GetSampleRate(clips);
				for (float t = allRange.first; t <= allRange.second; t += deltaTime)
					CalculateFrameBounds(t, nodeMap, nodeParents, animatedSkinnedMeshes, restPoseMatrices, matrices, clipBounds);

				// Transform animation bounds from world to local space
				for (float t = allRange.first; t <= allRange.second; t += deltaTime)
					if (!TransformClipBounds(t, nodeMap, nodeParents, restPoseMatrices, matrices, clipBounds))
					{
						// none of the nodes for clipBounds is animated - we skip consequent cycles, 
						// because matrices will be identical for all frames
						break;
					}
				
				ApplyAnimationBounds(clips, clipBounds, animationClipNames == NULL);
			}
		}

		CalculateBoundsAtRestPoseForSkinnedMeshes(nodeMap, nodeParents, restPoseMatrices, matrices);

		ApplySkinnedMeshBounds(nodeMap);

		#ifdef PRINT_TIME
		time = GetTimeSinceStartup() - time;

		{
			std::ostringstream oss;
			oss << "Bounds calculation time: " << time << "s;\n";
			//printf_console("%s", oss.str().c_str());
			LogString(oss.str());
		}
		#endif
	}
}

// Rename these methods into something more appropriate
void CalculateClipsBoundsDeprecated(GameObject& gameObject)
{
	CalculateClipBoundsInternal(gameObject, NULL);
}

void CalculateClipsBoundsDeprecated(GameObject& gameObject, const std::set<std::string>& animationClipNames)
{
	CalculateClipBoundsInternal(gameObject, &animationClipNames);
}
