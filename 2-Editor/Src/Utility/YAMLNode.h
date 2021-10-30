#ifndef YAMLNODE_H
#define YAMLNODE_H
/*
 *  YAMLNode.h
 *  Xcode
 *
 *  Created by Hrafnkell Hlodversson on 2009-05-05.
 *  Copyright 2009 Unity Technologies ApS. All rights reserved.
 *  A simple wrapper around libYaml for reading and saving yaml files.
 *
 */

#include "Runtime/Utilities/GUID.h"
#include "Runtime/Utilities/Word.h"
#include "Runtime/BaseClasses/BaseObject.h"
#include <list>

typedef struct yaml_node_s yaml_node_t;
typedef struct yaml_document_s yaml_document_t;

using std::string;
using std::vector;
using std::pair;
using std::make_pair;
using std::multimap;

class YAMLSequence;
class YAMLScalar;
class YAMLMapping;


class YAMLNode {
public:
	virtual ~YAMLNode();

	string EmitYAMLString() ;
	bool EmitYAMLFile(const string& filePath) ;
	virtual int PopulateDocument(yaml_document_t* doc)=0; 
};

class YAMLScalar : public YAMLNode {

private:	
	int intValue;
	bool intValid;
	
	float floatValue;
	bool floatValid;
	
	string stringValue;
	bool stringValid;
	
public:
	YAMLScalar() : intValid(false), floatValid(false), stringValid(false) {}
	YAMLScalar(SInt32 i) : intValid(true), floatValid(false), stringValid(false), intValue(i) {} 
	YAMLScalar(UInt32 i) : intValid(true), floatValid(false), stringValid(false), intValue(i) {} 
	YAMLScalar(UInt64 i) : intValid(false), floatValid(false), stringValid(false) {} 
	YAMLScalar(float f) : intValid(false), floatValid(true), stringValid(false), floatValue(f) {}
	 
	YAMLScalar(const UnityGUID& g) : intValid(false), floatValid(false), stringValid(true), stringValue(GUIDToString(g)) {}
	YAMLScalar(const string& s) : intValid(false), floatValid(false), stringValid(true), stringValue(s) {}
	YAMLScalar(const char* s) : intValid(false), floatValid(false), stringValid(true), stringValue(string(s)) {}
	
	bool Defined() { return intValid || floatValid || stringValid; } 
	
	int	GetIntValue();
	float GetFloatValue();
	const string & GetStringValue();

	operator UInt64() { return 0; }
	operator SInt32() { return GetIntValue(); }
	operator UInt32() { return GetIntValue(); }
	operator SInt16() { return GetIntValue(); }
	operator UInt16() { return GetIntValue(); }
	operator SInt8() { return GetIntValue(); }
	operator UInt8() { return GetIntValue(); }
	operator char() { return GetIntValue(); }
	operator float() { return GetFloatValue(); }
	operator const string& () { return GetStringValue(); }
	operator const UnityGUID () { return StringToGUID(GetStringValue()); }
	
	virtual int PopulateDocument(yaml_document_t* doc); 
};

template<class T>
YAMLNode* MakeYAMLNode(const T& value) 
{
	return (YAMLNode*) new YAMLScalar(value);
}

YAMLNode* MakeYAMLNode(const PPtr<Object>& value);
YAMLNode* MakeYAMLNode(YAMLNode * value);
YAMLNode* MakeYAMLNode(YAMLScalar * value);
YAMLNode* MakeYAMLNode(YAMLSequence * value);
YAMLNode* MakeYAMLNode(YAMLMapping * value);

class YAMLMapping : public YAMLNode {
private:
	vector<pair<YAMLScalar*,YAMLNode*> > content;
	multimap<string, int> index;
	bool useInlineStyle;


public:
	YAMLMapping(): useInlineStyle (false) {}
	explicit YAMLMapping(bool inlineStyle): useInlineStyle (inlineStyle) {}
	YAMLMapping(const PPtr<Object>& value) ;

	void Reserve (size_t size) {
		content.reserve (size);
	}
	
	void Append(YAMLScalar* key, YAMLNode* value) {
		content.push_back(make_pair(key, value));
		index.insert(make_pair(key->GetStringValue(), content.size()-1));
	}
	
	template<class T> void Append(YAMLScalar* key, const T& value) {
		Append( key, MakeYAMLNode(value) );
	}
	template<class T1,class T2> void Append(const T1& key, const T2& value) {
		Append( new YAMLScalar(key), MakeYAMLNode(value) );
	}

	PPtr<Object> GetPPtr() const;
	
	YAMLNode* Get (const string& key)  const {
		multimap<string, int>::const_iterator found=index.find(key);
		if( found != index.end() )
			return content[found->second].second;
		return NULL;
	}

	void Remove (const string& key);

	int size() const { return content.size(); }
	
	typedef vector<pair<YAMLScalar*,YAMLNode*> >::const_iterator const_iterator;
	typedef vector<pair<YAMLScalar*,YAMLNode*> >::iterator iterator;
	const_iterator begin() const {
		return content.begin();
	}
	const_iterator end() const {
		return content.end();
	}
	iterator begin() { return content.begin(); }
	iterator end() { return content.end(); }

	void Clear ()
	{
		for (vector<pair<YAMLScalar*,YAMLNode*> >::iterator i = content.begin(); i != content.end(); i++) {
			delete i->first;
			delete i->second;
		}
		content.clear();
		index.clear();
	}
	
	~YAMLMapping() {
		Clear();
	}

	virtual int PopulateDocument(yaml_document_t* doc);
};

class YAMLSequence : public YAMLNode {
private:
	vector<YAMLNode*> content;

public:
	YAMLSequence(){}

	void Reserve (size_t size) {
		content.reserve (size);
	}
	void Append(YAMLNode* value) {
		content.push_back(value);
	}
	template<class T> void Append(const T& value) {
		Append( MakeYAMLNode(value) );
	}
	
	YAMLNode* Get (int key)  const {
		if (key < 0 || key >= content.size()) return NULL;
		return content[key];
	}
	
	int size() { return content.size(); }
	
	typedef vector<YAMLNode*>::const_iterator const_iterator;
	const_iterator begin() {
		return content.begin();
	}
	const_iterator end() {
		return content.end();
	}
	
	~YAMLSequence() {
		for (vector<YAMLNode*>::iterator i = content.begin(); i != content.end(); i++) {
			delete *i;
		}
	}
	virtual int PopulateDocument(yaml_document_t* doc); 
};


YAMLNode* ParseYAMLString( string yaml ) ;
YAMLNode* ParseYAMLFile( const string& filename ) ;

YAMLNode* YAMLDocNodeToNode(yaml_document_t* doc, yaml_node_t*);

#endif
