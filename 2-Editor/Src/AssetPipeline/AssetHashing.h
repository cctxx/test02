#ifndef ASSET_HASHING_H
#define ASSET_HASHING_H

#include "CacheServer/CacheServer.h"
#include "Runtime/Serialize/SerializationMetaFlags.h"

struct HashAssetRequest
{
	// Input provided from outside
	UnityStr			assetPath;
	UnityGUID           guid;
	int                 importerClassID;
	UInt32              importerVersionHash;

	// Output during hashing process
	MdFour              hash;
	
	HashAssetRequest (const UnityGUID& g, const std::string& inPath, int inImporterClass, const UInt32& inImporterversionHash) 
		: guid(g), 
		assetPath (inPath), 
		importerClassID(inImporterClass), 
		importerVersionHash(inImporterversionHash)
		{ }
};

MdFour GenerateHashForAsset (const std::string& assetPath, int importerClassID, const UInt32& importerVersionHash, BuildTargetSelection target);
UInt64 ProcessHashRequests (std::vector<HashAssetRequest> &requests, BuildTargetSelection platform);

#endif