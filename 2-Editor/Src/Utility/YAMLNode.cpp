/*
 *  YAMLNode.cpp
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-05-05.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *  A simple wrapper around libYaml for reading and saving yaml files.
 *
 */
#include "UnityPrefix.h" 
#include <sstream>
#include <locale>

#include "YAMLNode.h"
#include "External/yaml/include/yaml.h"
#include "Runtime/Utilities/File.h"
#include "Runtime/Utilities/FileUtilities.h"
#include "Editor/Src/GUIDPersistentManager.h"
#include "Runtime/Serialize/SerializedFile.h"

YAMLNode::~YAMLNode()
{}


YAMLNode* YAMLDocNodeToNode(yaml_document_t* doc, yaml_node_t*);
YAMLScalar* YAMLDocNodeToScalar(yaml_document_t* doc, yaml_node_t*);
YAMLMapping* YAMLDocNodeToMapping(yaml_document_t* doc, yaml_node_t*, bool useInlineStyle = false);
YAMLSequence* YAMLDocNodeToSequence(yaml_document_t* doc, yaml_node_t*);

int YAMLScalar::GetIntValue () 
{
	if (! intValid ) 
	{
		if( floatValid ) 
		{
			intValue = (int)floatValue;
			intValid = true;
		}
		else if ( stringValid ) 
		{
			std::istringstream convert(stringValue.c_str());
			convert.imbue(std::locale("C")); // Ensure there are no std::locale issues when converting numbers to strings and back again
			convert >> intValue;
			intValid = true;
		}
		else 
		{
			return 0;
		}
	} 
	return intValue;
}

float YAMLScalar::GetFloatValue () 
{
	if (! floatValid ) 
	{
		if ( stringValid ) 
		{
			std::istringstream convert(stringValue.c_str());
			convert.imbue(std::locale("C")); // Ensure there are no std::locale issues when converting numbers to strings and back again
			convert >> floatValue;
			floatValid = true;
		}
		else if( intValid ) 
		{
			floatValue = (float)intValue;
			floatValid = true;
		}
		else 
		{
			return 0.0;
		}
	}
	return floatValue;
}

const string& YAMLScalar::GetStringValue () 
{
	if (!stringValid) 
	{
		std::ostringstream convert ;
		convert.imbue(std::locale("C")); // Ensure there are no std::locale issues when converting numbers to strings and back again
		if( floatValid ) 
		{
			convert.precision(7); // 7 digits of precision is about what a float can handle
			convert << floatValue;
			stringValue = std::string(convert.str().c_str());
			
			stringValid = true;
		}
		else if ( intValid ) 
		{
			convert << intValue;
			stringValue = std::string(convert.str().c_str());
			
			stringValid = true;
		}
	}
	
	return stringValue;
}

YAMLNode* ParseYAMLString(string input) 
{
	YAMLNode* result;
	yaml_parser_t parser;
	yaml_document_t document;
	memset(&parser, 0, sizeof(parser));
	memset(&document, 0, sizeof(document));
	
	
	/* Initialize the parser and document objects. */	
	if (!yaml_parser_initialize(&parser)) 
	{
		// todo handle error: Could not initialize the parser object
		return NULL;
	}
	
	yaml_parser_set_input_string(&parser,(const yaml_char_t*) input.c_str(), input.size() );
	yaml_parser_load(&parser, &document);
	
	result = YAMLDocNodeToNode(&document, yaml_document_get_root_node(&document));
	
	yaml_document_delete(&document);
	yaml_parser_delete(&parser);
	
	return result;
	
}

YAMLNode* ParseYAMLFile(const string& filename ) 
{
	InputString yaml;
	if (! ReadStringFromFile(&yaml, filename) )
		return NULL;
	return ParseYAMLString(yaml.c_str()); // Todo use an yaml_parser input handler instead of ReadStringFromFile
}


YAMLNode* YAMLDocNodeToNode(yaml_document_t* doc, yaml_node_t* node) 
{
	if(! node ) return NULL;
	switch( node->type ) 
	{
		case YAML_NO_NODE:
			return NULL;
		case YAML_SCALAR_NODE:
			return YAMLDocNodeToScalar(doc, node);
		case YAML_MAPPING_NODE:
		{
			bool useInlineStyle = node->data.mapping.style == YAML_FLOW_MAPPING_STYLE;
			return YAMLDocNodeToMapping(doc, node, useInlineStyle);
		}
		case YAML_SEQUENCE_NODE:
			return YAMLDocNodeToSequence(doc, node);
	}
	return NULL;
}

YAMLScalar* YAMLDocNodeToScalar(yaml_document_t* doc, yaml_node_t* node) 
{
	return new YAMLScalar(string((const char*)node->data.scalar.value));
}

YAMLMapping* YAMLDocNodeToMapping(yaml_document_t* doc, yaml_node_t* node, bool useInlineStyle)
{
	yaml_node_pair_t* start = node->data.mapping.pairs.start;
	yaml_node_pair_t* top   = node->data.mapping.pairs.top;
	
	YAMLMapping* result = new YAMLMapping(useInlineStyle);
	result->Reserve (top - start);
	for(yaml_node_pair_t* i = start; i != top; i++) 
	{
		YAMLScalar* key=dynamic_cast<YAMLScalar*> (YAMLDocNodeToNode(doc, yaml_document_get_node(doc, i->key)));
		// TODO : fix potential memory leak - key is not released if dynamic_cast fails
		if( ! key ) 
		{
			// TODO log error
			continue;
		}
		result->Append(key,YAMLDocNodeToNode(doc, yaml_document_get_node(doc, i->value)));
	}
					   
	return result;
	
}

YAMLSequence* YAMLDocNodeToSequence(yaml_document_t* doc, yaml_node_t* node) 
{
	yaml_node_item_t* start = node->data.sequence.items.start;
	yaml_node_item_t* top   = node->data.sequence.items.top;
	
	YAMLSequence* result = new YAMLSequence();
	result->Reserve (top - start);
	for(yaml_node_item_t* i = start; i != top; i++) 
	{
		result->Append(YAMLDocNodeToNode(doc, yaml_document_get_node(doc, *i)));
	}
					   
	return result;
}

static int StringOutputHandler(void *data, unsigned char *buffer, size_t size) 
{
	string* theString = reinterpret_cast<string*> (data);
	theString->append( (char *) buffer, size);
	return 1;
}

string YAMLNode::EmitYAMLString() 
{
	yaml_emitter_t emitter;
	yaml_document_t document;
	memset(&emitter, 0, sizeof(emitter));
	memset(&document, 0, sizeof(document));
	
	AssertIf(!yaml_document_initialize(&document, NULL, NULL, NULL, 1, 1));
	AssertIf(!yaml_emitter_initialize(&emitter));
	
	string result;
	
	PopulateDocument(&document);
	
	yaml_emitter_set_output(&emitter, StringOutputHandler, reinterpret_cast<void *>(&result) );
	yaml_emitter_dump(&emitter, &document);
	yaml_document_delete(&document);
	yaml_emitter_delete(&emitter);
	
	return result;
}

int YAMLScalar::PopulateDocument(yaml_document_t* doc) 
{
	const string & stringValue = GetStringValue();
	
	return yaml_document_add_scalar(doc, NULL, (yaml_char_t*) stringValue.c_str(), stringValue.size()	, YAML_ANY_SCALAR_STYLE);
}

int YAMLMapping::PopulateDocument(yaml_document_t* doc) 
{
	int nodeId = yaml_document_add_mapping(doc, NULL, useInlineStyle?YAML_FLOW_MAPPING_STYLE:YAML_ANY_MAPPING_STYLE);
	
	for ( const_iterator i = begin(); i != end(); i++) 
	{
		int keyId = i->first->PopulateDocument(doc);
		int valueId = i->second->PopulateDocument(doc);
		yaml_document_append_mapping_pair(doc, nodeId, keyId, valueId);
	}
	
	return nodeId;
}

int YAMLSequence::PopulateDocument(yaml_document_t* doc) 
{
	int nodeId = yaml_document_add_sequence(doc, NULL, YAML_ANY_SEQUENCE_STYLE);	
	for ( const_iterator i = begin(); i != end(); i++) 
	{
		int itemId = (*i)->PopulateDocument(doc);
		yaml_document_append_sequence_item(doc, nodeId, itemId);
	}
	
	return nodeId;
}

YAMLMapping::YAMLMapping(const PPtr<Object>& value) : useInlineStyle(true)
{
	GUIDPersistentManager& pm = GetGUIDPersistentManager();
	pm.Lock();
	
	SerializedObjectIdentifier identifier;
	if (pm.InstanceIDToSerializedObjectIdentifier(value.GetInstanceID(), identifier))
	{
		FileIdentifier id = pm.PathIDToFileIdentifierInternal(identifier.serializedFileIndex);
		#if LOCAL_IDENTIFIER_IN_FILE_SIZE > 32
		--- support more than 32 bit numbers
		#endif
		Append("fileID", (int)identifier.localIdentifierInFile);
		Append("guid", id.guid);
		Append("type", id.type);
	}
	pm.Unlock();
}

void YAMLMapping::Remove (const string& key)
{
	multimap<string, int>::iterator found=index.find(key);
	if( found != index.end() ) {
		int idx = found->second;
		
		// reindex map: update indices that are >found->second
		index.erase (found);
		for (multimap<string, int>::iterator it = index.begin (), end = index.end (); it != end; ++it)
		{
			if (it->second > idx)
				it->second--;
		}
		
		delete content[idx].first;
		delete content[idx].second;
		content.erase (content.begin () + idx);
	}
}


PPtr<Object> YAMLMapping::GetPPtr() const
{
	LocalIdentifierInFileType fileID;
	UnityGUID guid;
	int fileType;
	YAMLScalar* tmp;
	GUIDPersistentManager& pm = GetGUIDPersistentManager ();
	
	
	tmp = dynamic_cast<YAMLScalar*>( Get("fileID") );
	if ( ! tmp ) 
		return NULL;
	else
		fileID=int(*tmp);
	#if LOCAL_IDENTIFIER_IN_FILE_SIZE > 32
	--- support more than 32 bit numbers for fileID
	#endif
		
	
	tmp = dynamic_cast<YAMLScalar*>( Get("guid") );
	if ( ! tmp ) 
		return NULL;
	else
		guid=(UnityGUID)*tmp;

	tmp = dynamic_cast<YAMLScalar*>( Get("type") );
	if ( ! tmp ) 
		return NULL;
	else
		fileType=(int)*tmp;
		
	string pathName = GetPathNameFromGUIDAndType (guid, fileType);
	if (pathName.empty ())
		return NULL;

	return PPtr<Object>(pm.GetInstanceIDFromPathAndFileID(pathName, fileID));
}

YAMLNode* MakeYAMLNode(YAMLNode * value)
{
	return (YAMLNode*) value;
}

YAMLNode* MakeYAMLNode(YAMLScalar * value)
{
	return (YAMLNode*) value;
}

YAMLNode* MakeYAMLNode(YAMLSequence * value)
{
	return (YAMLNode*) value;
}

YAMLNode* MakeYAMLNode(YAMLMapping * value)
{
	return (YAMLNode*) value;
}

YAMLNode* MakeYAMLNode(const PPtr<Object>& value) 
{
	return (YAMLNode*) new YAMLMapping(value);
}

