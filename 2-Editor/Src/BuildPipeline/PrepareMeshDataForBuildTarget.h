#pragma once

#include "Runtime/Filters/Mesh/LodMesh.h"

// Temporarily (When building the player during the write operation).
// Clears various mesh attributes, and restores them afterwards.

struct PrepareMeshDataForBuildTarget
{
	Mesh&					m_Mesh;
	VertexData*			    m_OriginalVertexData;
	VertexData*			    m_BackupVertexData;
	BuildTargetPlatform		m_BuildTargetPlatform;

	dynamic_array<BoneInfluence> m_BackupSkin;
	std::vector<UInt8>			m_BackupIndexBuffer;
	std::vector<SubMesh>		m_BackupSubMeshes;
	UInt8						m_BackupMeshCompression;
	std::vector<MeshPartition>			m_BackupPartitions;
	std::vector<MeshPartitionInfo>		m_BackupPartitionInfos;

	bool						m_BackupKeepIndices;
	bool						m_BackupKeepVertices;

	PrepareMeshDataForBuildTarget (Mesh& inMesh, BuildTargetPlatform targetPlatform, UInt32 keepVertexComponents, UInt32 meshUsageFlags);
	~PrepareMeshDataForBuildTarget ();

	static bool SupportsCompressedVertexStreams (BuildTargetPlatform targetPlatform);
	static bool SupportsAggressiveCompressedVertexStreams (BuildTargetPlatform targetPlatform);
};
