#include "UnityPrefix.h"
#include "YAMLRead.h"
#include "../FileCache.h"

int YAMLRead::GetDataVersion ()
{
	if (m_Versions.back() == -1)
	{
		yaml_node_t *node = m_CurrentNode;
		int i = m_MetaParents.size();
		do
		{
			yaml_node_t *versionNode = GetValueForKey(node, "serializedVersion");
			if (versionNode)
			{
				Assert (versionNode->type == YAML_SCALAR_NODE);				
				sscanf ((char*)versionNode->data.scalar.value, "%d", &m_Versions.back());
				return m_Versions.back();
			}
			// If "serializedVersion" is not found, look for "importerVersion" for backwards compatibility.
			versionNode = GetValueForKey(node, "importerVersion");
			if (versionNode)
			{
				Assert (versionNode->type == YAML_SCALAR_NODE);				
				sscanf ((char*)versionNode->data.scalar.value, "%d", &m_Versions.back());
				return m_Versions.back();
			}
			if (i>0)
				node = m_MetaParents[--i];
			else
				node = NULL;
		}
		while (node != NULL);
		m_Versions.back() = 1;
	}
	return m_Versions.back();
}

yaml_node_t *YAMLRead::GetValueForKey (yaml_node_t* parentNode, const char* keystr)
{
	if (parentNode && parentNode->type == YAML_MAPPING_NODE)
	{
		// The code below does not handle empty yaml arrays.
		if (parentNode->data.mapping.pairs.top == parentNode->data.mapping.pairs.start)
			return NULL;

		yaml_node_pair_t* start;
		if (m_CachedIndex < parentNode->data.mapping.pairs.top 
			&& m_CachedIndex >= parentNode->data.mapping.pairs.start)
			start = m_CachedIndex;
		else
			start = parentNode->data.mapping.pairs.start;
			
		yaml_node_pair_t* top = parentNode->data.mapping.pairs.top;
		yaml_node_pair_t* i = start;
		
		do			
		{
			yaml_node_pair_t* next = i+1;
			if (next == top)
				next = parentNode->data.mapping.pairs.start;

			yaml_node_t* key = yaml_document_get_node(m_ActiveDocument, i->key);
			if (key == NULL)
			{
				// I've seen a crash bug report with no repro, indicating that this is happening.
				// If you ever get this error and can repro it, let me know! jonas.
				ErrorString ("YAML Node is NULL!\n");
			}
			else
			{
				Assert (key->type == YAML_SCALAR_NODE);
				
				if (strcmp((char*)key->data.scalar.value, keystr) == 0)
				{
					m_CachedIndex = next;
					return yaml_document_get_node(m_ActiveDocument, i->value);
				}
			}
			i = next;
		}
		while (i != start);
	}
	return NULL;
}


void YAMLRead::Init(int flags, yaml_read_handler_t *handler, std::string *debugFileName, int debugLineCount)
{
	m_UserData = NULL;
	m_CurrentVersion = 0;
	m_Flags = flags;
	m_CachedIndex = NULL;
	m_ReadHandler = handler;
	
	yaml_parser_t parser;

	memset(&parser, 0, sizeof(parser));
	memset(&m_Document, 0, sizeof(m_Document));
	
	if (!yaml_parser_initialize(&parser)) 
	{
		ErrorString("Could not initialize yaml parser\n");
		return;
	}
	
	yaml_parser_set_input(&parser, handler, this );
	yaml_parser_load(&parser, &m_Document);
	
	if (parser.error != YAML_NO_ERROR)
	{
		if (debugFileName != NULL)
		{
			ErrorStringMsg("Unable to parse file %s: [%s] at line %d\n", debugFileName->c_str(), parser.problem, debugLineCount + (int)parser.problem_mark.line);
		}
		else
		{
			ErrorStringMsg("Unable to parse YAML file: [%s] at line %d\n", parser.problem, debugLineCount + (int)parser.problem_mark.line);
		}
	}
	
	yaml_parser_delete(&parser);
	
	m_Versions.push_back(-1);
	m_CurrentNode = yaml_document_get_root_node(&m_Document);
	m_ActiveDocument = &m_Document;
	m_DidReadLastProperty = false;
}

YAMLRead::YAMLRead (yaml_document_t* yamlDocument, int flags)
:	m_ReadHandler (NULL)
,	m_ActiveDocument (yamlDocument)
,	m_CurrentVersion (0)
,	m_CachedIndex (0)
,   m_DidReadLastProperty (false)
{
	m_Flags = flags;
	memset(&m_Document, 0, sizeof(m_Document));
	m_Versions.push_back(-1);
	m_CurrentNode = yaml_document_get_root_node(m_ActiveDocument);
}


int YAMLRead::YAMLReadCacheHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read)
{
	YAMLRead *read = (YAMLRead*)data;

	if (read->m_ReadOffset + size > read->m_EndOffset)
		size = read->m_EndOffset - read->m_ReadOffset;
		
	ReadFileCache (*(CacheReaderBase*)read->m_ReadData, buffer, read->m_ReadOffset, size);
	read->m_ReadOffset += size;
	*size_read = size;
	
	return true;
}

int YAMLRead::YAMLReadStringHandler(void *data, unsigned char *buffer, size_t size, size_t *size_read)
{
	YAMLRead *read = (YAMLRead*)data;

	if (read->m_ReadOffset + size > read->m_EndOffset)
		size = read->m_EndOffset - read->m_ReadOffset;
	
	const char* readData = reinterpret_cast<const char*> (read->m_ReadData);
	
	memcpy (buffer, readData + read->m_ReadOffset, size);
	read->m_ReadOffset += size;
	*size_read = size;
	
	return true;
}

YAMLRead::YAMLRead (const char* strBuffer, int size, int flags, std::string *debugFileName, int debugLineCount) 
{ 
	m_ReadOffset = 0;
	m_EndOffset = size;
	m_ReadData = const_cast<char*> (strBuffer);
	
	Init (flags, YAMLReadStringHandler, debugFileName, debugLineCount);
}

YAMLRead::YAMLRead (const CacheReaderBase *input, size_t readOffset, size_t endOffset, int flags, std::string *debugFileName, int debugLineCount) 
{ 
	m_ReadOffset = readOffset;
	m_EndOffset = endOffset;
	m_ReadData = (void*)input;
	
	Init (flags, YAMLReadCacheHandler, debugFileName, debugLineCount);
}

YAMLRead::~YAMLRead()
{
	yaml_document_delete(&m_Document);
}

YAMLNode* YAMLRead::GetCurrentNode ()
{
	return YAMLDocNodeToNode(m_ActiveDocument, m_CurrentNode);
}

YAMLNode* YAMLRead::GetValueNodeForKey (const char* key)
{
	return YAMLDocNodeToNode (m_ActiveDocument, GetValueForKey (m_CurrentNode, key));
}

int YAMLRead::StringOutputHandler(void *data, unsigned char *buffer, size_t size) 
{
	string* theString = reinterpret_cast<string*> (data);
	theString->append( (char *) buffer, size);
	return 1;
}

void YAMLRead::BeginMetaGroup (std::string name)
{
	m_MetaParents.push_back(m_CurrentNode);
	m_CurrentNode = GetValueForKey(m_CurrentNode, name.c_str());
}

void YAMLRead::EndMetaGroup ()
{
	m_CurrentNode = m_MetaParents.back();
	m_MetaParents.pop_back();
}

void YAMLRead::TransferTypelessData (unsigned size, void* data, int metaFlag) 
{
	UnityStr dataString;
	Transfer(dataString, "_typelessdata", metaFlag);
	dataString.resize (size * 2);
	HexStringToBytes (&dataString[0], size, data);
}

bool YAMLRead::HasNode (const char* name)
{
	return GetValueForKey(m_CurrentNode, name) != NULL;
}

