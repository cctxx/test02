#include "UnityPrefix.h"
#include "Runtime/Filters/Renderer.h"
#include "Runtime/Filters/Mesh/MeshPartitioner.h"
#include "PrepareMeshDataForBuildTarget.h"


PrepareMeshDataForBuildTarget::PrepareMeshDataForBuildTarget (Mesh& inMesh, BuildTargetPlatform targetPlatform, const UInt32 keepVertexComponents, const UInt32 meshUsageFlags)
: m_Mesh(inMesh)
, m_BuildTargetPlatform(targetPlatform)
, m_OriginalVertexData(NULL)
{
	// We restore flags afterwards as they are only used in player
	m_BackupKeepVertices = inMesh.GetKeepVertices();
	m_BackupKeepIndices = inMesh.GetKeepIndices();

	if (inMesh.IsSuitableSizeForDynamicBatching() || (meshUsageFlags & kMeshMustKeepVertexAndIndexData) != 0)
	{
		inMesh.SetKeepVertices(true);
		inMesh.SetKeepIndices(true);
	}

	bool needUncompressed = inMesh.GetKeepVertices() || inMesh.GetIsReadable() || !inMesh.GetSkin().empty() || inMesh.GetMeshCompression();
	bool compressStreams = !needUncompressed && SupportsCompressedVertexStreams(targetPlatform);
	UInt32 availableChannels = inMesh.GetAvailableChannels();
	UInt32 neededChannels = availableChannels;
	if (keepVertexComponents)
	{
		neededChannels &= (keepVertexComponents 
			| (1<<kShaderChannelVertex)); // always keep position for safety
	}

	// atm SPU code doesn't handle compression, so skinned meshes have to be uncompressed for now.
	if(kBuildPS3 == m_BuildTargetPlatform && !inMesh.GetSkin().empty())
	{
		if (inMesh.GetMeshOptimized() && !inMesh.GetIsReadable() && m_OriginalVertexData == NULL)
		{
		#if DEBUG_PARTITIONING
			const VertexStreamsLayout& streamConfig = VertexDataInfo::kVertexStreamsDefault;
		#else
			const VertexStreamsLayout& streamConfig = VertexDataInfo::kVertexStreamsSkinnedHotColdSplitPS3;
		#endif
		m_OriginalVertexData = new VertexData (inMesh.GetVertexData(), neededChannels, streamConfig);
		swap (*m_OriginalVertexData, inMesh.GetVertexData());
		}

		m_BackupMeshCompression	= inMesh.GetMeshCompression();
		inMesh.SetMeshCompression(kMeshCompressionOff);

		if (inMesh.GetSkin().size())
		{
			m_BackupSkin.resize_initialized(inMesh.GetSkin().size());
			m_BackupSkin.assign(inMesh.GetSkin().begin(), inMesh.GetSkin().end());
		}

		if (!inMesh.GetIndexBuffer().empty())
		{
			m_BackupIndexBuffer.resize(inMesh.GetIndexBuffer().size());
			m_BackupIndexBuffer.assign(inMesh.GetIndexBuffer().begin(), inMesh.GetIndexBuffer().end());
		}

		if (!inMesh.GetSubMeshes().empty())
		{
			m_BackupSubMeshes.resize(inMesh.GetSubMeshes().size());
			m_BackupSubMeshes.assign(inMesh.GetSubMeshes().begin(), inMesh.GetSubMeshes().end());
		}

		if (!inMesh.m_Partitions.empty())
		{
			m_BackupPartitions.resize(inMesh.m_Partitions.size());
			m_BackupPartitions.assign(inMesh.m_Partitions.begin(), inMesh.m_Partitions.end());
		}

		if (!inMesh.m_PartitionInfos.empty())
		{
			m_BackupPartitionInfos.resize(inMesh.m_PartitionInfos.size());
			m_BackupPartitionInfos.assign(inMesh.m_PartitionInfos.begin(), inMesh.m_PartitionInfos.end());
		}

		if(inMesh.GetMeshOptimized() && !inMesh.GetIsReadable())
			PartitionMesh(&inMesh);
	}
	else if (compressStreams || neededChannels != availableChannels)
	{
		m_OriginalVertexData = new VertexData (inMesh.GetVertexData(), neededChannels);
		swap (*m_OriginalVertexData, inMesh.GetVertexData());
		UInt8 compression = compressStreams ? Mesh::kStreamCompressionCompressed : Mesh::kStreamCompressionDefault;
		if (compressStreams)
		{
			if (SupportsAggressiveCompressedVertexStreams(targetPlatform))
			{
				compression = Mesh::kStreamCompressionCompressedAggressive;
			}
		}
		inMesh.SetStreamCompression (compression);
		inMesh.FormatVertices (neededChannels);
	}


}

PrepareMeshDataForBuildTarget::~PrepareMeshDataForBuildTarget ()
{
#if DEBUG_PARTITIONING
	return;
#endif
	if(kBuildPS3 == m_BuildTargetPlatform && !m_Mesh.GetSkin().empty())
	{
		if (!m_BackupSkin.empty())
		{
			m_Mesh.GetSkin().assign(m_BackupSkin.begin(), m_BackupSkin.end());
		}
		if (!m_BackupIndexBuffer.empty())
		{
			m_Mesh.GetIndexBuffer().resize(m_BackupIndexBuffer.size());
			m_Mesh.GetIndexBuffer().assign(m_BackupIndexBuffer.begin(), m_BackupIndexBuffer.end());
		}
		if (!m_BackupSubMeshes.empty())
		{
			m_Mesh.GetSubMeshes().resize(m_BackupSubMeshes.size());
			m_Mesh.GetSubMeshes().assign(m_BackupSubMeshes.begin(), m_BackupSubMeshes.end());
		}
		// we need to make sure the partitions are reset, otherwise bad things will happen next time we build.
		m_Mesh.m_Partitions.resize(m_BackupPartitions.size());
		m_Mesh.m_Partitions.assign(m_BackupPartitions.begin(), m_BackupPartitions.end());
		m_Mesh.m_PartitionInfos.resize(m_BackupPartitionInfos.size());
		m_Mesh.m_PartitionInfos.assign(m_BackupPartitionInfos.begin(), m_BackupPartitionInfos.end());
		m_Mesh.SetMeshCompression(m_BackupMeshCompression);
	}
	if (m_OriginalVertexData)
	{
		swap (*m_OriginalVertexData, m_Mesh.GetVertexData());
		delete m_OriginalVertexData;
		m_Mesh.SetChannelsDirty(m_Mesh.GetAvailableChannels(), true);
		m_Mesh.SetStreamCompression(Mesh::kStreamCompressionDefault);
	}

	// Restore player-only flags
	m_Mesh.SetKeepVertices(m_BackupKeepVertices);
	m_Mesh.SetKeepIndices(m_BackupKeepIndices);
}

bool PrepareMeshDataForBuildTarget::SupportsCompressedVertexStreams (BuildTargetPlatform targetPlatform)
{
	switch (targetPlatform)
	{
		case kBuildStandaloneOSXIntel:
		case kBuildStandaloneOSXIntel64:
		case kBuildStandaloneOSXUniversal:
		case kBuildStandaloneWinPlayer:
		case kBuildWebPlayerLZMA:
		case kBuildWebPlayerLZMAStreamed:
		case kBuildXBOX360:
		case kBuild_Android:
		case kBuildPS3:
		case kBuildStandaloneLinux:
        case kBuildStandaloneLinux64:
		case kBuildStandaloneLinuxUniversal:
		case kBuildStandaloneWin64Player:
			return true;
	}
	return false;
}
bool PrepareMeshDataForBuildTarget::SupportsAggressiveCompressedVertexStreams (BuildTargetPlatform targetPlatform)
{
	switch (targetPlatform)
	{
		case kBuild_Android:
			return false; // disabled until we get user settings for this
	}
	return false;
}
