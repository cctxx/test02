#include "UnityPrefix.h"
#include "ProfilerStats.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include "MemoryProfiler.h"
#include "SerializationUtility.h"

#if ENABLE_PROFILER

using namespace std;
struct ProfilerSample;


void InitializeStatisticsProperties (dynamic_array<StatisticsProperty>& statisticProperties)
{
	AllProfilerStats* proxy = NULL;
	

	#define ADD_STAT(_area,_graph,_name,_val,_format) { \
	StatisticsProperty& prop = statisticProperties.push_back(); \
	new (&prop) StatisticsProperty(); \
	prop.name = _name; \
	prop.offset = reinterpret_cast<UInt8*>(&proxy->_val) - reinterpret_cast<UInt8*>(proxy); \
	prop.format = _format; \
	prop.showGraph = _graph; \
	prop.area = _area; \
	}
	
	// Any int stats value can be added by specifying the name, variable and display format into the macro
#define ADD_STAT_CHART(_area,_name,_val) \
	ADD_STAT(_area, true, _name, chartSample._val, kFormatTime); \
	ADD_STAT(_area, false, "Selected" _name, chartSampleSelected._val, kFormatTime)
	
	// CPU overview
	ADD_STAT_CHART(kProfilerAreaCPU, "Rendering", rendering);
	ADD_STAT_CHART(kProfilerAreaCPU, "Scripts", scripts);
	ADD_STAT_CHART(kProfilerAreaCPU, "Physics", physics);
	ADD_STAT_CHART(kProfilerAreaCPU, "GarbageCollector", gc);
	ADD_STAT_CHART(kProfilerAreaCPU, "VSync", vsync);
	ADD_STAT_CHART(kProfilerAreaCPU, "Others", others);
	
	// GPU overview
	
	ADD_STAT_CHART(kProfilerAreaGPU, "Opaque", gpuOpaque);
	ADD_STAT_CHART(kProfilerAreaGPU, "Transparent", gpuTransparent);
	ADD_STAT_CHART(kProfilerAreaGPU, "Shadows/Depth", gpuShadows);
	ADD_STAT_CHART(kProfilerAreaGPU, "Deferred PrePass", gpuDeferredPrePass);
	ADD_STAT_CHART(kProfilerAreaGPU, "Deferred Lighting", gpuDeferredLighting);
	ADD_STAT_CHART(kProfilerAreaGPU, "PostProcess", gpuPostProcess);
	ADD_STAT_CHART(kProfilerAreaGPU, "Other", gpuOther);


	// Graphics
	ADD_STAT(kProfilerAreaRendering, true, "Draw Calls", drawStats.drawCalls, kFormatCount);
	ADD_STAT(kProfilerAreaRendering, true, "Triangles", drawStats.triangles, kFormatCount);
	ADD_STAT(kProfilerAreaRendering, true, "Vertices", drawStats.vertices, kFormatCount);
	
	// Memory
	ADD_STAT(kProfilerAreaMemory, true, "Total Allocated", memoryStats.bytesUsedTotal, kFormatBytes);
	ADD_STAT(kProfilerAreaMemory, true, "Texture Memory", drawStats.usedTextureBytes, kFormatBytes);
	ADD_STAT(kProfilerAreaMemory, false, "Texture Count", memoryStats.textureCount, kFormatCount);
	ADD_STAT(kProfilerAreaMemory, true, "Mesh Count", memoryStats.meshCount, kFormatCount);
	ADD_STAT(kProfilerAreaMemory, true, "Material Count", memoryStats.materialCount, kFormatCount);
	ADD_STAT(kProfilerAreaMemory, true, "Object Count", memoryStats.totalObjectsCount, kFormatCount);
	
	// Audio
	ADD_STAT(kProfilerAreaAudio, true, "Playing Sources", audioStats.playingSources, kFormatCount);
	ADD_STAT(kProfilerAreaAudio, true, "Paused Sources", audioStats.pausedSources, kFormatCount);
	ADD_STAT(kProfilerAreaAudio, true, "Audio Voices", audioStats.audioVoices, kFormatCount);
	ADD_STAT(kProfilerAreaAudio, false, "Audio CPU Usage", audioStats.audioCPUusage, kFormatPercentage);
	ADD_STAT(kProfilerAreaAudio, true, "Audio Memory", audioStats.audioMemUsage, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Max Audio Memory Usage", audioStats.audioMaxMemUsage, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, true, "Audio Clip Count", audioStats.audioClipCount, kFormatCount);
	ADD_STAT(kProfilerAreaAudio, true, "Audio Source Count", audioStats.audioSourceCount, kFormatCount);
	ADD_STAT(kProfilerAreaAudio, false, "Detailed Audio Memory Usage", audioStats.audioMemDetailsUsage, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Memory not accounted for by other types", audioStats.audioMemDetails.other, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "String data", audioStats.audioMemDetails.string, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "System object and various internals", audioStats.audioMemDetails.system, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Plugin objects and internals", audioStats.audioMemDetails.plugins, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Output module object and internals", audioStats.audioMemDetails.output, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Channel related memory", audioStats.audioMemDetails.channel, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "ChannelGroup objects and internals", audioStats.audioMemDetails.channelgroup, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Codecs allocated for streaming", audioStats.audioMemDetails.codec, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "File buffers and structures", audioStats.audioMemDetails.file, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sound objects and internals", audioStats.audioMemDetails.sound, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sound data stored in secondary RAM", audioStats.audioMemDetails.secondaryram, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "SoundGroup objects and internals", audioStats.audioMemDetails.soundgroup, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Stream buffer memory", audioStats.audioMemDetails.streambuffer, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "DSPConnection objects and internals", audioStats.audioMemDetails.dspconnection, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "DSP implementation objects", audioStats.audioMemDetails.dsp, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Realtime file format decoding DSP objects", audioStats.audioMemDetails.dspcodec, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Profiler memory footprint", audioStats.audioMemDetails.profile, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Buffer used to store recorded data from microphone", audioStats.audioMemDetails.recordbuffer, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Reverb implementation objects", audioStats.audioMemDetails.reverb, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Reverb channel properties structs", audioStats.audioMemDetails.reverbchannelprops, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Geometry objects and internals", audioStats.audioMemDetails.geometry, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sync point memory", audioStats.audioMemDetails.syncpoint, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "EventSystem and various internals", audioStats.audioMemDetails.eventsystem, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "MusicSystem and various internals", audioStats.audioMemDetails.musicsystem, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Definition of objects contained in all loaded projects e.g. events, groups, categories", audioStats.audioMemDetails.fev, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Data loaded with preloadFSB", audioStats.audioMemDetails.memoryfsb, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "EventProject objects and internals", audioStats.audioMemDetails.eventproject, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "EventGroup objects and internals", audioStats.audioMemDetails.eventgroupi, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Objects used to manage wave banks", audioStats.audioMemDetails.soundbankclass, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Data used to manage lists of wave bank usage", audioStats.audioMemDetails.soundbanklist, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Stream objects and internals", audioStats.audioMemDetails.streaminstance, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sound definition objects", audioStats.audioMemDetails.sounddefclass, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sound definition static data objects", audioStats.audioMemDetails.sounddefdefclass, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Sound definition pool data", audioStats.audioMemDetails.sounddefpool, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Reverb definition objects", audioStats.audioMemDetails.reverbdef, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Reverb objects", audioStats.audioMemDetails.eventreverb, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "User property objects", audioStats.audioMemDetails.userproperty, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event instance base objects", audioStats.audioMemDetails.eventinstance, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Complex event instance objects", audioStats.audioMemDetails.eventinstance_complex, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Simple event instance objects", audioStats.audioMemDetails.eventinstance_simple, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event layer instance objects", audioStats.audioMemDetails.eventinstance_layer, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event sound instance objects", audioStats.audioMemDetails.eventinstance_sound, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event envelope objects", audioStats.audioMemDetails.eventenvelope, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event envelope definition objects", audioStats.audioMemDetails.eventenvelopedef, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event parameter objects", audioStats.audioMemDetails.eventparameter, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event category objects", audioStats.audioMemDetails.eventcategory, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event envelope point objects", audioStats.audioMemDetails.eventenvelopepoint, kFormatBytes);
	ADD_STAT(kProfilerAreaAudio, false, "Event instance pool memory", audioStats.audioMemDetails.eventinstancepool, kFormatBytes);


	// Physics
	////@TODO: Add some kind of warning when moving static colliders because that has a very high performance impact!
	ADD_STAT(kProfilerAreaPhysics, true, "Active Rigidbodies", physicsStats.activeRigidbodies, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics, false, "Sleeping Rigidbodies", physicsStats.sleepingRigidbodies, kFormatCount);
	
	ADD_STAT(kProfilerAreaPhysics, true, "Number of Contacts", physicsStats.numberOfShapePairs, kFormatCount);
	
	ADD_STAT(kProfilerAreaPhysics, false,  "Static Colliders", physicsStats.numberOfStaticColliders, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics, false, "Dynamic Colliders", physicsStats.numberOfDynamicColliders, kFormatCount);

	// Physics (2D).	
	ADD_STAT(kProfilerAreaPhysics2D, false, "Total Bodies", physics2DStats.m_TotalBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Active Bodies", physics2DStats.m_ActiveBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Sleeping Bodies", physics2DStats.m_SleepingBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Dynamic Bodies", physics2DStats.m_DynamicBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Kinematic Bodies", physics2DStats.m_KinematicBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Discrete Bodies", physics2DStats.m_DiscreteBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Continuous Bodies", physics2DStats.m_ContinuousBodyCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Joints", physics2DStats.m_JointCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, true, "Contacts", physics2DStats.m_ContactCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Active Collider Shapes", physics2DStats.m_ActiveColliderShapesCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Sleeping Collider Shapes", physics2DStats.m_SleepingColliderShapesCount, kFormatCount);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Step Time", physics2DStats.m_StepTime, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Contact Time", physics2DStats.m_CollideTime, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve Time", physics2DStats.m_SolveTime, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve Initialization Time", physics2DStats.m_SolveInitialization, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve Velocity Time", physics2DStats.m_SolveVelocity, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve Position Time", physics2DStats.m_SolvePosition, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve Broadphase Time", physics2DStats.m_SolveBroadphase, kFormatTime);
	ADD_STAT(kProfilerAreaPhysics2D, false, "Solve TOI Time", physics2DStats.m_SolveTimeOfImpact, kFormatTime);
}

static inline ProfilerString FormatNumber (int num)
{
	if (num < 1000)
		return FormatString<ProfilerString>("%d", num);
	else if (num < 1000000)
		return FormatString<ProfilerString>("%1.1fk", (num*0.001F));
	else
		return FormatString<ProfilerString>("%1.1fM", num*0.000001F);
}

ProfilerString DrawStats::ToString () const 
{
	int vramUsageMin = screenBytes + renderTextureBytes;
	int vramUsageMax = screenBytes + usedTextureBytes + renderTextureBytes + vboTotalBytes;
	
	return 
		FormatString<ProfilerString>("Draw Calls: %d \tTris: %s \t Verts: %s", drawCalls, FormatNumber(triangles).c_str(), FormatNumber(vertices).c_str()) +
		FormatString<ProfilerString>("\nBatched Draw Calls: %d \tBatched Tris: %s \t Batched Verts: %s", batchedDrawCalls, FormatNumber(batchedTriangles).c_str(), FormatNumber(batchedVertices).c_str()) +
		FormatString<ProfilerString>("\nUsed Textures: %d / %s", usedTextureCount, FormatBytes(usedTextureBytes).c_str()) + 
		FormatString<ProfilerString>("\nRenderTextures: %d / %s", renderTextureCount, FormatBytes(renderTextureBytes).c_str()) +
		FormatString<ProfilerString>("\nRenderTexture Switches: %d", renderTextureStateChanges) +
		FormatString<ProfilerString>("\nScreen: %s / %s", GetScreenResString().c_str(), FormatBytes(screenBytes).c_str()) +
		FormatString<ProfilerString>("\nVRAM usage: %s to %s (of %s)", FormatBytes(vramUsageMin).c_str(), FormatBytes(vramUsageMax).c_str(), FormatBytes(totalAvailableVRamMBytes * 1024 * 1024).c_str()) +
		FormatString<ProfilerString>("\nVBO Total: %d - %s", vboTotal, FormatBytes(vboTotalBytes).c_str()) +
		FormatString<ProfilerString>("\nVB Uploads: %d - %s", vboUploads, FormatBytes(vboUploadBytes).c_str()) +
		FormatString<ProfilerString>("\nIB Uploads: %d - %s", ibUploads, FormatBytes(ibUploadBytes).c_str()) +
		FormatString<ProfilerString>("\nShadow Casters: %d\t ", shadowCasters);
}


ProfilerString GetFormattedSmallTime(ProfileTimeFormat time)
{
	if (time < 100000)
		return FormatString<ProfilerString>("%d nano", (int)time);
	
	time /= 100;
	
	ProfilerString value = FormatString<ProfilerString>("%u", (unsigned)time);
	
	int length = value.length();
	
	if (length > 4)
	{
		value.insert(length - 4, 1, '.');
	}
	else
	{
		ProfilerString tmp;
		tmp.assign(4, '0');
		
		value.copy((char*) tmp.c_str() + (4 - length), length);
		
		return "0." + tmp;
	}
	
	return value;
}

ProfilerString DebugStats::ToString () const 
{	
	return 
	FormatString<ProfilerString>("Profiler Memory: %s\n", FormatBytes(m_ProfilerMemoryUsage).c_str()) +
	FormatString<ProfilerString>("Profiler Memory Misc: %s\n", FormatBytes(m_ProfilerMemoryUsageOthers).c_str())
#if DEBUGMODE
	+ FormatString<ProfilerString>("Profiler Sample Count: %d\n", m_AllocatedProfileSamples)
#endif
	;
}

ProfilerString MemoryStats::ToString () const 
{	
	ProfilerString str =
		FormatString<ProfilerString>("Used Total: %s   ", FormatBytes(bytesUsedTotal).c_str()) +
		FormatString<ProfilerString>("Unity: %s   ", FormatBytes(bytesUsedUnity).c_str()) +
		FormatString<ProfilerString>("Mono: %s   ", FormatBytes(bytesUsedMono).c_str()) +
		FormatString<ProfilerString>("GfxDriver: %s   ", FormatBytes(bytesUsedGFX).c_str()) +
		FormatString<ProfilerString>("FMOD: %s   ", FormatBytes(bytesUsedFMOD).c_str()) +
		FormatString<ProfilerString>("Profiler: %s   ", FormatBytes(bytesUsedProfiler).c_str()) +
		FormatString<ProfilerString>("\nReserved Total: %s   ", FormatBytes(bytesReservedTotal).c_str()) +
		FormatString<ProfilerString>("Unity: %s   ", FormatBytes(bytesReservedUnity).c_str()) +
		FormatString<ProfilerString>("Mono: %s   ", FormatBytes(bytesReservedMono).c_str()) +
		FormatString<ProfilerString>("GfxDriver: %s   ", FormatBytes(bytesReservedGFX).c_str()) +
		FormatString<ProfilerString>("FMOD: %s   ", FormatBytes(bytesReservedFMOD).c_str()) +
		FormatString<ProfilerString>("Profiler: %s   ", FormatBytes(bytesReservedProfiler).c_str()) +

		FormatString<ProfilerString>("\nTotal System Memory Usage: %s   ", FormatBytes(bytesVirtual).c_str()) +
		FormatString<ProfilerString>("\n(WP8) Commited Limit: %s   ", FormatBytes(bytesCommitedLimit).c_str()) +
		FormatString<ProfilerString>("Commited Total: %s   ", FormatBytes(bytesCommitedTotal).c_str()) +

	FormatString<ProfilerString>("\n\nTextures: %d / %s", textureCount, FormatBytes(textureBytes).c_str()) +
	FormatString<ProfilerString>("\nMeshes: %d / %s", meshCount, FormatBytes(meshBytes).c_str()) + 
	FormatString<ProfilerString>("\nMaterials: %d / %s", materialCount, FormatBytes(materialBytes).c_str()) + 
	FormatString<ProfilerString>("\nAnimationClips: %d / %s", animationClipCount, FormatBytes(animationClipBytes).c_str()) + 
	FormatString<ProfilerString>("\nAudioClips: %d / %s", audioCount, FormatBytes(audioBytes).c_str()) + 
	FormatString<ProfilerString>("\nAssets: %d ", assetCount) + 
	FormatString<ProfilerString>("\nGameObjects in Scene: %d ", gameObjectCount) + 
	FormatString<ProfilerString>("\nTotal Objects in Scene: %d ", sceneObjectCount) + 
	FormatString<ProfilerString>("\nTotal Object Count: %d", totalObjectsCount);// +
/*	Format("\nMost Occurring Objects:");
	std::vector<int> sorted;
	sorted.resize(classCount.size());
	for(int i = 0; i < classCount.size(); i++)
		sorted[i] = (classCount[i]<<12)+i;
	std::sort(sorted.begin(), sorted.end());

	for(int i = sorted.size()-1; i >= 0 ; i--)
	{	
		int count = sorted[i] >> 12;
		int index = sorted[i] & 0xFFF;
		if(count != 0)
			str += Format("\n\t%s: %d",Object::ClassIDToString(index).c_str(), count);
	}*/
	/*str += "\n\n";
	for(int i = 0; i < memoryAllocatorInformation.size(); i++)
	{
		str += FormatString<ProfilerString>("\n%s: used %s (reserved %s)",memoryAllocatorInformation[i].name.c_str(), FormatBytes(memoryAllocatorInformation[i].used).c_str(), FormatBytes(memoryAllocatorInformation[i].reserved).c_str());
	}
	str += "\n\n";
	str += memoryOverview;
	*/
	return str;
}

ProfilerString DrawStats::GetScreenResString () const 
{
	if (screenFSAA > 1)
		return FormatString<ProfilerString>("%ix%i %ixAA", screenWidth, screenHeight, screenFSAA);
	else
		return FormatString<ProfilerString>("%ix%i", screenWidth, screenHeight);
}

#if ENABLE_PLAYERCONNECTION

void AllProfilerStats::Serialize( dynamic_array<int>& bitstream )
{
	memoryStats.Serialize(bitstream);
	WriteIntArray (bitstream, drawStats);
	WriteIntArray (bitstream, physicsStats);
	WriteIntArray (bitstream, physics2DStats);
	debugStats.Serialize(bitstream); // not all ints
	WriteIntArray (bitstream, audioStats);
	WriteIntArray (bitstream, chartSample);
	WriteIntArray (bitstream, chartSampleSelected);
}

void DebugStats::Serialize( dynamic_array<int>& bitstream )
{
	bitstream.push_back (m_ProfilerMemoryUsage);
	bitstream.push_back (m_ProfilerMemoryUsageOthers);
	bitstream.push_back (m_AllocatedProfileSamples);
}

void MemoryStats::Serialize( dynamic_array<int>& bitstream )
{
	bitstream.push_back (bytesUsedTotal/1024);
	bitstream.push_back (bytesUsedUnity/1024);
	bitstream.push_back (bytesUsedMono/1024);
	bitstream.push_back (bytesUsedGFX/1024);
	bitstream.push_back (bytesUsedFMOD/1024);
	bitstream.push_back (bytesUsedProfiler/1024);

	bitstream.push_back (bytesReservedTotal/1024);
	bitstream.push_back (bytesReservedUnity/1024);
	bitstream.push_back (bytesReservedMono/1024);
	bitstream.push_back (bytesReservedGFX/1024);
	bitstream.push_back (bytesReservedFMOD/1024);
	bitstream.push_back (bytesReservedProfiler/1024);

	bitstream.push_back (bytesVirtual/1024);
	bitstream.push_back (bytesCommitedLimit/1024);
	bitstream.push_back (bytesCommitedTotal/1024);

	bitstream.push_back (textureCount);
	bitstream.push_back (textureBytes);
	bitstream.push_back (meshCount);
	bitstream.push_back (meshBytes);
	bitstream.push_back (materialCount);
	bitstream.push_back (materialBytes);
	bitstream.push_back (animationClipCount);
	bitstream.push_back (animationClipBytes);
	bitstream.push_back (audioCount);
	bitstream.push_back (audioBytes);
	bitstream.push_back (assetCount);
	bitstream.push_back (sceneObjectCount);
	bitstream.push_back (gameObjectCount);
	bitstream.push_back (totalObjectsCount);
	
	// write size, and index,entry for all non 0 entries
	bitstream.push_back (classCount.size());
	for(int i = 0; i < classCount.size(); i++)
	{
		if(classCount[i] != 0)
		{
			bitstream.push_back (i);
			bitstream.push_back (classCount[i]);
		}
	}
	bitstream.push_back (-1);
	
	/// TODO
//	WriteString(bitstream, memoryOverview.c_str());

}


void AllProfilerStats::Deserialize( int** bitstream, bool swapdata )
{
	memoryStats.Deserialize(bitstream, swapdata);
	ReadIntArray (bitstream, drawStats);
	ReadIntArray (bitstream, physicsStats);
	ReadIntArray (bitstream, physics2DStats);	
	debugStats.Deserialize(bitstream); // not all ints
	ReadIntArray (bitstream, audioStats);
	ReadIntArray (bitstream, chartSample);
	ReadIntArray (bitstream, chartSampleSelected);
}

void DebugStats::Deserialize( int** bitstream )
{
	m_ProfilerMemoryUsage = *((*bitstream)++);
	m_ProfilerMemoryUsageOthers = *((*bitstream)++);
	m_AllocatedProfileSamples = *((*bitstream)++);
}

void MemoryStats::Deserialize( int** bitstream, bool swapdata )
{
	bytesUsedTotal = *((*bitstream)++)*1024;
	bytesUsedUnity = *((*bitstream)++)*1024;
	bytesUsedMono = *((*bitstream)++)*1024;
	bytesUsedGFX = *((*bitstream)++)*1024;
	bytesUsedFMOD = *((*bitstream)++)*1024;
	bytesUsedProfiler = *((*bitstream)++)*1024;

	bytesReservedTotal = *((*bitstream)++)*1024;
	bytesReservedUnity = *((*bitstream)++)*1024;
	bytesReservedMono = *((*bitstream)++)*1024;
	bytesReservedGFX = *((*bitstream)++)*1024;
	bytesReservedFMOD = *((*bitstream)++)*1024;
	bytesReservedProfiler = *((*bitstream)++)*1024;

	bytesVirtual = *((*bitstream)++)*1024;
	bytesCommitedLimit = *((*bitstream)++)*1024;
	bytesCommitedTotal = *((*bitstream)++)*1024;

	textureCount = *((*bitstream)++);
	textureBytes = *((*bitstream)++);
	meshCount = *((*bitstream)++);
	meshBytes = *((*bitstream)++);
	materialCount = *((*bitstream)++);
	materialBytes = *((*bitstream)++);
	animationClipCount = *((*bitstream)++);
	animationClipBytes = *((*bitstream)++);
	audioCount = *((*bitstream)++);
	audioBytes = *((*bitstream)++);
	assetCount = *((*bitstream)++);
	sceneObjectCount = *((*bitstream)++);
	gameObjectCount = *((*bitstream)++);
	totalObjectsCount = *((*bitstream)++);
	int count;
	count = *((*bitstream)++);
	classCount.resize_initialized(count,0);
	int index = *((*bitstream)++);
	while(index != -1)
	{
		classCount[index] = *((*bitstream)++);
		index = *((*bitstream)++);
	}
	
//	ReadString(bitstream, memoryOverview, swapdata);

}
#endif

#endif


