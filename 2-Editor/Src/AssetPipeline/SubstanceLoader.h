#pragma once

#include "Configuration/UnityConfigure.h"

#if ENABLE_SUBSTANCE

#include <string>
#include "External/Allegorithmic/builds/Engines/include/substance/offline/tinyxml.h"
#include "External/Allegorithmic/builds/Engines/include/substance/handle.h"

#include "Runtime/Graphics/SubstanceInput.h"

struct SubstanceTextureInformation
{
	std::string name;
	std::string usage;
	unsigned int UID;
};


// Load a .SBSAR
bool LoadSubstanceArchive (const std::string& assetName, std::vector<UInt8>& assemblyContent, std::vector<UInt8>& binaryContent, TiXmlDocument& xmlData, const bool onlyNeedNames = false);

// Parse the XML to retrieve all substance names
void GetSubstanceNamesFromXml (const TiXmlDocument& xmlData, std::vector<std::string>& substanceNames);

// Generate substance import information from the XML data
bool GenerateImportInformation (const TiXmlDocument& xmlData, SubstanceHandle* substanceHandle, const std::string& substanceName, SubstanceInputs& inputInformation,
	std::vector<SubstanceTextureInformation>& outputInformation);

#endif
