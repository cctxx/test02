#include "UnityPrefix.h"
#include "Editor/Src/AssetPipeline/SubstanceLoader.h"
#include "Editor/Platform/Interface/EditorUtility.h"
#include "External/Allegorithmic/builds/Engines/include/substance/substance.h"
#include "External/Allegorithmic/builds/Engines/include/substance/linker/linker.h"

#include <fstream>
#include <algorithm>

std::string ParseSubstanceName (std::string packageURL)
{
	std::string substanceName = packageURL;
	if (!substanceName.empty())
	{
		// Extract the graph name which is the last part of the URL
		int	lastSlashIndex = substanceName.find_last_of('/');
		
		if (lastSlashIndex!=std::string::npos)
			substanceName = substanceName.substr(lastSlashIndex+1);
	}
	else
	{
		substanceName = "Default Graph";
	}

	return substanceName;
}

void GetSubstanceNamesFromXml (const TiXmlDocument& xmlData, std::vector<std::string>& substanceNames)
{
	const TiXmlElement* rootElement = xmlData.FirstChildElement("sbsdescription");
	
	if (!rootElement)
	{
		ErrorString("Failed to parse substance description");
		return;
	}

	const TiXmlElement* graphsElement = rootElement->FirstChildElement("graphs");

	if (!graphsElement)
	{
		ErrorString("Failed to parse substance graph description");
		return;
	}

	const TiXmlElement* graphElement = graphsElement->FirstChildElement("graph");
	
	while (graphElement)
	{
		substanceNames.push_back( ParseSubstanceName(graphElement->Attribute("pkgurl")));
		graphElement = graphElement->NextSiblingElement("graph");
	}
}

bool TranslateWidgetType (ProceduralPropertyType& type, SubstanceInputType internalType, const char* widgetName)
{
	string widgetString(widgetName!=NULL?widgetName:"");
	
	switch( internalType )
	{
		case Substance_IType_Float:
			if (widgetString=="togglebutton")		type = ProceduralPropertyType_Boolean;
			else type = ProceduralPropertyType_Float;
			return true;
		case Substance_IType_Float2:
			type = ProceduralPropertyType_Vector2;
			return true;
		case Substance_IType_Float3:
			if (widgetString=="color")				type = ProceduralPropertyType_Color3;
			else									type = ProceduralPropertyType_Vector3;
			return true;
		case Substance_IType_Float4:
			if (widgetString=="color")				type = ProceduralPropertyType_Color4;
			else									type = ProceduralPropertyType_Vector4;
			return true;	
		case Substance_IType_Integer:
			if (widgetString=="togglebutton")		type = ProceduralPropertyType_Boolean;
			else if (widgetString=="combobox")		type = ProceduralPropertyType_Enum;
			else									type = ProceduralPropertyType_Float;
			return true;
		case Substance_IType_Integer2:
			type = ProceduralPropertyType_Vector2;
			return true;
		case Substance_IType_Integer3:
			if (widgetString=="color")				type = ProceduralPropertyType_Color3;
			else									type = ProceduralPropertyType_Vector3;
			return true;
		case Substance_IType_Integer4:
			if (widgetString=="color")				type = ProceduralPropertyType_Color4;
			else									type = ProceduralPropertyType_Vector4;
			return true;	
		case Substance_IType_Image:
			type = ProceduralPropertyType_Texture;
			return true;
	}

	return false;
}

void ParseInputDependencies(const char* dependencies, std::set<unsigned int>& alteredOutputs)
{
	if (!dependencies || strlen(dependencies)==0)
		return;

	std::string alterOutputs(dependencies);
	std::string	delimiter = ",";
	std::string::size_type lastPos = 0;
	std::string::size_type pos = 0;

	while (pos!=std::string::npos)
	{
		std::string	outputUID;

		pos = alterOutputs.find_first_of(delimiter,pos);
		outputUID = alterOutputs.substr(lastPos, pos==std::string::npos ? pos : pos-lastPos);
		alteredOutputs.insert(strtoul(outputUID.c_str(), NULL, 10));
		
		if (pos == std::string::npos)
			break;

		lastPos = ++pos;
	}
}

bool GenerateInputInformation (const TiXmlElement& xmlElement, SubstanceHandle* substanceHandle, SubstanceInput& inputInformation)
{	
	// Name
	const char* nameAttribute = xmlElement.Attribute("identifier");
	if (!nameAttribute)
		return false;
	inputInformation.name = nameAttribute;
	
	// Defaulting label to name, since this information may be empty.
	inputInformation.label = nameAttribute;

	// Flags
	if (inputInformation.name=="$randomseed" || inputInformation.name=="$outputsize")
	{
		inputInformation.EnableFlag(SubstanceInput::Flag_SkipHint);
	}

	// UID
	const char* uidAttribute = xmlElement.Attribute("uid");
	if (!uidAttribute)
		return false;
	inputInformation.internalIdentifier = strtoul(uidAttribute, NULL, 10);
	const char* typeAttribute = xmlElement.Attribute( "type");
	if (!typeAttribute)
		return false;
	inputInformation.internalType = (SubstanceInputType)atoi(typeAttribute);

	// ProceduralPropertyType
	const TiXmlElement* widgetElement = xmlElement.FirstChildElement("inputgui");
	const char* widgetAttribute = NULL;
	if (widgetElement)
		widgetAttribute = widgetElement->Attribute("widget");
	if (!TranslateWidgetType(inputInformation.type, inputInformation.internalType, widgetAttribute))
		return false;

	// Label
	if (widgetElement)
	{
		const char* labelAttribute = widgetElement->Attribute("label");
		if (labelAttribute)
			inputInformation.label = labelAttribute;
	}

	// Group
	if (widgetElement)
	{
		const char* groupAttribute = widgetElement->Attribute("group");
		if (groupAttribute)
			inputInformation.group = groupAttribute;
	}

	// Altered outputs
	const char* alteredAttribute = xmlElement.Attribute("alteroutputs");
	if (!alteredAttribute)
		return false;
	ParseInputDependencies(alteredAttribute, inputInformation.alteredTexturesUID);

	// Default, min, max values	
	const char* defaultAttribute = xmlElement.Attribute("default");
	const char* minAttribute(NULL);
	const char* maxAttribute(NULL);
	if (widgetElement)
	{
		const TiXmlElement* guiElement;
		if (widgetAttribute!=NULL && string(widgetAttribute)=="angle")
		{
			guiElement = widgetElement->FirstChildElement("guiangle");
		}
		else
		{
			guiElement = widgetElement->FirstChildElement("guicombobox");

			// Enum special parsing
			if (guiElement && inputInformation.type==ProceduralPropertyType_Enum)
			{
				int min(0), max(0);
				
				// Set default as min/max (in case there is no guicomboboxitem)
				if (defaultAttribute && strlen(defaultAttribute) > 0)
				{
					if (sscanf(defaultAttribute, "%d", &min)!=1)
						return false;
					
					max = min;
				}

				inputInformation.EnableFlag(SubstanceInput::Flag_Clamp);
				inputInformation.minimum = (float)min;
				inputInformation.maximum = (float)max;

				// Parse combo values
				const TiXmlElement* valueElement = guiElement->FirstChildElement("guicomboboxitem");
				while (valueElement)
				{
					const char* valueAttribute = valueElement->Attribute("value");
					const char* textAttribute = valueElement->Attribute("text");
					if (valueAttribute && strlen(valueAttribute) > 0
						&& textAttribute && strlen(textAttribute) > 0)
					{
						SubstanceEnumItem item;
						
						if (sscanf(valueAttribute, "%d", &item.value)!=1)
							return false;

						item.text = textAttribute;
						inputInformation.enumValues.push_back(item);
					}
					valueElement = valueElement->NextSiblingElement("guicomboboxitem");
				}
			}

			guiElement = widgetElement->FirstChildElement("guislider");
		}
		if (guiElement)
		{
			// ignore min/max of $outputsize since its not a requirement
			if (inputInformation.name!="$outputsize")
			{
				minAttribute = guiElement->Attribute("min");
				maxAttribute = guiElement->Attribute("max");	
			}

			// The "clamp" flag is not correctly set on Substances
			//const char* clampAttribute = guiElement->Attribute("clamp");
			//inputInformation.EnableFlag(SubstanceInput::Flag_Clamp,
			//	clampAttribute && strcmp(clampAttribute, "on")==0);
		}
	}

	inputInformation.step = IsSubstanceAnyIntType(inputInformation.internalType)?1.0F:0.0F;

	int requiredCount, result;
	if (IsSubstanceAnyFloatType(inputInformation.internalType)
		|| IsSubstanceAnyIntType(inputInformation.internalType))
	{
		requiredCount = GetRequiredInputComponentCount(inputInformation.internalType);
		
		if (defaultAttribute && strlen(defaultAttribute) > 0)
		{
			result = sscanf(defaultAttribute, "%f,%f,%f,%f", inputInformation.value.scalar + 0, inputInformation.value.scalar + 1, inputInformation.value.scalar + 2, inputInformation.value.scalar + 3);
			if (result != requiredCount)
				return false;
		}

		if (minAttribute)
		{
			result = sscanf(minAttribute, "%f", &inputInformation.minimum);
			if (result != 1)
				return false;
		}

		if (maxAttribute)
		{
			result = sscanf(maxAttribute, "%f", &inputInformation.maximum);
			if (result != 1)
				return false;
		}
	}
	else if (inputInformation.type != ProceduralPropertyType_Texture)
	{
		return false;
	}

	inputInformation.EnableFlag(SubstanceInput::Flag_Clamp,
		inputInformation.minimum >= -std::numeric_limits<float>::max()/2.0F
		&& inputInformation.maximum <= std::numeric_limits<float>::max()/2.0F);

	return true;
}

bool GenerateInputsInformation(const TiXmlElement& parentElement, SubstanceHandle* substanceHandle, SubstanceInputs& inputInformation)
{
	const TiXmlElement* inputsElement = parentElement.FirstChildElement("inputs");

	if (inputsElement)
	{
		const TiXmlElement* inputElement = inputsElement->FirstChildElement("input");
		unsigned int inputCount = atoi(inputsElement->Attribute("count"));
		unsigned int inputIndex = inputInformation.size();
		unsigned int removedCount = 0;
		inputInformation.reserve(inputIndex+inputCount);

		while (inputElement)
		{
			inputInformation.resize(inputIndex+1);
			if (!GenerateInputInformation(*inputElement, substanceHandle, inputInformation[inputIndex++]))
			{
				--inputIndex;
			}
			
			inputElement = inputElement->NextSiblingElement("input");
		}
		inputInformation.resize(inputIndex);
			
		if (removedCount>0)
		{
			ErrorString("Failed to import some of the substance inputs");
			inputInformation.resize(inputInformation.size()-removedCount);
		}
	}
	return true;
}

bool ParseOutputUsage(const TiXmlElement& xmlOutputElement, std::string& output_usage)
{
	const TiXmlElement *guiElement(NULL), *channelsElement(NULL), *channelElement(NULL);
	const char *usageAttribute(NULL);

	guiElement = xmlOutputElement.FirstChildElement("outputgui");
	if (guiElement) channelsElement = guiElement->FirstChildElement("channels");
	if (channelsElement) channelElement = channelsElement->FirstChildElement("channel");
	if (channelElement) usageAttribute = channelElement->Attribute("names");
	if (usageAttribute)
	{
		output_usage = usageAttribute;
		return true;
	}

	output_usage = "";
	return false;
}

bool GenerateOutputInformation (const std::string& substanceName, const TiXmlElement& xmlElement, SubstanceTextureInformation& outputInformation)
{
	if (xmlElement.Attribute( "identifier" ) == NULL)
		return false; 
	outputInformation.name = xmlElement.Attribute( "identifier" );
		
	if (xmlElement.Attribute("uid") == NULL)
		return false;
	outputInformation.UID = strtoul(xmlElement.Attribute("uid"), NULL, 10 );
	return ParseOutputUsage(xmlElement, outputInformation.usage);
}

bool GenerateImportInformation (const TiXmlDocument& xmlData, SubstanceHandle* substanceHandle, const std::string& substanceName,
	SubstanceInputs& inputInformation, std::vector<SubstanceTextureInformation>& outputInformation)
{
	const TiXmlElement* rootElement = xmlData.FirstChildElement( "sbsdescription" );
	if (!rootElement)
		return false;

	const TiXmlElement* graphsElement = rootElement->FirstChildElement( "graphs" );
	if (!graphsElement)
		return false;

	const TiXmlElement* substanceElement = NULL;
	const TiXmlElement* graphElement = graphsElement->FirstChildElement( "graph" );
	
	while (graphElement)
	{
		if (substanceName==ParseSubstanceName(graphElement->Attribute("pkgurl")))
		{
			substanceElement = graphElement;
			break;
		}
		
		graphElement = graphElement->NextSiblingElement("graph");
	}

	if (!substanceElement)
		return false;

	// Retrieve the global inputs
	const TiXmlElement* globalElement = rootElement->FirstChildElement( "global" );
	if (globalElement)
	{
		if (!GenerateInputsInformation(*globalElement, substanceHandle, inputInformation))
			return false;
	}

	// Retrieve the graph inputs
	if (!GenerateInputsInformation(*substanceElement, substanceHandle, inputInformation))
		return false;

	// Get substance description
	SubstanceDataDesc descriptor;
	substanceHandleGetDesc(substanceHandle, &descriptor);
	
	// Do input remapping
	{
		std::map<unsigned int, unsigned int> UIDtoIndex;
		for (int inputIndex=0;inputIndex<descriptor.inputsCount;++inputIndex)
		{
			SubstanceInputDesc inputDescription;
			unsigned int res = substanceHandleGetInputDesc(substanceHandle, inputIndex, &inputDescription);
			if (res != 0)
				return false;
			UIDtoIndex[inputDescription.inputId] = inputIndex;	
		}


		for (SubstanceInputs::iterator it=inputInformation.begin();it!=inputInformation.end();++it)
		{
			if (UIDtoIndex.count(it->internalIdentifier) == 0)
				return false;

			it->internalIndex = UIDtoIndex[it->internalIdentifier];
		}
	}

	// Retrieve the outputs
	const TiXmlElement* outputsElement = substanceElement->FirstChildElement("outputs");
	if (!outputsElement)
	{
		return false;
	}

	const TiXmlElement* outputElement = outputsElement->FirstChildElement("output");
	unsigned int outputCount = atoi(outputsElement->Attribute("count"));
	unsigned int outputIndex = 0;
	unsigned int removedOutputCount = 0;
	outputInformation.reserve(outputCount);

	while (outputElement)
	{
		outputInformation.resize(outputIndex+1);
		if (!GenerateOutputInformation(substanceName, *outputElement, outputInformation[outputIndex++]))
		{
			++removedOutputCount;
			--outputIndex;
		}
		outputElement = outputElement->NextSiblingElement( "output" );
	}
	outputInformation.resize(outputIndex);
		
	if (removedOutputCount>0)
	{
		ErrorString("Failed to import some of the substance outputs");
		outputInformation.resize(outputInformation.size()-removedOutputCount);
	}
	return true;
}

bool LinkSubstance (const std::vector<UInt8>& assemblyContent, std::vector<UInt8>& binaryContent)
{
	unsigned size = assemblyContent.size();
	
	if (size == 0)
		return false;

	const UInt8* bufferData = &assemblyContent[0];
	
	SubstanceLinkerContext*	linkerContext;
	if (substanceLinkerContextInit(&linkerContext, SUBSTANCE_LINKER_API_VERSION, Substance_EngineID_sse2)!=0)
		return false;

	// Create linker handle
	SubstanceLinkerHandle* linkerHandle;
	if (substanceLinkerHandleInit(&linkerHandle, linkerContext)!=0)
		return false;
	
	// Push assembly
	if (substanceLinkerHandlePushAssemblyMemory(linkerHandle, (const char*)bufferData, size)!=0)
		return false;

	// Link
	size_t bufferSize = 0;
	UInt8* m_SubstanceData = NULL;
	if (substanceLinkerHandleLink(linkerHandle, (const unsigned char**)&m_SubstanceData, &bufferSize )!=0)
		return false;

	binaryContent.assign(m_SubstanceData, m_SubstanceData+bufferSize);
	
	// Release handle
	substanceLinkerHandleRelease(linkerHandle);
	
	// Release context
	substanceLinkerContextRelease(linkerContext);

	return true;
}

std::string* internalXmlDescription;
std::vector<UInt8>* internalAssemblyContent;

extern "C" void SUBSTANCE_CALLBACK internalCallbackLinkerArchiveXml(SubstanceLinkerHandle *handle, const unsigned short* basename, const char* xmlContent)
{
	*internalXmlDescription = xmlContent;
}

extern "C" void SUBSTANCE_CALLBACK internalCallbackLinkerArchiveAsm(SubstanceLinkerHandle *handle, const unsigned short* basename, const char* asmContent, size_t size)
{
	internalAssemblyContent->resize(size);
	memcpy(&(*internalAssemblyContent)[0], asmContent, size);
}

extern "C" void SUBSTANCE_CALLBACK internalCallbackLinkerError(SubstanceLinkerHandle*, unsigned int severity, const char* msg)
{
	ErrorString("Failed to load substance archive");
}

bool LoadSubstanceArchive (const std::string& assetName, std::vector<UInt8>& assemblyContent, std::vector<UInt8>& binaryContent, TiXmlDocument& xmlData, const bool onlyNeedNames)
{
	if (assetName.size()==0)
	{
		ErrorString("Failed to load substance archive");
		return false;
	}

	// Retrieve substance archive content
	std::vector<char> substanceArchiveContent;
	FILE *file = fopen(assetName.c_str(), "rb");
	size_t size;
	if (file==NULL)
	{
		ErrorString("Failed to load substance archive");
		return false;
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, 0);
	substanceArchiveContent.resize(size);
	fread(&substanceArchiveContent[0], 1, size, file);
	fclose(file);

	// Use linker to retrieve the binary & XML contents
	std::string xmlDescription;
	internalXmlDescription = &xmlDescription;
	internalAssemblyContent = &assemblyContent;
	SubstanceLinkerContext *context;
	substanceLinkerContextInit(&context, SUBSTANCE_LINKER_API_VERSION, Substance_EngineID_sse2);
	substanceLinkerContextSetCallback(context, Substance_Linker_Callback_ArchiveXml, (void*)internalCallbackLinkerArchiveXml);	
	substanceLinkerContextSetCallback(context, Substance_Linker_Callback_ArchiveAsm, (void*)internalCallbackLinkerArchiveAsm);	
	substanceLinkerContextSetCallback(context, Substance_Linker_Callback_Error, (void*)internalCallbackLinkerError);	
	SubstanceLinkerHandle *handle;
	substanceLinkerHandleInit(&handle,context);
	substanceLinkerHandlePushAssemblyMemory(handle, &substanceArchiveContent[0], size);
	substanceLinkerHandleRelease(handle);
	substanceLinkerContextRelease(context);

	if (assemblyContent.empty())
	{	
		ErrorString( "Failed to retrieve a single SBSASM file in the archive... Did you use the Substance cooker to generated the archive ?" );
		return false;
	}

	if (xmlDescription.empty())
	{	
		ErrorString( "Failed to retrieve a single XML description file in the archive... Did you use the Substance cooker to generated the archive ?" );
		return false;
	}

	// Parse the XML
	xmlData.Parse( xmlDescription.c_str() );
	if (xmlData.Error())
	{
		return false;
	}

	// Only link if we need more than the Substance names (that are now in xmlData)
	if (!onlyNeedNames)
	{
		 return LinkSubstance(assemblyContent, binaryContent);
	}
	return true;
}
