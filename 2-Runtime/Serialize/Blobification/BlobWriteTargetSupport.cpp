#include "UnityPrefix.h"
#include "BlobWriteTargetSupport.h"

bool DoesBuildTargetSupportBlobification (BuildTargetPlatform target, TransferInstructionFlags flags)
{
	// If we are writing typetrees, then we can't use blobification
	bool writeTypeTree = (flags & kDisableWriteTypeTree) == 0;
	if (writeTypeTree)
		return false;

	
	// Webplayer & Editor should never use blobification
	Assert(target != kBuildWebPlayerLZMA && target != kBuildWebPlayerLZMAStreamed && target != kBuildAnyPlayerData || target == kBuildNoTargetPlatform);
	return true;
}

bool IsBuildTarget64BitBlob (BuildTargetPlatform target)
{
	Assert(target != kBuildAnyPlayerData && target != kBuildWebPlayerLZMA && target != kBuildWebPlayerLZMAStreamed);

	// Building blob for the editor (Choose whatever we are running with)
	if (target == kBuildNoTargetPlatform)
		return sizeof(size_t) == sizeof(UInt64);

	// Known 64 bit platform?
	bool target64Bit = target == kBuildMetroPlayerX64 || target == kBuildStandaloneWin64Player || target == kBuildStandaloneLinux64 || target == kBuildStandaloneLinuxUniversal;
	if (target64Bit)
		return true;

	return false;
}