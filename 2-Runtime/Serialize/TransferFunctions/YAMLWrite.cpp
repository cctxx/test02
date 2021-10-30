#include "UnityPrefix.h"
#include "YAMLWrite.h"
#include "../CacheWrap.h"
#include <string>

void YAMLWrite::TransferStringToCurrentNode (const char* str)
{
	if (m_Error)
		return;
	int node = yaml_document_add_scalar(&m_Document, NULL, (yaml_char_t*)str, strlen(str), YAML_ANY_SCALAR_STYLE);
	if (node)
		m_CurrentNode = node;
	else
		m_Error = true;
}

int YAMLWrite::NewMapping ()
{
	int node = yaml_document_add_mapping(&m_Document, NULL, (m_MetaFlags.back() & kTransferUsingFlowMappingStyle)? YAML_FLOW_MAPPING_STYLE : YAML_ANY_MAPPING_STYLE);
	if (node == 0)
		m_Error = true;
	return node;
}

int YAMLWrite::NewSequence ()
{
	int node = yaml_document_add_sequence(&m_Document, NULL, YAML_ANY_SEQUENCE_STYLE);
	if (node == 0)
		m_Error = true;
	return node;
}

int YAMLWrite::GetNode ()
{
	if (m_CurrentNode == -1)
		m_CurrentNode = NewMapping();
	return m_CurrentNode;
}

void YAMLWrite::AppendToNode(int parentNode, const char* keyStr, int valueNode)
{
	yaml_node_t* parent = yaml_document_get_node(&m_Document, parentNode);
	switch (parent->type)
	{
		case YAML_MAPPING_NODE:
			{
				int keyNode = yaml_document_add_scalar(&m_Document, NULL, (yaml_char_t*)keyStr, strlen(keyStr), YAML_ANY_SCALAR_STYLE);
				if (keyNode == 0)
					m_Error = true;
				yaml_document_append_mapping_pair(&m_Document, parentNode, keyNode, valueNode);
			}
			break;

		case YAML_SEQUENCE_NODE:
			yaml_document_append_sequence_item(&m_Document, parentNode, valueNode);
			break;
		
		default:
			ErrorString("Unexpected node type.");
	}
}

int YAMLWrite::StringOutputHandler(void *data, unsigned char *buffer, size_t size) 
{
	string* theString = reinterpret_cast<string*> (data);
	theString->append( (char *) buffer, size);
	return 1;
}

int YAMLWrite::CacheOutputHandler(void *data, unsigned char *buffer, size_t size) 
{
	CachedWriter* cache = reinterpret_cast<CachedWriter*> (data);		
	cache->Write(buffer, size);
	return 1;
}

void YAMLWrite::OutputToHandler (yaml_write_handler_t *handler, void *data)
{
	yaml_node_t *root = yaml_document_get_root_node (&m_Document);
	if (root->type == YAML_MAPPING_NODE && root->data.mapping.pairs.start != root->data.mapping.pairs.top)
	{
		yaml_emitter_t emitter;
		memset(&emitter, 0, sizeof(emitter));
		
		if (!yaml_emitter_initialize (&emitter))
		{
			ErrorStringMsg ("Unable to write text file %s: yaml_emitter_initialize failed.", m_DebugFileName.c_str());	
			return;
		}
		
		yaml_emitter_set_output(&emitter, handler, data );
		yaml_emitter_dump(&emitter, &m_Document);
		
		if (emitter.error != YAML_NO_ERROR)
			ErrorStringMsg ("Unable to write text file %s: %s.", m_DebugFileName.c_str(), emitter.problem);	

		yaml_emitter_delete(&emitter);
	}
}

YAMLWrite::YAMLWrite (int flags, std::string *debugFileName) 
{ 
	if (debugFileName)
		m_DebugFileName = *debugFileName;

	memset(&m_Document, 0, sizeof(m_Document));
		
	m_CurrentNode = -1;
	m_Flags = flags;
	m_Error = false;
	m_MetaFlags.push_back (0);
	m_UserData = NULL;

	if (!yaml_document_initialize(&m_Document, NULL, NULL, NULL, 1, 1))
	{
		ErrorStringMsg ("Unable to write text file %s: yaml_document_initialize failed.", m_DebugFileName.c_str());	
		m_Error = true;
	}
}

YAMLWrite::~YAMLWrite()
{
	yaml_document_delete(&m_Document);
}

void YAMLWrite::OutputToCachedWriter (CachedWriter* writer)
{
	if (m_Error)
	{
		ErrorStringMsg ("Could not serialize text file %s because an error occured - we probably ran out of memory.", m_DebugFileName.c_str());
		return;
	}
	OutputToHandler (CacheOutputHandler, reinterpret_cast<void *>(writer));
}

void YAMLWrite::OutputToString (std::string& str)
{
	if (m_Error)
	{
		ErrorStringMsg ("Could not serialize text file %s because an error occured - we probably ran out of memory.", m_DebugFileName.c_str());
		return;
	}
	
	OutputToHandler (StringOutputHandler, reinterpret_cast<void *>(&str));
}

void YAMLWrite::SetVersion (int version) 
{
	char valueStr[256];
	snprintf(valueStr, 256, "%d", version);
	int value = yaml_document_add_scalar(&m_Document, NULL, (yaml_char_t*)valueStr, strlen(valueStr), YAML_ANY_SCALAR_STYLE);

	AppendToNode (GetNode(), "serializedVersion", value);
}

void YAMLWrite::BeginMetaGroup (std::string name)
{
	m_MetaParents.push_back (MetaParent());
	m_MetaParents.back().node = GetNode ();
	m_MetaParents.back().name = name;
	m_CurrentNode = NewMapping ();
}

void YAMLWrite::EndMetaGroup ()
{
	AppendToNode (m_MetaParents.back().node, m_MetaParents.back().name.c_str(), m_CurrentNode);
	m_CurrentNode = m_MetaParents.back().node;
	m_MetaParents.pop_back();
}

void YAMLWrite::StartSequence ()
{
	m_CurrentNode = NewSequence();
}

void YAMLWrite::TransferTypelessData (unsigned size, void* data, int metaFlag) 
{
	UnityStr dataString;
	dataString.resize (size * 2);
	BytesToHexString (data, size, &dataString[0]);
	Transfer(dataString, "_typelessdata", metaFlag);
}

